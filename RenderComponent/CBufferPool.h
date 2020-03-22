#pragma once
#include "../Common/d3dUtil.h"
#include "../Common/MObject.h"
#include "../Common/MetaLib.h"
#include "UploadBuffer.h"
struct ConstBufferElement
{
	UploadBuffer* buffer;
	UINT element;
};
class CBufferPool
{
private:
	struct UploadBufferChunk
	{
		StackObject<UploadBuffer> buffer;
		bool isInitialized = false;
		UploadBufferChunk() {}
		UploadBufferChunk(const UploadBufferChunk&);
		~UploadBufferChunk();
		void operator=(const UploadBufferChunk&);
	};
	std::vector<UploadBufferChunk> arr;
	std::vector<ConstBufferElement> poolValue;
	UINT capacity;
	UINT stride;
	void Add(ID3D12Device* device);
public:
	CBufferPool(UINT stride, UINT initCapacity);
	~CBufferPool();
	ConstBufferElement Get(ID3D12Device* device);
	void Return(ConstBufferElement& target);
};