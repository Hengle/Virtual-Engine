#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "ConcurrentQueue.h"
#include "../Common/Pool.h"
#include <DirectXMath.h>
#include "../Common/TypeWiper.h"
#include "../Common/MetaLib.h"
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
	inline static const size_t STORAGE_SIZE = 16 * sizeof(__m128);
	struct FuncStorage
	{
		__m128 arr[16];
	};
	std::atomic<unsigned int> targetDepending;
	StackObject<std::vector<JobNode*>> dependingEvent;
	bool dependedEventInitialized = false;
	FuncStorage stackArr;
	void* ptr;
	void(*destructorFunc)(void*) = nullptr;
	void(*executeFunc)(void*);
	std::mutex* threadMtx;

	void Create(const FunctorData& funcData, void* funcPtr, std::mutex* threadMtx, JobHandle* dependedJobs, uint dependCount);
	JobNode* Execute(ConcurrentQueue<JobNode*>& taskList, std::condition_variable& cv);
public:
	void Reset();
	void Dispose();
	~JobNode();
};