#pragma once
template <class T>
class Runnable;
#define RUNNABLE_FUNC_SIZE 128
template<class _Ret,
	class... _Types>
	class Runnable<_Ret(_Types...)>
{
private:
	alignas(sizeof(int) * 4) char placePtr[RUNNABLE_FUNC_SIZE];
	_Ret(*funcPtr)(void*, _Types...);
	void(*disposeFunc)(void*);
	void(*constructFunc)(void*, void*) = nullptr;
public:
	bool operator==(const Runnable<_Ret(_Types...)>& obj)
	{
		return funcPtr == obj.funcPtr;
	}
	bool operator!=(const Runnable<_Ret(_Types...)>& obj)
	{
		return funcPtr != obj.funcPtr;
	}

	Runnable() :
		funcPtr(nullptr),
		disposeFunc(nullptr),
		constructFunc(nullptr)
	{
	}

	Runnable(std::nullptr_t) :
		funcPtr(nullptr),
		disposeFunc(nullptr),
		constructFunc(nullptr)
	{
	}

	Runnable(_Ret(*p)(_Types...)) :
		disposeFunc(nullptr)
	{
		constructFunc = [](void* dest, void* source)->void
		{
			*(size_t*)dest = *(size_t*)source;
		};
		memcpy(placePtr, &p, sizeof(size_t));
		funcPtr = [](void* pp, _Types... tt)->_Ret
		{
			_Ret(*fp)(_Types...) = (_Ret(*)(_Types...))(*(void**)pp);
			return fp(tt...);
		};
	}

	template <typename Functor>
	Runnable(const Functor& f)
	{
		//Functor should be less than 128 byte and have less align than int4, float4
		static_assert((sizeof(Functor) <= RUNNABLE_FUNC_SIZE) && alignof(Functor) <= (sizeof(int) * 4));
		new (placePtr) Functor{ (Functor&)f };
		constructFunc = [](void* dest, void* source)->void
		{
			new (dest)Functor(*(Functor*)source);
		};
		funcPtr = [](void* pp, _Types... tt)->_Ret
		{
			Functor* ff = (Functor*)pp;
			return (*ff)(tt...);
		};
		disposeFunc = [](void* pp)->void
		{
			Functor* ff = (Functor*)pp;
			ff->~Functor();
		};
	}

	Runnable(const Runnable<_Ret(_Types...)>& f) :
		funcPtr(f.funcPtr),
		constructFunc(f.constructFunc),
		disposeFunc(f.disposeFunc)
	{
		if (constructFunc)
		{
			constructFunc(placePtr, (char*)f.placePtr);
		}
	}

	void operator=(const Runnable<_Ret(_Types...)>& f)
	{
		if (disposeFunc) disposeFunc(placePtr);
		funcPtr = f.funcPtr;
		constructFunc = f.constructFunc;
		disposeFunc = f.disposeFunc;
		if (constructFunc)
		{
			constructFunc(placePtr, (char*)f.placePtr);
		}
	}

	void operator=(_Ret(*p)(_Types...))
	{
		if (disposeFunc) disposeFunc(placePtr);
		disposeFunc = nullptr;
		constructFunc = [](void* dest, void* source)->void
		{
			*(size_t*)dest = *(size_t*)source;
		};
		memcpy(placePtr, &p, sizeof(size_t));
		funcPtr = [](void* pp, _Types... tt)->_Ret
		{
			_Ret(*fp)(_Types...) = (_Ret(*)(_Types...))(*(void**)pp);
			return fp(tt...);
		};
	}

	template <typename Functor>
	void operator=(const Functor& f)
	{
		if (disposeFunc) disposeFunc(placePtr);
		static_assert((sizeof(Functor) <= RUNNABLE_FUNC_SIZE) && alignof(Functor) <= (sizeof(int) * 4));
		new (placePtr) Functor{ (Functor&)f };
		constructFunc = [](void* dest, void* source)->void
		{
			new (dest)Functor(*(Functor*)source);
		};
		funcPtr = [](void* pp, _Types... tt)->_Ret
		{
			Functor* ff = (Functor*)pp;
			return (*ff)(tt...);
		};
		disposeFunc = [](void* pp)->void
		{
			Functor* ff = (Functor*)pp;
			ff->~Functor();
		};
	}

	_Ret operator()(_Types... t)
	{
		return funcPtr(placePtr, t...);
	}
	bool isAvaliable()
	{
		return funcPtr;
	}
	~Runnable()
	{
		if (disposeFunc) disposeFunc(placePtr);
	}
};
#undef RUNNABLE_FUNC_SIZE

template <class T>
class DestructGuard
{
private:
	T t;
public:
	DestructGuard(const T& tt) : t(tt)
	{
	}
	~DestructGuard()
	{
		t();
	}
};