#pragma once
#include <type_traits>
#include <vector>
#include <atomic>
#include <mutex>
#include "MetaLib.h"

template <typename T>
class Pool
{
private:
	std::vector<T*> allPtrs;
	std::vector<void*> allocatedPtrs;
	int capacity;
	constexpr void AllocateMemory()
	{
		using StorageT = Storage<T, 1>;
		StorageT* ptr = reinterpret_cast<StorageT*>(malloc(sizeof(StorageT) * capacity));
		for (int i = 0; i < capacity; ++i)
		{
			allPtrs.push_back(reinterpret_cast<T*>(ptr + i));
		}
		allocatedPtrs.push_back(ptr);
	}
public:
	constexpr Pool(int capa) : capacity(capa)
	{
		allPtrs.reserve(capa);
		allocatedPtrs.reserve(10);
		AllocateMemory();
	}

	template <typename... Args>
	constexpr T* New(Args&&... args)
	{
		if (allPtrs.size() <= 0)
			AllocateMemory();
		T* value = allPtrs[allPtrs.size() - 1];
		allPtrs.erase(allPtrs.end() - 1);
		new (value)T(std::forward<Args>(args)...);
		return value;
	}

	constexpr void Delete(T* ptr)
	{
		ptr->~T();
		allPtrs.push_back(ptr);
	}

	~Pool()
	{
		for (int i = 0; i < allocatedPtrs.size(); ++i)
		{
			free(allocatedPtrs[i]);
		}
	}
};

template <typename T>
class ConcurrentPool
{
private:
	typedef Storage<T, 1> StorageT;
	struct Array
	{
		StorageT** objs;
		std::atomic<int64_t> count;
		int64_t capacity;
	};

	Array unusedObjects[2];
	std::mutex mtx;
	bool objectSwitcher = true;
public:
	constexpr void UpdateSwitcher()
	{
		if (unusedObjects[objectSwitcher].count < 0) unusedObjects[objectSwitcher].count = 0;
		objectSwitcher = !objectSwitcher;
	}

	constexpr void Delete(T* targetPtr)
	{
		targetPtr->~T();
		Array* arr = unusedObjects + !objectSwitcher;
		int64_t currentCount = arr->count++;
		if (currentCount >= arr->capacity)
		{
			std::lock_guard<std::mutex> lck(mtx);
			if (currentCount >= arr->capacity)
			{
				int64_t newCapacity = arr->capacity * 2;
				StorageT** newArray = new StorageT * [newCapacity];
				memcpy(newArray, arr->objs, sizeof(StorageT*) * arr->capacity);
				delete arr->objs;
				arr->objs = newArray;
				arr->capacity = newCapacity;
			}
		}
		arr->objs[currentCount] = (StorageT*)targetPtr;
	}
	template <typename ... Args>
	constexpr T* New(Args&&... args)
	{
		Array* arr = unusedObjects + objectSwitcher;
		int64_t currentCount = --arr->count;
		T* t;
		if (currentCount >= 0)
		{
			t = (T*)arr->objs[currentCount];

		}
		else
		{
			t = (T*)malloc(sizeof(StorageT));
		}
		new (t)T(std::forward<Args>(args)...);
		return t;
	}

	constexpr ConcurrentPool(unsigned int initCapacity)
	{
		if (initCapacity < 3) initCapacity = 3;
		unusedObjects[0].objs = new StorageT * [initCapacity];
		unusedObjects[0].capacity = initCapacity;
		unusedObjects[0].count = initCapacity / 2;
		for (unsigned int i = 0; i < unusedObjects[0].count; ++i)
		{
			unusedObjects[0].objs[i] = (StorageT*)malloc(sizeof(StorageT));
		}
		unusedObjects[1].objs = new StorageT * [initCapacity];
		unusedObjects[1].capacity = initCapacity;
		unusedObjects[1].count = initCapacity / 2;
		for (unsigned int i = 0; i < unusedObjects[1].count; ++i)
		{
			unusedObjects[1].objs[i] = (StorageT*)malloc(sizeof(StorageT));
		}
	}
	~ConcurrentPool()
	{
		for (int64_t i = 0; i < unusedObjects[0].count; ++i)
		{
			delete unusedObjects[0].objs[i];

		}
		delete unusedObjects[0].objs;
		for (int64_t i = 0; i < unusedObjects[1].count; ++i)
		{
			delete unusedObjects[1].objs[i];

		}
		delete unusedObjects[1].objs;
	}
};

typedef uint32_t uint;
template <typename T>
class JobPool
{
private:
	std::vector<T*> allocatedPool;
	std::vector<T*> list[2];
	std::mutex mtx;
	bool switcher = false;
	uint capacity;
	void ReserveList(std::vector<T*>& vec)
	{
		T* t = new T[capacity];
		allocatedPool.push_back(t);
		for (uint i = 0; i < capacity; ++i)
		{
			vec[i] = t + i;
		}
	}
public:
	JobPool(uint capacity) : capacity(capacity)
	{
		allocatedPool.reserve(10);
		list[0].reserve(capacity * 2);
		list[0].resize(capacity);
		list[1].reserve(capacity * 2);
		list[1].resize(capacity);
		ReserveList(list[0]);
		ReserveList(list[1]);
	}

	void UpdateSwitcher() {
		switcher = !switcher;
	}

	T* New()
	{
		std::vector<T*>& lst = list[switcher];
		if (lst.empty()) ReserveList(lst);
		auto ite = lst.end() - 1;
		T* value = *ite;
		value->Reset();
		lst.erase(ite);
		return value;
	}

	void Delete(T* value)
	{
		std::vector<T*>& lst = list[!switcher];
		value->Dispose();
		std::lock_guard<std::mutex> lck(mtx);
		lst.push_back(value);
	}

	~JobPool()
	{
		for (auto ite = allocatedPool.begin(); ite != allocatedPool.end(); ++ite)
		{
			delete[] * ite;
		}
	}
};