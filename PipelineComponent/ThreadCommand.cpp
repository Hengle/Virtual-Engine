#include "ThreadCommand.h"
#include "../RenderComponent/RenderTexture.h"
#include "../RenderComponent/StructuredBuffer.h"
#include "../Singleton/Graphics.h"
#include "../Common/d3dApp.h"
void ThreadCommand::ResetCommand()
{
	ThrowIfFailed(cmdAllocator->Reset());
	ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), nullptr));
}
void ThreadCommand::CloseCommand()
{

	/*for (auto ite = rtStateMap.begin(); ite != rtStateMap.end(); ++ite)
	{
		if (ite->second)
		{
			D3D12_RESOURCE_STATES writeState, readState;
			writeState = ite->first->GetWriteState();
			readState = ite->first->GetReadState();
			buffer.AddCommand(readState, writeState, ite->first->GetResource());
		}
	}
	for (auto ite = sbufferStateMap.begin(); ite != sbufferStateMap.end(); ++ite)
	{
		if (ite->second)
		{
			buffer.AddCommand(D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, ite->first->GetResource());
		}
	}*/
	buffer.ExecuteCommand(GetCmdList());
	rtStateMap.clear();
	sbufferStateMap.clear();

	cmdList->Close();
}
ThreadCommand::ThreadCommand(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type)
{
	rtStateMap.reserve(20);
	sbufferStateMap.reserve(20);
	ThrowIfFailed(device->CreateCommandAllocator(
		type,
		IID_PPV_ARGS(&cmdAllocator)));
	ThrowIfFailed(device->CreateCommandList(
		0,
		type,
		cmdAllocator.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(&cmdList)));
	cmdList->Close();
}

bool ThreadCommand::UpdateState(RenderTexture* rt, ResourceReadWriteState state)
{
	auto ite = rtStateMap.find(rt);
	if (state)
	{
		if (ite == rtStateMap.end())
		{
			rtStateMap.insert_or_assign(rt, state);
		}
		else if (ite->second != state)
		{
			ite->second = state;
		}
		else
			return false;
	}
	else
	{
		if (ite != rtStateMap.end())
		{
			rtStateMap.erase(ite);
		}
		else
			return false;
	}
	return true;
}
bool ThreadCommand::UpdateState(StructuredBuffer* rt, ResourceReadWriteState state)
{
	auto ite = sbufferStateMap.find(rt);
	if (state)
	{
		if (ite == sbufferStateMap.end())
		{
			sbufferStateMap.insert_or_assign(rt, state);
		}
		else if (ite->second != state)
		{
			ite->second = state;
		}
		else
			return false;
	}
	else
	{
		if (ite != sbufferStateMap.end())
		{
			sbufferStateMap.erase(ite);
		}
		else
			return false;
	}
	return true;
}

void ThreadCommand::SetResourceReadWriteState(RenderTexture* rt, ResourceReadWriteState state)
{
	if (!UpdateState(rt, state)) return;
	D3D12_RESOURCE_STATES writeState, readState;
	writeState = rt->GetWriteState();
	readState = rt->GetReadState();
	if (state)
	{
		buffer.AddCommand(writeState, readState, rt->GetResource());
	}
	else
	{
		buffer.AddCommand( readState, writeState, rt->GetResource());
	}
}

void ThreadCommand::SetResourceReadWriteState(StructuredBuffer* rt, ResourceReadWriteState state)
{
	if (!UpdateState(rt, state)) return;
	if (state)
	{
		buffer.AddCommand(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ, rt->GetResource());
	}
	else
	{
		buffer.AddCommand(D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, rt->GetResource());
	}
}