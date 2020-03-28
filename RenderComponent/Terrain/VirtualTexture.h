#pragma once
#include "../RenderTexture.h"
#include "../../Common/MetaLib.h"
#include "../../Common/MObject.h"
#include "../../Common/d3dUtil.h"
#include "../TextureHeap.h"
#include "../UploadBuffer.h"
#include "../StructuredBuffer.h"
#include "../DescriptorHeap.h"
#include "../CBufferPool.h"
#include "../../Common/Runnable.h"
#include "../../Common/MetaLib.h"
#include "../../Singleton/PSOContainer.h"


class TransitionBarrierBuffer;
class Shader;
class ComputeShader;
class FrameResource;
class VirtualTexture;
struct VirtualChunk
{
	uint index;
	uint size;
};

struct VirtualTextureRenderArgs
{
	ID3D12Device* device;
	ID3D12GraphicsCommandList* commandList;
	FrameResource* frameResource;
	TransitionBarrierBuffer* barrierBuffer;
	uint2 startIndex;
	uint size;
	uint renderTextureID;
};
class VirtualTexture : public MObject
{
	//typedef Runnable<void(ID3D12Device*, ID3D12GraphicsCommandList*, FrameResource*, TransitionBarrierBuffer)> RenderTask;
	struct uint2Hash
	{
		size_t operator()(const uint2& value) const
		{
			return std::hash<uint>::_Do_hash(value.x) ^ std::hash<uint>::_Do_hash(value.y);
		}
	};

	struct IndirectParams
	{
		uint4 _Var; //XY: start Z: size W: result
		uint2 _IndirectTexelSize; //XY: start Z: size W: result
	};

	struct CombineParams
	{
		uint2 _TexelSize;	//XY: 1 / resolution		ZW: resolution
	};

	struct CombineCommand
	{
		RenderTexture* combineSource[4];
		RenderTexture* combineDest;
		CombineParams combineParams;
	};

	struct VirtualExecutor
	{
		Runnable<void(const VirtualTextureRenderArgs&)> executor;
		uint2 index;
		uint size;
	};

	struct VTUpdateCommand
	{
		enum UpdateCommandType
		{
			UpdateCommandType_Print = 0,
			UpdateCommandType_Combine = 1,
			UpdateCommandType_GenerateMip = 2,
			UpdateCommandType_Executor = 3,
			UpdateCommandType_ClearBuffer = 4,
			UpdateCommandType_SetBuffer = 5,
			UpdateCommandType_Num = 6
		};
		UpdateCommandType type;
		union
		{
			IndirectParams indirectParams;
			CombineCommand combineCommand;
			StackObject<VirtualExecutor> executor;
			uint generateMipTarget;
			int2 setBufferIndex;
		};
		bool executorInitialized = false;
		VTUpdateCommand() {}
		VTUpdateCommand(const VTUpdateCommand& command);
		void operator=(const VTUpdateCommand& command);
		~VTUpdateCommand();
	};

	struct VTUpdateCommandBuffer
	{
	private:

		uint count[VTUpdateCommand::UpdateCommandType_Num];
	public:
		std::vector<VTUpdateCommand> updateCommand;
		VTUpdateCommandBuffer()
		{
			updateCommand.reserve(20);
			memset(count, 0, sizeof(uint) * VTUpdateCommand::UpdateCommandType_Num);
		}
		void AddCommand(const VTUpdateCommand& command);
		void ClearCommand();

		uint GetCommandCount(VTUpdateCommand::UpdateCommandType type)
		{
			return count[type];
		}
	};

	struct uint2Equal
	{
		bool operator()(const uint2& a, const uint2& b) const
		{
			return a.x == b.x && a.y == b.y;
		}
	};

	struct PropertyIDs
	{
		bool initialized = false;
		uint IndirectParams;
		uint CombineParams;
		uint _IndirectTex;
		uint _CombineResultTex;
		uint _CombineTex;
		uint _TexMipLevels;
		uint _VirtualTex;
		uint _IndirectBuffer;
		uint VirtualTextureParams;
		uint _TextureIndexBuffer;
		uint _SettingCommand;
		uint SetBufferParams;
		void Init();
	};
	static PropertyIDs propIDs;
	struct TextureBlock
	{
		StackObject<RenderTexture> renderTexture;
		int heapID;
		bool initialized = false;
		TextureBlock() {}
		TextureBlock(const TextureBlock& block);
		~TextureBlock();
	};
	struct RenderConstSettings
	{
		float2 _ChunkTexelSize;
		uint2 _IndirectTexelSize;
		uint _MaxMipLevel;
	};
	StackObject<PSOContainer, true> psoContainer;
	StackObject<CBufferPool, true> cbufferPool;
	StackObject<StructuredBuffer, true> textureIndexBuffer;
	StackObject<RenderTexture, true> indirectTex;
	StackObject<TextureHeap, true> texHeap;
	StackObject<UploadBuffer, true> renderConstDataBuffer;
	std::vector<int2> bufferSetCommand;
	VTUpdateCommandBuffer commandBuffer;
	std::vector<std::vector<TextureBlock>> textureChunks;
	std::unordered_map<uint2, VirtualChunk, uint2Hash, uint2Equal> chunkPoses;
	std::vector<uint> indexPool;
	ComputeShader* settingShader;
	D3D12_RESOURCE_STATES indirectCurrentState;
	uint texelSize;
	uint textureCapacity;
	uint indirectSize;
	std::vector<DXGI_FORMAT> mapFormats;
	bool InternalCreate(uint2 index, uint size, VirtualChunk& result);
public:
	uint GetChunkResolution() const { return texelSize; }
	uint GetIndirectResolution() const { return indirectSize; }
	uint GetTextureCapacity() const { return textureCapacity; }
	DXGI_FORMAT* GetFormats() const noexcept
	{
		return (DXGI_FORMAT*)mapFormats.data();
	}
	size_t GetFormatCount() const noexcept
	{
		return mapFormats.size();
	}
	VirtualTexture(
		ID3D12Device* device,
		uint indirectSize,
		uint chunkSize,
		DXGI_FORMAT* formats,
		uint formatCount,
		uint chunkCount);
	~VirtualTexture();
	void AddRenderCommand(
		const Runnable<void(const VirtualTextureRenderArgs&)>& runnable,
		uint2 index,
		uint size)
	{
		auto updatecommand = VTUpdateCommand();
		updatecommand.type = VTUpdateCommand::UpdateCommandType_Executor;
		updatecommand.executor.InPlaceNew(runnable, index, size);
		updatecommand.executorInitialized = true;
		commandBuffer.AddCommand(updatecommand);

	}
	PSOContainer* GetPSOContainer() const
	{
		return psoContainer;
	}
	size_t GetLeftedChunkSize() const { return indexPool.size(); }
	RenderTexture* GetRenderTexture(uint textureType, uint textureIndex) const
	{
		return textureChunks[textureType][textureIndex].renderTexture;
	}
	bool GetChunk(uint2 index, uint size, VirtualChunk& result);
	bool GetChunk(uint2 index, VirtualChunk& result);
	bool CreateChunk(uint2 index, uint size, VirtualChunk& result);
	void ReturnChunk(uint2 index, bool sendBufferCommand);
	void ExecuteUpdate(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		FrameResource* currentResource,
		TransitionBarrierBuffer* barrierBuffer);

	bool CombineUpdate(uint2 index, uint targetSize);
	bool GenerateMip(uint2 index);
	void SetTextureProperty(
		Shader* targetShader,
		ID3D12GraphicsCommandList* commandList);
};