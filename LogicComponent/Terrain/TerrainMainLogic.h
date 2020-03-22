#pragma once
#include "TerrainQuadTree.h"
#include "../../Common/MObject.h"
#include "../../Common/d3dUtil.h"
#include <mutex>
class VirtualTexture;
class TerrainQuadTree;
class TerrainLoadTask;
class Camera;
class TerrainMainLogic final
{
private:
	struct CameraData
	{
		Math::Vector3 right;
		Math::Vector3 up;
		Math::Vector3 forward;
		Math::Vector3 position;
		double fov;
		double nearZ;
		double farZ;
		double nearWindowHeight;
		double farWindowHeight;
		double aspect;
	};
	static TerrainMainLogic* current;
	TerrainQuadTree* quadTreeMain;
	StackObject<TerrainData> terrainData;
	std::unique_ptr<TerrainLoadTask> loadTask;
	ObjWeakPtr<VirtualTexture> virtualTexture;
	std::mutex mtx;
	CameraData data;
public:
	TerrainMainLogic(const ObjectPtr<VirtualTexture>& vt);
	void UpdateCameraData(Camera* cam);
	void JobUpdate();
	~TerrainMainLogic();
};