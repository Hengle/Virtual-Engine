#pragma once
#include "JobSystem.h"
#include "JobBucket.h"
#include "JobNode.h"

template <typename Func>
constexpr JobHandle JobBucket::GetTask(const Func& func, JobHandle* dependedJobs, uint dependCount)
{
	JobNode* node = sys->jobNodePool.New();
	allJobNodeCount++;
	node->bucket = this;
	node->Create<Func>(func, &sys->vectorPool, &sys->threadMtx, dependedJobs, dependCount);
	if(node->targetDepending == 0) jobNodesVec.push_back(node);
	return JobHandle(node);
}


template <typename Func>
constexpr void JobNode::Create(const Func& func, VectorPool* vectorPool, std::mutex* threadMtx, JobHandle* dependedJobs, uint dependCount)
{
	targetDepending = dependCount;
	this->threadMtx = threadMtx;
	this->vectorPool = vectorPool;
	dependingEvent = vectorPool->New();
	if (sizeof(Func) >= sizeof(FuncStorage))	//Create in heap
	{
		assert(false);
		ptr = new Func{
			std::move((Func&)func)
		};
		destructorFunc = [](void* currPtr)->void
		{
			delete ((Func*)currPtr);
		};
	}
	else
	{
		ptr = &stackArr;
		new (ptr)Func{
			std::move((Func&)func)
		};
		destructorFunc = [](void* currPtr)->void
		{
			((Func*)currPtr)->~Func();
		};
	}
	executeFunc = [](void* currPtr)->void
	{
		(*(Func*)currPtr)();
	};
	for (uint i = 0; i < dependCount; ++i)
	{
		dependedJobs[i].node->dependingEvent->push_back(this);
	}
}