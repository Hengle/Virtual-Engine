#include "UploadBuffer.h"
#include "../Singleton/FrameResource.h"
UploadBuffer::UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer, size_t stride)
{
	mIsConstantBuffer = isConstantBuffer;
	// Constant buffer elements need to be multiples of 256 bytes.
	// This is because the hardware can only view constant data 
	// at m*256 byte offsets and of n*256 byte lengths. 
	// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
	// UINT64 OffsetInBytes; // multiple of 256
	// UINT   SizeInBytes;   // multiple of 256
	// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
	mElementCount = elementCount;
	if (isConstantBuffer)
		mElementByteSize = d3dUtil::CalcConstantBufferByteSize(stride);
	else mElementByteSize = stride;
	mStride = stride;
	if (mUploadBuffer)
	{
		mUploadBuffer->Unmap(0, nullptr);
		mUploadBuffer = nullptr;
	}
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize*elementCount),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mUploadBuffer)));
	ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));

	// We do not need to unmap until we are done with the resource.  However, we must not write to
	// the resource while it is in use by the GPU (so we must use synchronization techniques).
}

void UploadBuffer::Create(ID3D12Device* device, UINT elementCount, bool isConstantBuffer, size_t stride)
{
	mIsConstantBuffer = isConstantBuffer;
	// Constant buffer elements need to be multiples of 256 bytes.
	// This is because the hardware can only view constant data 
	// at m*256 byte offsets and of n*256 byte lengths. 
	// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
	// UINT64 OffsetInBytes; // multiple of 256
	// UINT   SizeInBytes;   // multiple of 256
	// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
	mElementCount = elementCount;
	if (isConstantBuffer)
		mElementByteSize = d3dUtil::CalcConstantBufferByteSize(stride);
	else mElementByteSize = stride;
	mStride = stride;
	if (mUploadBuffer)
	{
		mUploadBuffer->Unmap(0, nullptr);
		mUploadBuffer = nullptr;
	}
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize*elementCount),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mUploadBuffer)));
	ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
}

void UploadBuffer::ReleaseAfterFlush(FrameResource* res) { res->ReleaseResourceAfterFlush(mUploadBuffer); }