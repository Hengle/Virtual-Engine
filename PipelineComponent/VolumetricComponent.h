#pragma once
#include "PipelineComponent.h"
#include "../RenderComponent/UploadBuffer.h"
class RenderTexture;
class VolumetricComponent : public PipelineComponent
{
private:
	std::vector<TemporalResourceCommand> tempResource;
public:
	static std::unique_ptr<RenderTexture> volumeRT;
	virtual CommandListType GetCommandListType() {
		return CommandListType::CommandListType_Compute;
	}
	virtual void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	virtual void Dispose();
	virtual std::vector<TemporalResourceCommand>& SendRenderTextureRequire(EventData& evt);
	virtual void RenderEvent(EventData& data, ThreadCommand* commandList);
};

class VolumetricCameraData : public IPipelineResource
{
public:
	inline static const uint HEAP_SIZE = 9;
	std::unique_ptr<RenderTexture> lastVolume;
	DescriptorHeap heap;
	UploadBuffer cbuffer;
	VolumetricCameraData(ID3D12Device* device);
	~VolumetricCameraData();
};