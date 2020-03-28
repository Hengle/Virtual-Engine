#pragma once
#include "TerrainQuadTree.h"
#include "../../Common/MObject.h"
#include "../../Common/d3dUtil.h"
#include "../../RenderComponent/UploadBuffer.h"
#include <mutex>
class VirtualTexture;
class TerrainQuadTree;
class TerrainLoadTask;
class VirtualTextureData;
class Camera;
class FrameResource;
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
	StackObject<TerrainData, true> terrainData;
	StackObject<UploadBuffer, true> indirectBuffer;
	std::unique_ptr<TerrainLoadTask> loadTask;
	ObjWeakPtr<VirtualTexture> virtualTexture;
	std::mutex mtx;
	FrameResource* frameResource;
	ID3D12Device* device;
	CameraData data;
	VirtualTextureData* vtData;
public:
	TerrainMainLogic(ID3D12Device* device, const ObjectPtr<VirtualTexture>& vt, VirtualTextureData* vtData);
	void UpdateCameraData(Camera* cam, FrameResource* resource, ID3D12Device* device);
	void JobUpdate();
	~TerrainMainLogic();
};