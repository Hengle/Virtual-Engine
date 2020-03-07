#pragma once
#include "../Common/d3dUtil.h"
#include <mutex>
#include <atomic>
#include "../RenderComponent/TransitionBarrierBuffer.h"
struct StateTransformBuffer
{
	ID3D12Resource* targetResource;
	D3D12_RESOURCE_STATES beforeState;
	D3D12_RESOURCE_STATES afterState;
};
class PipelineComponent;
class StructuredBuffer;
class RenderTexture;

class ThreadCommand final
{
	friend class PipelineComponent;
private:
	TransitionBarrierBuffer buffer;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
	std::unordered_map<RenderTexture*, ResourceReadWriteState> rtStateMap;
	std::unordered_map<StructuredBuffer*, ResourceReadWriteState> sbufferStateMap;
	bool UpdateState(RenderTexture* rt, ResourceReadWriteState state);
	bool UpdateState(StructuredBuffer* rt, ResourceReadWriteState state);
	template <uint maxCount>
	inline constexpr static void MultiResourceStateTransform(
		ID3D12GraphicsCommandList* commandList,
		std::pair<D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES>* statesTransformation,
		ID3D12Resource** resources,
		uint resourceCount);
public:
	TransitionBarrierBuffer* GetBarrierBuffer()
	{
		return &buffer;
	}
	inline ID3D12CommandAllocator* GetAllocator() const { return cmdAllocator.Get(); }
	inline ID3D12GraphicsCommandList* GetCmdList() const { return cmdList.Get(); }
	ThreadCommand(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
	void SetResourceReadWriteState(RenderTexture* rt, ResourceReadWriteState state);
	void SetResourceReadWriteState(StructuredBuffer* rt, ResourceReadWriteState state);
	template<uint resourceCount>
	void SetResourcesReadWriteState(RenderTexture** rt, ResourceReadWriteState* state);
	template<uint resourceCount>
	void SetResourcesReadWriteState(StructuredBuffer** rt, ResourceReadWriteState* state);
	void ResetCommand();
	void CloseCommand();
};

template <uint maxCount>
inline constexpr void ThreadCommand::MultiResourceStateTransform(
	ID3D12GraphicsCommandList* commandList,
	std::pair<D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES>* statesTransformation,
	ID3D12Resource** resources,
	uint resourceCount)
{
	D3D12_RESOURCE_BARRIER barrier[maxCount];
	for (uint i = 0; i < resourceCount; ++i)
	{
		barrier[i] = CD3DX12_RESOURCE_BARRIER::Transition(
			resources[i],
			statesTransformation[i].first,
			statesTransformation[i].second
		);
	}
	commandList->ResourceBarrier(resourceCount, barrier);
}

template<uint resourceCount>
void ThreadCommand::SetResourcesReadWriteState(RenderTexture** rt, ResourceReadWriteState* state)
{
	std::pair<D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES> reses[resourceCount];
	ID3D12Resource* resources[resourceCount];
	uint count = 0;
	for (uint i = 0; i < resourceCount; ++i)
	{
		if (UpdateState(rt[i], state[i]))
		{
			auto writeState = rt[i]->GetWriteState();
			auto readState = rt[i]->GetReadState();
			if (state[i])
			{
				reses[count] = { writeState, readState };
			}
			else
			{
				reses[count] = { readState, writeState };
			}
			resources[count] = rt[i]->GetResource();
			count++;
		}
	}
	MultiResourceStateTransform<resourceCount>(cmdList.Get(), reses, resources, count);
}

template<uint resourceCount>
void ThreadCommand::SetResourcesReadWriteState(StructuredBuffer** rt, ResourceReadWriteState* state)
{
	std::pair<D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES> reses[resourceCount];
	ID3D12Resource* resources[resourceCount];
	uint count = 0;
	for (uint i = 0; i < resourceCount; ++i)
	{
		if (UpdateState(rt[i], state[i]))
		{
			const auto writeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			const auto readState = D3D12_RESOURCE_STATE_GENERIC_READ;
			if (state[i])
			{
				reses[count] = { writeState, readState };
			}
			else
			{
				reses[count] = { readState, writeState };
			}
			resources[count] = rt[i]->GetResource();
			count++;
		}
	}
	MultiResourceStateTransform<resourceCount>(cmdList.Get(), reses, resources, count);
}