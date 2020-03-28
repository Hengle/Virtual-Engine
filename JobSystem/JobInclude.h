#pragma once
#include "JobSystem.h"
#include "JobBucket.h"
#include "JobNode.h"

template <typename Func>
constexpr JobHandle JobBucket::GetTask(JobHandle* dependedJobs, unsigned int dependCount, const Func& func)
{
	static_assert(sizeof(Func) <= JobNode::STORAGE_SIZE);
	static_assert(alignof(Func) <= alignof(__m128));
	FunctorData funcData = GetFunctor<Func>();
	return GetTask(dependedJobs, dependCount, funcData, (Func*)(&func));
}