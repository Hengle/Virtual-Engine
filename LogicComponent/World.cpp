#include "World.h"
#include "../Common/GeometryGenerator.h"
#include "../Singleton/ShaderCompiler.h"
#include "../Singleton/ShaderID.h"
#include "../Singleton/FrameResource.h"
#include "../RenderComponent/Mesh.h"
#include "../RenderComponent/DescriptorHeap.h"
#include "../RenderComponent/GRPRenderManager.h"
#include "Transform.h"
#include "../CJsonObject/CJsonObject.hpp"
#include "../Common/Input.h"
#include "../RenderComponent/Texture.h"
#include "../RenderComponent/PBRMaterial.h"
using namespace Math;
using namespace neb;
World* World::current = nullptr;
void BuildShapeGeometry(GeometryGenerator::MeshData& box, ObjectPtr<Mesh>& bMesh, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, FrameResource* res);
namespace WorldTester
{
	ObjectPtr<Mesh> mesh;
	ObjectPtr<Transform> testObj;
	ObjectPtr<PBRMaterial> pbrMat;
	ObjectPtr<ITexture> testTex;
}
using namespace WorldTester;	
void World::Rebuild(ID3D12GraphicsCommandList* commandList, ID3D12Device* device)
{
	current = this;
	globalDescriptorHeap.Destroy();
	pbrMat.Destroy();
	mesh.Destroy();
	delete grpRenderer;
	delete grpMaterialManager;
	usedDescs.Reset(false);
	unusedDescs.resize(MAXIMUM_HEAP_COUNT);
	for (UINT i = 0; i < MAXIMUM_HEAP_COUNT; ++i)
	{
		unusedDescs[i] = i;
	}
	globalDescriptorHeap = ObjectPtr<DescriptorHeap>::MakePtr(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAXIMUM_HEAP_COUNT, true));
	grpMaterialManager = new PBRMaterialManager(device, 256);
	//meshRenderer = new MeshRenderer(trans.operator->(), device, mesh, ShaderCompiler::GetShader("OpaqueStandard"));
	
	GeometryGenerator geoGen;
	mesh = ObjectPtr<Mesh>::MakePtr(Mesh::LoadMeshFromFile("Resource/Wheel.vmesh", device,
		true, true, true, false, true, true, true, false));
	if (!mesh)
	{
		BuildShapeGeometry(geoGen.CreateBox(1, 1, 1, 1), mesh, device, commandList, nullptr);
	}
	{
		Storage<CJsonObject, 1> jsonObjStorage;
		CJsonObject* jsonObj = (CJsonObject*)&jsonObjStorage;
		if (ReadJson("Resource/Test.mat", jsonObj))
		{
			auto func = [=]()->void
			{
				jsonObj->~CJsonObject();
			};
			DestructGuard<decltype(func)> fff(func);
			pbrMat = ObjectPtr<PBRMaterial>::MakePtr(new PBRMaterial(device, grpMaterialManager, *jsonObj));
		}
		else
		{
			pbrMat = ObjectPtr<PBRMaterial>::MakePtr(new PBRMaterial(device, grpMaterialManager, CJsonObject("")));
		}
	}
	uint v = pbrMat->GetMaterialIndex();
	grpRenderer = new GRPRenderManager(
		mesh->GetLayoutIndex(),
		256,
		ShaderCompiler::GetShader("OpaqueStandard"),
		device
	);
	grpRenderer->AddRenderElement(
		testObj, mesh, device, 0, pbrMat->GetMaterialIndex()
	);
}

World::World(ID3D12GraphicsCommandList* commandList, ID3D12Device* device) :
	usedDescs(MAXIMUM_HEAP_COUNT),
	unusedDescs(MAXIMUM_HEAP_COUNT),
	globalDescriptorHeap(ObjectPtr<DescriptorHeap>::MakePtr(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAXIMUM_HEAP_COUNT, true))),
	allTransformsPtr(500)
{
	current = this;
	testObj = Transform::GetTransform();
	testObj->SetRotation(Vector4(-0.113517, 0.5999342, 0.6212156, 0.4912069));

	for (UINT i = 0; i < MAXIMUM_HEAP_COUNT; ++i)
	{
		unusedDescs[i] = i;
	}
	grpMaterialManager = new PBRMaterialManager(device, 256);
	//meshRenderer = new MeshRenderer(trans.operator->(), device, mesh, ShaderCompiler::GetShader("OpaqueStandard"));
	
	GeometryGenerator geoGen;
	mesh = ObjectPtr<Mesh>::MakePtr(Mesh::LoadMeshFromFile("Resource/Wheel.vmesh", device,
		true, true, true, false, true, true, true, false));
	
	if (!mesh)
	{
		BuildShapeGeometry(geoGen.CreateBox(1, 1, 1, 1), mesh, device, commandList, nullptr);
	}
	{
		Storage<CJsonObject, 1> jsonObjStorage;
		CJsonObject* jsonObj = (CJsonObject*)&jsonObjStorage;
		if (ReadJson("Resource/Test.mat", jsonObj))
		{
			auto func = [=]()->void
			{
				jsonObj->~CJsonObject();
			};
			DestructGuard<decltype(func)> fff(func);
			pbrMat = ObjectPtr<PBRMaterial>::MakePtr(new PBRMaterial(device, grpMaterialManager, *jsonObj));
		}
		else
		{
			pbrMat = ObjectPtr<PBRMaterial>::MakePtr(new PBRMaterial(device, grpMaterialManager, CJsonObject("")));
		}
	}
	
	uint v = pbrMat->GetMaterialIndex();
	grpRenderer = new GRPRenderManager(
		mesh->GetLayoutIndex(),
		256,
		ShaderCompiler::GetShader("OpaqueStandard"),
		device
	);
	grpRenderer->AddRenderElement(
		testObj, mesh, device, 0, pbrMat->GetMaterialIndex()
	);
	/*light = new Light(trans);
	light->SetEnabled(true)*/
}

UINT World::GetDescHeapIndexFromPool()
{
	std::lock_guard lck(mtx);
	auto last = unusedDescs.end() - 1;
	UINT value = *last;
	unusedDescs.erase(last);
	usedDescs[value] = true;
	return value;
}

World::~World()
{
	for (uint i = 0; i < allTransformsPtr.Length(); ++i)
	{
		allTransformsPtr[i].Destroy();
	}
	pbrMat.Destroy();
	testTex.Destroy();
	globalDescriptorHeap.Destroy();
	delete grpRenderer;
	delete grpMaterialManager;
}

void World::ReturnDescHeapIndexToPool(UINT target)
{
	std::lock_guard lck(mtx);
	auto ite = usedDescs[target];
	if (ite)
	{
		unusedDescs.push_back(target);
		ite = false;
	}
}

void World::ForceCollectAllHeapIndex()
{
	std::lock_guard lck(mtx);
	for (UINT i = 0; i < MAXIMUM_HEAP_COUNT; ++i)
	{
		unusedDescs[i] = i;
	}
	usedDescs.Reset(false);
}

void World::Update(FrameResource* resource, ID3D12Device* device)
{
	grpRenderer->UpdateFrame(resource, device);
	if (!testTex)
	{
		testTex = ObjectPtr<ITexture>::MakePtr(new Texture(device, "Resource/testTex.vtex"));
		pbrMat->SetAlbedoTexture(testTex.CastTo<ITexture>());
		pbrMat->UpdateMaterialToBuffer();
	}
}


void BuildShapeGeometry(GeometryGenerator::MeshData& box, ObjectPtr<Mesh>& bMesh, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, FrameResource* res)
{
	std::vector<XMFLOAT3> positions(box.Vertices.size());
	std::vector<XMFLOAT3> normals(box.Vertices.size());
	std::vector<XMFLOAT2> uvs(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		positions[i] = box.Vertices[i].Position;
		normals[i] = box.Vertices[i].Normal;
		uvs[i] = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = box.GetIndices16();
	bMesh = ObjectPtr< Mesh>::MakePtr(new Mesh(
		box.Vertices.size(),
		positions.data(),
		normals.data(),
		nullptr,
		nullptr,
		uvs.data(),
		nullptr,
		nullptr,
		nullptr,
		device,
		DXGI_FORMAT_R16_UINT,
		indices.size(),
		indices.data()
	));

}