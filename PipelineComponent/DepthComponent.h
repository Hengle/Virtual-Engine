#pragma once 
#include "PipelineComponent.h"
class RenderTexture;
class UploadBuffer;
class DescriptorHeap;
class PSOContainer;
class DepthCameraResource : public IPipelineResource
{
public:
	UploadBuffer ub;
	UploadBuffer cullBuffer;
	DescriptorHeap hizHeap;
	inline static const uint descHeapSize = 11;

	uint descIndex;
	RenderTexture* hizTexture = nullptr;
	DepthCameraResource(ID3D12Device* device);
	~DepthCameraResource() noexcept;
};
class DepthComponent : public PipelineComponent
{
private:

public:
	PSOContainer* depthPrepassContainer = nullptr;
	std::vector<TemporalResourceCommand> tempRTRequire;
	virtual CommandListType GetCommandListType()
	{
		return CommandListType_Graphics;
	}
	virtual void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	virtual void Dispose();
	virtual std::vector<TemporalResourceCommand>& SendRenderTextureRequire(EventData& evt);
	virtual void RenderEvent(EventData& data, ThreadCommand* commandList);
};