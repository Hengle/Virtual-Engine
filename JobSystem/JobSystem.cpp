#include "JobSystem.h"
#include <thread>
#include "ConcurrentQueue.h"
#include <condition_variable>
#include <atomic>
#include "JobBucket.h"
#include "JobNode.h"
void JobSystem::UpdateNewBucket()
{
START:
	if (currentBucketPos >= buckets.size())
	{
		{
			mainThreadFinished = true;
			std::lock_guard<std::mutex> lck(mainThreadWaitMutex);
			mainThreadFinished = true;
			mainThreadWaitCV.notify_all();
		}
		return;
	}

	JobBucket* bucket = buckets[currentBucketPos];
	if (bucket->jobNodesVec.empty() || bucket->sys != this)
	{
		currentBucketPos++;
		goto START;
	}
	bucketMissionCount = bucket->allJobNodeCount;
	executingNode.ResizeAndClear(bucket->allJobNodeCount);
	for (size_t i = 0; i < bucket->jobNodesVec.size(); ++i)
	{
		JobNode* node = bucket->jobNodesVec[i];
		if (node->targetDepending == 0)
		{
			executingNode.Push(node);

		}
	}
	bucket->jobNodesVec.clear();
	bucket->allJobNodeCount = 0;
	currentBucketPos++;
	UINT size = executingNode.GetSize();
	if (executingNode.GetSize() < mThreadCount) {
		for (int64_t i = 0; i < executingNode.GetSize(); ++i) {
			std::lock_guard<std::mutex> lck(threadMtx);
			cv.notify_one();
		}
	}
	else
	{
		std::lock_guard<std::mutex> lck(threadMtx);
		cv.notify_all();
	}
}

class JobThreadRunnable
{
public:
	JobSystem* sys;
	/*bool* JobSystemInitialized;
	std::condition_variable* cv;
	ConcurrentQueue<JobNode*>* executingNode;
	std::atomic<int>* bucketMissionCount;*/
	void operator()()
	{
		while (!sys->jobSystemStart && sys->JobSystemInitialized)
		{
			std::unique_lock<std::mutex> lck(sys->threadMtx);
			sys->cv.wait(lck);
		}
		int value = (int)-1;
	MAIN_THREAD_LOOP:
		{
			JobNode* node = nullptr;
			while (sys->executingNode.TryPop(&node))
			{
			START_LOOP:
				JobNode* nextNode = node->Execute(sys->executingNode, sys->cv);
				sys->jobNodePool.Delete(node);
				value = sys->bucketMissionCount.fetch_add(-1) - 1;
				if (nextNode != nullptr)
				{
					node = nextNode;
					goto START_LOOP;
				}
				if (value <= 0)
				{
					sys->UpdateNewBucket();
				}
			}
			std::unique_lock<std::mutex> lck(sys->threadMtx);
			if (sys->JobSystemInitialized)
			{
				sys->cv.wait(lck);
				goto MAIN_THREAD_LOOP;
			}
		}
	}
};

JobBucket* JobSystem::GetJobBucket()
{
	if (releasedBuckets.empty())
	{
		JobBucket* bucket = new JobBucket(this);
		usedBuckets.push_back(bucket);
		return bucket;
	}
	else
	{
		auto&& ite = releasedBuckets.end() - 1;
		JobBucket* cur = *ite;
		cur->jobNodesVec.clear();
		releasedBuckets.erase(ite);
		return cur;
	}
}
void JobSystem::ReleaseJobBucket(JobBucket* node)
{
	node->jobNodesVec.clear();
	releasedBuckets.push_back(node);
}

JobSystem::JobSystem(int threadCount)  noexcept :
	executingNode(100),
	mainThreadFinished(true),
	jobNodePool(50)
{
	mThreadCount = threadCount;
	usedBuckets.reserve(20);
	releasedBuckets.reserve(20);
	allThreads.resize(threadCount);
	for (int i = 0; i < threadCount; ++i)
	{
		JobThreadRunnable j;
		j.sys = this;
		allThreads[i] = new std::thread(j);
	}
}

JobSystem::~JobSystem() noexcept
{
	JobSystemInitialized = false;
	{
		std::lock_guard<std::mutex> lck(threadMtx);
		cv.notify_all();
	}
	for (int i = 0; i < allThreads.size(); ++i)
	{
		allThreads[i]->join();
		delete allThreads[i];
	}
	for (auto ite = usedBuckets.begin(); ite != usedBuckets.end(); ++ite)
	{
		delete *ite;
	}
}

void JobSystem::ExecuteBucket(JobBucket** bucket, int bucketCount)
{
	jobNodePool.UpdateSwitcher();
	currentBucketPos = 0;
	buckets.resize(bucketCount);
	memcpy(buckets.data(), bucket, sizeof(JobBucket*) * bucketCount);
	mainThreadFinished = false;
	jobSystemStart = true;
	UpdateNewBucket();
}
void JobSystem::ExecuteBucket(JobBucket* bucket, int bucketCount)
{
	jobNodePool.UpdateSwitcher();
	currentBucketPos = 0;
	buckets.resize(bucketCount);
	for (int i = 0; i < bucketCount; ++i)
	{
		buckets[i] = bucket + i;
	}
	mainThreadFinished = false;
	jobSystemStart = true;
	UpdateNewBucket();
}

void JobSystem::Wait()
{
	std::unique_lock<std::mutex> lck(mainThreadWaitMutex);
	while (!mainThreadFinished)
	{
		mainThreadWaitCV.wait(lck);
	}
}