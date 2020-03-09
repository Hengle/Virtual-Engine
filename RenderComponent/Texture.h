#pragma once
#include "../Common/d3dUtil.h"
#include <string>
#include <vector>
#include "../Common/MObject.h"
#include "ITexture.h"
class DescriptorHeap;
class UploadBuffer;
class FrameResource;
class TextureHeap;

struct TextureData
{
	UINT width;
	UINT height;
	UINT depth;
	TextureDimension textureType;
	UINT mipCount;
	enum LoadFormat
	{
		LoadFormat_RGBA8 = 0,
		LoadFormat_RGBA16 = 1,
		LoadFormat_RGBAFloat16 = 2,
		LoadFormat_RGBAFloat32 = 3,
		LoadFormat_RGFLOAT16 = 4,
		LoadFormat_RG16 = 5,
		LoadFormat_BC7 = 6,
		LoadFormat_BC6H = 7,
		LoadFormat_Num = 8
	};
	LoadFormat format;
};

class Texture : public ITexture
{
private:
	void GetResourceViewDescriptor(D3D12_SHADER_RESOURCE_VIEW_DESC& desc);
public:

	//Async Load
	Texture(
		ID3D12Device* device,
		const std::string& filePath,
		TextureDimension type = TextureDimension::Tex2D,
		TextureHeap* placedHeap = nullptr,
		size_t placedOffset = 0
	);
	//Sync Copy
	Texture(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		FrameResource* resource,
		const UploadBuffer& buffer,
		UINT width,
		UINT height,
		UINT depth,
		TextureDimension textureType,
		UINT mipCount,
		TextureData::LoadFormat format,
		TextureHeap* placedHeap = nullptr,
		size_t placedOffset = 0
	);
	virtual void BindSRVToHeap(DescriptorHeap* targetHeap, UINT index, ID3D12Device* device);
};

