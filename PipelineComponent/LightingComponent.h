#pragma once
#include "PipelineComponent.h"
class PrepareComponent;
class LightFrameData : public IPipelineResource
{
public:
	UploadBuffer lightsInFrustum;
	
	LightFrameData(ID3D12Device* device);
};
class LightCameraData : public IPipelineResource
{
public:
	UploadBuffer lightCBuffer;
	LightCameraData(ID3D12Device* device);
};

class LightingComponent : public PipelineComponent
{
private:
	std::vector<TemporalResourceCommand> tempResources;
	PrepareComponent* prepareComp;
public:
	StackObject<RenderTexture> xyPlaneTexture;
	StackObject<RenderTexture> zPlaneTexture;
	StackObject<StructuredBuffer> lightIndexBuffer;
	StackObject<DescriptorHeap> cullingDescHeap;
	virtual CommandListType GetCommandListType() { return CommandListType_Compute; }
	virtual void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	virtual void Dispose();
	virtual std::vector<TemporalResourceCommand>& SendRenderTextureRequire(EventData& evt);
	virtual void RenderEvent(EventData& data, ThreadCommand* commandList);
};