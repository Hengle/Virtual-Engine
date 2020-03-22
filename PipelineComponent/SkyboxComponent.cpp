#include "SkyboxComponent.h"
#include "../Singleton/ShaderID.h"
#include "../RenderComponent/Skybox.h"
#include "../Singleton/PSOContainer.h"
#include "RenderPipeline.h"
#include "../RenderComponent/Texture.h"
#include "../RenderComponent/MeshRenderer.h"
#include "../RenderComponent/Terrain/VirtualTexture.h"
#include "LightingComponent.h";
#include "../Singleton/ShaderCompiler.h"
#include "../LogicComponent/World.h"
#include "../RenderComponent/GRPRenderManager.h"
#include "DepthComponent.h"
#include "../RenderComponent/PBRMaterial.h"
#include "../RenderComponent/Terrain/TerrainDrawer.h"
//#include "BaseColorComponent.h"
StackObject<Skybox> defaultSkybox;
uint _DefaultMaterials;
StackObject<PSOContainer> gbufferContainer;
DepthComponent* depthComp;
Shader* terrainTestShader;
//PrepareComponent* 
struct TextureIndices
{
	uint _SkyboxCubemap;
	uint _PreintTexture;
};
const uint TEX_COUNT = sizeof(TextureIndices) / 4;

LightingComponent* lightComp_GBufferGlobal;
uint SkyboxComponent::_AllLight(0);
uint SkyboxComponent::_LightIndexBuffer(0);
uint SkyboxComponent::LightCullCBuffer(0);
uint SkyboxComponent::TextureIndices(0);
uint SkyboxComponent::_GreyTex;
uint SkyboxComponent::_IntegerTex;
uint SkyboxComponent::_Cubemap;

struct SkyboxBuffer
{
	XMFLOAT4X4 invvp;
};

class SkyboxPerCameraData : public IPipelineResource
{
public:
	UploadBuffer posBuffer;
	SkyboxPerCameraData(ID3D12Device* device) :
		posBuffer(device,
			3, true,
			sizeof(SkyboxBuffer))
	{
	}
};

struct GBufferCameraData : public IPipelineResource
{
	UploadBuffer texIndicesBuffer;
	uint descs[TEX_COUNT * 3];
	GBufferCameraData(ID3D12Device* device) :
		texIndicesBuffer(
			device, 3, true, sizeof(TextureIndices)
		)
	{
		World* world = World::GetInstance();
		for (uint i = 0; i < TEX_COUNT * 3; ++i)
		{
			descs[i] = world->GetDescHeapIndexFromPool();
		}
		TextureIndices ind;
		memcpy(&ind, descs, sizeof(uint) * TEX_COUNT);
		texIndicesBuffer.CopyData(0, &ind);
		texIndicesBuffer.CopyData(1, &ind);
		texIndicesBuffer.CopyData(2, &ind);
	}

	~GBufferCameraData()
	{
		World* world = World::GetInstance();
		if (world)
		{
			for (uint i = 0; i < TEX_COUNT * 3; ++i)
			{
				world->ReturnDescHeapIndexToPool(descs[i]);
			}
		}
	}
};

XMMATRIX CalculateViewMatrix(Camera* cam)
{
	XMVECTOR R = cam->GetRight();
	XMVECTOR U = cam->GetUp();
	XMVECTOR L = cam->GetLook();
	// Keep camera's axes orthogonal to each other and of unit length.
	L = XMVector3Normalize(L);
	U = XMVector3Normalize(XMVector3Cross(L, R));

	// U, L already ortho-normal, so no need to normalize cross product.
	R = XMVector3Cross(U, L);

	XMFLOAT3* mRight = (XMFLOAT3*)&R;
	XMFLOAT3* mUp = (XMFLOAT3*)&U;
	XMFLOAT3* mLook = (XMFLOAT3*)&L;
	XMFLOAT4X4 mView;
	mView(0, 0) = mRight->x;
	mView(1, 0) = mRight->y;
	mView(2, 0) = mRight->z;
	mView(3, 0) = 0;

	mView(0, 1) = mUp->x;
	mView(1, 1) = mUp->y;
	mView(2, 1) = mUp->z;
	mView(3, 1) = 0;

	mView(0, 2) = mLook->x;
	mView(1, 2) = mLook->y;
	mView(2, 2) = mLook->z;
	mView(3, 2) = 0;

	mView(0, 3) = 0.0f;
	mView(1, 3) = 0.0f;
	mView(2, 3) = 0.0f;
	mView(3, 3) = 1.0f;
	return *(XMMATRIX*)&mView;
}

class SkyboxRunnable
{
public:
	RenderTexture* emissionTex;
	RenderTexture* mvTex;
	RenderTexture* depthTex;
	RenderTexture* gbuffer0Tex;
	RenderTexture* gbuffer1Tex;
	RenderTexture* gbuffer2Tex;
	SkyboxComponent* selfPtr;
	ThreadCommand* commandList;
	FrameResource* resource;
	ID3D12Device* device;
	Camera* cam;
	uint frameIndex;
	void RenderGBuffer()
	{
		char* selfPtr = (char*)this->selfPtr + 1;//This is only just a unique key
		auto commandList = this->commandList->GetCmdList();
		auto world = World::GetInstance();
		DepthCameraResource* depthCameraRes = (DepthCameraResource*)cam->GetResource(depthComp, [=]()->DepthCameraResource*
			{
				return new DepthCameraResource(device);
			});

		GBufferCameraData* camData = (GBufferCameraData*)cam->GetResource(selfPtr, [&]()->GBufferCameraData*
			{
				return new GBufferCameraData(device);
			});



		LightFrameData* lightFrameData = (LightFrameData*)resource->GetPerCameraResource(lightComp_GBufferGlobal, cam, []()->LightFrameData*
			{
#ifndef NDEBUG
				throw "No Light Data Exception!";
#endif
				return nullptr;	//Get Error if there is no light coponent in pipeline
			});
		LightCameraData* lightCameraData = (LightCameraData*)cam->GetResource(lightComp_GBufferGlobal, []()->LightCameraData*
			{
#ifndef NDEBUG
				throw "No Light Data Exception!";
#endif
				return nullptr;	//Get Error if there is no light coponent in pipeline
			});
		DescriptorHeap* worldHeap = World::GetInstance()->GetGlobalDescHeap();
		this->selfPtr->skyboxTex->BindSRVToHeap(worldHeap, camData->descs[0 + TEX_COUNT * frameIndex], device);
		this->selfPtr->preintTexture->BindSRVToHeap(worldHeap, camData->descs[1 + TEX_COUNT * frameIndex], device);
		Shader* gbufferShader = world->GetGRPRenderManager()->GetShader();

		auto setGBufferShaderFunc = [&]()->void
		{
			gbufferShader->BindRootSignature(commandList, worldHeap);
			gbufferShader->SetResource(commandList, ShaderID::GetMainTex(), worldHeap, 0);
			gbufferShader->SetResource(commandList, SkyboxComponent::_GreyTex, worldHeap, 0);
			gbufferShader->SetResource(commandList, SkyboxComponent::_IntegerTex, worldHeap, 0);
			gbufferShader->SetResource(commandList, SkyboxComponent::_Cubemap, worldHeap, 0);
			gbufferShader->SetResource(commandList, _DefaultMaterials, world->GetPBRMaterialManager()->GetMaterialBuffer(), 0);
			gbufferShader->SetStructuredBufferByAddress(commandList, SkyboxComponent::_AllLight, lightFrameData->lightsInFrustum.GetAddress(0));
			gbufferShader->SetStructuredBufferByAddress(commandList, SkyboxComponent::_LightIndexBuffer, lightComp_GBufferGlobal->lightIndexBuffer->GetAddress(0, 0));
			gbufferShader->SetResource(commandList, SkyboxComponent::LightCullCBuffer, &lightCameraData->lightCBuffer, frameIndex);
			gbufferShader->SetResource(commandList, SkyboxComponent::TextureIndices, &camData->texIndicesBuffer, frameIndex);
		};
		setGBufferShaderFunc();
		D3D12_CPU_DESCRIPTOR_HANDLE handles[5];
		handles[0] = gbuffer0Tex->GetColorDescriptor(0, 0);
		handles[1] = gbuffer1Tex->GetColorDescriptor(0, 0);
		handles[2] = gbuffer2Tex->GetColorDescriptor(0, 0);
		handles[3] = mvTex->GetColorDescriptor(0, 0);
		handles[4] = emissionTex->GetColorDescriptor(0, 0);
		emissionTex->SetViewport(commandList);
		//Draw GBuffer
		commandList->OMSetRenderTargets(5, handles, false, &depthTex->GetColorDescriptor(0, 0));
		world->GetGRPRenderManager()->DrawCommand(
			commandList,
			device,
			0,
			ShaderID::GetPerCameraBufferID(),
			resource->cameraCBs[cam->GetInstanceID()],
			gbufferContainer, 0
		);
		if (world->terrainDrawer)
		{
			terrainTestShader->BindRootSignature(commandList);
			world->terrainDrawer->Draw(
				device,
				commandList,
				terrainTestShader,
				0,
				gbufferContainer, 0,
				resource->cameraCBs[cam->GetInstanceID()],
				resource,
				world->virtualTexture
			);
		}
		//Recheck Culling
		ConstBufferElement cullEle;
		cullEle.buffer = &depthCameraRes->cullBuffer;
		cullEle.element = 0 + DepthCameraResource::descHeapSize * frameIndex;
		depthCameraRes->hizTexture->BindSRVToHeap(World::GetInstance()->GetGlobalDescHeap(), depthCameraRes->descIndex, device);

		//Culling
		world->GetGRPRenderManager()->OcclusionRecheck(
			commandList,
			*this->commandList->GetBarrierBuffer(),
			device,
			resource,
			cullEle,
			depthCameraRes->descIndex);
		emissionTex->SetViewport(commandList);
		commandList->OMSetRenderTargets(0, nullptr, true, &depthTex->GetColorDescriptor(0, 0));
		setGBufferShaderFunc();
		this->commandList->GetBarrierBuffer()->ExecuteCommand(commandList);
		world->GetGRPRenderManager()->DrawCommand(
			commandList,
			device,
			1,
			ShaderID::GetPerCameraBufferID(),
			resource->cameraCBs[cam->GetInstanceID()],
			gbufferContainer, 1
		);

		commandList->OMSetRenderTargets(5, handles, false, &depthTex->GetColorDescriptor(0, 0));
		world->GetGRPRenderManager()->DrawCommand(
			commandList,
			device,
			0,
			ShaderID::GetPerCameraBufferID(),
			resource->cameraCBs[cam->GetInstanceID()],
			gbufferContainer, 0
		);
	}

	void operator()()
	{
		commandList->ResetCommand();

		auto world = World::GetInstance();
		ID3D12GraphicsCommandList* cmdList = commandList->GetCmdList();
		if (!selfPtr->preintTexture)
		{
			RenderTextureFormat format;
			format.colorFormat = DXGI_FORMAT_R16G16_UNORM;
			selfPtr->preintContainer.New(
				DXGI_FORMAT_UNKNOWN,
				1,
				&format.colorFormat
			);

			format.usage = RenderTextureUsage::ColorBuffer;
			selfPtr->preintTexture = new RenderTexture(
				device,
				256,
				256,
				format,
				TextureDimension::Tex2D,
				1,
				0);
			Shader* preintShader = ShaderCompiler::GetShader("PreInt");
			preintShader->BindRootSignature(cmdList);
			Graphics::Blit(
				cmdList,
				device,
				&selfPtr->preintTexture->GetColorDescriptor(0, 0),
				1,
				nullptr,
				selfPtr->preintContainer, 0,
				selfPtr->preintTexture->GetWidth(),
				selfPtr->preintTexture->GetHeight(),
				preintShader,
				0
			);
		}
		RenderGBuffer();
		SkyboxPerCameraData* camData = (SkyboxPerCameraData*)cam->GetResource(selfPtr, [&]()->SkyboxPerCameraData*
			{
				return new SkyboxPerCameraData(device);
			});
		XMMATRIX view = CalculateViewMatrix(cam);
		XMMATRIX viewProj = XMMatrixMultiply(view, cam->GetProj());
		SkyboxBuffer bf;
		memcpy(&bf.invvp, &XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj), sizeof(XMFLOAT4X4));
		camData->posBuffer.CopyData(frameIndex, &bf);
		emissionTex->SetViewport(cmdList);
		D3D12_CPU_DESCRIPTOR_HANDLE rtHandles[2];
		rtHandles[0] = emissionTex->GetColorDescriptor(0, 0);
		rtHandles[1] = mvTex->GetColorDescriptor(0, 0);
		D3D12_CPU_DESCRIPTOR_HANDLE depthHandle = depthTex->GetColorDescriptor(0, 0);
		auto format = depthTex->GetFormat();
		cmdList->OMSetRenderTargets(
			2,
			rtHandles,
			false,
			&depthHandle
		);
		ConstBufferElement skyboxData;
		skyboxData.buffer = &camData->posBuffer;
		skyboxData.element = frameIndex;
		defaultSkybox->Draw(
			0,
			cmdList,
			device,
			&skyboxData,
			resource,
			gbufferContainer, 2
		);
		commandList->CloseCommand();
	}
};

std::vector<TemporalResourceCommand>& SkyboxComponent::SendRenderTextureRequire(EventData& evt)
{
	for (int i = 0; i < tempRT.size(); ++i)
	{
		tempRT[i].descriptor.rtDesc.width = evt.width;
		tempRT[i].descriptor.rtDesc.height = evt.height;
	}
	return tempRT;
}
void SkyboxComponent::RenderEvent(EventData& data, ThreadCommand* commandList)
{
	ScheduleJob<SkyboxRunnable>(
		{
			 (RenderTexture*)allTempResource[0],
			  (RenderTexture*)allTempResource[1],
			  (RenderTexture*)allTempResource[2],
			  (RenderTexture*)allTempResource[3],
			  (RenderTexture*)allTempResource[4],
			  (RenderTexture*)allTempResource[5],
			 this,
			 commandList,
			 data.resource,
			 data.device,
			 data.camera,
			 data.ringFrameIndex
		});
}


void SkyboxComponent::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	SetCPUDepending<LightingComponent>();
	SetGPUDepending<LightingComponent>();
	tempRT.resize(6);
	terrainTestShader = ShaderCompiler::GetShader("Terrain");
	lightComp_GBufferGlobal = RenderPipeline::GetComponent<LightingComponent>();
	depthComp = RenderPipeline::GetComponent<DepthComponent>();
	TemporalResourceCommand& emissionBuffer = tempRT[0];
	emissionBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	emissionBuffer.uID = ShaderID::PropertyToID("_CameraRenderTarget");
	emissionBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	emissionBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	emissionBuffer.descriptor.rtDesc.depthSlice = 1;
	emissionBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	emissionBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;
	tempRT[1].type = TemporalResourceCommand::CommandType_Require_RenderTexture;
	tempRT[1].uID = ShaderID::PropertyToID("_CameraMotionVectorsTexture");
	tempRT[2].type = TemporalResourceCommand::CommandType_Require_RenderTexture;
	tempRT[2].uID = ShaderID::PropertyToID("_CameraDepthTexture");

	TemporalResourceCommand& specularBuffer = tempRT[3];
	specularBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	specularBuffer.uID = ShaderID::PropertyToID("_CameraGBufferTexture0");
	specularBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	specularBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	specularBuffer.descriptor.rtDesc.depthSlice = 1;
	specularBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	specularBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;
	TemporalResourceCommand& albedoBuffer = tempRT[4];
	albedoBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	albedoBuffer.uID = ShaderID::PropertyToID("_CameraGBufferTexture1");
	albedoBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	albedoBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	albedoBuffer.descriptor.rtDesc.depthSlice = 1;
	albedoBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	albedoBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;
	TemporalResourceCommand& normalBuffer = tempRT[5];
	normalBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	normalBuffer.uID = ShaderID::PropertyToID("_CameraGBufferTexture2");
	normalBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
	normalBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	normalBuffer.descriptor.rtDesc.depthSlice = 1;
	normalBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	normalBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;

	PSORTSetting settings[3];
	settings[0].rtCount = 5;
	settings[0].rtFormat[0] = albedoBuffer.descriptor.rtDesc.rtFormat.colorFormat;
	settings[0].rtFormat[1] = specularBuffer.descriptor.rtDesc.rtFormat.colorFormat;
	settings[0].rtFormat[2] = normalBuffer.descriptor.rtDesc.rtFormat.colorFormat;
	settings[0].rtFormat[3] = DXGI_FORMAT_R16G16_SNORM;
	settings[0].rtFormat[4] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	settings[0].depthFormat = DXGI_FORMAT_D32_FLOAT;
	settings[1].rtCount = 0;
	settings[1].depthFormat = DXGI_FORMAT_D32_FLOAT;
	settings[2].depthFormat = DXGI_FORMAT_D32_FLOAT;
	settings[2].rtCount = 2;
	settings[2].rtFormat[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	settings[2].rtFormat[1] = DXGI_FORMAT_R16G16_SNORM;

	gbufferContainer.New(settings, 3);
	skyboxTex = ObjectPtr< Texture>::MakePtr(new Texture(
		device,
		"Resource/Sky.vtex",
		TextureDimension::Cubemap
	));
	defaultSkybox.New(
		skyboxTex,
		device
	);
	_AllLight = ShaderID::PropertyToID("_AllLight");
	_LightIndexBuffer = ShaderID::PropertyToID("_LightIndexBuffer");
	LightCullCBuffer = ShaderID::PropertyToID("LightCullCBuffer");
	TextureIndices = ShaderID::PropertyToID("TextureIndices");
	_DefaultMaterials = ShaderID::PropertyToID("_DefaultMaterials");
	_GreyTex = ShaderID::PropertyToID("_GreyTex");
	_IntegerTex = ShaderID::PropertyToID("_IntegerTex");
	_Cubemap = ShaderID::PropertyToID("_Cubemap");
}
void SkyboxComponent::Dispose()
{
	preintContainer.Delete();
	defaultSkybox.Delete();
	gbufferContainer.Delete();
}