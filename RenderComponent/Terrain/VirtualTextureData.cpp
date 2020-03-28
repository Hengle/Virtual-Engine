#include "VirtualTextureData.h"
#include "../../CJsonObject/CJsonObject.hpp"
using namespace neb;
VirtualTextureData::VirtualTextureData(
	ID3D12Device* device,
	std::string& folderName,
	const std::string& fileName) noexcept
{
	splatMaps.reserve(64);
	loadedSplatMaps.reserve(64);
	materialTextures.reserve(32);
	StackObject<CJsonObject> rootJsonPtr;
	if (!folderName.empty() && (folderName[folderName.length() - 1] != '\\' || folderName[folderName.length() - 1] != '/'))
	{
		folderName += '\\';
	}
	bool readSuccess = ReadJson(folderName + fileName, rootJsonPtr);
	//Read Splat Textures
	{
		CJsonObject splatTexturesJson;
		rootJsonPtr->Get("splat", splatTexturesJson);
		std::string key;
		CJsonObject splatObj;
		std::string path0, path1;
		while (splatTexturesJson.GetKey(key))
		{
			if (splatTexturesJson.Get(key, splatObj))
			{
				uint2 value = { 0,0 };
				ReadStringToIntVector<uint2>((char*)key.c_str(), key.size(), &value);
				static std::string splatStr = "splat";
				static std::string indexStr = "index";
				if (splatObj.Get(splatStr, path0) && splatObj.Get(indexStr, path1))
				{
					splatMaps.insert_or_assign(
						value,
						TerrainChunkPath()
					);
					auto pack = splatMaps.find(value);
					pack->second.indexPath = folderName + path1;
					pack->second.splatPath = folderName + path0;
				}
			}
		}

	}
	//Read Albedo Textures

	CJsonObject albedoJson;
	if (rootJsonPtr->Get("texture", albedoJson))
	{
		std::string key;
		std::string path0, path1;
		CJsonObject packObject;
		while (albedoJson.GetKey(key))
		{
			if (albedoJson.Get(key, packObject))
			{
				static std::string albedo = "albedo";
				static std::string normal = "normal";
				if (packObject.Get(albedo, path0) && packObject.Get(normal, path1))
				{
					TerrainMaterial& pack = (TerrainMaterial&)materialTextures.emplace_back();
					pack.albedoTex.Initialize(
						device,
						folderName + path0
					);
					pack.normalTex.Initialize(
						device,
						folderName + path1
					);
				}
			}
		}
	}
	rootJsonPtr.Delete();
}
Texture* VirtualTextureData::GetAlbedo(uint index) const
{
	return materialTextures[index].albedoTex.tex;
}
Texture* VirtualTextureData::GetNormal(uint index) const
{
	return materialTextures[index].normalTex.tex;
}
std::pair<Texture*, Texture*> VirtualTextureData::GetSplatIndex(uint2 splat) const
{
	auto ite = loadedSplatMaps.find(splat);
	if (ite == loadedSplatMaps.end()) return { nullptr ,nullptr };
	return { (Texture*)ite->second.splatTex.tex, (Texture*)ite->second.indexTex.tex };
}

bool VirtualTextureData::StartLoadSplat(ID3D12Device* device, uint2 target)
{
	auto ite = splatMaps.find(target);
	if (ite == splatMaps.end()) return false;
	if (loadedSplatMaps.find(target) != loadedSplatMaps.end()) return true;
	loadedSplatMaps.insert_or_assign(
		target, TerrainChunk()
	);
	auto loadedIte = loadedSplatMaps.find(target);
	loadedIte->second.splatTex.Initialize(
		device, ite->second.splatPath
	);
	loadedIte->second.indexTex.Initialize(
		device, ite->second.indexPath
	);
	return true;
}

void  VirtualTextureData::RemoveLoadedSplat(uint2 target)
{
	loadedSplatMaps.erase(target);
}