#include "MObject.h"
std::atomic<unsigned int> MObject::CurrentID = 0;
bool LinkHeap::initialized = false;
std::vector<LinkHeap*> LinkHeap::heapPtrs;
std::mutex LinkHeap::mtx;
bool PtrLink::globalEnabled = true;

MObject::~MObject() noexcept
{
	for (auto ite = disposeFunc.begin(); ite != disposeFunc.end(); ++ite)
	{
		(*ite)(this);
	}

}

void PtrWeakLink::Dispose() noexcept
{
	auto a = heapPtr;
	heapPtr = nullptr;
	if (a && PtrLink::globalEnabled && (--a->looseRefCount) == 0)
	{
		LinkHeap::ReturnHeap(a);
	}
}

void PtrLink::Destroy() noexcept
{
	auto bb = heapPtr;
	heapPtr = nullptr;
	if (bb && globalEnabled && bb->ptr)
	{
		auto a = bb->ptr;
		bb->ptr = nullptr;
		delete a;
	}
}

void PtrLink::Dispose() noexcept
{
	auto a = heapPtr;
	heapPtr = nullptr;
	if (a && globalEnabled &&
		(--a->refCount) == 0 &&
		a->ptr)
	{
		auto bb = a->ptr;
		a->ptr = nullptr;
		delete bb;
		if ((--a->looseRefCount) == 0)
			LinkHeap::ReturnHeap(a);
	}

}

void LinkHeap::Resize() noexcept
{
	if (heapPtrs.empty())
	{
		LinkHeap* ptrs = (LinkHeap*)malloc(sizeof(LinkHeap) * 100);
		for (uint32_t i = 0; i < 100; ++i)
		{
			heapPtrs.push_back(ptrs + i);
		}
	}
}
void  LinkHeap::Initialize() noexcept
{
	if (initialized) return;
	initialized = true;
	heapPtrs.reserve(101);
}
LinkHeap* LinkHeap::GetHeap(MObject* mobj, size_t refCount) noexcept
{
	mtx.lock();
	Initialize();
	Resize();
	auto ite = heapPtrs.end() - 1;
	LinkHeap* ptr = *ite;
	heapPtrs.erase(ite);
	mtx.unlock();
	ptr->ptr = mobj;
	ptr->refCount = refCount;
	ptr->looseRefCount = refCount;
	return ptr;
}
void  LinkHeap::ReturnHeap(LinkHeap* value) noexcept
{
	mtx.lock();
	heapPtrs.push_back(value);
	mtx.unlock();
}