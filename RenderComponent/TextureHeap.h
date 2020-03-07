#pragma once
#include "../Common/d3dUtil.h"
#include "../Common/MObject.h"
class TextureHeap
{
private:
	Microsoft::WRL::ComPtr<ID3D12Heap> heap;
	size_t chunkSize;
public:
	TextureHeap() : heap(nullptr), chunkSize(0) {}
	size_t GetChunkSize() const { return chunkSize; }
	TextureHeap(ID3D12Device* device, size_t chunkSize);
	ID3D12Heap* GetHeap() const
	{
		return heap.Get();
	}
};