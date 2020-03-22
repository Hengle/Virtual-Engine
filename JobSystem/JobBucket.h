#pragma once
#include <vector>
#include "JobHandle.h"
#include "../Common/Pool.h"
class JobSystem;
class JobThreadRunnable;
class JobNode;
class JobBucket
{
	friend class JobSystem;
	friend class JobNode;
	friend class JobHandle;
	friend class JobThreadRunnable;
private:
	std::vector<JobNode*> jobNodesVec;
	unsigned int allJobNodeCount = 0;
	JobSystem* sys = nullptr;
	JobBucket(JobSystem* sys) noexcept;
	~JobBucket() noexcept{}
public:
	template <typename Func>
	constexpr JobHandle GetTask(JobHandle* dependedJobs, unsigned int dependCount, const Func& func);
};