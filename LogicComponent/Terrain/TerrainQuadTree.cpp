#include "TerrainQuadTree.h"
#include "../../Singleton/MathLib.h"
using namespace Math;
TerrainData* TerrainQuadTree::terrainData = nullptr;
TerrainData::TerrainData() :
	pool(256),
	maskLoadList(256),
	loadDataList(32),
	initializeLoadList(64)
{
}
void TerrainQuadTree::SetRendering(bool value)
{
	if (value == m_isRendering) return;
	m_isRendering = value;
	if (value)
		terrainData->textureCapacity--;
	else
		terrainData->textureCapacity++;
}
TerrainQuadTree::TerrainQuadTree(int parentLodLevel, LocalPos sonPos, int2 parentPos, int2 parentRenderingPos, double worldSize, double3 maskScaleOffset, int2 rootPos)
{
	toPoint = double3(0, 0, 0);
	initializing = true;
	separate = false;
	dist = 0;
	isInRange = true;
	this->worldSize = worldSize;
	distOffset = terrainData->lodDeferredOffset;
	m_isRendering = false;
	lodLevel = parentLodLevel + 1;
	leftDown = nullptr;
	leftUp = nullptr;
	rightDown = nullptr;
	rightUp = nullptr;
	localPosition = parentPos * int2(2, 2);
	switch (sonPos)
	{
	case LocalPos::LeftUp:
		localPosition.y += 1;
		break;
	case LocalPos::RightDown:
		localPosition.x += 1;
		break;
	case LocalPos::RightUp:
		localPosition += int2(1, 1);
		break;
	}
	int decalLayer = lodLevel - terrainData->decalLayerOffset;
	decalMask = decalLayer < 0 ? 0 : terrainData->allDecalLayers[decalLayer];
	if (lodLevel >= terrainData->lodOffset)
	{

		//Larger
		if (lodLevel > terrainData->lodOffset)
		{
			double subScale = maskScaleOffset.x * 0.5;
			renderingLocalPosition = parentRenderingPos * int2(2, 2);
			double2 offset = double2(maskScaleOffset.y, maskScaleOffset.z);
			switch (sonPos)
			{
			case LocalPos::LeftUp:
				offset += double2(0, subScale);
				renderingLocalPosition += int2(0, 1);
				break;
			case LocalPos::RightDown:
				offset += double2(subScale, 0);
				renderingLocalPosition += int2(1, 0);
				break;
			case LocalPos::RightUp:
				offset += double2(subScale, subScale);
				renderingLocalPosition += int2(1, 1);
				break;
			}
			this->maskScaleOffset = double3(subScale, offset.x, offset.y);
			this->rootPos = rootPos;
		}
		//Equal
		else
		{
			this->rootPos = localPosition;
			this->maskScaleOffset = maskScaleOffset;
			renderingLocalPosition = int2(0, 0);
			terrainData->maskLoadList.Push(
				{
					true,//load
					this->rootPos//pos
				});
			/*lock(terrainData)
			{
				terrainData->boundBoxLoadList.Add(loadCommand);
			}*/
			//TODO

		}
	}
	else
	{
		this->rootPos = localPosition;
		renderingLocalPosition = parentRenderingPos;
		this->maskScaleOffset = maskScaleOffset;
	}

}
double2 TerrainQuadTree::CornerWorldPos()
{

	double2 chunkPos = terrainData->screenOffset;
	//    chunkPos *= largestChunkSize;
	chunkPos += (terrainData->largestChunkSize / pow(2, lodLevel)) * double2(localPosition.x, localPosition.y);
	return chunkPos;

}
double4 TerrainQuadTree::BoundedWorldPos()
{

	double2 leftCorner = CornerWorldPos();
	double2 rightCorner = terrainData->screenOffset + (terrainData->largestChunkSize / pow(2, lodLevel)) * (double2(localPosition.x, localPosition.y) + double2(1, 1));
	return double4(leftCorner.x, leftCorner.y, rightCorner.x, rightCorner.y);

}
double2 TerrainQuadTree::CenterWorldPos()
{
	double2 chunkPos = terrainData->screenOffset;
	//chunkPos *= largestChunkSize;
	chunkPos += (terrainData->largestChunkSize / pow(2, lodLevel)) * (double2(localPosition.x, localPosition.y) + 0.5);
	return chunkPos;
}
void TerrainQuadTree::GetMaterialMaskRoot(double2 xzPosition, double radius, std::vector<TerrainQuadTree*>& allTreeNode)
{
	if (lodLevel == terrainData->lodOffset)
	{
		double4 bounded = BoundedWorldPos();
		bool vx = xzPosition.x + radius > bounded.x;
		bool vy = xzPosition.y + radius > bounded.y;
		bool vz = xzPosition.x - radius < bounded.z;
		bool vw = xzPosition.y - radius < bounded.w;
		if (vx && vy && vz && vw)
		{
			allTreeNode.push_back(this);
		}
	}
	else if (lodLevel < terrainData->lodOffset)
	{
		if (leftDown != nullptr)
		{
			leftDown->GetMaterialMaskRoot(xzPosition, radius, allTreeNode);
			rightDown->GetMaterialMaskRoot(xzPosition, radius, allTreeNode);
			leftUp->GetMaterialMaskRoot(xzPosition, radius, allTreeNode);
			rightUp->GetMaterialMaskRoot(xzPosition, radius, allTreeNode);
		}
	}
}

void TerrainQuadTree::UpdateChunks(double3 circleRange)
{
	if (isRendering())
	{
		double4 boundedPos = BoundedWorldPos();
		if (boundedPos.x - circleRange.x < circleRange.z &&
			boundedPos.y - circleRange.y < circleRange.z &&
			circleRange.x - boundedPos.z < circleRange.z &&
			circleRange.y - boundedPos.w < circleRange.z)
		{
			terrainData->loadDataList.Push(
				{
					maskScaleOffset,//maskScaleOffset
					TerrainLoadData::Operator::Update,	//ope
					VirtualTextureSize(),//size
					decalMask,//targetDecalLayer
					VirtualTextureIndex(),//startIndex
					rootPos//rootPos
				}
			);
		}
	}
	if (leftDown != nullptr)
	{
		leftDown->UpdateChunks(circleRange);
		leftUp->UpdateChunks(circleRange);
		rightDown->UpdateChunks(circleRange);
		rightUp->UpdateChunks(circleRange);
	}
}
TerrainQuadTree::~TerrainQuadTree()
{
	if (lodLevel == terrainData->lodOffset)
	{
		MaskLoadCommand loadCommand =
		{
			false,
			rootPos
		};
		terrainData->maskLoadList.Push(loadCommand);
		/*lock(terrainData)
		{
			terrainData->boundBoxLoadList.Add(loadCommand);
		}*/
		//TODO
	}

	if (isRendering())
	{
		int2 startIndex = VirtualTextureIndex();
		TerrainLoadData loadData;
		loadData.ope = TerrainLoadData::Operator::Unload;
		loadData.startIndex = startIndex;
		terrainData->loadDataList.Push(loadData);
	}
	SetRendering(false);

	if (leftDown != nullptr)
	{
		terrainData->pool.Delete(leftDown);
		terrainData->pool.Delete(leftUp);
		terrainData->pool.Delete(rightDown);
		terrainData->pool.Delete(rightUp);

		leftDown = nullptr;
		leftUp = nullptr;
		rightDown = nullptr;
		rightUp = nullptr;
	}
}
void TerrainQuadTree::EnableRendering()
{
	if (terrainData->textureCapacity < 1) return;
	if (!isRendering())
	{
		int2 vtIndex = VirtualTextureIndex();
		int3 pack = int3(vtIndex.x, vtIndex.y, VirtualTextureSize());
		if (!terrainData->initializing)
		{
			terrainData->loadDataList.Push(
				{
					maskScaleOffset,//markScaleOffset
					TerrainLoadData::Operator::Load,//ope
					pack.z,//Size
					decalMask,//TargetDecalLayer
					int2(pack.x, pack.y),//startIndex
					rootPos//rootPos
				}
			);
		}
	}
	SetRendering(true);
}
void TerrainQuadTree::InitializeRenderingCommand()
{
	if (isRendering())
	{
		int2 vtIndex = VirtualTextureIndex();
		int3 pack = int3(vtIndex.x, vtIndex.y, VirtualTextureSize());
		terrainData->initializeLoadList.Push({
			maskScaleOffset,//maskScaleOffset
			TerrainLoadData::Operator::Load,//ope
			pack.z,//size
			decalMask,//targetDecalLayer
			int2(pack.x, pack.y),//startIndex,
			rootPos//rootPos
			});
	}
	if (leftDown != nullptr)
	{
		leftDown->InitializeRenderingCommand();
		leftUp->InitializeRenderingCommand();
		rightDown->InitializeRenderingCommand();
		rightUp->InitializeRenderingCommand();
	}
}

void TerrainQuadTree::DisableRendering()
{
	if (isRendering())
	{

		int2 startIndex = VirtualTextureIndex();
		TerrainLoadData loadData;
		loadData.ope = TerrainLoadData::Operator::Unload;
		loadData.startIndex = startIndex;
		terrainData->loadDataList.Push(loadData);
	}

	SetRendering(false);
}

void TerrainQuadTree::LogicSeparate()
{
	if (leftDown == nullptr)
	{

		double subSize = worldSize * 0.5;

		leftDown = terrainData->pool.New(
			lodLevel, LocalPos::LeftDown, localPosition, renderingLocalPosition, subSize, maskScaleOffset, rootPos
		);
		leftUp = terrainData->pool.New(
			lodLevel, LocalPos::LeftUp, localPosition, renderingLocalPosition, subSize, maskScaleOffset, rootPos
		);
		rightDown = terrainData->pool.New(
			lodLevel, LocalPos::RightDown, localPosition, renderingLocalPosition, subSize, maskScaleOffset, rootPos
		);
		rightUp = terrainData->pool.New(
			lodLevel, LocalPos::RightUp, localPosition, renderingLocalPosition, subSize, maskScaleOffset, rootPos
		);
	}
}
void TerrainQuadTree::Separate()
{
	if (lodLevel >= terrainData->allLodLevles.size() - 1)
	{
		EnableRendering();
	}
	else if (leftDown == nullptr && terrainData->textureCapacity >= 4)
	{
		SetRendering(false);

		double subSize = worldSize * 0.5;
		leftDown = terrainData->pool.New(
			lodLevel, LocalPos::LeftDown, localPosition, renderingLocalPosition, subSize, maskScaleOffset, rootPos
		);
		leftUp = terrainData->pool.New(
			lodLevel, LocalPos::LeftUp, localPosition, renderingLocalPosition, subSize, maskScaleOffset, rootPos
		);
		rightDown = terrainData->pool.New(
			lodLevel, LocalPos::RightDown, localPosition, renderingLocalPosition, subSize, maskScaleOffset, rootPos
		);
		rightUp = terrainData->pool.New(
			lodLevel, LocalPos::RightUp, localPosition, renderingLocalPosition, subSize, maskScaleOffset, rootPos
		);
		double3 maskScaleOffset = double3(this->maskScaleOffset.x, this->maskScaleOffset.y, this->maskScaleOffset.z);
		maskScaleOffset.x *= 0.5f;
		leftDown->SetRendering(true);
		leftUp->SetRendering(true);
		rightDown->SetRendering(true);
		rightUp->SetRendering(true);
		if (!terrainData->initializing)
		{
			terrainData->loadDataList.Push(
				{
					maskScaleOffset,//maskScaleOffset
					TerrainLoadData::Operator::Separate,//ope
					VirtualTextureSize(),//size
					leftDown->decalMask,//targetDecalLayer
					VirtualTextureIndex(),//startIndex
					rootPos//rootPos

				}
			);
		}
	}
	distOffset = -terrainData->lodDeferredOffset;
}
void TerrainQuadTree::Combine(bool enableSelf)
{
	distOffset = terrainData->lodDeferredOffset;
	if (leftDown != nullptr)
	{
		if (!leftDown->isRendering() || !leftUp->isRendering() || !rightDown->isRendering() || !rightUp->isRendering())
		{
			return;
		}
		leftDown->SetRendering(false);
		leftUp->SetRendering(false);
		rightDown->SetRendering(false);
		rightUp->SetRendering(false);
		terrainData->pool.Delete(leftDown);
		terrainData->pool.Delete(leftUp);
		terrainData->pool.Delete(rightDown);
		terrainData->pool.Delete(rightUp);
		leftDown = nullptr;
		leftUp = nullptr;
		rightDown = nullptr;
		rightUp = nullptr;
		if (enableSelf)
		{
			int2 vtIndex = VirtualTextureIndex();
			int3 pack = int3(vtIndex.x, vtIndex.y, VirtualTextureSize());
			TerrainLoadData loadData;
			loadData.ope = TerrainLoadData::Operator::Combine;
			loadData.startIndex = int2(pack.x, pack.y);
			loadData.size = pack.z;
			terrainData->loadDataList.Push(loadData);
			SetRendering(true);
		}
		else
		{
			DisableRendering();
		}


	}
	else
	{
		if (enableSelf)
			EnableRendering();
		else
			DisableRendering();
	}
}
void TerrainQuadTree::PushDrawRequest(std::vector<TerrainDrawCommand>& loadedBufferList)
{
	if (lodLevel == terrainData->lodOffset)
	{
		if (isRendering() || leftDown != nullptr)
		{
			TerrainDrawCommand loadData;
			loadData.startPos = CornerWorldPos();
			loadData.startVTIndex = VirtualTextureIndex();
			loadData.rootPos = rootPos + int2(maskScaleOffset.y, maskScaleOffset.z);
			loadedBufferList.push_back(loadData);
		}
	}
	else if (lodLevel < terrainData->lodOffset)
	{
		if (leftDown != nullptr)
		{
			leftDown->PushDrawRequest(loadedBufferList);
			leftUp->PushDrawRequest(loadedBufferList);
			rightDown->PushDrawRequest(loadedBufferList);
			rightUp->PushDrawRequest(loadedBufferList);
		}
	}
}

void TerrainQuadTree::CombineUpdate()
{
	if (leftDown != nullptr)
	{
		leftDown->CombineUpdate();
		leftUp->CombineUpdate();
		rightDown->CombineUpdate();
		rightUp->CombineUpdate();
	}
	double backface = isInRange ? 1 : terrainData->backfaceCullingLevel;
	auto Sample = [&](uint index)->double
	{
		double distance = index >= terrainData->allLodLevles.size() ? 0 : terrainData->allLodLevles[index];
		distance *= backface;
		//index++;
		//distance = Max(distance, index >= terrainData->allLodLevles.size() ? 0 : terrainData->allLodLevles[index]);
		return distance;
	};
	if (dist > Sample(lodLevel) - distOffset)
	{
		separate = false;
		Combine(lodLevel > terrainData->lodOffset);
	}
	else if (dist > Sample(lodLevel + 1) - distOffset)
	{
		separate = false;
		Combine(lodLevel >= terrainData->lodOffset);
	}
	else
		separate = true;

}

void TerrainQuadTree::SeparateUpdate()
{
	if (!terrainData->initializing && initializing)
	{
		initializing = false;
		return;
	}
	else
		initializing = false;
	if (separate)
	{
		Separate();
		if (leftDown != nullptr)
		{
			leftDown->SeparateUpdate();
			leftUp->SeparateUpdate();
			rightDown->SeparateUpdate();
			rightUp->SeparateUpdate();
		}

	}
}
void TerrainQuadTree::UpdateData(double3 camPos, double2 heightScaleOffset, double3 camFrustumMin, double3 camFrustumMax, Math::Vector4* planes)
{
	double2 centerworldPosXZ = CornerWorldPos();
	double extent = worldSize * 0.5;
	double2 cornerRightPos = centerworldPosXZ + extent * 2;
	double4 xzBounding = double4(centerworldPosXZ.x, centerworldPosXZ.y, cornerRightPos.x, cornerRightPos.y);
	centerworldPosXZ += extent;
	double2 texMinMax = double2(0, 1);
	/*lock(MTerrain.current)
	{
		MTerrainBoundingTree boundTree;
		int2 currentRootPos = rootPos + (int2)maskScaleOffset.yz;
		int targetLevel = lodLevel - MTerrain.current.lodOffset;
		if (targetLevel >= 0 && MTerrain.current.boundingDict.Get(currentRootPos, out boundTree) && boundTree.isCreate)
		{
			texMinMax = boundTree[renderingLocalPosition, targetLevel];

		}

	}*/
	double2 heightMinMax = heightScaleOffset.y + texMinMax * heightScaleOffset.x;
	double2 heightCenterExtent = double2(heightMinMax.x + heightMinMax.y, heightMinMax.y - heightMinMax.x) * 0.5;
	double3 centerWorldPos = double3(centerworldPosXZ.x, 0, centerworldPosXZ.y);
	double3 centerExtent = double3(extent, heightCenterExtent.y + /*terrainData->maxDisplaceHeight*/0, extent);	//TODO

	isInRange = MathLib::BoxContactWithBox(camFrustumMin, camFrustumMax, double3(xzBounding.x, heightMinMax.x, xzBounding.y), double3(xzBounding.z, heightMinMax.y, xzBounding.w));
	if (isInRange)
	{
		isInRange = MathLib::BoxIntersect(Vector3(centerWorldPos.x, centerWorldPos.y, centerWorldPos.z), Vector3(worldSize * 0.5, 0, worldSize * 0.5), planes, 6);
	}
	toPoint = camPos - centerWorldPos;
	Vector3 distVec = Vector3(toPoint.x, toPoint.y, toPoint.z);
	dist = MathLib::DistanceToQuad(extent, float2(toPoint.x, toPoint.z));
	//dist = MathLib::DistanceToCube(Vector3(extent, 1000, extent), distVec);
	//TODO: dist' height calculate
	if (leftDown != nullptr)
	{
		leftDown->UpdateData(camPos, heightScaleOffset, camFrustumMin, camFrustumMax, planes);
		leftUp->UpdateData(camPos, heightScaleOffset, camFrustumMin, camFrustumMax, planes);
		rightDown->UpdateData(camPos, heightScaleOffset, camFrustumMin, camFrustumMax, planes);
		rightUp->UpdateData(camPos, heightScaleOffset, camFrustumMin, camFrustumMax, planes);
	}
}