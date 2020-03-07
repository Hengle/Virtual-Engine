#include "GBufferComponent.h"
#include "../Common/d3dUtil.h"
#include "../Singleton/Graphics.h"
#include "../LogicComponent/World.h"
#include "../Singleton/ShaderCompiler.h"
#include "../Singleton/ShaderID.h"
#include "../RenderComponent/DescriptorHeap.h"
#include "../RenderComponent/StructuredBuffer.h"
#include "../RenderComponent/MeshRenderer.h"
#include "RenderPipeline.h"
#include "../LogicComponent/Transform.h"
#include "../Common/GeometryGenerator.h"
#include "../RenderComponent/GRPRenderManager.h"
#include "../Singleton/MeshLayout.h"
#include "PrepareComponent.h"
#include "LightingComponent.h"
#include "../RenderComponent/Light.h"
#include "SkyboxComponent.h"
#include "../RenderComponent/PBRMaterial.h"
#include "VolumetricComponent.h";
#include "../RenderComponent/Texture.h"
#include "DepthComponent.h"
using namespace DirectX;
namespace GBufferGlobalVars
{
	PSOContainer* gbufferContainer(nullptr);
	DepthComponent* depthComp;
	//PrepareComponent* 
	struct TextureIndices
	{
		uint _SkyboxCubemap;
		uint _PreintTexture;
	};
	const uint TEX_COUNT = sizeof(TextureIndices) / 4;
	uint _DefaultMaterials;
	RenderTexture* gbufferTempRT[10];
	LightingComponent* lightComp;
}
using namespace GBufferGlobalVars;

uint GBufferComponent::_AllLight(0);
uint GBufferComponent::_LightIndexBuffer(0);
uint GBufferComponent::LightCullCBuffer(0);
uint GBufferComponent::TextureIndices(0);

#define ALBEDO_RT (gbufferTempRT[0])
#define SPECULAR_RT  (gbufferTempRT[1])
#define NORMAL_RT  (gbufferTempRT[2])
#define MOTION_VECTOR_RT (gbufferTempRT[3])
#define EMISSION_RT (gbufferTempRT[4])
#define DEPTH_RT (gbufferTempRT[5])
#define GBUFFER_COUNT 5


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
		for (uint i = 0; i < TEX_COUNT * 3; ++i)
		{
			world->ReturnDescHeapIndexToPool(descs[i]);
		}
	}
};

class GBufferRunnable
{
public:
	ID3D12Device* device;		//Dx12 Device
	ThreadCommand* tcmd;		//Command List
	Camera* cam;				//Camera
	FrameResource* resource;	//Per Frame Data
	GBufferComponent* selfPtr;//Singleton Component
	World* world;				//Main Scene
	static uint _GreyTex;
	static uint _IntegerTex;
	static uint _Cubemap;
	uint frameIndex;
	void operator()()
	{
		tcmd->ResetCommand();
		ID3D12GraphicsCommandList* commandList = tcmd->GetCmdList();
		//Clear
		DepthCameraResource* depthCameraRes = (DepthCameraResource*)cam->GetResource(depthComp, [=]()->DepthCameraResource*
		{
			return new DepthCameraResource(device);
		});

		GBufferCameraData* camData = (GBufferCameraData*)cam->GetResource(selfPtr, [&]()->GBufferCameraData*
		{
			return new GBufferCameraData(device);
		});
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
			preintShader->BindRootSignature(commandList);
			Graphics::Blit(
				commandList,
				device,
				&selfPtr->preintTexture->GetColorDescriptor(0),
				1,
				nullptr,
				selfPtr->preintContainer, 0,
				selfPtr->preintTexture->GetWidth(),
				selfPtr->preintTexture->GetHeight(),
				preintShader,
				0
			);
		}


		LightFrameData* lightFrameData = (LightFrameData*)resource->GetPerCameraResource(lightComp, cam, []()->LightFrameData*
		{
#ifndef NDEBUG
			throw "No Light Data Exception!";
#endif
			return nullptr;	//Get Error if there is no light coponent in pipeline
		});
		LightCameraData* lightCameraData = (LightCameraData*)cam->GetResource(lightComp, []()->LightCameraData*
			{
#ifndef NDEBUG
				throw "No Light Data Exception!";
#endif
				return nullptr;	//Get Error if there is no light coponent in pipeline
			});
		DescriptorHeap* worldHeap = World::GetInstance()->GetGlobalDescHeap();
		selfPtr->skboxComp->skyboxTex->BindSRVToHeap(worldHeap, camData->descs[0 + TEX_COUNT * frameIndex], device);
		selfPtr->preintTexture->BindSRVToHeap(worldHeap, camData->descs[1 + TEX_COUNT * frameIndex], device);
		Shader* gbufferShader = world->GetGRPRenderManager()->GetShader();
		
		auto setGBufferShaderFunc = [&]()->void
		{
			gbufferShader->BindRootSignature(commandList, worldHeap);
			gbufferShader->SetResource(commandList, ShaderID::GetMainTex(), worldHeap, 0);
			gbufferShader->SetResource(commandList, _GreyTex, worldHeap, 0);
			gbufferShader->SetResource(commandList, _IntegerTex, worldHeap, 0);
			gbufferShader->SetResource(commandList, _Cubemap, worldHeap, 0);
			gbufferShader->SetResource(commandList, _DefaultMaterials, world->GetPBRMaterialManager()->GetMaterialBuffer(), 0);
			gbufferShader->SetStructuredBufferByAddress(commandList, GBufferComponent::_AllLight, lightFrameData->lightsInFrustum.GetAddress(0));
			gbufferShader->SetStructuredBufferByAddress(commandList, GBufferComponent::_LightIndexBuffer, lightComp->lightIndexBuffer->GetAddress(0, 0));
			gbufferShader->SetResource(commandList, GBufferComponent::LightCullCBuffer, &lightCameraData->lightCBuffer, frameIndex);
			gbufferShader->SetResource(commandList, GBufferComponent::TextureIndices, &camData->texIndicesBuffer, frameIndex);
		};
		setGBufferShaderFunc();
	//	EMISSION_RT->ClearRenderTarget(commandList, 0);
		//Prepare RenderTarget
		D3D12_CPU_DESCRIPTOR_HANDLE handles[GBUFFER_COUNT];
		auto st = [&](UINT p)->void
		{
			handles[p] = gbufferTempRT[p]->GetColorDescriptor(0);
		};

		InnerLoop<decltype(st), GBUFFER_COUNT>(st);
		EMISSION_RT->SetViewport(commandList);
		
		//Draw GBuffer
		commandList->OMSetRenderTargets(GBUFFER_COUNT, handles, false, &DEPTH_RT->GetColorDescriptor(0));
		world->GetGRPRenderManager()->DrawCommand(
			commandList,
			device,
			0,
			ShaderID::GetPerCameraBufferID(),
			resource->cameraCBs[cam->GetInstanceID()],
			gbufferContainer, 0
		);
		//Recheck Culling
		ConstBufferElement cullEle;
		cullEle.buffer = &depthCameraRes->cullBuffer;
		cullEle.element = 0 + DepthCameraResource::descHeapSize * frameIndex;
		depthCameraRes->hizTexture->BindSRVToHeap(World::GetInstance()->GetGlobalDescHeap(), depthCameraRes->descIndex, device);

		//Culling
		world->GetGRPRenderManager()->OcclusionRecheck(
			commandList,
			*tcmd->GetBarrierBuffer(),
			device,
			resource,
			cullEle,
			depthCameraRes->descIndex);
		EMISSION_RT->SetViewport(commandList);
		commandList->OMSetRenderTargets(0, nullptr, true, &DEPTH_RT->GetColorDescriptor(0));
		setGBufferShaderFunc();
		tcmd->GetBarrierBuffer()->ExecuteCommand(commandList);
		world->GetGRPRenderManager()->DrawCommand(
			commandList,
			device,
			1,
			ShaderID::GetPerCameraBufferID(),
			resource->cameraCBs[cam->GetInstanceID()],
			gbufferContainer, 1
		);
		
		commandList->OMSetRenderTargets(GBUFFER_COUNT, handles, false, &DEPTH_RT->GetColorDescriptor(0));
		world->GetGRPRenderManager()->DrawCommand(
			commandList,
			device,
			0,
			ShaderID::GetPerCameraBufferID(),
			resource->cameraCBs[cam->GetInstanceID()],
			gbufferContainer, 0
		);
		tcmd->CloseCommand();
	}
};
uint GBufferRunnable::_GreyTex;
uint GBufferRunnable::_IntegerTex;
uint GBufferRunnable::_Cubemap;
std::vector<TemporalResourceCommand>& GBufferComponent::SendRenderTextureRequire(EventData& evt) {
	for (int i = 0; i < tempRTRequire.size(); ++i)
	{
		tempRTRequire[i].descriptor.rtDesc.width = evt.width;
		tempRTRequire[i].descriptor.rtDesc.height = evt.height;
	}
	return tempRTRequire;
}
void GBufferComponent::RenderEvent(EventData& data, ThreadCommand* commandList)
{
	memcpy(gbufferTempRT, allTempResource.data(), sizeof(RenderTexture*) * tempRTRequire.size());
	GBufferRunnable runnable
	{
		data.device,
		commandList,
		data.camera,
		data.resource,
		this,
		data.world,
		data.ringFrameIndex
	};
	//Schedule MultiThread Job
	ScheduleJob(runnable);
}

void GBufferComponent::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	SetCPUDepending<LightingComponent>();
	SetGPUDepending<LightingComponent>();
	lightComp = RenderPipeline::GetComponent<LightingComponent>();
	depthComp = RenderPipeline::GetComponent<DepthComponent>();
	tempRTRequire.resize(6);
	GBufferRunnable::_GreyTex = ShaderID::PropertyToID("_GreyTex");
	GBufferRunnable::_IntegerTex = ShaderID::PropertyToID("_IntegerTex");
	GBufferRunnable::_Cubemap = ShaderID::PropertyToID("_Cubemap");
	TemporalResourceCommand& specularBuffer = tempRTRequire[0];
	specularBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	specularBuffer.uID = ShaderID::PropertyToID("_CameraGBufferTexture0");
	specularBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	specularBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	specularBuffer.descriptor.rtDesc.depthSlice = 1;
	specularBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	specularBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;
	TemporalResourceCommand& albedoBuffer = tempRTRequire[1];
	albedoBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	albedoBuffer.uID = ShaderID::PropertyToID("_CameraGBufferTexture1");
	albedoBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	albedoBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	albedoBuffer.descriptor.rtDesc.depthSlice = 1;
	albedoBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	albedoBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;
	TemporalResourceCommand& normalBuffer = tempRTRequire[2];
	normalBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	normalBuffer.uID = ShaderID::PropertyToID("_CameraGBufferTexture2");
	normalBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
	normalBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	normalBuffer.descriptor.rtDesc.depthSlice = 1;
	normalBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	normalBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;
	TemporalResourceCommand& motionVectorBuffer = tempRTRequire[3];
	motionVectorBuffer.type = TemporalResourceCommand::CommandType_Require_RenderTexture;
	motionVectorBuffer.uID = ShaderID::PropertyToID("_CameraMotionVectorsTexture");
	motionVectorBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R16G16_SNORM;
	motionVectorBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;

	TemporalResourceCommand& emissionBuffer = tempRTRequire[4];
	emissionBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	emissionBuffer.uID = ShaderID::PropertyToID("_CameraRenderTarget");
	emissionBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	emissionBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	emissionBuffer.descriptor.rtDesc.depthSlice = 1;
	emissionBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	emissionBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;

	TemporalResourceCommand& depthBuffer = tempRTRequire[5];
	depthBuffer.type = TemporalResourceCommand::CommandType_Require_RenderTexture;
	depthBuffer.uID = ShaderID::PropertyToID("_CameraDepthTexture");

	std::vector<DXGI_FORMAT> colorFormats(GBUFFER_COUNT);
	for (int i = 0; i < GBUFFER_COUNT; ++i)
	{
		colorFormats[i] = tempRTRequire[i].descriptor.rtDesc.rtFormat.colorFormat;
	}
	PSORTSetting settings[2];
	settings[0].rtCount = GBUFFER_COUNT;
	memcpy(settings[0].rtFormat, colorFormats.data(), sizeof(DXGI_FORMAT) * GBUFFER_COUNT);
	settings[0].depthFormat = DXGI_FORMAT_D32_FLOAT;
	settings[1].rtCount = 0;
	settings[1].depthFormat = DXGI_FORMAT_D32_FLOAT;
	gbufferContainer = new PSOContainer(settings, 2);
	_AllLight = ShaderID::PropertyToID("_AllLight");
	_LightIndexBuffer = ShaderID::PropertyToID("_LightIndexBuffer");
	LightCullCBuffer = ShaderID::PropertyToID("LightCullCBuffer");
	TextureIndices = ShaderID::PropertyToID("TextureIndices");
	_DefaultMaterials = ShaderID::PropertyToID("_DefaultMaterials");
	skboxComp = RenderPipeline::GetComponent<SkyboxComponent>();
}

void GBufferComponent::Dispose()
{
	preintContainer.Delete();
	//meshRenderer->Destroy();
	//trans.Destroy();
	delete gbufferContainer;

}
