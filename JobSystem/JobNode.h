#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "ConcurrentQueue.h"
#include "../Common/Pool.h"
#include <DirectXMath.h>
class JobHandle;
class JobThreadRunnable;
class JobBucket;
class VectorPool;
typedef unsigned int uint;
class JobNode
{
	friend class JobBucket;
	friend class JobSystem;
	friend class JobHandle;
	friend class JobThreadRunnable;
private:
	struct FuncStorage
	{
		__m128 arr[16];
	};
	JobBucket* bucket;
	std::atomic<unsigned int> targetDepending;
	std::vector<JobNode*>* dependingEvent = nullptr;
	FuncStorage stackArr;
	void* ptr = nullptr;
	void(*destructorFunc)(void*) = nullptr;
	void(*executeFunc)(void*);
	VectorPool* vectorPool = nullptr;
	std::mutex* threadMtx;
	template <typename Func>
	constexpr void Create(const Func& func, VectorPool* vectorPool, std::mutex* threadMtx, JobHandle* dependedJobs, uint dependCount);
	JobNode* Execute(ConcurrentQueue<JobNode*>& taskList, std::condition_variable& cv);
public:

	~JobNode();
};