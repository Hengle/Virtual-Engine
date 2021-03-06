#pragma once
#include "../Common/d3dUtil.h"
class PSOContainer;
class Shader;
class RenderTexture;
class UploadBuffer;
enum BackBufferState
{
	BackBufferState_Present = 0,
	BackBufferState_RenderTarget = 1
};

class TransitionBarrierBuffer;
class Graphics
{
public:
	static void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	static void Blit(
		ID3D12GraphicsCommandList* commandList,
		ID3D12Device* device,
		D3D12_CPU_DESCRIPTOR_HANDLE* renderTarget,
		UINT renderTargetCount,
		D3D12_CPU_DESCRIPTOR_HANDLE* depthTarget,
		PSOContainer* container, uint containerIndex,
		UINT width, UINT height,
		Shader* shader, UINT pass);


	inline static void UAVBarrier(
		ID3D12GraphicsCommandList* commandList,
		ID3D12Resource* resource)
	{
		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.UAV = { resource };
		commandList->ResourceBarrier(1, &barrier);
	}

	static void CopyTexture(
		ID3D12GraphicsCommandList* commandList,
		RenderTexture* source, UINT sourceSlice, UINT sourceMipLevel,
		RenderTexture* dest, UINT destSlice, UINT destMipLevel);

	static void CopyBufferToTexture(
		ID3D12GraphicsCommandList* commandList,
		UploadBuffer* sourceBuffer, size_t sourceBufferOffset,
		ID3D12Resource* textureResource, UINT targetMip,
		UINT width, UINT height, UINT depth, DXGI_FORMAT targetFormat, UINT pixelSize, TransitionBarrierBuffer* barrierBuffer);
	static void CopyBufferToBC5Texture(
		ID3D12GraphicsCommandList* commandList,
		UploadBuffer* sourceBuffer, size_t sourceBufferOffset,
		ID3D12Resource* textureResource, UINT targetMip,
		UINT width, UINT height, UINT depth, DXGI_FORMAT targetFormat, UINT pixelSize, TransitionBarrierBuffer* barrierBuffer);
};