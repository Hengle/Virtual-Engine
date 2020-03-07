#include "TemporalAntiAlias.h"
#include "../Singleton/PSOContainer.h"
#include "../Singleton/ShaderID.h"
#include "../Singleton/ShaderCompiler.h"
#include "../Singleton/Graphics.h"
#include "../LogicComponent/World.h"
#include "../RenderComponent/DescriptorHeap.h"
#include "CameraData/CameraTransformData.h"
#include "../Singleton/FrameResource.h"
#include "../RenderComponent/RenderTexture.h"
#include "PrepareComponent.h"
#include "RenderPipeline.h"
#include "VolumetricComponent.h"
#include "../RenderComponent/TransitionBarrierBuffer.h"
using namespace DirectX;		//Only for single .cpp, namespace allowed
struct TAAConstBuffer
{
	XMFLOAT4X4 _InvNonJitterVP;
	XMFLOAT4X4 _InvLastVp;
	XMFLOAT4 _FinalBlendParameters;
	XMFLOAT4 _CameraDepthTexture_TexelSize;
	XMFLOAT4 _ScreenParams;		//x is the width of the camera¡¯s target texture in pixels, y is the height of the camera¡¯s target texture in pixels, z is 1.0 + 1.0 / width and w is 1.0 + 1.0 / height.
	XMFLOAT4 _ZBufferParams;		// x is (1-far/near), y is (far/near), z is (x/far) and w is (y/far).
	//Align
	XMFLOAT3 _TemporalClipBounding;
	float _Sharpness;
	//Align
	XMFLOAT2 _Jitter;
	XMFLOAT2 _LastJitter;

};

std::unique_ptr<PSOContainer> renderTextureContainer;
namespace TemporalGlobal
{
	VolumetricComponent* volumeComp;
	uint _DepthTex;
	uint _MotionVectorTex;
	uint _LastDepthTex;
	uint _LastMotionVectorTex;
	uint _HistoryTex;
	uint _VolumeTex;
}
using namespace TemporalGlobal;


struct TAACameraData : public IPipelineResource
{
	UploadBuffer taaBuffer;
	DescriptorHeap srvHeap;
	StackObject<RenderTexture> lastRenderTarget;
	StackObject<RenderTexture> lastDepthTexture;
	StackObject<RenderTexture> lastMotionVectorTexture;
	UINT width, height;
	inline static const uint SRVHeapSize = 7;
private:
	void Update(UINT width, UINT height, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
	{
		this->width = width;
		this->height = height;
		RenderTextureFormat rtFormat;
		rtFormat.usage = RenderTextureUsage::ColorBuffer;
		rtFormat.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		lastRenderTarget.New(device, width, height, rtFormat, TextureDimension::Tex2D, 0, 0);
		rtFormat.colorFormat = DXGI_FORMAT_R32_FLOAT;
		lastDepthTexture.New(device, width, height, rtFormat, TextureDimension::Tex2D, 0, 0);
		rtFormat.colorFormat = DXGI_FORMAT_R16G16_SNORM;
		lastMotionVectorTexture.New(device, width, height, rtFormat, TextureDimension::Tex2D, 0, 0);
		Graphics::ResourceStateTransform(cmdList, lastRenderTarget->GetWriteState(), lastRenderTarget->GetReadState(), lastRenderTarget->GetResource());
		Graphics::ResourceStateTransform(cmdList, lastDepthTexture->GetWriteState(), lastDepthTexture->GetReadState(), lastDepthTexture->GetResource());
		Graphics::ResourceStateTransform(cmdList, lastMotionVectorTexture->GetWriteState(), lastMotionVectorTexture->GetReadState(), lastMotionVectorTexture->GetResource());
	}
public:
	~TAACameraData()
	{
		lastRenderTarget.Delete();
		lastDepthTexture.Delete();
		lastMotionVectorTexture.Delete();
	}
	TAACameraData(UINT width, UINT height, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList) :
		taaBuffer(device, 3, true, sizeof(TAAConstBuffer)),
		srvHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SRVHeapSize * 3, true)
	{
		Update(width, height, device, cmdList);
	}

	bool UpdateFrame(UINT width, UINT height, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
	{
		if (width != this->width || height != this->height)
		{
			Update(width, height, device, cmdList);
			return true;
		}
		return false;
	}
};

class TemporalAA
{
public:
	PrepareComponent* prePareComp;
	ID3D12Device* device;
	PSOContainer* toRTContainer;
	UINT TAAConstBuffer_Index;
	Shader* taaShader;
	TemporalAA()
	{
		TAAConstBuffer_Index = ShaderID::PropertyToID("TAAConstBuffer");
		taaShader = ShaderCompiler::GetShader("TemporalAA");
	}
	void Run(
		RenderTexture* inputColorBuffer,
		RenderTexture* inputDepthBuffer,
		RenderTexture* motionVector,
		RenderTexture* renderTargetTex,
		ThreadCommand* tCmd,
		FrameResource* res,
		Camera* cam, uint frameIndex,
		UINT width, UINT height)
	{
		auto commandList = tCmd->GetCmdList();
		TransitionBarrierBuffer& transitionBarrier = *tCmd->GetBarrierBuffer();
		CameraTransformData* camTransData = (CameraTransformData*)cam->GetResource(prePareComp, [&]()->CameraTransformData*
			{
				return new CameraTransformData;
			});
		TAACameraData* tempCamData = (TAACameraData*)cam->GetResource(this, [&]()->TAACameraData*
			{
				return new TAACameraData(width, height, device, commandList);
			});

		transitionBarrier.AddCommand(inputDepthBuffer->GetWriteState(), inputDepthBuffer->GetReadState(), inputDepthBuffer->GetResource());
		transitionBarrier.AddCommand(motionVector->GetWriteState(), motionVector->GetReadState(), motionVector->GetResource());
		if (tempCamData->UpdateFrame(width, height, device, commandList))
		{
			//Refresh & skip
			transitionBarrier.AddCommand(renderTargetTex->GetWriteState(), D3D12_RESOURCE_STATE_COPY_DEST, renderTargetTex->GetResource());
			transitionBarrier.AddCommand(inputColorBuffer->GetWriteState(), inputColorBuffer->GetReadState(), renderTargetTex->GetResource());
			transitionBarrier.ExecuteCommand(commandList);
			Graphics::CopyTexture(commandList, inputColorBuffer, 0, 0, renderTargetTex, 0, 0);
			transitionBarrier.AddCommand(D3D12_RESOURCE_STATE_COPY_DEST, renderTargetTex->GetWriteState(), renderTargetTex->GetResource());
			transitionBarrier.AddCommand(inputColorBuffer->GetReadState(), inputColorBuffer->GetWriteState(), renderTargetTex->GetResource());
		}
		else
		{

			TAAConstBuffer constBufferData;
			inputColorBuffer->BindSRVToHeap(&tempCamData->srvHeap, 0 + TAACameraData::SRVHeapSize * frameIndex, device);
			inputDepthBuffer->BindSRVToHeap(&tempCamData->srvHeap, 1 + TAACameraData::SRVHeapSize * frameIndex, device);
			motionVector->BindSRVToHeap(&tempCamData->srvHeap, 2 + TAACameraData::SRVHeapSize * frameIndex, device);
			tempCamData->lastDepthTexture->BindSRVToHeap(&tempCamData->srvHeap, 3 + TAACameraData::SRVHeapSize * frameIndex, device);
			tempCamData->lastMotionVectorTexture->BindSRVToHeap(&tempCamData->srvHeap, 4 + TAACameraData::SRVHeapSize * frameIndex, device);
			tempCamData->lastRenderTarget->BindSRVToHeap(&tempCamData->srvHeap, 5 + TAACameraData::SRVHeapSize * frameIndex, device);
			VolumetricComponent::volumeRT->BindSRVToHeap(&tempCamData->srvHeap, 6 + TAACameraData::SRVHeapSize * frameIndex, device);
			constBufferData._InvNonJitterVP = *(XMFLOAT4X4*)&XMMatrixInverse(&XMMatrixDeterminant(camTransData->nonJitteredVPMatrix), camTransData->nonJitteredVPMatrix);
			constBufferData._InvLastVp = *(XMFLOAT4X4*)&XMMatrixInverse(&XMMatrixDeterminant(camTransData->lastNonJitterVP), camTransData->lastNonJitterVP);
			constBufferData._FinalBlendParameters = { 0.95f,0.85f, 6000.0f, 0 };		//Stationary Move, Const, 0
			constBufferData._CameraDepthTexture_TexelSize = { 1.0f / width, 1.0f / height, (float)width, (float)height };
			constBufferData._ScreenParams = { (float)width, (float)height, 1.0f / width + 1, 1.0f / height + 1 };
			constBufferData._ZBufferParams = prePareComp->_ZBufferParams;
			constBufferData._TemporalClipBounding = { 3.0f, 1.25f, 3000.0f };
			constBufferData._Sharpness = 0.02f;
			constBufferData._Jitter = camTransData->jitter;
			constBufferData._LastJitter = camTransData->lastFrameJitter;
			tempCamData->taaBuffer.CopyData(frameIndex, &constBufferData);
			static uint froxelParams = 0;
			if (froxelParams == 0)
				froxelParams = ShaderID::PropertyToID("FroxelParams");
			taaShader->BindRootSignature(commandList, &tempCamData->srvHeap);
			auto camBuffer = res->cameraCBs[cam->GetInstanceID()];
			VolumetricCameraData* volumeCamData = (VolumetricCameraData*)cam->GetResource(volumeComp, [&]()->VolumetricCameraData*
				{
					throw "Null";
				});
			taaShader->SetResource(commandList, ShaderID::GetPerCameraBufferID(), camBuffer.buffer, camBuffer.element);
			taaShader->SetResource(commandList, froxelParams, &volumeCamData->cbuffer, frameIndex);
			taaShader->SetResource(commandList, TAAConstBuffer_Index, &tempCamData->taaBuffer, frameIndex);
			taaShader->SetResource(commandList, ShaderID::GetMainTex(), &tempCamData->srvHeap, frameIndex * TAACameraData::SRVHeapSize);
			taaShader->SetResource(commandList, _DepthTex, &tempCamData->srvHeap, frameIndex * TAACameraData::SRVHeapSize + 1);
			taaShader->SetResource(commandList, _MotionVectorTex, &tempCamData->srvHeap, frameIndex * TAACameraData::SRVHeapSize + 2);
			taaShader->SetResource(commandList, _LastDepthTex, &tempCamData->srvHeap, frameIndex * TAACameraData::SRVHeapSize + 3);
			taaShader->SetResource(commandList, _LastMotionVectorTex, &tempCamData->srvHeap, frameIndex * TAACameraData::SRVHeapSize + 4);
			taaShader->SetResource(commandList, _HistoryTex, &tempCamData->srvHeap, frameIndex * TAACameraData::SRVHeapSize + 5);
			taaShader->SetResource(commandList, _VolumeTex, &tempCamData->srvHeap, frameIndex * TAACameraData::SRVHeapSize + 6);
			transitionBarrier.AddCommand(
				VolumetricComponent::volumeRT->GetWriteState(),
				VolumetricComponent::volumeRT->GetReadState(),
				VolumetricComponent::volumeRT->GetResource());
			transitionBarrier.ExecuteCommand(commandList);
			Graphics::Blit(
				commandList,
				device,
				&inputColorBuffer->GetColorDescriptor(0), 1,
				nullptr,
				toRTContainer, 0,
				width, height,
				taaShader, 1
			);
			transitionBarrier.AddCommand(inputColorBuffer->GetWriteState(), inputColorBuffer->GetReadState(), inputColorBuffer->GetResource());
			transitionBarrier.ExecuteCommand(commandList);
			Graphics::Blit(
				commandList,
				device,
				&renderTargetTex->GetColorDescriptor(0), 1,
				nullptr,
				toRTContainer, 0,
				width, height,
				taaShader, 0
			);
			transitionBarrier.AddCommand(inputColorBuffer->GetReadState(), inputColorBuffer->GetWriteState(), inputColorBuffer->GetResource());
			transitionBarrier.AddCommand(
				VolumetricComponent::volumeRT->GetReadState(),
				VolumetricComponent::volumeRT->GetWriteState(),
				VolumetricComponent::volumeRT->GetResource());
		}
		
		transitionBarrier.AddCommand( tempCamData->lastRenderTarget->GetReadState(), D3D12_RESOURCE_STATE_COPY_DEST, tempCamData->lastRenderTarget->GetResource());
		transitionBarrier.AddCommand( tempCamData->lastMotionVectorTexture->GetReadState(), D3D12_RESOURCE_STATE_COPY_DEST, tempCamData->lastMotionVectorTexture->GetResource());
		transitionBarrier.AddCommand( tempCamData->lastDepthTexture->GetReadState(), D3D12_RESOURCE_STATE_COPY_DEST, tempCamData->lastDepthTexture->GetResource());
		transitionBarrier.AddCommand( renderTargetTex->GetWriteState(), D3D12_RESOURCE_STATE_COPY_SOURCE, renderTargetTex->GetResource());
		transitionBarrier.AddCommand( inputDepthBuffer->GetReadState(), D3D12_RESOURCE_STATE_COPY_SOURCE, inputDepthBuffer->GetResource());
		transitionBarrier.ExecuteCommand(commandList);
		Graphics::CopyTexture(commandList, renderTargetTex, 0, 0, tempCamData->lastRenderTarget, 0, 0);
		Graphics::CopyTexture(commandList, motionVector, 0, 0, tempCamData->lastMotionVectorTexture, 0, 0);
		Graphics::CopyTexture(commandList, inputDepthBuffer, 0, 0, tempCamData->lastDepthTexture, 0, 0);
		transitionBarrier.AddCommand( D3D12_RESOURCE_STATE_COPY_DEST, tempCamData->lastRenderTarget->GetReadState(), tempCamData->lastRenderTarget->GetResource());
		transitionBarrier.AddCommand( D3D12_RESOURCE_STATE_COPY_DEST, tempCamData->lastMotionVectorTexture->GetReadState(), tempCamData->lastMotionVectorTexture->GetResource());
		transitionBarrier.AddCommand( D3D12_RESOURCE_STATE_COPY_DEST, tempCamData->lastDepthTexture->GetReadState(), tempCamData->lastDepthTexture->GetResource());
		transitionBarrier.AddCommand( D3D12_RESOURCE_STATE_COPY_SOURCE, renderTargetTex->GetWriteState(), renderTargetTex->GetResource());
		transitionBarrier.AddCommand( D3D12_RESOURCE_STATE_COPY_SOURCE, inputDepthBuffer->GetWriteState(), inputDepthBuffer->GetResource());
		transitionBarrier.AddCommand(motionVector->GetReadState(), motionVector->GetWriteState(), motionVector->GetResource());
	}
};


Storage<TemporalAA, 1> taaStorage;
std::vector<TemporalResourceCommand>& TemporalAntiAlias::SendRenderTextureRequire(EventData& evt)
{
	for (uint i = 0; i < tempRT.size(); ++i)
	{
		auto& a = tempRT[i].descriptor.rtDesc;
		a.width = evt.width;
		a.height = evt.height;
	}
	return tempRT;
}

void TemporalAntiAlias::RenderEvent(EventData& data, ThreadCommand* commandList)
{
	RenderTexture* sourceTex;
	RenderTexture* destTex;
	RenderTexture* depthTex;
	RenderTexture* motionVector;
	sourceTex = (RenderTexture*)allTempResource[0];
	destTex = (RenderTexture*)allTempResource[1];
	depthTex = (RenderTexture*)allTempResource[2];
	motionVector = (RenderTexture*)allTempResource[3];
	FrameResource* res = data.resource;
	Camera* cam = data.camera;
	uint width = data.width;
	uint height = data.height;
	uint ring = data.ringFrameIndex;
	ScheduleJob([=]()->void
		{
			TemporalAA* ptr = (TemporalAA*)&taaStorage;
			commandList->ResetCommand();
			ptr->Run(
				sourceTex,
				depthTex,
				motionVector,
				destTex,
				commandList,
				res,
				cam, ring,
				width,
				height);
			commandList->CloseCommand();
		});
}

void TemporalAntiAlias::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	_DepthTex = ShaderID::PropertyToID("_DepthTex");
	_MotionVectorTex = ShaderID::PropertyToID("_MotionVectorTex");
	_LastDepthTex = ShaderID::PropertyToID("_LastDepthTex");
	_LastMotionVectorTex = ShaderID::PropertyToID("_LastMotionVectorTex");
	_HistoryTex = ShaderID::PropertyToID("_HistoryTex");
	_VolumeTex = ShaderID::PropertyToID("_VolumeTex");
	SetCPUDepending<VolumetricComponent>();
	SetGPUDepending<VolumetricComponent>();
	volumeComp = RenderPipeline::GetComponent<VolumetricComponent>();
	tempRT.resize(4);
	tempRT[0].type = TemporalResourceCommand::CommandType_Require_RenderTexture;
	tempRT[0].uID = ShaderID::PropertyToID("_CameraRenderTarget");
	tempRT[1].type = TemporalResourceCommand::CommandType_Create_RenderTexture;
	tempRT[1].uID = ShaderID::PropertyToID("_PostProcessBlitTarget");
	tempRT[2].type = TemporalResourceCommand::CommandType_Require_RenderTexture;
	tempRT[2].uID = ShaderID::PropertyToID("_CameraDepthTexture");
	tempRT[3].type = TemporalResourceCommand::CommandType_Require_RenderTexture;
	tempRT[3].uID = ShaderID::PropertyToID("_CameraMotionVectorsTexture");
	auto& desc = tempRT[1].descriptor;
	desc.rtDesc.rtFormat.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.rtDesc.rtFormat.usage = RenderTextureUsage::ColorBuffer;
	desc.rtDesc.depthSlice = 1;
	desc.rtDesc.width = 0;
	desc.rtDesc.height = 0;
	desc.rtDesc.type = TextureDimension::Tex2D;
	desc.rtDesc.state = RenderTextureState::Render_Target;
	renderTextureContainer = std::unique_ptr<PSOContainer>(
		new PSOContainer(DXGI_FORMAT_UNKNOWN, 1, &desc.rtDesc.rtFormat.colorFormat)
		);
	TemporalAA* ptr = (TemporalAA*)&taaStorage;
	ptr->prePareComp = RenderPipeline::GetComponent<PrepareComponent>();
	ptr->device = device;
	ptr->toRTContainer = renderTextureContainer.get();
	new (ptr)TemporalAA();
}

void TemporalAntiAlias::Dispose()
{
	TemporalAA* ptr = (TemporalAA*)&taaStorage;
	ptr->~TemporalAA();
	renderTextureContainer = nullptr;
}