#pragma once
#include "../Common/MObject.h"
#include "CommandSignature.h"
#include "../Common/MetaLib.h"
#include "CBufferPool.h"
class DescriptorHeap;
class Mesh;
class Transform;
class FrameResource;
class ComputeShader;
class PSOContainer;
class StructuredBuffer;
class TransitionBarrierBuffer;
class GRPRenderManager
{
public:
	struct CullData
	{
		float4x4 _LastVPMatrix;
		float4x4 _VPMatrix;
		DirectX::XMFLOAT4 planes[6];
		//Align
		DirectX::XMFLOAT3 _FrustumMinPoint;
		UINT _Count;
		//Align
		DirectX::XMFLOAT3 _FrustumMaxPoint;
	};
	struct RenderElement
	{
		ObjectPtr<Transform> transform;
		DirectX::XMFLOAT3 boundingCenter;
		DirectX::XMFLOAT3 boundingExtent;
		RenderElement(
			const ObjectPtr<Transform>& anotherTrans,
			DirectX::XMFLOAT3 boundingCenter,
			DirectX::XMFLOAT3 boundingExtent
		);
	};
private:
	CommandSignature cmdSig;
	Shader* shader;
	std::unique_ptr<StructuredBuffer> cullResultBuffer;
	std::unique_ptr<StructuredBuffer> dispatchIndirectBuffer;;
	std::vector<RenderElement> elements;
	std::unordered_map<Transform*, UINT> dicts;
	UINT meshLayoutIndex;
	UINT capacity;
	UINT _InputBuffer;
	UINT _InputDataBuffer;
	UINT _OutputBuffer;
	uint _InputIndexBuffer;
	uint _DispatchIndirectBuffer;
	uint _HizDepthTex;
	UINT _CountBuffer;
	UINT CullBuffer;
	ComputeShader* cullShader;
public:
	GRPRenderManager(
		UINT meshLayoutIndex,
		UINT initCapacity,
		Shader* shader,
		ID3D12Device* device
	);
	~GRPRenderManager();
	RenderElement& AddRenderElement(
		const ObjectPtr<Transform>& targetTrans,
		Mesh* mesh,
		ID3D12Device* device,
		UINT shaderID,
		UINT materialID
	);
	inline Shader* GetShader() const { return shader; }
	static CBufferPool* GetCullDataPool(UINT initCapacity);
	void RemoveElement(const ObjectPtr<Transform>& trans, ID3D12Device* device);
	void UpdateRenderer(const ObjectPtr<Transform>& targetTrans, Mesh* mesh, ID3D12Device* device);
	CommandSignature* GetCmdSignature() { return &cmdSig; }
	void UpdateFrame(FrameResource*, ID3D12Device*);//Should be called Per frame
	void UpdateTransform(
		const ObjectPtr<Transform>& targetTrans,
		ID3D12Device* device,
		UINT shaderID,
		UINT materialID);
	void Culling(
		ID3D12GraphicsCommandList* commandList,
		TransitionBarrierBuffer& barrierBuffer,
		ID3D12Device* device,
		FrameResource* targetResource,
		const ConstBufferElement& cullDataBuffer,
		DirectX::XMFLOAT4* frustumPlanes,
		DirectX::XMFLOAT3 frustumMinPoint,
		DirectX::XMFLOAT3 frustumMaxPoint,
		const float4x4& vpMatrix,
		const float4x4& lastVPMatrix,
		uint hizDepthIndex,
		bool occlusion
	);
	void OcclusionRecheck(
		ID3D12GraphicsCommandList* commandList,
		TransitionBarrierBuffer& barrierBuffer,
		ID3D12Device* device,
		FrameResource* targetResource,
		const ConstBufferElement& cullDataBuffer,
		uint hizDepthIndex);
	void DrawCommand(
		ID3D12GraphicsCommandList* commandList,
		ID3D12Device* device,
		UINT targetShaderPass,
		uint cameraPropertyID,
		const ConstBufferElement& cameraProperty,
		PSOContainer* container, uint containerIndex
	);
	
};
