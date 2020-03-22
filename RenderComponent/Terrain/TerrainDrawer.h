#pragma once
#include "../../Common/d3dUtil.h"
#include "../../Common/MObject.h"
#include "../../Common/MetaLib.h"
#include "../StructuredBuffer.h"
#include "../CBufferPool.h"
#include "../Mesh.h"
class Shader;
class FrameResource;
class TransitionBarrierBuffer;
class PSOContainer;
class VirtualTexture;
class TerrainDrawerFrameData;
class TerrainDrawer : public MObject
{
private:
	friend class TerrainDrawerFrameData;
	struct ObjectData
	{
		float2 worldPosition;
		float2 worldScale;
		uint2 uvIndex;
	};
	StackObject<Mesh> planeMesh;
	StackObject<StructuredBuffer> objectBuffer;
	uint2 drawCount;
	struct PropID
	{
		bool initialized = false;
		uint _ObjectDataBuffer;
		void Initialize();
	};
	static PropID propID;
public:
	TerrainDrawer(
		ID3D12Device* device,
		uint2 drawCount,
		double2 perObjectSize
	);
	~TerrainDrawer();
	void Draw(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		Shader* shader,
		uint pass,
		PSOContainer* psoContainer,
		uint containerIndex,
		ConstBufferElement cameraBuffer,
		FrameResource* frameResource,
		VirtualTexture*
	);
};