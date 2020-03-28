#pragma once
#include "../../Common/d3dUtil.h"
#include "../../Common/MetaLib.h"
#include "../Texture.h"
class Texture;
class VirtualTextureData
{
	class TexturePack
	{
		bool initialized = false;
	public:
		StackObject<Texture> tex;
		TexturePack() {}
		template <typename ... Args>
		void Initialize(Args&&... args)
		{
			if (initialized) return;
			tex.New(std::forward<Args>(args)...);
			initialized = true;
		}
		~TexturePack()
		{
			if (initialized) tex.Delete();
		}
		TexturePack(const TexturePack& another)
		{
			if (!another.initialized) return;
			initialized = true;
			tex.New(*another.tex);
		}
		void operator=(const TexturePack& another)
		{
			if (initialized) tex.Delete();
			if (!another.initialized)
			{
				initialized = true;
				return;
			}
			initialized = true;
			tex.New(*another.tex);
		}
	};
	struct uint2Hash
	{
		size_t operator()(const uint2& source) const
		{
			uint64_t value;
			memcpy(&value, &source, sizeof(uint64_t));
			return std::hash<size_t>::_Do_hash(value);
		}
	};
	struct uint2Equal
	{
		bool operator()(const uint2& a, const uint2& b) const
		{
			return a.x == b.x && a.y == b.y;
		}
	};
public:
	struct TerrainChunk
	{
		TexturePack splatTex;
		TexturePack indexTex;
	};
	struct TerrainMaterial
	{
		TexturePack albedoTex;
		TexturePack normalTex;
	};
	struct TerrainChunkPath
	{
		std::string splatPath;
		std::string indexPath;
	};
	std::unordered_map<uint2, TerrainChunkPath, uint2Hash, uint2Equal> splatMaps;
	std::unordered_map<uint2, TerrainChunk, uint2Hash, uint2Equal> loadedSplatMaps;
	std::vector<TerrainMaterial> materialTextures;
	VirtualTextureData(
		ID3D12Device* device,
		std::string& folderPath,
		const std::string& fileName)noexcept;
	Texture* GetAlbedo(uint index) const;
	Texture* GetNormal(uint index) const;
	size_t MaterialCount() const { return materialTextures.size(); }
	std::pair<Texture*, Texture*> GetSplatIndex(uint2 splat) const;
	bool StartLoadSplat(ID3D12Device* device, uint2 target);
	void RemoveLoadedSplat(uint2 target);
};