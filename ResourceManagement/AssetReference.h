#include "../Common/d3dUtil.h"
#include "../Common/MObject.h"
#include "../Common/Runnable.h"
#pragma once
enum class AssetLoadType : uint
{
	Mesh = 0,
	Texture2D = 1,
	Texture3D = 2,
	Cubemap = 3,
	GPURPMaterial = 4,
	Scene = 5
};

class AssetReference
{
private:
	Runnable<void(const ObjectPtr<MObject>&)> mainThreadFinishFunctor;
	AssetLoadType loadType;
	std::string path;
	std::string guid;
	bool isLoad;
	static ObjectPtr<MObject> LoadObject(AssetLoadType loadType, ID3D12Device* device, const std::string& path, const std::string& guid);
public:
	bool IsLoad() const { return isLoad; }
	AssetReference(
		AssetLoadType loadType,
		const std::string& guid,
		Runnable<void(const ObjectPtr<MObject>&)> mainThreadFinishFunctor,
		bool isLoad);
	static ObjectPtr<MObject> SyncLoad(
		ID3D12Device* device,
		const std::string& guid,
		AssetLoadType loadType);
	void Load(
		ID3D12Device* device,
		std::vector<std::pair<Runnable<void(const ObjectPtr<MObject>&)>, ObjectPtr<MObject>>>& mainThreadCallList);
	void Unload();
};