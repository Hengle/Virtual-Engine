#include "TransitionBarrierBuffer.h"
TransitionBarrierBuffer::TransitionBarrierBuffer()
{
	commands.reserve(20);
	barrierRecorder.reserve(20);
}
void TransitionBarrierBuffer::AddCommand(D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState, ID3D12Resource* resource)
{
	auto ite = barrierRecorder.find(resource);
	if (ite == barrierRecorder.end())
	{
		Command cmd;
		cmd.beforeState = beforeState;
		cmd.afterState = afterState;
		cmd.index = commands.size();
		barrierRecorder.insert_or_assign(resource, cmd);
		commands.push_back(
			CD3DX12_RESOURCE_BARRIER::Transition(
				resource,
				beforeState,
				afterState
				)
			);
	}
	else
	{
#ifdef _DEBUG
		if (ite->second.afterState != beforeState)
		{
			throw "State Unmatch!";
		}
#endif
		ite->second.afterState = afterState;
		if (ite->second.beforeState == afterState)
		{
			auto lastOne = commands.end() - 1;
			commands[ite->second.index] = *lastOne;
			commands.erase(lastOne);
			barrierRecorder.erase(ite);
		}
		else {
			commands[ite->second.index].Transition.StateAfter = afterState;
		}
	}
}
void TransitionBarrierBuffer::ExecuteCommand(ID3D12GraphicsCommandList* commandList)
{
	if (!commands.empty())
	{
		commandList->ResourceBarrier(commands.size(), commands.data());
		commands.clear();
		barrierRecorder.clear();
	}
}