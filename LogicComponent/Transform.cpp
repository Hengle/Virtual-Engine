#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include "Transform.h"
#include "World.h"
#include "../Common/MetaLib.h"
#include "../Common/RandomVector.h"
#include "../Common/Runnable.h"
#include "../CJsonObject/CJsonObject.hpp"
RandomVector<TransformData> Transform::randVec(100);
using namespace DirectX;
using namespace neb;
void Transform::SetRotation(const Math::Vector4& quaternion)
{
	XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(quaternion);
	rotationMatrix = XMMatrixTranspose(rotationMatrix);
	TransformData& data = randVec[vectorPos];
	XMStoreFloat3(&data.right, rotationMatrix.r[0]);
	XMStoreFloat3(&data.up, rotationMatrix.r[1]);
	XMStoreFloat3(&data.forward, rotationMatrix.r[2]);
}
void Transform::SetPosition(const XMFLOAT3& position)
{
	randVec[vectorPos].position = position;
}
Transform::Transform(const CJsonObject& jsonObj, ObjectPtr<Transform>& targetPtr) : Transform(targetPtr)
{
	std::string s;
	if (jsonObj.Get("position", s))
	{
		float3 pos;
		ReadStringToVector<float3>(s.data(), s.length(), &pos);
		SetPosition(pos);
	}
	if (jsonObj.Get("rotation", s))
	{
		float4 rot;
		ReadStringToVector<float4>(s.data(), s.length(), &rot);
		SetRotation(rot);
	}
	if (jsonObj.Get("localscale", s))
	{
		float3 scale;
		ReadStringToVector<float3>(s.data(), s.length(), &scale);
		SetLocalScale(scale);
	}
}

Transform::Transform(ObjectPtr<Transform>& ptr)
{
	World* world = World::GetInstance();
	if (world != nullptr)
	{
		std::lock_guard<std::mutex> lck(world->mtx);
		ptr = ObjectPtr<Transform>::MakePtr(this);
		world->allTransformsPtr.Add(ptr, (uint*)&worldIndex);
	}
	else
	{
		worldIndex = -1;
	}
	randVec.Add(
		{
			{0,1,0},
			{0,0,1},
			{1,0,0},
			{1,1,1},
			{0,0,0}
		},
		&vectorPos
		);
}

ObjectPtr<Transform> Transform::GetTransform()
{
	ObjectPtr<Transform> objPtr;
	new Transform(objPtr);
	return objPtr;
}
ObjectPtr<Transform> Transform::GetTransform(const neb::CJsonObject& path)
{
	ObjectPtr<Transform> objPtr;
	new Transform(path, objPtr);
	return objPtr;
}

void Transform::SetLocalScale(const XMFLOAT3& localScale)
{
	randVec[vectorPos].localScale = localScale;
}

Math::Matrix4 Transform::GetLocalToWorldMatrix()
{
	Math::Matrix4 target;
	TransformData& data = randVec[vectorPos];
	XMVECTOR vec = XMLoadFloat3(&data.right);
	vec *= data.localScale.x;
	target[0] = vec;
	vec = XMLoadFloat3(&data.up);
	vec *= data.localScale.y;
	target[1] = vec;
	vec = XMLoadFloat3(&data.forward);
	vec *= data.localScale.z;
	target[2] = vec;
	target[3] = { data.position.x, data.position.y, data.position.z, 1 };
	return target;
}

Math::Matrix4 Transform::GetWorldToLocalMatrix()
{
	TransformData& data = randVec[vectorPos];
	return GetInverseTransformMatrix(
	(Math::Vector3)data.right * data.localScale.x,
		(Math::Vector3)data.up * data.localScale.y,
		(Math::Vector3)data.forward * data.localScale.z,
		data.position);
}

void Transform::Dispose()
{
	for (uint i = 0; i < allComponents.Length(); ++i)
	{
		if (allComponents[i])
		{
			allComponents[i].Destroy();
		}
	}
	allComponents.Clear();
}

Transform::~Transform()
{
	if (allComponents.Length() > 0)
		throw "Components Not Disposed!";
	World* world = World::GetInstance();
	if (world && worldIndex >= 0)
	{
		std::lock_guard<std::mutex> lck(world->mtx);
		world->allTransformsPtr.Remove(worldIndex);
	}
}

void Transform::DisposeTransform(ObjectPtr<Transform>& trans)
{
	trans->Dispose();
	trans.Destroy();
}