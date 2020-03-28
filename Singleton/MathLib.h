#pragma once
#include "../Common/d3dUtil.h"

const float Deg2Rad = 0.0174532924;
const float Rad2Deg = 57.29578;
struct Cone
{
	DirectX::XMFLOAT3 vertex;
	float height;
	DirectX::XMFLOAT3 direction;
	float radius;
	Cone(DirectX::XMFLOAT3& position, float distance, DirectX::XMFLOAT3& direction, float angle) :
		vertex(position),
		height(distance),
		direction(direction)
	{
		radius = tan(angle * 0.5) * height;
	}
};
class MathLib final
{
public:
	MathLib() = delete;
	~MathLib() = delete;
	static Math::Vector4 GetPlane(
		const Math::Vector3& normal,
		const Math::Vector3& inPoint);
	static Math::Vector4 GetPlane(
		const Math::Vector3& a,
		const Math::Vector3& b,
		const Math::Vector3& c);
	static bool BoxIntersect(
		const Math::Matrix4& localToWorldMatrix,
		Math::Vector4* planes,
		const Math::Vector3& position,
		const Math::Vector4& localExtent);
	static void GetCameraNearPlanePoints(
		const Math::Matrix4& localToWorldMatrix,
		double fov,
		double aspect,
		double distance,
		Math::Vector3* corners
		);

	static void GetCameraNearPlanePoints(
		const Math::Vector3& right,
		const Math::Vector3& up,
		const Math::Vector3& forward,
		const Math::Vector3& position,
		double fov,
		double aspect,
		double distance,
		Math::Vector3* corners
		);

	static void GetPerspFrustumPlanes(
		const Math::Matrix4& localToWorldMatrix,
		double fov,
		double aspect,
		double nearPlane,
		double farPlane,
		DirectX::XMFLOAT4* frustumPlanes
		);
	static void GetPerspFrustumPlanes(
		const Math::Matrix4& localToWorldMatrix,
		double fov,
		double aspect,
		double nearPlane,
		double farPlane,
		Math::Vector4* frustumPlanes
		);
	static void GetFrustumBoundingBox(
		const Math::Matrix4& localToWorldMatrix,
		double nearWindowHeight,
		double farWindowHeight,
		double aspect,
		double nearZ,
		double farZ,
		Math::Vector3* minValue,
		Math::Vector3* maxValue
		);

	static float GetDistanceToPlane(
		const Math::Vector4& plane,
		const Math::Vector4& point)
	{
		Math::Vector3 dotValue = DirectX::XMVector3Dot(plane, point);
		return ((DirectX::XMFLOAT4*) & dotValue)->x + ((DirectX::XMFLOAT4*) & point)->w;
	}
	static bool ConeIntersect(const Cone& cone, const Math::Vector4& plane);
	static void GetOrthoCamFrustumPlanes(
		const Math::Vector3& right,
		const Math::Vector3& up,
		const Math::Vector3& forward,
		const Math::Vector3& position,
		float xSize,
		float ySize,
		float nearPlane,
		float farPlane,
		Math::Vector4* results);
	static void GetOrthoCamFrustumPoints(
		const Math::Vector3& right,
		const Math::Vector3& up,
		const Math::Vector3& forward,
		const Math::Vector3& position,
		float xSize,
		float ySize,
		float nearPlane,
		float farPlane,
		Math::Vector3* results);

	static double DistanceToCube(const Math::Vector3& size, const Math::Vector3& quadToTarget);
	static double DistanceToQuad(double size, float2 quadToTarget);
	static bool BoxIntersect(const Math::Vector3& position, const Math::Vector3& extent, Math::Vector4* planes, int len);
	static bool BoxContactWithBox(double3 min0, double3 max0, double3 min1, double3 max1);
	static Math::Vector4 CameraSpacePlane(const Math::Matrix4& worldToCameraMatrix, const Math::Vector3& pos, const Math::Vector3& normal, float clipPlaneOffset);
	static Math::Matrix4 CalculateObliqueMatrix(const Math::Vector4& clipPlane, const Math::Matrix4& projection);
};