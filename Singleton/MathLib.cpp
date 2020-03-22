#include "MathLib.h"
#include "../Common/MetaLib.h"
using namespace Math;
#define GetVec(name, v) Vector4 name = XMLoadFloat3(&##v);
#define StoreVec(ptr, v) XMStoreFloat3(ptr, v);

Vector4 MathLib::GetPlane(
	const Vector3& a,
	const Vector3& b,
	const Vector3& c)
{
	Vector3 normal = normalize(cross(b - a, c - a));
	float disVec = -dot(normal, a);
	Vector4& v = (Vector4&)normal;
	v.SetW(disVec);
	return v;
}
Vector4 MathLib::GetPlane(
	const Vector3& normal,
	const Vector3& inPoint)
{
	float dt = -dot(normal, inPoint);
	Vector4 result = normal;
	result.SetW(dt);
	return result;
}
bool MathLib::BoxIntersect(const Matrix4& localToWorldMatrix, Vector4* planes, const Vector3& position, const Vector4& localExtent)
{
	Vector4 pos = mul(localToWorldMatrix, position);
	const_cast<Vector4&>(localExtent).SetW(0);
	auto func = [&](UINT i)->bool
	{
		Vector4 plane = planes[i];
		plane.SetW(0);
		Vector4 absNormal = abs(mul(localToWorldMatrix, plane));
		float result = dot(pos, plane) - dot(absNormal, (Vector4&)localExtent);
		if (result > -plane.GetW()) return false;
	};
	return InnerLoopEarlyBreak<decltype(func), 6>(func);
}

void MathLib::GetCameraNearPlanePoints(
	const Matrix4& localToWorldMat,
	double fov,
	double aspect,
	double distance,
	Vector3* corners
)
{
	Matrix4& localToWorldMatrix = (Matrix4&)localToWorldMat;
	double upLength = distance * tan(fov * 0.5);
	double rightLength = upLength * aspect;
	Vector4 farPoint = localToWorldMatrix[3] + distance * localToWorldMatrix[2];
	Vector4 upVec = upLength * localToWorldMatrix[1];
	Vector4 rightVec = rightLength * localToWorldMatrix[0];
	corners[0] = farPoint - upVec - rightVec;
	corners[1] = farPoint - upVec + rightVec;
	corners[2] = farPoint + upVec - rightVec;
	corners[3] = farPoint + upVec + rightVec;
}

void MathLib::GetPerspFrustumPlanes(
	const Matrix4& localToWorldMat,
	double fov,
	double aspect,
	double nearPlane,
	double farPlane,
	XMFLOAT4* frustumPlanes
)
{
	Matrix4& localToWorldMatrix = (Matrix4&)localToWorldMat;
	Vector3 nearCorners[4];
	GetCameraNearPlanePoints((localToWorldMatrix), fov, aspect, nearPlane, nearCorners);
	*(Vector4*)frustumPlanes = GetPlane((localToWorldMatrix[2]), (localToWorldMatrix[3] + farPlane * localToWorldMatrix[2]));
	*(Vector4*)(frustumPlanes + 1) = GetPlane(-localToWorldMatrix[2], (localToWorldMatrix[3] + nearPlane * localToWorldMatrix[2]));
	*(Vector4*)(frustumPlanes + 2) = GetPlane((nearCorners[1]), (nearCorners[0]), (localToWorldMatrix[3]));
	*(Vector4*)(frustumPlanes + 3) = GetPlane((nearCorners[2]), (nearCorners[3]), (localToWorldMatrix[3]));
	*(Vector4*)(frustumPlanes + 4) = GetPlane((nearCorners[0]), (nearCorners[2]), (localToWorldMatrix[3]));
	*(Vector4*)(frustumPlanes + 5) = GetPlane((nearCorners[3]), (nearCorners[1]), (localToWorldMatrix[3]));
}

void MathLib::GetPerspFrustumPlanes(
	const Math::Matrix4& localToWorldMat,
	double fov,
	double aspect,
	double nearPlane,
	double farPlane,
	Math::Vector4* frustumPlanes)
{
	Matrix4& localToWorldMatrix = (Matrix4&)localToWorldMat;
	Vector3 nearCorners[4];
	GetCameraNearPlanePoints((localToWorldMatrix), fov, aspect, nearPlane, nearCorners);
	*frustumPlanes = GetPlane((localToWorldMatrix[2]), (localToWorldMatrix[3] + farPlane * localToWorldMatrix[2]));
	*(frustumPlanes + 1) = GetPlane(-localToWorldMatrix[2], (localToWorldMatrix[3] + nearPlane * localToWorldMatrix[2]));
	*(frustumPlanes + 2) = GetPlane((nearCorners[1]), (nearCorners[0]), (localToWorldMatrix[3]));
	*(frustumPlanes + 3) = GetPlane((nearCorners[2]), (nearCorners[3]), (localToWorldMatrix[3]));
	*(frustumPlanes + 4) = GetPlane((nearCorners[0]), (nearCorners[2]), (localToWorldMatrix[3]));
	*(frustumPlanes + 5) = GetPlane((nearCorners[3]), (nearCorners[1]), (localToWorldMatrix[3]));
}

void MathLib::GetFrustumBoundingBox(
	const Matrix4& localToWorldMat,
	double nearWindowHeight,
	double farWindowHeight,
	double aspect,
	double nearZ,
	double farZ,
	Vector3* minValue,
	Vector3* maxValue)
{
	Matrix4& localToWorldMatrix = (Matrix4&)localToWorldMat;
	double halfNearYHeight = nearWindowHeight * 0.5;
	double halfFarYHeight = farWindowHeight * 0.5;
	double halfNearXWidth = halfNearYHeight * aspect;
	double halfFarXWidth = halfFarYHeight * aspect;
	Vector4 poses[8];
	Vector4 pos = localToWorldMatrix[3];
	Vector4 right = localToWorldMatrix[0];
	Vector4 up = localToWorldMatrix[1];
	Vector4 forward = localToWorldMatrix[2];
	poses[0] = pos + forward * nearZ - right * halfNearXWidth - up * halfNearYHeight;
	poses[1] = pos + forward * nearZ - right * halfNearXWidth + up * halfNearYHeight;
	poses[2] = pos + forward * nearZ + right * halfNearXWidth - up * halfNearYHeight;
	poses[3] = pos + forward * nearZ + right * halfNearXWidth + up * halfNearYHeight;
	poses[4] = pos + forward * farZ - right * halfFarXWidth - up * halfFarYHeight;
	poses[5] = pos + forward * farZ - right * halfFarXWidth + up * halfFarYHeight;
	poses[6] = pos + forward * farZ + right * halfFarXWidth - up * halfFarYHeight;
	poses[7] = pos + forward * farZ + right * halfFarXWidth + up * halfFarYHeight;
	*minValue = poses[7];
	*maxValue = poses[7];
	auto func = [&](UINT i)->void
	{
		*minValue = XMVectorMin(poses[i], *minValue);
		*maxValue = XMVectorMax(poses[i], *maxValue);
	};
	InnerLoop<decltype(func), 7>(func);
}

bool MathLib::ConeIntersect(const Cone& cone, const Vector4& plane)
{
	Vector4 dir = XMLoadFloat3(&cone.direction);
	Vector4 vertex = XMLoadFloat3(&cone.vertex);
	Vector4 m = XMVector3Cross(XMVector3Cross(plane, dir), dir);
	Vector4 Q = vertex + dir * cone.height + (Vector4)XMVector3Normalize(m) * cone.radius;
	return (GetDistanceToPlane((vertex), (plane)) < 0) || (GetDistanceToPlane((Q), (plane)) < 0);
}

void MathLib::GetOrthoCamFrustumPlanes(
	const Math::Vector3& right,
	const Math::Vector3& up,
	const Math::Vector3& forward,
	const Math::Vector3& position,
	float xSize,
	float ySize,
	float nearPlane,
	float farPlane,
	Vector4* results)
{
	Vector3 normals[6];
	Vector3 positions[6];
	normals[0] = up;
	positions[0] = position + up * ySize;
	normals[1] = -up;
	positions[1] = position - up * ySize;
	normals[2] = right;
	positions[2] = position + right * xSize;
	normals[3] = -right;
	positions[3] = position - right * xSize;
	normals[4] = forward;
	positions[4] = position + forward * farPlane;
	normals[5] = -forward;
	positions[5] = position + forward * nearPlane;
	auto func = [&](uint i)->void
	{
		results[i] = GetPlane(normals[i], positions[i]);
	};
	InnerLoop<decltype(func), 6>(func);
}
void MathLib::GetOrthoCamFrustumPoints(
	const Math::Vector3& right,
	const Math::Vector3& up,
	const Math::Vector3& forward,
	const Math::Vector3& position,
	float xSize,
	float ySize,
	float nearPlane,
	float farPlane,
	Vector3* results)
{
	results[0] = position + xSize * right + ySize * up + farPlane * forward;
	results[1] = position + xSize * right + ySize * up + nearPlane * forward;
	results[2] = position + xSize * right - ySize * up + farPlane * forward;
	results[3] = position + xSize * right - ySize * up + nearPlane * forward;
	results[4] = position - xSize * right + ySize * up + farPlane * forward;
	results[5] = position - xSize * right + ySize * up + nearPlane * forward;
	results[6] = position - xSize * right - ySize * up + farPlane * forward;
	results[7] = position - xSize * right - ySize * up + nearPlane * forward;
}

double MathLib::DistanceToCube(const Vector3& size, const Vector3& quadToTarget)
{
	Vector3 absQuad = abs(quadToTarget);
	double len = length(absQuad);
	absQuad /= len;
	double dotV = Min((double)size.GetX() / (double)absQuad.GetX(), (double)size.GetY() / (double)absQuad.GetY());
	dotV = Min<double>(dotV, (double)size.GetZ() / (double)absQuad.GetZ());
	return len - dotV;
}
double MathLib::DistanceToQuad(double size, float2 quadToTarget)
{
	Vector3 quadVec = Vector3(quadToTarget.x, quadToTarget.y, 0);
	quadVec = abs(quadVec);
	double len = length(quadVec);
	quadVec /= len;
	double dotV = Max(dot(Vector3(0, 1, 0), quadVec), dot(Vector3(1, 0, 0), quadVec));
	double leftLen = size / dotV;
	return len - leftLen;
};

bool MathLib::BoxIntersect(const Math::Vector3& position, const Math::Vector3& extent, Math::Vector4* planes, int len)
{
	for (uint i = 0; i < len; ++i)
	{
		Vector4 plane = planes[i];
		Vector3 planeXYZ = plane;
		Vector3 absNormal = abs(planeXYZ);
		if ((dot(position, planeXYZ) - dot(absNormal, extent)) > -(float)plane.GetW())
			return false;
	}
	return true;
	/*

	for(i = 0; i < 6; ++i)
    {
        float4 plane = planes[i];
        float3 absNormal = abs(mul(plane.xyz, (float3x3)localToWorld));
        if((dot(position, plane.xyz) - dot(absNormal, extent)) > -plane.w)
        {
            return 0;
        }
    }*/
}

bool MathLib::BoxContactWithBox(double3 min0, double3 max0, double3 min1, double3 max1)
{
	bool vx, vy, vz;
	vx = min0.x > max1.x;
	vy = min0.y > max1.y;
	vz = min0.z > max1.z;
	if (vx || vy || vz) return false;
	vx = min1.x > max0.x;
	vy = min1.y > max0.y;
	vz = min1.z > max0.z;
	if (vx || vy || vz) return false;
	return true;
}