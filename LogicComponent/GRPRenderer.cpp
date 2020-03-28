#include "GRPRenderer.h"
#include "../RenderComponent/Mesh.h"
#include "Transform.h"
#include "../RenderComponent/PBRMaterial.h"
#include "../CJsonObject/CJsonObject.hpp"
#include "../RenderComponent/GRPRenderManager.h"
#include "../ResourceManagement/AssetDatabase.h"
#include "World.h"
GRPRenderer::GRPRenderer(
	const ObjectPtr<Transform>& trans, ObjectPtr<Component>& ptr) :
	Component(trans, ptr)
{

}
GRPRenderer::~GRPRenderer()
{
	World* world = World::GetInstance();
	if (world && loaded && transform)
	{
		GRPRenderManager* rManager = world->GetGRPRenderManager();
		if (rManager)
			rManager->RemoveElement(transform, device);
	}
}

ObjectPtr<GRPRenderer> GRPRenderer::GetRendererFromFile(ID3D12Device* device, const ObjectPtr<Transform>& trans, const neb::CJsonObject& obj)
{
	std::string s;
	ObjectPtr<Mesh> m;
	ObjectPtr<PBRMaterial> mat;
	ObjectPtr<Component> comp;
	if (obj.Get("Mesh", s))
	{
		m = AssetDatabase::GetInstance()->GetLoadedObject(s).CastTo<Mesh>();
	}
	if (obj.Get("Material", s))
	{
		mat = AssetDatabase::GetInstance()->GetLoadedObject(s).CastTo<PBRMaterial>();
	}
	auto ptr = new GRPRenderer(trans, comp);
	ptr->mesh = m;
	ptr->device = device;
	ptr->material = mat;
	World* world = World::GetInstance();
	if (ptr->mesh && ptr->material && world)
	{
		ptr->loaded = true;
		GRPRenderManager* rManager = world->GetGRPRenderManager();
		rManager->AddRenderElement(ptr->transform, ptr->mesh, device, 0, ptr->material->GetMaterialIndex());
	}
	return comp.CastTo<GRPRenderer>();
}

ObjectPtr<GRPRenderer> GRPRenderer::GetRenderer(ID3D12Device* device, const ObjectPtr<Transform>& trans, const ObjectPtr<Mesh>& mesh, const ObjectPtr<PBRMaterial>& mat)
{
	ObjectPtr<Component> comp;
	auto ptr = new GRPRenderer(trans, comp);
	ptr->mesh = mesh;
	ptr->device = device;
	ptr->material = mat;
	World* world = World::GetInstance();
	if (ptr->mesh && ptr->material && world)
	{
		ptr->loaded = true;
		GRPRenderManager* rManager = world->GetGRPRenderManager();
		rManager->AddRenderElement(ptr->transform, ptr->mesh, device, 0, ptr->material->GetMaterialIndex());
	}
	return comp.CastTo<GRPRenderer>();
}