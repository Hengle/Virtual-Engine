#include "TerrainMainLogic.h"
#include "../../RenderComponent/Terrain/VirtualTexture.h"
#include "../Utility/IEnumerator.h"
#include "../../Common/Camera.h"
#include "../../Singleton/MathLib.h"
#include "../../Singleton/FrameResource.h"
#include "../../RenderComponent/Terrain/VirtualTextureData.h"
#include "../../RenderComponent/Texture.h"
#include "../../RenderComponent/StructuredBuffer.h"
#include "../../Singleton/ShaderCompiler.h"
#include "../../Singleton/ShaderID.h"
#include "../../Singleton/PSOContainer.h"
#include "../World.h"
TerrainMainLogic* TerrainMainLogic::current = nullptr;
using namespace Math;
class TerrainLoadTask : public IEnumerator
{
public:
	ID3D12Device* device;
	VirtualTexture* vt;
	TerrainData* terrainData;
	int initializedListLength;
	TerrainLoadData loadData;
	int targetElement;
	CBufferPool projectParamPool;
	FrameResource* frameResource;
	VirtualTextureData* vtData;
	Shader* projectShader;
	StackObject<PSOContainer, true> psoContainer;
	UploadBuffer* indirectProjectUpload;
	static uint _IndexMap;
	static uint _DescriptorIndexBuffer;
	struct ProjectParams
	{
		float2 _IndexMapSize;
		float2 _SplatScale;
		float2 _SplatOffset;
		float _VTSize;
		uint _SplatMapIndex;
		uint _IndexMapIndex;
		uint _DescriptorCount;
	};
	class PerFrameData : public IPipelineResource
	{
	public:
		ConstBufferElement projectBufferParam;

		PerFrameData(ID3D12Device* device, CBufferPool& bufferPool)
		{
			projectBufferParam = bufferPool.Get(device);
		}
		~PerFrameData()
		{

		}
	};
	Runnable<void(const VirtualTextureRenderArgs&)> GetLoadFunction(Texture* splatTex, Texture* indexTex)
	{
		TerrainLoadTask* thsPtr = this;
		CBufferPool* paramPool = &projectParamPool;
		return [=](const VirtualTextureRenderArgs& args)->void {
			auto commandList = args.commandList;
			auto device = args.device;
			auto resource = args.frameResource;
			auto barrierBuffer = args.barrierBuffer;
			RenderTexture* albedoTex = vt->GetRenderTexture(0, args.renderTextureID);
			RenderTexture* normalTex = vt->GetRenderTexture(1, args.renderTextureID);
			
			D3D12_CPU_DESCRIPTOR_HANDLE handles[2] = { albedoTex->GetColorDescriptor(0, 0), normalTex->GetColorDescriptor(0,0) };
			World* world = World::GetInstance();
			PerFrameData* frameData = (PerFrameData*)resource->GetResource(thsPtr, [&]()->PerFrameData*
				{
					return new PerFrameData(device, *paramPool);
				});
			ConstBufferElement ele = frameData->projectBufferParam;
			ProjectParams* data = (ProjectParams*)ele.buffer->GetMappedDataPtr(ele.element);
			data->_DescriptorCount = indirectProjectUpload->GetElementCount();
			data->_IndexMapIndex = indexTex->GetGlobalDescIndex();
			data->_IndexMapSize = { (float)indexTex->GetWidth(), (float)indexTex->GetHeight() };
			data->_SplatMapIndex = splatTex->GetGlobalDescIndex();
			uint2 startIndex = args.startIndex;
			startIndex.x %= vt->GetIndirectResolution();
			startIndex.y %= vt->GetIndirectResolution();
			float2 splatSize = float2(splatTex->GetWidth(), splatTex->GetHeight());
			data->_SplatOffset = float2(startIndex.x, startIndex.y) / float2(vt->GetIndirectResolution(), vt->GetIndirectResolution());
			data->_SplatScale = float2(args.size, args.size) / float2(vt->GetIndirectResolution(), vt->GetIndirectResolution());
			data->_VTSize = args.size;
			//TODO
			projectShader->BindRootSignature(commandList, world->GetGlobalDescHeap());
			projectShader->SetResource(commandList, ShaderID::GetParams(), ele.buffer, ele.element);
			projectShader->SetResource(commandList, ShaderID::GetMainTex(), world->GetGlobalDescHeap(), 0);
			projectShader->SetResource(commandList, _IndexMap, world->GetGlobalDescHeap(), 0);
			projectShader->SetStructuredBufferByAddress(commandList, _DescriptorIndexBuffer, indirectProjectUpload->GetAddress(0));
			barrierBuffer->AddCommand(albedoTex->GetReadState(), RenderTexture::GetState(RenderTextureState::Render_Target), albedoTex->GetResource());
			barrierBuffer->AddCommand(normalTex->GetReadState(), RenderTexture::GetState(RenderTextureState::Render_Target), normalTex->GetResource());
			barrierBuffer->ExecuteCommand(commandList);
			Graphics::Blit(
				commandList,
				device,
				handles, _countof(handles),
				nullptr, psoContainer, 0,
				albedoTex->GetWidth(), albedoTex->GetHeight(),
				projectShader, 0
				);
			barrierBuffer->AddCommand(RenderTexture::GetState(RenderTextureState::Render_Target), normalTex->GetReadState(), normalTex->GetResource());
			barrierBuffer->AddCommand(RenderTexture::GetState(RenderTextureState::Render_Target), albedoTex->GetReadState(), albedoTex->GetResource());
		};
	}
	TerrainLoadTask() :
		projectParamPool(sizeof(ProjectParams), 36)
	{
		_IndexMap = ShaderID::PropertyToID("_IndexMap");
		_DescriptorIndexBuffer = ShaderID::PropertyToID("_DescriptorIndexBuffer");
		projectShader = ShaderCompiler::GetShader("TerrainProject");
		executors.reserve(20);
		AddTask([&]()->bool {
			psoContainer.New(
				DXGI_FORMAT_UNKNOWN, vt->GetFormatCount(), vt->GetFormats()
				);
			return true;
			});
		AddTask([&]()->bool
			{
				MaskLoadCommand maskCommand;
				while (terrainData->maskLoadList.TryGetLast(&maskCommand))
				{
					std::pair<Texture*, Texture*> splatIndexResult = vtData->GetSplatIndex(
						uint2(maskCommand.pos.x, maskCommand.pos.y));
					if (splatIndexResult.first == nullptr && splatIndexResult.second == nullptr)
					{
						if (!vtData->StartLoadSplat(device, uint2(maskCommand.pos.x, maskCommand.pos.y)))
						{
							terrainData->maskLoadList.TryPop();
						}
						return false;
					}
					else if (!splatIndexResult.first->IsLoaded() || !splatIndexResult.second->IsLoaded())
					{
						return false;
					}

					terrainData->maskLoadList.TryPop();
				}
				//TODO : Deal with mask data
				initializedListLength = terrainData->initializeLoadList.Length();
				if (initializedListLength > 0)
				{

					for (uint i = 0; i < initializedListLength; ++i)
					{
						if (terrainData->initializeLoadList.TryPop(&loadData))
						{
							if (loadData.ope == TerrainLoadData::Operator::Load)
							{
								VirtualChunk chunk;
								bool elementAva = vt->CreateChunk(
									uint2(loadData.startIndex.x, loadData.startIndex.y),
									loadData.size,
									chunk);
								if (elementAva)
								{
									std::pair<Texture*, Texture*> splatIndex = vtData->GetSplatIndex(uint2(loadData.rootPos.x, loadData.rootPos.y));
									vt->AddRenderCommand(
										GetLoadFunction(splatIndex.first, splatIndex.second),
										uint2(loadData.startIndex.x, loadData.startIndex.y),
										loadData.size);
								}
							}
							terrainData->initializeLoadList.Push(loadData);
						}
					}
					while (terrainData->initializeLoadList.TryPop(&loadData))
					{
						if (loadData.ope == TerrainLoadData::Operator::Load)
						{
							//TODO
							//Draw Decal
							vt->GenerateMip(uint2(loadData.startIndex.x, loadData.startIndex.y));
						}
					}
					return false;
				}
				else
					return true;
			});
		AddTask([&]()->bool
			{
				VirtualChunk chunk; MaskLoadCommand maskCommand;
				if (terrainData->maskLoadList.TryGetLast(&maskCommand))
				{
					std::pair<Texture*, Texture*> splatIndexResult = vtData->GetSplatIndex(
						uint2(maskCommand.pos.x, maskCommand.pos.y));
					if (splatIndexResult.first == nullptr && splatIndexResult.second == nullptr)
					{
						if (!vtData->StartLoadSplat(device, uint2(maskCommand.pos.x, maskCommand.pos.y)))
						{
							terrainData->maskLoadList.TryPop();
						}
						return false;
					}
					else if (!splatIndexResult.first->IsLoaded() || !splatIndexResult.second->IsLoaded())
					{
						return false;
					}

					terrainData->maskLoadList.TryPop();
				}
				if (terrainData->loadDataList.TryPop(&loadData))
				{
					uint2 startIndex = uint2(loadData.startIndex.x, loadData.startIndex.y);
					switch (loadData.ope)
					{
					case TerrainLoadData::Operator::Combine:
						if (vt->CombineUpdate(startIndex, loadData.size))
						{
							vt->GenerateMip(startIndex);
						}

						break;
					case TerrainLoadData::Operator::Load:
						if (vt->CreateChunk(startIndex, loadData.size, chunk))
						{
							std::pair<Texture*, Texture*> splatIndex = vtData->GetSplatIndex(uint2(loadData.rootPos.x, loadData.rootPos.y));
							vt->AddRenderCommand(
								GetLoadFunction(splatIndex.first, splatIndex.second),
								uint2(loadData.startIndex.x, loadData.startIndex.y),
								loadData.size);
							vt->GenerateMip(uint2(loadData.startIndex.x, loadData.startIndex.y));
						}
						break;
					case TerrainLoadData::Operator::Separate:
					{
						uint subSize = loadData.size / 2;
						uint2 leftDownIndex = startIndex;
						uint2 leftUpIndex = startIndex + uint2(0, subSize);
						uint2 rightDownIndex = startIndex + uint2(subSize, 0);
						uint2 rightUpIndex = startIndex + uint2(subSize, subSize);
						VirtualChunk chunks[4];
						if (vt->GetLeftedChunkSize() >= 3)
						{
							vt->ReturnChunk(startIndex, false);
							vt->CreateChunk(leftDownIndex, subSize, chunks[0]);
							vt->CreateChunk(leftUpIndex, subSize, chunks[1]);
							vt->CreateChunk(rightDownIndex, subSize, chunks[2]);
							vt->CreateChunk(rightUpIndex, subSize, chunks[3]);
							auto func = [&](uint2 startIndex, uint size)->void
							{
								std::pair<Texture*, Texture*> splatIndex = vtData->GetSplatIndex(uint2(loadData.rootPos.x, loadData.rootPos.y));
								vt->AddRenderCommand(
									GetLoadFunction(splatIndex.first, splatIndex.second),
									startIndex,
									size);
								vt->GenerateMip(startIndex);
							};
							func(leftDownIndex, subSize);
							func(leftUpIndex, subSize);
							func(rightDownIndex, subSize);
							func(rightUpIndex, subSize);
						}
						else
						{
#ifdef _DEBUG
							throw "No Enough Capacity";
#endif
						}
					}
					break;
					case TerrainLoadData::Operator::Unload:
						vt->ReturnChunk(startIndex, true);
						break;
					case TerrainLoadData::Operator::Update:
					{
						if (vt->GetChunk(startIndex, chunk))
						{
							std::pair<Texture*, Texture*> splatIndex = vtData->GetSplatIndex(uint2(loadData.rootPos.x, loadData.rootPos.y));
							vt->AddRenderCommand(
								GetLoadFunction(splatIndex.first, splatIndex.second),
								uint2(loadData.startIndex.x, loadData.startIndex.y),
								loadData.size);
							vt->GenerateMip(uint2(loadData.startIndex.x, loadData.startIndex.y));
						}
					}
					break;
					}
				}
				return false;
			});
	}
};
uint TerrainLoadTask::_IndexMap;
uint TerrainLoadTask::_DescriptorIndexBuffer;
TerrainMainLogic::TerrainMainLogic(
	ID3D12Device* device, const ObjectPtr<VirtualTexture>& vt, VirtualTextureData* vtData) :
	virtualTexture(vt),
	loadTask(new TerrainLoadTask()),
	vtData(vtData)
{
	memset(&data, 0, sizeof(CameraData));
	indirectBuffer.New(device, vtData->MaterialCount(), false, sizeof(uint2));
	if (current)
	{
		throw "Terrain Main Logic Must Be Unique";
	}
	//Load Task:
	loadTask->device = device;
	loadTask->vt = vt;
	loadTask->terrainData = terrainData;
	loadTask->vtData = vtData;
	loadTask->indirectProjectUpload = indirectBuffer;

	current = this;
	TerrainQuadTree::terrainData = terrainData;
	terrainData.New();
	//TODO
	//Set TerrainData
	//Read from file

	terrainData->allLodLevles.resize(10);
	terrainData->allLodLevles[0] = 1024;
	terrainData->allLodLevles[1] = 768;
	terrainData->allLodLevles[2] = 512;
	terrainData->allLodLevles[3] = 350;
	terrainData->allLodLevles[4] = 220;
	terrainData->allLodLevles[5] = 90;
	terrainData->allLodLevles[6] = 50;
	terrainData->allLodLevles[7] = 16;
	terrainData->allLodLevles[8] = 8;
	terrainData->allLodLevles[9] = 4;
	terrainData->allDecalLayers.resize(terrainData->allLodLevles.size());
	memset(terrainData->allDecalLayers.data(), 255, sizeof(uint) * terrainData->allDecalLayers.size());
	terrainData->textureCapacity = vt->GetTextureCapacity();
	terrainData->largestChunkSize = 1024;
	terrainData->screenOffset = -512;
	quadTreeMain = terrainData->pool.New(-1, TerrainQuadTree::LocalPos::LeftDown, int2(0, 0), int2(0, 0), terrainData->largestChunkSize, double3(1, 0, 0), int2(0, 0));
}

void TerrainMainLogic::UpdateCameraData(Camera* camera, FrameResource* resource, ID3D12Device* device)
{
	std::lock_guard<std::mutex> lck(mtx);
	data.right = camera->GetRight();
	data.up = camera->GetUp();
	data.forward = camera->GetLook();
	data.position = camera->GetPosition();
	data.fov = camera->GetFovY();
	data.nearZ = camera->GetNearZ();
	data.farZ = camera->GetFarZ();
	data.nearWindowHeight = camera->GetNearWindowHeight();
	data.farWindowHeight = camera->GetFarWindowHeight();
	data.aspect = camera->GetAspect();
	frameResource = resource;
	this->device = device;
}

void TerrainMainLogic::JobUpdate()
{
	CameraData data;
	{
		std::lock_guard<std::mutex> lck(mtx);
		memcpy(&data, &this->data, sizeof(CameraData));
	}

	Matrix4 localToWorldMatrix;
	localToWorldMatrix[0] = data.right;
	localToWorldMatrix[1] = data.up;
	localToWorldMatrix[2] = data.forward;
	XMFLOAT3 position = data.position;
	localToWorldMatrix[3] = { position.x, position.y, position.z, 1 };
	Vector4 frustumPlanes[6];
	Vector3 frustumMinPos, frustumMaxPos;
	MathLib::GetPerspFrustumPlanes(localToWorldMatrix, data.fov, data.aspect, data.nearZ, data.farZ, frustumPlanes);
	//Calculate Frustum Bounding
	MathLib::GetFrustumBoundingBox(
		localToWorldMatrix,
		data.nearWindowHeight,
		data.farWindowHeight,
		data.aspect,
		data.nearZ,
		data.farZ,
		&frustumMinPos,
		&frustumMaxPos
		);
	auto pos = data.position;
	quadTreeMain->UpdateData(
		double3(pos.GetX(), pos.GetY(), pos.GetZ()),
		double2(0, 0)/*TODO: Height Scale Offset*/,
		double3(frustumMinPos.GetX(), frustumMinPos.GetY(), frustumMinPos.GetZ()),
		double3(frustumMaxPos.GetX(), frustumMaxPos.GetY(), frustumMaxPos.GetZ()),
		frustumPlanes);

	quadTreeMain->CombineUpdate();
	quadTreeMain->SeparateUpdate();
	if (terrainData->initializing)
	{
		//quadTreeMain->InitializeRenderingCommand();
		terrainData->initializing = false;
	}
	loadTask->frameResource = frameResource;
	loadTask->ExecuteOne();
	//for(auto ite = terrainData->initializeLoadList)
}

TerrainMainLogic::~TerrainMainLogic()
{
	current = nullptr;
	if (quadTreeMain) terrainData->pool.Delete(quadTreeMain);
	TerrainQuadTree::terrainData = nullptr;
}

