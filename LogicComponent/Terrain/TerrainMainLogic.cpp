#include "TerrainMainLogic.h"
#include "../../RenderComponent/Terrain/VirtualTexture.h"
#include "../Utility/IEnumerator.h"
#include "../../Common/Camera.h"
#include "../../Singleton/MathLib.h"
TerrainMainLogic* TerrainMainLogic::current = nullptr;
using namespace Math;
class TerrainLoadTask : public IEnumerator
{
public:
	VirtualTexture* vt;
	TerrainData* terrainData;
	int initializedListLength;
	TerrainLoadData loadData;
	int targetElement;
	TerrainLoadTask()
	{
		executors.reserve(20);
		AddTask([&]()->bool
			{
				terrainData->maskLoadList.Clear();
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
									//TODO
									//Load Texture Here
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
						}
					}
					return false;
				}
				else
					return true;
			});
		AddTask([&]()->bool
			{
				VirtualChunk chunk;
				if (terrainData->loadDataList.TryPop(&loadData))
				{
					uint2 startIndex = uint2(loadData.startIndex.x, loadData.startIndex.y);
					switch (loadData.ope)
					{
					case TerrainLoadData::Operator::Combine:
						if (vt->CombineUpdate(startIndex, loadData.size))
						{
							//TODO
							//Generate Mip
						}

						break;
					case TerrainLoadData::Operator::Load:
						if (vt->CreateChunk(startIndex, loadData.size, chunk))
						{
							//TODO
							//Load & Draw & Generate Mip
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
							//TODO
							//Load & Draw & Generate Mip
						}
					}
					break;
					}
				}
				return false;
			});
	}
};
TerrainMainLogic::TerrainMainLogic(
	const ObjectPtr<VirtualTexture>& vt) :
	virtualTexture(vt),
	loadTask(new TerrainLoadTask())
{
	memset(&data, 0, sizeof(CameraData));
	if (current)
	{
		throw "Terrain Main Logic Must Be Unique";
	}
	//Load Task:
	loadTask->vt = vt;
	loadTask->terrainData = terrainData;

	current = this;
	TerrainQuadTree::terrainData = terrainData;
	terrainData.New();
	//TODO
	//Set TerrainData
	//Read from file

	terrainData->allLodLevles.resize(10);
	terrainData->allLodLevles[0] = 1024;
	terrainData->allLodLevles[1] = 512;
	terrainData->allLodLevles[2] = 300;
	terrainData->allLodLevles[3] = 150;
	terrainData->allLodLevles[4] = 80;
	terrainData->allLodLevles[5] = 55;
	terrainData->allLodLevles[6] = 35;
	terrainData->allLodLevles[7] = 22;
	terrainData->allLodLevles[8] = 13;
	terrainData->allLodLevles[9] = 6;
	terrainData->allDecalLayers.resize(terrainData->allLodLevles.size());
	memset(terrainData->allDecalLayers.data(), 255, sizeof(uint) * terrainData->allDecalLayers.size());
	terrainData->textureCapacity = vt->GetTextureCapacity();
	terrainData->largestChunkSize = 1024;
	terrainData->screenOffset = -512;
	quadTreeMain = terrainData->pool.New(-1, TerrainQuadTree::LocalPos::LeftDown, int2(0, 0), int2(0, 0), terrainData->largestChunkSize, double3(1, 0, 0), int2(0, 0));
}

void TerrainMainLogic::UpdateCameraData(Camera* camera)
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
	loadTask->ExecuteOne();
	//for(auto ite = terrainData->initializeLoadList)
}

TerrainMainLogic::~TerrainMainLogic()
{
	current = nullptr;
	if (quadTreeMain) terrainData->pool.Delete(quadTreeMain);
	terrainData.Delete();
	TerrainQuadTree::terrainData = nullptr;
}

