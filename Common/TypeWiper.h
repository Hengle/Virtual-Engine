#pragma once
struct FunctorData
{
	void(*constructor)(const void*, const void*);//arg0: placement ptr  arg1: copy source
	void(*disposer)(void*);
	void(*run)(void*);
};
template <typename T>
FunctorData GetFunctor()
{
	FunctorData data;
	data.constructor = [](const void* place, const void* source)->void
	{
		new (place)T{ *((T*)source) };
	};
	data.disposer = [](void* ptr)->void
	{
		((T*)ptr)->~T();
	};
	data.run = [](void* ptr)->void
	{
		(*((T*)ptr))();
	};
	return data;
}