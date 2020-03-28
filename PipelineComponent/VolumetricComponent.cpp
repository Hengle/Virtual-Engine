#include "VolumetricComponent.h"
#include "PrepareComponent.h"
#include "LightingComponent.h"
#include "../RenderComponent/Light.h"
#include "CameraData/CameraTransformData.h"
#include "RenderPipeline.h"
#include "../Singleton/MathLib.h"
#include "../RenderComponent/ComputeShader.h"
#include "../Singleton/ShaderCompiler.h"
#include "../Singleton/ShaderID.h"
#include "../LogicComponent/Transform.h"
#include "../LogicComponent/DirectionalLight.h"
#include "../RenderComponent/GRPRenderManager.h"
#include "../LogicComponent/World.h"
#include "../Singleton/Graphics.h"
#include "../Singleton/PSOContainer.h"
#include "CameraData/LightCBuffer.h"
#include "CSMComponent.h"
#include "../RenderComponent/RenderTexture.h"
namespace VolumetricGlobal
{
	LightingComponent* lightComp_Volume;
	PrepareComponent* prepareComp_VolumeComp;
	const uint3 FROXEL_RESOLUTION = { 160, 90, 128 };
	uint LightCullCBuffer_ID;
	uint FroxelParams;
	uint _GreyTex;
	uint _VolumeTex_ID_VolumeComponent;
	uint _LastVolume;
	uint _RWLastVolume;
	ComputeShader* froxelShader;

	float indirectIntensity = 0.16f;
	float darkerWeight = 0.75f;
	float brighterWeight = 0.95f;
	const uint marchStep = 64;
	float availableDistance = 32;
	float fogDensity = 0;
}
#define _VolumeTex _VolumeTex_ID_VolumeComponent
std::unique_ptr<RenderTexture> VolumetricComponent::volumeRT;
using namespace VolumetricGlobal;
struct FroxelParamsStruct
{
	float4 _FroxelSize;
	float4 _VolumetricLightVar;
	float4 _TemporalWeight;
	float _LinearFogDensity;
};

VolumetricCameraData::VolumetricCameraData(ID3D12Device* device) :
	cbuffer(device, 3, true, sizeof(FroxelParamsStruct)), heap(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, HEAP_SIZE * 3, true)
{
	RenderTextureFormat volumeFormat;
	volumeFormat.usage = RenderTextureUsage::ColorBuffer;
	volumeFormat.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	lastVolume = std::unique_ptr<RenderTexture>(new RenderTexture(
		device,
		FROXEL_RESOLUTION.x,
		FROXEL_RESOLUTION.y,
		volumeFormat,
		TextureDimension::Tex3D,
		FROXEL_RESOLUTION.z,
		0,
		RenderTextureState::Unordered_Access));
}
VolumetricCameraData::~VolumetricCameraData()
{

}

struct VolumetricRunnable
{
	FrameResource* res;
	Camera* cam;
	ID3D12Device* device;
	ThreadCommand* tCmd;
	VolumetricComponent* selfPtr;
	uint frameIndex;
	void operator()()
	{
		tCmd->ResetCommand();
		auto commandList = tCmd->GetCmdList();
		VolumetricCameraData* camData = (VolumetricCameraData*)cam->GetResource(selfPtr, [&]()->VolumetricCameraData*
		{
			return new VolumetricCameraData(device);
		});
		LightCameraData* lightData = (LightCameraData*)cam->GetResource(lightComp_Volume, [&]()->LightCameraData*
		{
			throw "Null!";
		});
		FroxelParamsStruct* params = (FroxelParamsStruct*)camData->cbuffer.GetMappedDataPtr(frameIndex);
		params->_VolumetricLightVar = { (float)cam->GetNearZ(), (float)(availableDistance - cam->GetNearZ()), availableDistance, indirectIntensity };
		params->_TemporalWeight = { darkerWeight, brighterWeight, 1, 1 };
		params->_FroxelSize = { (float)FROXEL_RESOLUTION.x, (float)FROXEL_RESOLUTION.y, (float)FROXEL_RESOLUTION.z , 1};
		params->_LinearFogDensity = fogDensity;
		DescriptorHeap* heap = &camData->heap;
		froxelShader->BindRootSignature(commandList, heap);
		camData->lastVolume->BindSRVToHeap(heap, camData->HEAP_SIZE * frameIndex + 6, device);
		camData->lastVolume->BindUAVToHeap(heap, camData->HEAP_SIZE * frameIndex + 7, device, 0);
		VolumetricComponent::volumeRT->BindUAVToHeap(heap, camData->HEAP_SIZE * frameIndex + 8, device, 0);
		for (uint i = 0; i < DirectionalLight::CascadeLevel; ++i)
		{
			DirectionalLight::GetInstance()->GetShadowmap(i)->BindSRVToHeap(heap, camData->HEAP_SIZE * frameIndex + i, device);
		}
		froxelShader->SetResource(commandList, FroxelParams, &camData->cbuffer, frameIndex);
		froxelShader->SetResource(commandList, LightCullCBuffer_ID, &lightData->lightCBuffer, frameIndex);
		auto constBufferID = res->cameraCBs[cam];
		froxelShader->SetResource(commandList, ShaderID::GetPerCameraBufferID(), constBufferID.buffer, constBufferID.element);
		froxelShader->SetResource(commandList, _GreyTex, heap, camData->HEAP_SIZE * frameIndex);
		froxelShader->SetResource(commandList, _VolumeTex, heap, camData->HEAP_SIZE * frameIndex + 8);
		froxelShader->SetResource(commandList, _LastVolume, heap, camData->HEAP_SIZE * frameIndex + 6);
		froxelShader->SetResource(commandList, _RWLastVolume, heap, camData->HEAP_SIZE * frameIndex + 7);
		uint3 dispatchCount = { FROXEL_RESOLUTION.x / 2, FROXEL_RESOLUTION.y / 2, FROXEL_RESOLUTION.z / marchStep };
		//Clear
		froxelShader->Dispatch(commandList, 2, dispatchCount.x, dispatchCount.y, dispatchCount.z);
		Graphics::UAVBarrier(commandList, camData->lastVolume->GetResource());
		//Draw Direct Light
		froxelShader->Dispatch(commandList, 0, dispatchCount.x, dispatchCount.y, dispatchCount.z);
		//Copy
		Graphics::UAVBarrier(commandList, VolumetricComponent::volumeRT->GetResource());
		froxelShader->Dispatch(commandList, 3, dispatchCount.x, dispatchCount.y, dispatchCount.z);
		Graphics::UAVBarrier(commandList, VolumetricComponent::volumeRT->GetResource());
		//Scatter
		froxelShader->Dispatch(commandList, 1, FROXEL_RESOLUTION.x / 32, FROXEL_RESOLUTION.y / 2, 1);
		tCmd->CloseCommand();
	}
};

void VolumetricComponent::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	lightComp_Volume = RenderPipeline::GetComponent<LightingComponent>();
	prepareComp_VolumeComp = RenderPipeline::GetComponent<PrepareComponent>();
	SetCPUDepending<LightingComponent>();
	SetGPUDepending<CSMComponent>();
	RenderTextureFormat volumeFormat;
	volumeFormat.usage = RenderTextureUsage::ColorBuffer;
	volumeFormat.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	volumeRT = std::unique_ptr<RenderTexture>(new RenderTexture(
		device,
		FROXEL_RESOLUTION.x,
		FROXEL_RESOLUTION.y,
		volumeFormat,
		TextureDimension::Tex3D,
		FROXEL_RESOLUTION.z,
		0,
		RenderTextureState::Unordered_Access));
	LightCullCBuffer_ID = ShaderID::PropertyToID("LightCullCBuffer");
	FroxelParams = ShaderID::PropertyToID("FroxelParams");
	_GreyTex = ShaderID::PropertyToID("_GreyTex");
	_VolumeTex = ShaderID::PropertyToID("_VolumeTex");
	_LastVolume = ShaderID::PropertyToID("_LastVolume");
	_RWLastVolume = ShaderID::PropertyToID("_RWLastVolume");
	froxelShader = ShaderCompiler::GetComputeShader("Froxel");
}

void VolumetricComponent::Dispose()
{
	volumeRT = nullptr;
}
std::vector<TemporalResourceCommand>& VolumetricComponent::SendRenderTextureRequire(EventData& evt)
{
	return tempResource;
}
void VolumetricComponent::RenderEvent(EventData& data, ThreadCommand* commandList)
{
	ScheduleJob<VolumetricRunnable>(
		{
			data.resource,
			data.camera,
			data.device,
			commandList,
			this,
			data.ringFrameIndex
		});
}
#undef _VolumeTex