#include "AssetReference.h"
#include "../Common/MObject.h"
#include "../RenderComponent/Mesh.h"
#include "../RenderComponent/PBRMaterial.h"
#include "../RenderComponent/Texture.h"
#include "AssetDatabase.h"
#include "../LogicComponent/World.h"
#include "../LogicComponent/Transform.h"
#include "../CJsonObject/CJsonObject.hpp"
#include "Scene.h"
//#include "../RenderComponent/"
using namespace neb;
AssetReference::AssetReference(
	AssetLoadType loadType,
	const std::string& guid,
	Runnable<void(MObject*)> mainThreadFinishFunctor,
	bool isLoad) :
	loadType(loadType),
	mainThreadFinishFunctor(mainThreadFinishFunctor),
	guid(guid),
	isLoad(isLoad)
{
	AssetDatabase::GetInstance()->GetPath(guid, path);
}

void AssetReference::Unload()
{
	ObjectPtr<MObject> objPtr = AssetDatabase::GetInstance()->GetLoadedObject(guid);
	objPtr.Destroy();
}

ObjectPtr<MObject> AssetReference::SyncLoad(
	ID3D12Device* device,
	const std::string& guid,
	AssetLoadType loadType)
{
	ObjectPtr<MObject> obj = AssetDatabase::GetInstance()->GetLoadedObject(guid);
	if (!obj)
	{
		std::string path;
		if (AssetDatabase::GetInstance()->GetPath(guid, path))
		{
			obj = LoadObject(loadType, device, path, guid);
			if (obj)
			{
				AssetDatabase::GetInstance()->AddLoadedObjects(guid, obj);
				obj->AddEventBeforeDispose([=](MObject* obj)->void
					{
						AssetDatabase::GetInstance()->RemoveLoadedObjects(guid);
					});
			}
		}
	}
	return obj;
}

ObjectPtr<MObject> AssetReference::LoadObject(AssetLoadType loadType, ID3D12Device* device, const std::string& path, const std::string& guid)
{
	ObjectPtr<MObject> obj;
	switch (loadType)
	{
	case AssetLoadType::Mesh:
		obj = ObjectPtr<Mesh>::MakePtr(Mesh::LoadMeshFromFile(path, device, true, true, true, false, true, true, true, false)).CastTo<MObject>();
		break;
	case AssetLoadType::Texture2D:
		obj = ObjectPtr<Texture>::MakePtr(new Texture(device, path, TextureDimension::Tex2D)).CastTo<MObject>();
		break;
	case AssetLoadType::Texture3D:
		obj = ObjectPtr<Texture>::MakePtr(new Texture(device, path, TextureDimension::Tex3D)).CastTo<MObject>();
		break;
	case AssetLoadType::Cubemap:
		obj = ObjectPtr<Texture>::MakePtr(new Texture(device, path, TextureDimension::Cubemap)).CastTo<MObject>();
		break;
	case AssetLoadType::GPURPMaterial:
	{
		Storage<CJsonObject, 1> jsonObjStorage;
		CJsonObject* jsonObj = (CJsonObject*)&jsonObjStorage;
		if (ReadJson(path, jsonObj))
		{
			auto func = [=]()->void
			{
				jsonObj->~CJsonObject();
			};
			DestructGuard<decltype(func)> fff(func);
			obj = ObjectPtr< PBRMaterial>::MakePtr(new PBRMaterial(device, World::GetInstance()->GetPBRMaterialManager(), *jsonObj, true)).CastTo<MObject>();
		}
	}
	break;
	case AssetLoadType::Scene:
	{
		Storage<CJsonObject, 1> jsonObjStorage;
		CJsonObject* jsonObj = (CJsonObject*)&jsonObjStorage;
		if (ReadJson(path, jsonObj))
		{
			auto func = [=]()->void
			{
				jsonObj->~CJsonObject();
			};
			DestructGuard<decltype(func)> fff(func);
			obj = ObjectPtr<Scene>::MakePtr(new Scene(guid, *jsonObj, device)).CastTo<MObject>();
		}
	}
	break;
	}
	return obj;
}

void AssetReference::Load(
	ID3D12Device* device,
	std::vector<std::pair<Runnable<void(MObject*)>, ObjectPtr<MObject>>>& mainThreadCallList)
{
	ObjectPtr<MObject> obj = AssetDatabase::GetInstance()->GetLoadedObject(guid);
	if (obj)
	{
		if (mainThreadFinishFunctor.isAvaliable())
		{
			mainThreadCallList.emplace_back(mainThreadFinishFunctor, obj);
		}
	}
	else
	{

		obj = ObjectPtr<MObject>::MakePtr(LoadObject(loadType, device, path, guid));
		AssetDatabase::GetInstance()->AddLoadedObjects(guid, obj);
		obj->AddEventBeforeDispose([=](MObject* obj)->void
			{
				AssetDatabase::GetInstance()->RemoveLoadedObjects(guid);
			});
		if (mainThreadFinishFunctor.isAvaliable())
		{
			mainThreadCallList.emplace_back(mainThreadFinishFunctor, obj);
		}

	}
}