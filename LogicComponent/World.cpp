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
#include "../Singleton/MeshLayout.h"
#include "../RenderComponent/Texture.h"
#include "../RenderComponent/MeshRenderer.h"
#include "../RenderComponent/PBRMaterial.h"
#include "../RenderComponent/Terrain/VirtualTexture.h"
#include "../RenderComponent/Terrain/TerrainDrawer.h"
#include "../RenderComponent/Shader.h"
#include "../JobSystem/JobInclude.h"
#include "CameraMove.h"
#include "Terrain/TerrainMainLogic.h"
#include "../ResourceManagement/AssetDatabase.h"
#include "../ResourceManagement/AssetReference.h"
#include "../RenderComponent/Terrain/VirtualTextureData.h"
using namespace Math;
using namespace neb;
World* World::current = nullptr;
void BuildShapeGeometry(GeometryGenerator::MeshData& box, ObjectPtr<Mesh>& bMesh, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, FrameResource* res);
namespace WorldTester
{
	ObjectPtr<Mesh> mesh;
//	ObjectPtr<Transform> testObj;
	ObjectPtr<PBRMaterial> pbrMat;
	ObjectPtr<ITexture> testTex;
	ObjectPtr<Camera> mainCamera;
	std::unique_ptr<VirtualTextureData> texData;
	StackObject<CameraMove> camMove;
	std::unique_ptr<TerrainMainLogic> terrainMainLogic;
	ObjectPtr<Scene> testScene;
}
using namespace WorldTester;

World::World(ID3D12GraphicsCommandList* commandList, ID3D12Device* device) :
	usedDescs(MAXIMUM_HEAP_COUNT),
	unusedDescs(MAXIMUM_HEAP_COUNT),
	globalDescriptorHeap(ObjectPtr<DescriptorHeap>::MakePtr(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAXIMUM_HEAP_COUNT, true))),
	allTransformsPtr(500)
{

	allCameras.reserve(10);
	current = this;
	mainCamera = ObjectPtr< Camera>::MakePtr(new Camera(device, Camera::CameraRenderPath::DefaultPipeline));
	allCameras.push_back(mainCamera);
	camMove.New(mainCamera);
	//testObj = Transform::GetTransform();
	//testObj->SetRotation(Vector4(-0.113517, 0.5999342, 0.6212156, 0.4912069));
#pragma loop(hint_parallel(0))
	for (UINT i = 0; i < MAXIMUM_HEAP_COUNT; ++i)
	{
		unusedDescs[i] = i;
	}
	grpMaterialManager = new PBRMaterialManager(device, 256);
	//meshRenderer = new MeshRenderer(trans.operator->(), device, mesh, ShaderCompiler::GetShader("OpaqueStandard"));

	GeometryGenerator geoGen;
	mesh = ObjectPtr<Mesh>::MakePtr(Mesh::LoadMeshFromFile("Resource/Wheel.vmesh", device,
		true, true, true, false, true, true, false, false));
	
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
	grpRenderer = new GRPRenderManager(
		//mesh->GetLayoutIndex(),
		MeshLayout::GetMeshLayoutIndex(true, true, false, true, true, false, false, false),
		256,
		ShaderCompiler::GetShader("OpaqueStandard"),
		device
		);
	DXGI_FORMAT vtFormat[2] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM
	};
	std::string res = "Resource";
	texData = std::unique_ptr<VirtualTextureData>(new VirtualTextureData(device, res, "TestData.inf"));
	virtualTexture = ObjectPtr<VirtualTexture>::MakePtr(new VirtualTexture(device, 1024, 768, vtFormat, 2, 157));
	/*grpRenderer->AddRenderElement(
		testObj, mesh, device, 0, pbrMat->GetMaterialIndex()
		);*/
	terrainDrawer = ObjectPtr<TerrainDrawer>::MakePtr(
		new TerrainDrawer(device, uint2(1024, 1024), 1)
		);

	terrainMainLogic = std::unique_ptr<TerrainMainLogic>(new TerrainMainLogic(device, virtualTexture, texData.get()));
	
	Runnable<void(const ObjectPtr<MObject>&)> sceneFunc = [&](const ObjectPtr<MObject>& value)->void
	{
		testScene = value.CastTo<Scene>();
	};
	AssetReference ref(AssetLoadType::Scene, "Scene_SampleScene", sceneFunc, true);
	AssetDatabase::GetInstance()->AsyncLoad(ref);
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
		Transform::DisposeTransform(allTransformsPtr[i]);
	}
	pbrMat.Destroy();
	testTex.Destroy();
	camMove.Delete();
	mainCamera.Destroy();

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
bool vt_Initialized = false;
bool vt_Combined = false;
void World::Update(FrameResource* resource, ID3D12Device* device, GameTimer& timer, int2 screenSize)
{
	camMove->Run(timer.DeltaTime());
	mainCamera->SetLens(0.333333 * MathHelper::Pi, (float)screenSize.x / (float)screenSize.y, 0.3, 2000);
	if (!testTex)
	{
		testTex = ObjectPtr<ITexture>::MakePtr(new Texture(device, "Resource/testTex.vtex", TextureDimension::Tex2D, 20, 0));
		pbrMat->SetEmissionTexture(testTex.CastTo<ITexture>());
		pbrMat->UpdateMaterialToBuffer();

	}
	terrainMainLogic->UpdateCameraData(mainCamera, resource, device);
	if (Input::IsKeyDown(KeyCode::Space))
	{
		if (testScene) testScene.Destroy();
	}
	windowInfo = std::to_wstring(virtualTexture->GetLeftedChunkSize());
}

void World::PrepareUpdateJob(JobBucket* bucket, FrameResource* resource, ID3D12Device* device, GameTimer& timer, int2 screenSize)
{
	bucket->GetTask(nullptr, 0, [=]()->void
		{
			terrainMainLogic->JobUpdate();
		});
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