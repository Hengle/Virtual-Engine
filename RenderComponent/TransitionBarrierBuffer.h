#pragma once
#include "../Singleton/Graphics.h"
class ThreadCommand;
class TransitionBarrierBuffer
{
private:
	friend class ThreadCommand;
	std::vector<D3D12_RESOURCE_BARRIER> commands;
	
public:
	TransitionBarrierBuffer()
	{
		commands.reserve(20);
	}
	void AddCommand(D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState, ID3D12Resource* resource)
	{
		commands.push_back(
			CD3DX12_RESOURCE_BARRIER::Transition(
				resource,
				beforeState,
				afterState
			)
		);
	}
	void ExecuteCommand(ID3D12GraphicsCommandList* commandList)
	{
		if (!commands.empty())
		{
			commandList->ResourceBarrier(commands.size(), commands.data());
			commands.clear();
		}
	}
};