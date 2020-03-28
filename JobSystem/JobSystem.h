#pragma once
#include "../Common/Pool.h"
#include "ConcurrentQueue.h"
#include <atomic>
class JobNode;
class JobBucket;
class JobThreadRunnable;

class JobSystem
{
	friend class JobBucket;
	friend class JobThreadRunnable;
	friend class JobNode;
private:

	std::mutex threadMtx;
	void UpdateNewBucket();
	int mThreadCount;
	JobPool<JobNode> jobNodePool;
	ConcurrentQueue<JobNode*> executingNode;
	std::vector<std::thread*> allThreads;
	std::atomic<int> bucketMissionCount;
	int currentBucketPos;
	std::vector<JobBucket*> buckets;
	std::condition_variable cv;
	std::vector<JobBucket*> usedBuckets;
	std::vector<JobBucket*> releasedBuckets;
	std::mutex mainThreadWaitMutex;
	std::condition_variable mainThreadWaitCV;
	bool mainThreadFinished;
	bool JobSystemInitialized = true;
	bool jobSystemStart = false;
public:
	JobSystem(int threadCount) noexcept;
	void ExecuteBucket(JobBucket** bucket, int bucketCount);
	void ExecuteBucket(JobBucket* bucket, int bucketCount);
	void Wait();
	~JobSystem() noexcept;
	JobBucket* GetJobBucket();
	void ReleaseJobBucket(JobBucket* node);
};

