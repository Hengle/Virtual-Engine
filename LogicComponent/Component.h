#pragma once
#include "../Common/MObject.h"
#include "../Common/d3dUtil.h"
class Transform;
class Component : public MObject
{
	friend class Transform;
private:
	UINT componentIndex;
	bool enabled = false;
protected:
	Component(const ObjectPtr<Transform>& trans, ObjectPtr<Component>& ptr);
public:
	ObjectPtr<Transform> transform;
	bool GetEnabled() const
	{
		return enabled;
	}
	void SetEnabled(bool enabled)
	{
		if (this->enabled == enabled) return;
		this->enabled = enabled;
		if (enabled) OnEnable();
		else OnDisable();
	}
	virtual ~Component();
	virtual void Update() {}
	virtual void OnEnable() {}
	virtual void OnDisable() {}
	template <typename T>
	static ObjectPtr<T> GetComponent(const ObjectPtr<Transform>& trans)
	{
		ObjectPtr<Component> compPtr;
		new T(trans, compPtr);
		return compPtr.CastTo<T>();
	}
};

template <typename T>
struct ComponentAllocator;