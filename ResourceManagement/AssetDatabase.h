#pragma once
#include "../Common/d3dUtil.h"
#include "../Common/MObject.h"
#include "../Common/MetaLib.h"
#include "../CJsonObject/CJsonObject.hpp"
#include <mutex>
#include <thread>
#include <condition_variable>
#include "../Common/MetaLib.h"
#include "../Common/Runnable.h"
class AssetReference;
class AssetDatabase final
{
private:
	static AssetDatabase* current;
	std::mutex mtx;
	Storage<std::thread, 1> threadStorage;
	std::thread* loadingThread;
	std::unordered_map<std::string, ObjWeakPtr<MObject>> loadedObjects;
	std::unordered_map<std::string, uint> resourceReferenceCount;
	std::vector<AssetReference> loadList;
	bool shouldWaiting = false;
	std::vector<std::pair<Runnable<void(MObject*)>, ObjectPtr<MObject>>> mainThreadRunnable;
	std::mutex loadingThreadMtx;
	std::condition_variable cv;
	AssetDatabase();
	~AssetDatabase();
	Storage<neb::CJsonObject, 1> rootJsonObjStorage;
	neb::CJsonObject* rootJsonObj;
public:
	static void LoadingThreadMainLogic();
	bool IsObjectLoaded(const std::string& guid)
	{
		std::lock_guard<std::mutex> lck(mtx);
		return loadedObjects.find(guid) == loadedObjects.end();
	}
	void AsyncLoad(const AssetReference& ref);
	void AsyncLoads(AssetReference* refs, uint refCount);
	void AddLoadedObjects(
		const std::string& str,
		const ObjectPtr<MObject>& obj)
	{
		std::lock_guard<std::mutex> lck(mtx);
		loadedObjects.insert_or_assign(str, obj);
	}
	void MainThreadUpdate();
	
	void RemoveLoadedObjects(
		const std::string& str)
	{
		std::lock_guard<std::mutex> lck(mtx);
		loadedObjects.erase(str);
	}

	ObjectPtr<MObject> GetLoadedObject(const std::string& str);
	bool GetPath(const std::string& guid, std::string& path)
	{
		return rootJsonObj->Get(guid, path);
	}
	static AssetDatabase* GetInstance()
	{
		return current;
	}
	static AssetDatabase* CreateInstance()
	{
		if (current) return current;
		new AssetDatabase();
		return current;
	}
	static void DestroyInstance()
	{
		if (current)
		{
			delete current;
			current = nullptr;
		}
	}
};