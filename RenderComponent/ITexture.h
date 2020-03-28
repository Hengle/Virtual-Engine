#pragma once
#include "../Common/MObject.h"
#include "../Common/d3dUtil.h"
enum class TextureDimension : uint
{
	Tex2D = 0,
	Tex3D = 1,
	Cubemap = 2,
	Tex2DArray = 3,
	Num = 4
};
class DescriptorHeap;
class ITexture : public MObject
{
private:
	uint srvDescID = 0;
protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
	DXGI_FORMAT mFormat;
	UINT depthSlice;
	UINT mWidth = 0;
	UINT mHeight = 0;
	UINT mipCount = 1;
	size_t resourceSize = 0;
	TextureDimension dimension;
	ITexture();
	~ITexture();
public:
	
	size_t GetResourceSize() const
	{
		return resourceSize;
	}

	ID3D12Resource* GetResource() const
	{
		return Resource.Get();
	}
	TextureDimension GetDimension() const
	{
		return dimension;
	}
	DXGI_FORMAT GetFormat() const
	{
		return mFormat;
	}
	uint GetDepthSlice() const
	{
		return depthSlice;
	}
	uint GetWidth() const
	{
		return mWidth;
	}
	uint GetHeight() const
	{
		return mHeight;
	}
	uint GetMipCount() const
	{
		return mipCount;
	}
	uint GetGlobalDescIndex() const
	{
		return srvDescID;
	}
	virtual void BindSRVToHeap(DescriptorHeap* targetHeap, UINT index, ID3D12Device* device) = 0;
	void ReleaseAfterFrame();
};