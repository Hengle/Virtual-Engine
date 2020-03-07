#include "JobHandle.h"
#include "JobNode.h"
JobHandle::JobHandle(JobNode* otherNode) : node(otherNode) {};
JobHandle::JobHandle() : node(nullptr)
{

}


JobHandle::JobHandle(const JobHandle& other)
{
	node = other.node;
}

JobHandle& JobHandle::operator=(const JobHandle& other)
{
	node = other.node;
	return *this;
}
