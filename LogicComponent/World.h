#pragma once
//#include "../RenderComponent/MeshRenderer.h"
#include "../Common/d3dUtil.h"
#include "../Common/MObject.h"
#include "../Common/BitArray.h"
#include "../Common/RandomVector.h"
#include <mutex>
class PBRMaterialManager;
class FrameResource;
class Transform;
class DescriptorHeap;
class Mesh;
class GRPRenderManager;

//Only For Test!
class World final
{
	friend class Transform;
private:
	static const UINT MAXIMUM_HEAP_COUNT = 10000;
	ObjectPtr<DescriptorHeap> globalDescriptorHeap;
	BitArray usedDescs;
	std::vector<UINT> unusedDescs;
	World(ID3D12GraphicsCommandList* cmdList, ID3D12Device* device);
	static World* current;
	std::mutex mtx;
	GRPRenderManager* grpRenderer;
	PBRMaterialManager* grpMaterialManager;
	RandomVector<ObjectPtr<Transform>> allTransformsPtr;
public:
	GRPRenderManager* GetGRPRenderManager() const
	{
		return grpRenderer;
	}
	PBRMaterialManager* GetPBRMaterialManager() const
	{
		return grpMaterialManager;
	}
	void Rebuild(ID3D12GraphicsCommandList* cmdList, ID3D12Device* device);
	~World();
	UINT windowWidth;
	UINT windowHeight;
	inline static constexpr World* GetInstance() { return current; }
	inline static constexpr World* CreateInstance(ID3D12GraphicsCommandList* cmdList, ID3D12Device* device)
	{
		if (current)
			return current;
		new World(cmdList, device);
		return current;
	}
	inline static constexpr void DestroyInstance()
	{
		auto a = current;
		current = nullptr;
		if (a) delete a;
	}
	void Update(FrameResource* resource, ID3D12Device* device);
	inline constexpr DescriptorHeap* GetGlobalDescHeap() const
	{ return globalDescriptorHeap.operator->(); }
	UINT GetDescHeapIndexFromPool();
	void ReturnDescHeapIndexToPool(UINT targetIndex);
	void ForceCollectAllHeapIndex();
};