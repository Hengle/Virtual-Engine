#include "CBufferPool.h" 
#include <math.h>
void CBufferPool::Add(ID3D12Device* device)
{
	UploadBufferChunk& lastOne = (UploadBufferChunk&)arr.emplace_back();
	lastOne.isInitialized = true;
	lastOne.buffer.New(device, capacity, true, stride);
	for (UINT i = 0; i < capacity; ++i)
	{
		ConstBufferElement ele;
		ele.buffer = lastOne.buffer;
		ele.element = i;
		poolValue.push_back(ele);
	}
}

CBufferPool::CBufferPool(UINT stride, UINT initCapacity) :
	capacity(initCapacity),
	stride(stride)
{
	poolValue.reserve(initCapacity);
	arr.reserve(10);
}

CBufferPool::~CBufferPool()
{
}

ConstBufferElement CBufferPool::Get(ID3D12Device* device)
{
	UINT value = capacity;
	if (poolValue.empty())
	{
		Add(device);
	}
	auto ite = poolValue.end() - 1;
	ConstBufferElement pa = *ite;
	poolValue.erase(ite);
	return pa;

}

void CBufferPool::Return(ConstBufferElement& target)
{
	poolValue.push_back(target);
}

CBufferPool::UploadBufferChunk::UploadBufferChunk(const UploadBufferChunk& chunk)
{
	if (chunk.isInitialized)
	{
		isInitialized = true;
		buffer.New(*chunk.buffer);
	}
}
CBufferPool::UploadBufferChunk::~UploadBufferChunk()
{
	if (isInitialized)
		buffer.Delete();
}
void CBufferPool::UploadBufferChunk::operator=(const UploadBufferChunk& chunk)
{
	if (isInitialized) buffer.Delete();
	isInitialized = chunk.isInitialized;
	if (chunk.isInitialized)
	{
		buffer.New(*chunk.buffer);
	}
}