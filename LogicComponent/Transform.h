#pragma once
#include <DirectXMath.h>
#include "../Common/MObject.h"
#include "../LogicComponent/Component.h"
#include "../Common/RandomVector.h"
class World;
class AssetReference;
namespace neb
{
	class CJsonObject;
}
struct TransformData
{
	DirectX::XMFLOAT3 up;
	DirectX::XMFLOAT3 forward;
	DirectX::XMFLOAT3 right;
	DirectX::XMFLOAT3 localScale;
	DirectX::XMFLOAT3 position;
};
class Transform : public MObject
{
	friend class Scene;
	friend class Component;
	friend class AssetReference;
private:

	static RandomVector<TransformData> randVec;
	RandomVector<ObjectPtr<Component>> allComponents;
	int worldIndex;
	UINT vectorPos;
	Transform(const neb::CJsonObject& path, ObjectPtr<Transform>& targetPtr);
	Transform(ObjectPtr<Transform>& targetPtr);
	void Dispose();
public:
	uint GetComponentCount() const
	{
		return allComponents.Length();
	}
	static ObjectPtr<Transform> GetTransform();
	static ObjectPtr<Transform> GetTransform(const neb::CJsonObject& path);
	DirectX::XMFLOAT3 GetPosition() const { return randVec[vectorPos].position; }
	DirectX::XMFLOAT3 GetForward() const { return randVec[vectorPos].forward; }
	DirectX::XMFLOAT3 GetRight() const { return randVec[vectorPos].right; }
	DirectX::XMFLOAT3 GetUp() const { return randVec[vectorPos].up; }
	DirectX::XMFLOAT3 GetLocalScale() const { return randVec[vectorPos].localScale; }
	void SetRotation(const Math::Vector4& quaternion);
	void SetPosition(const DirectX::XMFLOAT3& position);
	void SetLocalScale(const DirectX::XMFLOAT3& localScale);
	Math::Matrix4 GetLocalToWorldMatrix();
	Math::Matrix4 GetWorldToLocalMatrix();
	~Transform();
	static void DisposeTransform(ObjectPtr<Transform>& trans);
};
