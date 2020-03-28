#pragma once
#include "../Singleton/Graphics.h"
class ThreadCommand;
class TransitionBarrierBuffer
{
private:
	friend class ThreadCommand;
	std::vector<D3D12_RESOURCE_BARRIER> commands;
	struct Command
	{
		D3D12_RESOURCE_STATES beforeState;
		D3D12_RESOURCE_STATES afterState;
		uint index;
	};
	std::unordered_map<ID3D12Resource*, Command> barrierRecorder;
public:
	TransitionBarrierBuffer();
	void AddCommand(D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState, ID3D12Resource* resource);
	void ExecuteCommand(ID3D12GraphicsCommandList* commandList);
};