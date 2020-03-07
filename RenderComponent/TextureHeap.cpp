#include "TextureHeap.h"
using namespace Microsoft::WRL;
TextureHeap::TextureHeap(ID3D12Device* device, size_t chunkSize) :
	chunkSize(chunkSize)
{
	
	D3D12_HEAP_DESC heapDesc;
	D3D12_HEAP_PROPERTIES prop;
	prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	prop.CreationNodeMask = 0;
	prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	prop.Type = D3D12_HEAP_TYPE_DEFAULT;
	prop.VisibleNodeMask = 0;
	heapDesc.Properties = prop;
	heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
	heapDesc.SizeInBytes = (size_t)chunkSize;
	device->CreateHeap(
		&heapDesc,
		IID_PPV_ARGS(&heap));
}