#pragma once
#include <type_traits>
#include <unordered_map>
typedef unsigned int UINT;
template <typename T, UINT size>
class Storage
{
	alignas(T) char c[size * sizeof(T)];
};
template <typename T>
class Storage<T, 0>
{};

template <typename T>
class StackObject
{
private:
	Storage<T, 1> storage;
public:
	template <typename... Args>
	constexpr void New(Args&&... args) noexcept
	{
		new (&storage)T(std::forward<Args>(args)...);
	}
	template <typename... Args>
	constexpr void InPlaceNew(Args&&... args) noexcept
	{
		new (&storage)T{ std::forward<Args>(args)... };
	}
	constexpr void operator=(const StackObject<T>& value)
	{
		*(T*)&storage = *value();
	}
	constexpr void operator=(StackObject<T>&& value)
	{
		*(T*)&storage = std::move(*value);
	}
	constexpr void Delete() noexcept
	{
		((T*)&storage)->~T();
	}
	constexpr T& operator*() const noexcept
	{
		return *(T*)&storage;
	}
	constexpr T* operator->() const noexcept
	{
		return (T*)&storage;
	}
	constexpr T* GetPtr() const noexcept
	{
		return (T*)&storage;
	}
	constexpr operator T* () const noexcept
	{
		return (T*)&storage;
	}
	bool operator==(const StackObject<T>&) const = delete;
	bool operator!=(const StackObject<T>&) const = delete;
	StackObject() {}
};

template <typename F, unsigned int count>
struct LoopClass
{
	static void Do(const F& f)
	{
		LoopClass<F, count - 1>::Do(std::move(f));
		f(count);
	}
};

template <typename F>
struct LoopClass<F, 0>
{
	static void Do(const F& f)
	{
		f(0);
	}
};

template <typename F, unsigned int count>
struct LoopClassEarlyBreak
{
	static bool Do(const F& f)
	{
		if (!LoopClassEarlyBreak<F, count - 1>::Do(std::move(f))) return false;
		return f(count);
	}
};

template <typename F>
struct LoopClassEarlyBreak<F, 0>
{
	static bool Do(const F& f)
	{
		return f(0);
	}
};

template <typename F, unsigned int count>
void InnerLoop(const F& function)
{
	LoopClass<F, count - 1>::Do(function);
}

template <typename F, unsigned int count>
bool InnerLoopEarlyBreak(const F& function)
{
	return LoopClassEarlyBreak<F, count - 1>::Do(function);
}
template <typename K, typename V>
class Dictionary
{
public:
	struct KVPair
	{
		K key;
		V value;
	};
	std::unordered_map<K, UINT> keyDicts;
	std::vector<KVPair> values;
	void Reserve(UINT capacity);
	V* operator[](K& key);

	void Add(K& key, V& value);
	void Remove(K& key);

	void Clear();
};
template <typename K, typename V>
void Dictionary<K,V>::Reserve(UINT capacity)
{
	keyDicts.reserve(capacity);
	values.reserve(capacity);
}
template <typename K, typename V>
V* Dictionary<K,V>::operator[](K& key)
{
	auto&& ite = keyDicts.find(key);
	if (ite == keyDicts.end()) return nullptr;
	return &values[ite->second].value;
}
template <typename K, typename V>
void Dictionary<K, V>::Add(K& key, V& value)
{
	keyDicts.insert_or_assign(key, std::move(values.size()));
	values.push_back({ std::move(key), std::move(value) });
}
template <typename K, typename V>
void Dictionary<K, V>::Remove(K& key)
{
	auto&& ite = keyDicts.find(key);
	if (ite == keyDicts.end()) return;
	KVPair& p = values[ite->second];
	p = values[values.size() - 1];
	keyDicts[p.key] = ite->second;
	values.erase(values.end() - 1);
	keyDicts.erase(ite->first);
}
template <typename K, typename V>
void Dictionary<K, V>::Clear()
{
	keyDicts.clear();
	values.clear();
}