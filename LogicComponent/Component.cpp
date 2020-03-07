#include "Component.h"
#include "Transform.h"
#include "../Common/RandomVector.h"
Component::Component(const ObjectPtr<Transform>& trans, ObjectPtr<Component>& ptr)
	: MObject(), transform(trans)
{
	assert(trans);
	ptr = ObjectPtr<Component>::MakePtr(this);
	if (trans)
	{
		trans->allComponents.Add(ptr, &componentIndex);
	}
}

Component::~Component()
{
	if (transform)
	{
		transform->allComponents.Remove(componentIndex);
	}
}