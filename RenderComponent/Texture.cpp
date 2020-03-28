#include "Texture.h"
#include "../Singleton/FrameResource.h"
#include "../RenderComponent/DescriptorHeap.h"
#include <fstream>
#include "ComputeShader.h"
#include "../Singleton/ShaderCompiler.h"
#include "../LogicComponent/World.h"
#include "RenderCommand.h"
#include "../Singleton/ShaderID.h"
#include "../Singleton/Graphics.h"
#include "TextureHeap.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
struct TextureFormat_LoadData
{
	DXGI_FORMAT format;
	uint pixelSize;
	bool bcCompress;
};
TextureFormat_LoadData Texture_GetFormat(TextureData::LoadFormat loadFormat)
{
	TextureFormat_LoadData loadData;
	loadData.bcCompress = false;
	switch (loadFormat)
	{
	case TextureData::LoadFormat_RGBA8:
		loadData.pixelSize = 4;
		loadData.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case TextureData::LoadFormat_RGBA16:
		loadData.pixelSize = 8;
		loadData.format = DXGI_FORMAT_R16G16B16A16_UNORM;
		break;
	case TextureData::LoadFormat_RGBAFloat16:
		loadData.pixelSize = 8;
		loadData.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;
	case TextureData::LoadFormat_RGBAFloat32:
		loadData.pixelSize = 16;
		loadData.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case TextureData::LoadFormat_RG16:
		loadData.pixelSize = 4;
		loadData.format = DXGI_FORMAT_R16G16_UNORM;
		break;
	case TextureData::LoadFormat_RGFLOAT16:
		loadData.pixelSize = 4;
		loadData.format = DXGI_FORMAT_R16G16_FLOAT;
		break;
	case TextureData::LoadFormat_BC7:
		loadData.pixelSize = 1;
		loadData.format = DXGI_FORMAT_BC7_UNORM;
		loadData.bcCompress = true;
		break;
	case TextureData::LoadFormat_BC6H:
		loadData.pixelSize = 1;
		loadData.format = DXGI_FORMAT_BC6H_UF16;
		loadData.bcCompress = true;
		break;
	case TextureData::LoadFormat_UINT:
		loadData.pixelSize = 2;
		loadData.format = DXGI_FORMAT_R16_UINT;
		break;
	case TextureData::LoadFormat_UINT2:
		loadData.pixelSize = 4;
		loadData.format = DXGI_FORMAT_R16G16_UINT;
		break;
	case TextureData::LoadFormat_UINT4:
		loadData.pixelSize = 8;
		loadData.format = DXGI_FORMAT_R16G16B16A16_UINT;
		break;
	}
	return loadData;
}

void ReadData(const std::string& str, TextureData& headerResult, std::vector<char>& dataResult, uint& startMipLevel, uint maximumMipLevel)
{
	std::ifstream ifs;
	ifs.open(str, std::ios::binary);
	if (!ifs)
	{
		throw "File Not Exists!";
	}
	ifs.read((char*)&headerResult, sizeof(TextureData));
	if (headerResult.mipCount < 1) headerResult.mipCount = 1;
	UINT formt = (UINT)headerResult.format;
	if (formt >= (UINT)(TextureData::LoadFormat_Num) ||
		(UINT)headerResult.textureType >= (UINT)TextureDimension::Num)
	{
		throw "Invalide Format";
	}
	UINT stride = 0;
	TextureFormat_LoadData loadData = Texture_GetFormat(headerResult.format);
	stride = loadData.pixelSize;
	if (headerResult.depth != 1 && startMipLevel != 0)
	{
		throw "Non-2D map can not use mip streaming!";
	}
	headerResult.mipCount = Min<uint32_t>(headerResult.mipCount, maximumMipLevel);
	startMipLevel = Min<uint32_t>(startMipLevel, headerResult.mipCount - 1);
	size_t size = 0;
	size_t offsetSize = 0;
	UINT depth = headerResult.depth;

	for (UINT j = 0; j < depth; ++j)
	{
		UINT width = headerResult.width;
		UINT height = headerResult.height;
		
		for (uint i = 0; i < startMipLevel; ++i)
		{
			UINT currentChunkSize = stride * width * height;
			offsetSize += Max<uint>(currentChunkSize, 512);
			width /= 2;
			height /= 2;
			width = Max<uint>(1, width);
			height = Max<uint>(1, height);
		}
		headerResult.width = width;
		headerResult.height = height;
		for (UINT i = startMipLevel; i < headerResult.mipCount; ++i)
		{
			UINT currentChunkSize = stride * width * height;
			size += Max<uint>(currentChunkSize, 512);
			width /= 2;
			height /= 2;
			width = Max<uint>(1, width);
			height = Max<uint>(1, height);
		}
	}
	dataResult.resize(size);
	ifs.seekg(offsetSize + sizeof(TextureData), std::ios::beg);
	ifs.read(dataResult.data(), size);
}
struct DispatchCBuffer
{
	XMUINT2 _StartPos;
	XMUINT2 _Count;
};

class TextureLoadCommand : public RenderCommand
{
private:
	UploadBuffer ubuffer;
	ID3D12Resource* res;
	TextureData::LoadFormat loadFormat;
	UINT width;
	UINT height;
	UINT mip;
	UINT arraySize;
	TextureDimension type;
	bool* flag;
public:
	TextureLoadCommand(ID3D12Device* device,
		UINT element,
		void* dataPtr,
		ID3D12Resource* res,
		TextureData::LoadFormat loadFormat,
		UINT width,
		UINT height,
		UINT mip,
		UINT arraySize,
		TextureDimension type, bool* flag) :
		res(res), loadFormat(loadFormat), width(width), height(height), mip(mip), arraySize(arraySize), type(type),
		ubuffer(device, (element + 2047) & ~2047, false, 1), flag(flag)
	{
		ubuffer.CopyDatas(0, element, dataPtr);
	}

	TextureLoadCommand(
		const UploadBuffer& buffer,
		ID3D12Resource* res,
		TextureData::LoadFormat loadFormat,
		UINT width,
		UINT height,
		UINT mip,
		UINT arraySize,
		TextureDimension type, bool* flag
	) :res(res), loadFormat(loadFormat), 
		width(width), height(height), mip(mip), arraySize(arraySize), type(type), ubuffer(buffer),
		flag(flag)
	{

	}
	virtual void operator()(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		FrameResource* resource,
		TransitionBarrierBuffer* barrier) override
	{
		ubuffer.ReleaseAfterFlush(resource);
		UINT offset = 0;

		TextureFormat_LoadData loadData = Texture_GetFormat(loadFormat);
		if (type == TextureDimension::Tex3D)
		{
			UINT curWidth = width;
			UINT curHeight = height;
			for (UINT i = 0; i < mip; ++i)
			{
				Graphics::CopyBufferToTexture(
					commandList,
					&ubuffer,
					offset,
					res,
					i,
					curWidth, curHeight,
					arraySize,
					loadData.format , loadData.pixelSize, barrier
				);
				UINT chunkOffset = loadData.pixelSize * curWidth * curHeight;
				offset += Max<uint>(chunkOffset, 512);
				curWidth /= 2;
				curHeight /= 2;
			}

		}
		else
		{
			for (UINT j = 0; j < arraySize; ++j)
			{
				UINT curWidth = width;
				UINT curHeight = height;


				for (UINT i = 0; i < mip; ++i)
				{
					if (loadData.bcCompress)
					{
						Graphics::CopyBufferToBC5Texture(
							commandList,
							&ubuffer,
							offset,
							res,
							(j * mip) + i,
							curWidth, curHeight,
							1,
							loadData.format, loadData.pixelSize, barrier
						);
					}
					else
					{
						Graphics::CopyBufferToTexture(
							commandList,
							&ubuffer,
							offset,
							res,
							(j * mip) + i,
							curWidth, curHeight,
							1,
							loadData.format, loadData.pixelSize, barrier
						);
					}
					UINT chunkOffset = loadData.pixelSize * curWidth * curHeight;
					offset += Max<uint>(chunkOffset, 512);
					curWidth /= 2;
					curHeight /= 2;
				}
			}
		}
		size_t ofst = offset;
		size_t size = ubuffer.GetElementCount();
		*flag = true;
	}
};

Texture::Texture(
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
	TextureHeap* placedHeap,
	size_t placedOffset) : ITexture()
{
	dimension = textureType;
	if (textureType == TextureDimension::Cubemap)
		depth = 6;
	auto loadData = Texture_GetFormat(format);
	mFormat = loadData.format;
	this->depthSlice = depth;
	this->mWidth = width;
	this->mHeight = height;
	mipCount = Max<uint>(1, mipCount);
	this->mipCount = mipCount;
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = depth;
	texDesc.MipLevels = mipCount;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceSize = device->GetResourceAllocationInfo(
		0, 1, &texDesc).SizeInBytes;
	if (placedHeap)
	{
		ThrowIfFailed(device->CreatePlacedResource(
			placedHeap->GetHeap(),
			placedOffset,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,//D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&Resource)));
	}
	else
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,//D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&Resource)));
	}
	TextureLoadCommand cmd(
		buffer,
		Resource.Get(),
		format,
		width,
		height,
		mipCount,
		depth,
		dimension, &loaded);
	TransitionBarrierBuffer barrierBuffer;
	cmd(device, commandList, resource, &barrierBuffer);
	barrierBuffer.ExecuteCommand(commandList);
	BindSRVToHeap(World::GetInstance()->GetGlobalDescHeap(), GetGlobalDescIndex(), device);
}

Texture::Texture(
	ID3D12Device* device,
	const std::string& filePath,
	TextureDimension type,
	uint32_t maximumLoadMipmap,
	uint32_t startMipMap,
	TextureHeap* placedHeap,
	size_t placedOffset
) : ITexture()
{
	maximumLoadMipmap = Max<uint32_t>(maximumLoadMipmap + startMipMap, startMipMap + 1);
	dimension = type;
	TextureData data;//TODO : Read From Texture
	ZeroMemory(&data, sizeof(TextureData));

	std::vector<char> dataResults;
	ReadData(filePath, data, dataResults, startMipMap, maximumLoadMipmap);
	if (data.textureType != type)
		throw "Texture Type Not Match Exception";

	if (type == TextureDimension::Cubemap && data.depth != 6)
		throw "Cubemap's tex size must be 6";
	auto loadData = Texture_GetFormat(data.format);
	mFormat = loadData.format;
	data.mipCount = Max<uint>(1, data.mipCount);
	data.mipCount -= startMipMap;
	this->depthSlice = data.depth;
	this->mWidth = data.width;
	this->mHeight;
	this->mipCount = data.mipCount;
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = data.width;
	texDesc.Height = data.height;
	texDesc.DepthOrArraySize = data.depth;
	texDesc.MipLevels = data.mipCount;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceSize = device->GetResourceAllocationInfo(
		0, 1, &texDesc).SizeInBytes;
	if (placedHeap)
	{
		ThrowIfFailed(device->CreatePlacedResource(
			placedHeap->GetHeap(),
			placedOffset,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,//D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&Resource)));
	}
	else
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,//D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&Resource)));
	}

	TextureLoadCommand* cmd = new TextureLoadCommand(
		device,
		dataResults.size(),
		dataResults.data(),
		Resource.Get(),
		data.format,
		data.width,
		data.height,
		texDesc.MipLevels,
		data.depth,
		data.textureType, &loaded);
	RenderCommand::AddCommand(cmd);
	BindSRVToHeap(World::GetInstance()->GetGlobalDescHeap(), GetGlobalDescIndex(), device);
}
void Texture::GetResourceViewDescriptor(D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc)
{
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	auto format = mFormat;
	switch (dimension) {
	case TextureDimension::Tex2D:
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = mipCount;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		break;
	case TextureDimension::Tex3D:
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = mipCount;
		srvDesc.Texture3D.MostDetailedMip = 0;
		srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
	case TextureDimension::Cubemap:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = mipCount;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		srvDesc.Format = format;
		break;
	}
}

void Texture::BindSRVToHeap(DescriptorHeap* targetHeap, UINT index, ID3D12Device* device)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	GetResourceViewDescriptor(srvDesc);
	device->CreateShaderResourceView(Resource.Get(), &srvDesc, targetHeap->hCPU(index));
}