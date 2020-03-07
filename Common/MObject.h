#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include "Runnable.h"
#include "d3dUtil.h"
class PtrLink;

class MObject
{
	friend class PtrLink;
private:
	std::vector<Runnable<void(MObject*)>> disposeFunc;
	static std::atomic<unsigned int> CurrentID;
	unsigned int instanceID;
protected:
	MObject()
	{
		instanceID = CurrentID++;
	}
public:
	void AddEventBeforeDispose(const Runnable<void(MObject*)>& func)
	{
		if (disposeFunc.capacity() == 0) disposeFunc.reserve(20);
		disposeFunc.push_back(func);
	}
	void RemoveEventBeforeDispose(const Runnable<void(MObject*)>& func)
	{
		for (auto ite = disposeFunc.begin(); ite != disposeFunc.end(); ++ite)
		{
			if (*ite == func)
			{
				disposeFunc.erase(ite);
				return;
			}
		}
	}
	unsigned int GetInstanceID() const { return instanceID; }
	virtual ~MObject() noexcept;
};



struct LinkHeap
{
	MObject* ptr;
	std::atomic<int> refCount;
	std::atomic<int> looseRefCount;
	static bool initialized;
	static std::vector<LinkHeap*> heapPtrs;
	static std::mutex mtx;
	static void Resize() noexcept;
	static void Initialize() noexcept;
	static LinkHeap* GetHeap(MObject* mobj, size_t refCount) noexcept;
	static void ReturnHeap(LinkHeap* value) noexcept;
};

class CrateApp;
class PtrWeakLink;
class PtrLink
{
	friend class CrateApp;
	friend class PtrWeakLink;
	static bool globalEnabled;
public:
	LinkHeap* heapPtr;
	PtrLink() noexcept : heapPtr(nullptr)
	{

	}
	void Dispose() noexcept;
	PtrLink(MObject* obj) noexcept
	{
		heapPtr = LinkHeap::GetHeap(obj, 1);
	}
	PtrLink(const PtrLink& p) noexcept
	{
		if (p.heapPtr) {
			p.heapPtr->refCount.fetch_add(1, std::memory_order_relaxed);
			p.heapPtr->looseRefCount.fetch_add(1, std::memory_order_relaxed);
		}
		heapPtr = p.heapPtr;
	}
	void operator=(const PtrLink& p) noexcept
	{
		if (p.heapPtr) {
			p.heapPtr->refCount.fetch_add(1, std::memory_order_relaxed);
			p.heapPtr->looseRefCount.fetch_add(1, std::memory_order_relaxed);
		}
		Dispose();
		heapPtr = p.heapPtr;
	}
	void Destroy() noexcept;
	~PtrLink() noexcept
	{
		Dispose();
	}
};
class PtrWeakLink
{
public:
	LinkHeap* heapPtr;
	PtrWeakLink() noexcept : heapPtr(nullptr)
	{

	}

	void Dispose() noexcept;
	PtrWeakLink(const PtrLink& p) noexcept
	{
		if (p.heapPtr) {
			p.heapPtr->looseRefCount.fetch_add(1, std::memory_order_relaxed);
		}
		heapPtr = p.heapPtr;
	}

	PtrWeakLink(const PtrWeakLink& p) noexcept
	{
		if (p.heapPtr) {
			p.heapPtr->looseRefCount.fetch_add(1, std::memory_order_relaxed);
		}
		heapPtr = p.heapPtr;
	}

	void operator=(const PtrLink& p) noexcept
	{
		if (p.heapPtr) {
			p.heapPtr->looseRefCount.fetch_add(1, std::memory_order_relaxed);
		}
		Dispose();
		heapPtr = p.heapPtr;
	}

	void operator=(const PtrWeakLink& p) noexcept
	{
		if (p.heapPtr) {
			p.heapPtr->looseRefCount.fetch_add(1, std::memory_order_relaxed);
		}
		Dispose();
		heapPtr = p.heapPtr;
	}
	~PtrWeakLink() noexcept
	{
		Dispose();
	}
};

template <typename T>
class ObjWeakPtr;
template <typename T>
class ObjectPtr
{
private:
	friend class ObjWeakPtr<T>;
	PtrLink link;
	inline constexpr ObjectPtr(T* ptr) noexcept :
		link(ptr)
	{

	}
public:
	constexpr ObjectPtr(const PtrLink& link) noexcept : link(link)
	{
	}
	inline constexpr ObjectPtr() noexcept :
		link() {}
	inline constexpr ObjectPtr(std::nullptr_t) noexcept : link()
	{

	}
	inline constexpr ObjectPtr(const ObjectPtr<T>& ptr) noexcept :
		link(ptr.link)
	{

	}
	static ObjectPtr<T> MakePtr(T* ptr) noexcept
	{
		return ObjectPtr<T>(ptr);
	}


	inline constexpr operator bool() const noexcept
	{
		return link.heapPtr != nullptr && link.heapPtr->ptr != nullptr;
	}

	inline constexpr operator T* () const noexcept
	{
		return (T*)link.heapPtr->ptr;
	}

	inline constexpr void Destroy() noexcept
	{
		link.Destroy();
	}

	template<typename F>
	inline constexpr ObjectPtr<F> CastTo() const noexcept
	{
		return ObjectPtr<F>(link);
	}

	inline constexpr void operator=(const ObjectPtr<T>& other) noexcept
	{
		link = other.link;
	}

	inline constexpr void operator=(T* other) noexcept = delete;
	inline constexpr void operator=(void* other) noexcept = delete;
	inline constexpr void operator=(std::nullptr_t t) noexcept
	{
		link.Dispose();
	}

	inline constexpr T* operator->() const noexcept
	{
		return (T*)link.heapPtr->ptr;
	}

	inline constexpr T& operator*() noexcept
	{
		return *(T*)link.heapPtr->ptr;
	}

	inline constexpr bool operator==(const ObjectPtr<T>& ptr) const noexcept
	{
		return link.heapPtr == ptr.link.heapPtr;
	}
	inline constexpr bool operator!=(const ObjectPtr<T>& ptr) const noexcept
	{
		return link.heapPtr != ptr.link.heapPtr;
	}
};

template <typename T>
class ObjWeakPtr
{
private:
	PtrWeakLink link;
public:
	inline constexpr ObjWeakPtr() noexcept :
		link() {}
	inline constexpr ObjWeakPtr(std::nullptr_t) noexcept : link()
	{

	}
	inline constexpr ObjWeakPtr(const ObjWeakPtr<T>& ptr) noexcept :
		link(ptr.link)
	{

	}
	inline constexpr ObjWeakPtr(const ObjectPtr<T>& ptr) noexcept :
		link(ptr.link)
	{

	}

	inline constexpr operator bool() const noexcept
	{
		return link.heapPtr != nullptr && link.heapPtr->ptr != nullptr;
	}

	inline constexpr operator T* () const noexcept
	{
		return (T*)link.heapPtr->ptr;
	}

	template<typename F>
	inline constexpr ObjWeakPtr<F> CastTo() const noexcept
	{
		return ObjWeakPtr<F>(link);
	}

	inline constexpr void operator=(const ObjWeakPtr<T>& other) noexcept
	{
		link = other.link;
	}

	inline constexpr void operator=(const ObjectPtr<T>& other) noexcept
	{
		link = other.link;
	}

	inline constexpr void operator=(T* other) noexcept = delete;
	inline constexpr void operator=(void* other) noexcept = delete;
	inline constexpr void operator=(std::nullptr_t t) noexcept
	{
		link.Dispose();
	}

	inline constexpr T* operator->() const noexcept
	{
		return (T*)link.heapPtr->ptr;
	}

	inline constexpr T& operator*() noexcept
	{
		return *(T*)link.heapPtr->ptr;
	}

	inline constexpr bool operator==(const ObjWeakPtr<T>& ptr) const noexcept
	{
		return link.heapPtr == ptr.link.heapPtr;
	}
	inline constexpr bool operator!=(const ObjWeakPtr<T>& ptr) const noexcept
	{
		return link.heapPtr != ptr.link.heapPtr;
	}
};
