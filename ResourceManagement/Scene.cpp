#include "Scene.h"
#include "../CJsonObject/CJsonObject.hpp"
#include "../LogicComponent/World.h"
#include "../LogicComponent/Transform.h"
#include "../LogicComponent/GRPRenderer.h"
#include "../RenderComponent/Mesh.h"
#include "../RenderComponent/PBRMaterial.h"
#include "AssetReference.h"
#include "AssetDatabase.h"
using namespace neb;
struct RendererLoadingCommand
{
	std::string meshGuid;
	std::string pbrMatGuid;
	 ObjectPtr<Transform> trans;
};
Scene::Scene(const std::string& guid, CJsonObject& jsonObj, ID3D12Device* device) : 
	guid(guid)
{
	loadedTransforms.reserve(50);
	std::string key;
	CJsonObject gameObjectJson;
	CJsonObject transformObjJson;
	CJsonObject rendererObjJson;
	std::string str;
	std::vector<RendererLoadingCommand> commands;
	commands.reserve(50);
	while (jsonObj.GetKey(key))
	{
		if (jsonObj.Get(key, gameObjectJson))//Get Per GameObject
		{
			if (gameObjectJson.Get("transform", transformObjJson)) // Get Transform
			{
				ObjectPtr<Transform> tr = Transform::GetTransform(transformObjJson);
				loadedTransforms.push_back(tr);
				if (gameObjectJson.Get("renderer", rendererObjJson))
				{
					RendererLoadingCommand renderLoadCommand;
					rendererObjJson.Get("mesh", renderLoadCommand.meshGuid);
					rendererObjJson.Get("material", renderLoadCommand.pbrMatGuid);
					renderLoadCommand.trans = tr;
					commands.push_back(renderLoadCommand);
				}
			}
		}
	}
	for (auto ite = commands.begin(); ite != commands.end(); ++ite)
	{
		auto mesh = AssetReference::SyncLoad(device, ite->meshGuid, AssetLoadType::Mesh).CastTo<Mesh>();
		auto mat = AssetReference::SyncLoad(device, ite->pbrMatGuid, AssetLoadType::GPURPMaterial).CastTo<PBRMaterial>();
		if (mesh && mat)
		{
			GRPRenderer::GetRenderer(device, ite->trans, mesh, mat);
		}
	}
}
Scene::~Scene()
{
	for (auto ite = loadedTransforms.begin(); ite != loadedTransforms.end(); ++ite)
	{
		delete *ite;
	}
}

void Scene::DestroyScene()
{
	AssetDatabase::GetInstance()->AsyncLoad(AssetReference(AssetLoadType::Scene, guid, nullptr, false));
}