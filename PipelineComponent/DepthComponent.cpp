#include "DepthComponent.h"
#include "../Singleton/ShaderID.h"
#include "../Singleton/ShaderCompiler.h"
#include "../RenderComponent/GRPRenderManager.h"
#include "../RenderComponent/RenderTexture.h"
#include "../LogicComponent/World.h"
#include "PrepareComponent.h"
#include "../Singleton/PSOContainer.h"
#include "RenderPipeline.h"
#include "../Singleton/Graphics.h"
#include "../RenderComponent/ComputeShader.h"
using namespace DirectX;
namespace DepthGlobal
{
	PrepareComponent* prepareComp_Depth = nullptr;
	ComputeShader* hizGenerator;
	const uint2 hizResolution = { 1024, 512 };
	uint _CameraDepthTexture;
	uint _DepthTexture;
}
using namespace DepthGlobal;

DepthCameraResource::DepthCameraResource(ID3D12Device* device) :
cullBuffer(device, descHeapSize * 3, true, sizeof(GRPRenderManager::CullData)),
ub(device, 2 * 3, true, sizeof(ObjectConstants)),
hizHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descHeapSize * 3, true)
{
	float4x4 mat = MathHelper::Identity4x4();
	XMMATRIX* vec = (XMMATRIX*)&mat;
	vec->r[3] = { 0, 0, 0, 1 };
	for (uint i = 0; i < ub.GetElementCount(); ++i)
	{
		ub.CopyData(i, &vec);
	}

	descIndex = World::GetInstance()->GetDescHeapIndexFromPool();
	RenderTextureFormat format;
	format.usage = RenderTextureUsage::ColorBuffer;
	format.colorFormat = DXGI_FORMAT_R32_FLOAT;
	hizTexture = new RenderTexture(
		device, hizResolution.x, hizResolution.y, format,
		TextureDimension::Tex2D,
		1,
		9,
		RenderTextureState::Generic_Read
	);
}
DepthCameraResource::~DepthCameraResource() noexcept
{
	World* world = World::GetInstance();
	if (world)
	{
		world->ReturnDescHeapIndexToPool(descIndex);
	}
	if (hizTexture) delete hizTexture;
}

void DepthComponent::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	SetCPUDepending<PrepareComponent>();
	tempRTRequire.resize(2);
	TemporalResourceCommand& depthBuffer = tempRTRequire[0];
	depthBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	depthBuffer.uID = ShaderID::PropertyToID("_CameraDepthTexture");
	depthBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::DepthBuffer;
	depthBuffer.descriptor.rtDesc.rtFormat.depthFormat = RenderTextureDepthSettings_Depth32;
	depthBuffer.descriptor.rtDesc.depthSlice = 1;
	depthBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;
	depthBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	TemporalResourceCommand& motionVectorBuffer = tempRTRequire[1];
	motionVectorBuffer.type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	motionVectorBuffer.uID = ShaderID::PropertyToID("_CameraMotionVectorsTexture");
	motionVectorBuffer.descriptor.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R16G16_SNORM;
	motionVectorBuffer.descriptor.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	motionVectorBuffer.descriptor.rtDesc.depthSlice = 1;
	motionVectorBuffer.descriptor.rtDesc.type = TextureDimension::Tex2D;
	motionVectorBuffer.descriptor.rtDesc.state = RenderTextureState::Render_Target;
	prepareComp_Depth = RenderPipeline::GetComponent<PrepareComponent>();
	depthPrepassContainer = new PSOContainer(DXGI_FORMAT_D32_FLOAT, 0, nullptr);
	hizGenerator = ShaderCompiler::GetComputeShader("HIZGenerator");
	_DepthTexture = ShaderID::PropertyToID("_DepthTexture");
	_CameraDepthTexture = ShaderID::PropertyToID("_CameraDepthTexture");
}
void DepthComponent::Dispose()
{
	delete depthPrepassContainer;
}
std::vector<TemporalResourceCommand>& DepthComponent::SendRenderTextureRequire(EventData& evt)
{
	for (auto ite = tempRTRequire.begin(); ite != tempRTRequire.end(); ++ite)
	{
		ite->descriptor.rtDesc.width = evt.width;
		ite->descriptor.rtDesc.height = evt.height;
	}
	return tempRTRequire;
}

class DepthRunnable
{
public:
	RenderTexture* depthRT;
	RenderTexture* motionVecRT;
	ID3D12Device* device;		//Dx12 Device
	ThreadCommand* tcmd;		//Command List
	Camera* cam;				//Camera
	FrameResource* resource;	//Per Frame Data
	DepthComponent* component;//Singleton Component
	World* world;				//Main Scene
	uint frameIndex;
	void operator()()
	{
		tcmd->ResetCommand();
		ID3D12GraphicsCommandList* commandList = tcmd->GetCmdList();
		DepthCameraResource* cameraRes = (DepthCameraResource*)cam->GetResource(component, [=]()->DepthCameraResource*
		{
			return new DepthCameraResource(device);
		});
		depthRT->ClearRenderTarget(commandList, 0, 0);
		motionVecRT->ClearRenderTarget(commandList, 0, 0);
		depthRT->SetViewport(commandList);
		ConstBufferElement cullEle;
		cullEle.buffer = &cameraRes->cullBuffer;
		cullEle.element = 0 + DepthCameraResource::descHeapSize * frameIndex;
		cameraRes->hizTexture->BindSRVToHeap(World::GetInstance()->GetGlobalDescHeap(), cameraRes->descIndex, device);
		//Culling
		world->GetGRPRenderManager()->Culling(
			commandList,
			*tcmd->GetBarrierBuffer(),
			device,
			resource,
			cullEle,
			prepareComp_Depth->frustumPlanes,
			*(XMFLOAT3*)&prepareComp_Depth->frustumMinPos,
			*(XMFLOAT3*)&prepareComp_Depth->frustumMaxPos,
			prepareComp_Depth->passConstants.nonJitterVP,
			prepareComp_Depth->passConstants.lastVP,
			cameraRes->descIndex,
			true
		);

		//Draw Depth prepass
		commandList->OMSetRenderTargets(0, nullptr, true, &depthRT->GetColorDescriptor(0, 0));
		world->GetGRPRenderManager()->GetShader()->BindRootSignature(commandList);
		tcmd->GetBarrierBuffer()->ExecuteCommand(commandList);
		world->GetGRPRenderManager()->DrawCommand(
			commandList,
			device,
			1,
			ShaderID::GetPerCameraBufferID(),
			resource->cameraCBs[cam->GetInstanceID()],
			component->depthPrepassContainer, 0
		);
		//Generate HIZ Depth
		for (uint i = 0; i < 9; ++i)
		{
			cameraRes->hizTexture->BindUAVToHeap(&cameraRes->hizHeap, i + DepthCameraResource::descHeapSize * frameIndex, device, i);
		}
		depthRT->BindSRVToHeap(&cameraRes->hizHeap, 10 + DepthCameraResource::descHeapSize * frameIndex, device);
		hizGenerator->BindRootSignature(commandList, &cameraRes->hizHeap);
		hizGenerator->SetResource(commandList, _DepthTexture, &cameraRes->hizHeap, DepthCameraResource::descHeapSize * frameIndex);
		hizGenerator->SetResource(commandList, _CameraDepthTexture, &cameraRes->hizHeap, 10 + DepthCameraResource::descHeapSize * frameIndex);
		std::pair< D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES> states[2] =
		{
		{depthRT->GetWriteState(), depthRT->GetReadState()},
		{cameraRes->hizTexture->GetReadState(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS}
		};
		ID3D12Resource* resources[2] =
		{
			depthRT->GetResource(),
			cameraRes->hizTexture->GetResource()
		};
		Graphics::MultiResourceStateTransform<2>(commandList, states, resources);
		//TODO
		hizGenerator->Dispatch(commandList, 2, hizResolution.x / 8, hizResolution.y / 8, 1);
		Graphics::UAVBarrier(commandList, cameraRes->hizTexture->GetResource());
		hizGenerator->Dispatch(commandList, 0, hizResolution.x / 32 / 2, hizResolution.y / 32 / 2, 1);
		Graphics::UAVBarrier(commandList, cameraRes->hizTexture->GetResource());
		hizGenerator->Dispatch(commandList, 1, 1, 1, 1);
		//Dispatch
		for (uint i = 0; i < 2; ++i)
		{
			auto a = states[i].first;
			states[i].first = states[i].second;
			states[i].second = a;
		}
		Graphics::MultiResourceStateTransform<2>(commandList, states, resources);

		tcmd->CloseCommand();
	}
};

void DepthComponent::RenderEvent(EventData& data, ThreadCommand* commandList)
{
	ScheduleJob<DepthRunnable>({
		(RenderTexture*)allTempResource[0],
		(RenderTexture*)allTempResource[1],
		data.device,
		commandList,
		data.camera,
		data.resource,
		this,
		data.world,
		data.ringFrameIndex
		});
}