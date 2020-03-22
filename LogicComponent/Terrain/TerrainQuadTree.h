#pragma once
#include "../../Common/d3dUtil.h"
#include "../../Common/MetaLib.h"
#include "../../Common/Pool.h"
#include "../../Common/RingQueue.h"
struct TerrainDrawCommand
{
	double2 startPos;
	int2 startVTIndex;
	int2 rootPos;
};
struct MaskLoadCommand
{
	bool load;
	int2 pos;
};

struct TerrainLoadData
{
	enum class Operator : uint
	{
		Load = 0,
		Separate = 1,
		Update = 2,
		Unload = 3,
		Combine = 4
	};
	double3 maskScaleOffset;	//Current Scale offset in the splat map
	Operator ope;				
	int size;					//Loaded chunk size
	uint targetDecalLayer;		
	int2 startIndex;			//Started chunk index
	int2 rootPos;				//splat index
};
class TerrainQuadTree;
struct TerrainData
{
	double2 screenOffset;
	double largestChunkSize;
	double backfaceCullingLevel = 0.5;
	double lodDeferredOffset = 0.8;
	int decalLayerOffset = 2;
	int lodOffset = 0;
	int textureCapacity = 0;
	bool initializing = true;
	std::vector<double> allLodLevles;//TODO
	std::vector<uint> allDecalLayers;//TODO
	RingQueue<MaskLoadCommand> maskLoadList;
	RingQueue<TerrainLoadData> loadDataList;
	RingQueue<TerrainLoadData> initializeLoadList;
	Pool<TerrainQuadTree> pool;

	TerrainData();
};

class TerrainQuadTree
{
private:
	bool m_isRendering = false;
	double distOffset = 0;
	int2 renderingLocalPosition = int2(0,0);
	uint decalMask = 0;
	bool initializing = false;
	void EnableRendering();
	void DisableRendering();
	void LogicSeparate();
	void Separate();
	void Combine(bool enableSelf);
public:
	static TerrainData* terrainData;
	enum class LocalPos : uint
	{
		LeftDown = 0,
		LeftUp = 1,
		RightDown = 2,
		RightUp = 3
	};
	double3 toPoint = double3(0,0,0);
	// double3 toPoint3D;
	double dist = 0;
	bool separate = false;
	bool isInRange = false;
	TerrainQuadTree* leftDown = nullptr;
	TerrainQuadTree* leftUp = nullptr;
	TerrainQuadTree* rightDown = nullptr;
	TerrainQuadTree* rightUp = nullptr;
	int lodLevel = 0;
	int2 localPosition = int2(0,0);
	inline int2 VirtualTextureIndex() const {
		const int a = (const int)(0.1 + pow(2.0, terrainData->allLodLevles.size() - lodLevel));
		return mul(localPosition, { a,a });
	}
	bool isRendering() const { return m_isRendering; }
	double worldSize = 0;
	int2 rootPos = int2(0,0);
	double3 maskScaleOffset = 0;
	void SetRendering(bool value);

	inline int  VirtualTextureSize() const
	{
		return (int)(0.1 + pow(2.0, terrainData->allLodLevles.size() - lodLevel));
	}
	TerrainQuadTree(int parentLodLevel, LocalPos sonPos, int2 parentPos, int2 parentRenderingPos, double worldSize, double3 maskScaleOffset, int2 rootPos);
	double2 CornerWorldPos();
	double4 BoundedWorldPos();
	double2 CenterWorldPos();
	void GetMaterialMaskRoot(double2 xzPosition, double radius, std::vector<TerrainQuadTree*>& allTreeNode);
	void UpdateChunks(double3 circleRange);
	~TerrainQuadTree();
	void InitializeRenderingCommand();
	void PushDrawRequest(std::vector<TerrainDrawCommand>& loadedBufferList);
	void CombineUpdate();
	void SeparateUpdate();
	void UpdateData(double3 camPos, double2 heightScaleOffset, double3 camFrustumMin, double3 camFrustumMax, Math::Vector4* planes);
};
