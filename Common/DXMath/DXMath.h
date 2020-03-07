#pragma once
#include "Vector.h"
#include "Quaternion.h"
#include "Matrix4.h"
using namespace DirectX;
inline float XM_CALLCONV CombineVector4(const XMVECTOR& V2)
{
	XMVECTOR vTemp2 = _mm_shuffle_ps(V2, V2, _MM_SHUFFLE(1, 0, 0, 0)); // Copy X to the Z position and Y to the W position
	vTemp2 = _mm_add_ps(vTemp2, V2);          // Add Z = X+Z; W = Y+W;
	return vTemp2.m128_f32[2] + vTemp2.m128_f32[3];
}

inline float XM_CALLCONV dot(const Math::Vector4& vec, const Math::Vector4& vec1)
{
	return CombineVector4(vec * vec1);
}

inline Math::Vector4 XM_CALLCONV abs(const Math::Vector4& vec)
{
	XMVECTOR& V = (XMVECTOR&)vec;
#if defined(_XM_NO_INTRINSICS_)
	XMVECTORF32 vResult = { { {
			fabsf(V.vector4_f32[0]),
			fabsf(V.vector4_f32[1]),
			fabsf(V.vector4_f32[2]),
			fabsf(V.vector4_f32[3])
		} } };
	return vResult.v;
#elif defined(_XM_ARM_NEON_INTRINSICS_)
	return vabsq_f32(V);
#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR vResult = _mm_setzero_ps();
	vResult = _mm_sub_ps(vResult, V);
	vResult = _mm_max_ps(vResult, V);
	return vResult;
#endif
}

inline Math::Vector3 XM_CALLCONV abs(const Math::Vector3& vec)
{
	XMVECTOR& V = (XMVECTOR&)vec;
#if defined(_XM_NO_INTRINSICS_)
	XMVECTORF32 vResult = { { {
			fabsf(V.vector4_f32[0]),
			fabsf(V.vector4_f32[1]),
			fabsf(V.vector4_f32[2]),
			fabsf(V.vector4_f32[3])
		} } };
	return vResult.v;
#elif defined(_XM_ARM_NEON_INTRINSICS_)
	return vabsq_f32(V);
#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR vResult = _mm_setzero_ps();
	vResult = _mm_sub_ps(vResult, V);
	vResult = _mm_max_ps(vResult, V);
	return vResult;
#endif
}

inline Math::Vector4 XM_CALLCONV clamp(
	const Math::Vector4& v,
	const Math::Vector4& min,
	const Math::Vector4& max)
{
	FXMVECTOR& V = (FXMVECTOR&)v;
	FXMVECTOR& Min = (FXMVECTOR&)min;
	FXMVECTOR& Max = (FXMVECTOR&)max;
#if defined(_XM_NO_INTRINSICS_)

	XMVECTOR Result;
	Result = XMVectorMax(Min, V);
	Result = XMVectorMin(Max, Result);
	return Result;

#elif defined(_XM_ARM_NEON_INTRINSICS_)
	XMVECTOR vResult;
	vResult = vmaxq_f32(Min, V);
	vResult = vminq_f32(Max, vResult);
	return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR vResult;
	vResult = _mm_max_ps(Min, V);
	vResult = _mm_min_ps(Max, vResult);
	return vResult;
#endif
}

inline Math::Vector3 XM_CALLCONV clamp(
	const Math::Vector3& v,
	const Math::Vector3& min,
	const Math::Vector3& max)
{
	FXMVECTOR& V = (FXMVECTOR&)v;
	FXMVECTOR& Min = (FXMVECTOR&)min;
	FXMVECTOR& Max = (FXMVECTOR&)max;
#if defined(_XM_NO_INTRINSICS_)

	XMVECTOR Result;
	Result = XMVectorMax(Min, V);
	Result = XMVectorMin(Max, Result);
	return Result;

#elif defined(_XM_ARM_NEON_INTRINSICS_)
	XMVECTOR vResult;
	vResult = vmaxq_f32(Min, V);
	vResult = vminq_f32(Max, vResult);
	return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR vResult;
	vResult = _mm_max_ps(Min, V);
	vResult = _mm_min_ps(Max, vResult);
	return vResult;
#endif
}

inline float XM_CALLCONV CombineVector3(const XMVECTOR& v)
{
	return v.m128_f32[0] + v.m128_f32[1] + v.m128_f32[2];
}

inline float XM_CALLCONV dot(const Math::Vector3& vec, const Math::Vector3& vec1)
{
	return CombineVector3(vec * vec1);
}

inline Math::Vector4 XM_CALLCONV mul(const Math::Matrix4& m, const Math::Vector4& vec)
{
	Math::Matrix4& mat = (Math::Matrix4&)m;
	return {
		dot(mat[0], vec),
		dot(mat[1], vec),
		dot(mat[2], vec),
		dot(mat[3], vec)
	};
}

inline Math::Vector3 XM_CALLCONV mul(const Math::Matrix3& m, const  Math::Vector3& vec)
{
	Math::Matrix4& mat = (Math::Matrix4&)m;
	return {
		CombineVector3(mat[0] * vec),
		CombineVector3(mat[1] * vec),
		CombineVector3(mat[2] * vec)
	};
}

inline Math::Vector3 XM_CALLCONV sqrt(const Math::Vector3& vec)
{
	return _mm_sqrt_ps((XMVECTOR&)vec);
}

inline Math::Vector4 XM_CALLCONV sqrt(const Math::Vector4& vec)
{
	return _mm_sqrt_ps((XMVECTOR&)vec);
}

inline float XM_CALLCONV length(const Math::Vector3& vec1)
{
	Math::Vector3 diff = vec1 * vec1;
	float dotValue = CombineVector3((const XMVECTOR&)diff);
	return sqrt(dotValue);
}

inline float XM_CALLCONV distance(const Math::Vector3& vec1, const Math::Vector3& vec2)
{
	Math::Vector3 diff = vec1 - vec2;
	diff *= diff;
	float dotValue = CombineVector3((const XMVECTOR&)diff);
	return sqrt(dotValue);
}

inline float XM_CALLCONV length(const Math::Vector4& vec1)
{
	Math::Vector4 diff = vec1 * vec1;
	float dotValue = CombineVector4((const XMVECTOR&)diff);
	return sqrt(dotValue);
}

inline float XM_CALLCONV distance(const Math::Vector4& vec1, const Math::Vector4& vec2)
{
	Math::Vector4 diff = vec1 - vec2;
	diff *= diff;
	float dotValue = CombineVector4((const XMVECTOR&)diff);
	return sqrt(dotValue);
}

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif


Math::Matrix4 XM_CALLCONV mul
(
	const Math::Matrix4& m1,
	const Math::Matrix4& m2);

template <typename T>
constexpr inline T max(const T& a, const T& b)
{
	return (((a) > (b)) ? (a) : (b));
}

template <typename T>
constexpr inline T min(const T& a, const T& b)
{
	return (((a) < (b)) ? (a) : (b));
}
template <>
inline Math::Vector3 max<Math::Vector3>(const Math::Vector3& vec1, const Math::Vector3& vec2)
{
	return _mm_max_ps((XMVECTOR&)vec1, (XMVECTOR&)vec2);
}
template <>
inline Math::Vector4 max<Math::Vector4>(const Math::Vector4& vec1, const Math::Vector4& vec2)
{
	return _mm_max_ps((XMVECTOR&)vec1, (XMVECTOR&)vec2);
}
template <>
inline Math::Vector3 min<Math::Vector3>(const Math::Vector3& vec1, const Math::Vector3& vec2)
{
	return _mm_min_ps((XMVECTOR&)vec1, (XMVECTOR&)vec2);
}
template <>
inline Math::Vector4 min<Math::Vector4>(const Math::Vector4& vec1, const Math::Vector4& vec2)
{
	return _mm_min_ps((XMVECTOR&)vec1, (XMVECTOR&)vec2);
}

inline Math::Vector4 XM_CALLCONV floor(const Math::Vector4& c)
{
	XMVECTOR& V = (XMVECTOR&)c;
#if defined(_XM_NO_INTRINSICS_)
	XMVECTORF32 Result = { { {
			floorf(V.vector4_f32[0]),
			floorf(V.vector4_f32[1]),
			floorf(V.vector4_f32[2]),
			floorf(V.vector4_f32[3])
		} } };
	return Result.v;
#elif defined(_XM_ARM_NEON_INTRINSICS_)
#if defined(_M_ARM64) || defined(_M_HYBRID_X86_ARM64)
	return vrndmq_f32(V);
#else
	float32x4_t vTest = vabsq_f32(V);
	vTest = vcltq_f32(vTest, g_XMNoFraction);
	// Truncate
	int32x4_t vInt = vcvtq_s32_f32(V);
	XMVECTOR vResult = vcvtq_f32_s32(vInt);
	XMVECTOR vLarger = vcgtq_f32(vResult, V);
	// 0 -> 0, 0xffffffff -> -1.0f
	vLarger = vcvtq_f32_s32(vLarger);
	vResult = vaddq_f32(vResult, vLarger);
	// All numbers less than 8388608 will use the round to int
	// All others, use the ORIGINAL value
	return vbslq_f32(vTest, vResult, V);
#endif
#elif defined(_XM_SSE4_INTRINSICS_)
	return _mm_floor_ps(V);
#elif defined(_XM_SSE_INTRINSICS_)
	// To handle NAN, INF and numbers greater than 8388608, use masking
	__m128i vTest = _mm_and_si128(_mm_castps_si128(V), g_XMAbsMask);
	vTest = _mm_cmplt_epi32(vTest, g_XMNoFraction);
	// Truncate
	__m128i vInt = _mm_cvttps_epi32(V);
	XMVECTOR vResult = _mm_cvtepi32_ps(vInt);
	__m128 vLarger = _mm_cmpgt_ps(vResult, V);
	// 0 -> 0, 0xffffffff -> -1.0f
	vLarger = _mm_cvtepi32_ps(_mm_castps_si128(vLarger));
	vResult = _mm_add_ps(vResult, vLarger);
	// All numbers less than 8388608 will use the round to int
	vResult = _mm_and_ps(vResult, _mm_castsi128_ps(vTest));
	// All others, use the ORIGINAL value
	vTest = _mm_andnot_si128(vTest, _mm_castps_si128(V));
	vResult = _mm_or_ps(vResult, _mm_castsi128_ps(vTest));
	return vResult;
#endif
}
inline Math::Vector3 XM_CALLCONV floor(const Math::Vector3& c)
{
	XMVECTOR& V = (XMVECTOR&)c;
#if defined(_XM_NO_INTRINSICS_)
	XMVECTORF32 Result = { { {
			floorf(V.vector4_f32[0]),
			floorf(V.vector4_f32[1]),
			floorf(V.vector4_f32[2]),
			floorf(V.vector4_f32[3])
		} } };
	return Result.v;
#elif defined(_XM_ARM_NEON_INTRINSICS_)
#if defined(_M_ARM64) || defined(_M_HYBRID_X86_ARM64)
	return vrndmq_f32(V);
#else
	float32x4_t vTest = vabsq_f32(V);
	vTest = vcltq_f32(vTest, g_XMNoFraction);
	// Truncate
	int32x4_t vInt = vcvtq_s32_f32(V);
	XMVECTOR vResult = vcvtq_f32_s32(vInt);
	XMVECTOR vLarger = vcgtq_f32(vResult, V);
	// 0 -> 0, 0xffffffff -> -1.0f
	vLarger = vcvtq_f32_s32(vLarger);
	vResult = vaddq_f32(vResult, vLarger);
	// All numbers less than 8388608 will use the round to int
	// All others, use the ORIGINAL value
	return vbslq_f32(vTest, vResult, V);
#endif
#elif defined(_XM_SSE4_INTRINSICS_)
	return _mm_floor_ps(V);
#elif defined(_XM_SSE_INTRINSICS_)
	// To handle NAN, INF and numbers greater than 8388608, use masking
	__m128i vTest = _mm_and_si128(_mm_castps_si128(V), g_XMAbsMask);
	vTest = _mm_cmplt_epi32(vTest, g_XMNoFraction);
	// Truncate
	__m128i vInt = _mm_cvttps_epi32(V);
	XMVECTOR vResult = _mm_cvtepi32_ps(vInt);
	__m128 vLarger = _mm_cmpgt_ps(vResult, V);
	// 0 -> 0, 0xffffffff -> -1.0f
	vLarger = _mm_cvtepi32_ps(_mm_castps_si128(vLarger));
	vResult = _mm_add_ps(vResult, vLarger);
	// All numbers less than 8388608 will use the round to int
	vResult = _mm_and_ps(vResult, _mm_castsi128_ps(vTest));
	// All others, use the ORIGINAL value
	vTest = _mm_andnot_si128(vTest, _mm_castps_si128(V));
	vResult = _mm_or_ps(vResult, _mm_castsi128_ps(vTest));
	return vResult;
#endif
}
inline Math::Vector4 XM_CALLCONV ceil
(
	const Math::Vector4& c
)
{
	XMVECTOR& V = (XMVECTOR&)c;
#if defined(_XM_NO_INTRINSICS_)
	XMVECTORF32 Result = { { {
			ceilf(V.vector4_f32[0]),
			ceilf(V.vector4_f32[1]),
			ceilf(V.vector4_f32[2]),
			ceilf(V.vector4_f32[3])
		} } };
	return Result.v;
#elif defined(_XM_ARM_NEON_INTRINSICS_)
#if defined(_M_ARM64) || defined(_M_HYBRID_X86_ARM64)
	return vrndpq_f32(V);
#else
	float32x4_t vTest = vabsq_f32(V);
	vTest = vcltq_f32(vTest, g_XMNoFraction);
	// Truncate
	int32x4_t vInt = vcvtq_s32_f32(V);
	XMVECTOR vResult = vcvtq_f32_s32(vInt);
	XMVECTOR vSmaller = vcltq_f32(vResult, V);
	// 0 -> 0, 0xffffffff -> -1.0f
	vSmaller = vcvtq_f32_s32(vSmaller);
	vResult = vsubq_f32(vResult, vSmaller);
	// All numbers less than 8388608 will use the round to int
	// All others, use the ORIGINAL value
	return vbslq_f32(vTest, vResult, V);
#endif
#elif defined(_XM_SSE4_INTRINSICS_)
	return _mm_ceil_ps(V);
#elif defined(_XM_SSE_INTRINSICS_)
	// To handle NAN, INF and numbers greater than 8388608, use masking
	__m128i vTest = _mm_and_si128(_mm_castps_si128(V), g_XMAbsMask);
	vTest = _mm_cmplt_epi32(vTest, g_XMNoFraction);
	// Truncate
	__m128i vInt = _mm_cvttps_epi32(V);
	XMVECTOR vResult = _mm_cvtepi32_ps(vInt);
	__m128 vSmaller = _mm_cmplt_ps(vResult, V);
	// 0 -> 0, 0xffffffff -> -1.0f
	vSmaller = _mm_cvtepi32_ps(_mm_castps_si128(vSmaller));
	vResult = _mm_sub_ps(vResult, vSmaller);
	// All numbers less than 8388608 will use the round to int
	vResult = _mm_and_ps(vResult, _mm_castsi128_ps(vTest));
	// All others, use the ORIGINAL value
	vTest = _mm_andnot_si128(vTest, _mm_castps_si128(V));
	vResult = _mm_or_ps(vResult, _mm_castsi128_ps(vTest));
	return vResult;
#endif
}
inline Math::Vector3 XM_CALLCONV ceil
(
	const Math::Vector3& c
)
{
	XMVECTOR& V = (XMVECTOR&)c;
#if defined(_XM_NO_INTRINSICS_)
	XMVECTORF32 Result = { { {
			ceilf(V.vector4_f32[0]),
			ceilf(V.vector4_f32[1]),
			ceilf(V.vector4_f32[2]),
			ceilf(V.vector4_f32[3])
		} } };
	return Result.v;
#elif defined(_XM_ARM_NEON_INTRINSICS_)
#if defined(_M_ARM64) || defined(_M_HYBRID_X86_ARM64)
	return vrndpq_f32(V);
#else
	float32x4_t vTest = vabsq_f32(V);
	vTest = vcltq_f32(vTest, g_XMNoFraction);
	// Truncate
	int32x4_t vInt = vcvtq_s32_f32(V);
	XMVECTOR vResult = vcvtq_f32_s32(vInt);
	XMVECTOR vSmaller = vcltq_f32(vResult, V);
	// 0 -> 0, 0xffffffff -> -1.0f
	vSmaller = vcvtq_f32_s32(vSmaller);
	vResult = vsubq_f32(vResult, vSmaller);
	// All numbers less than 8388608 will use the round to int
	// All others, use the ORIGINAL value
	return vbslq_f32(vTest, vResult, V);
#endif
#elif defined(_XM_SSE4_INTRINSICS_)
	return _mm_ceil_ps(V);
#elif defined(_XM_SSE_INTRINSICS_)
	// To handle NAN, INF and numbers greater than 8388608, use masking
	__m128i vTest = _mm_and_si128(_mm_castps_si128(V), g_XMAbsMask);
	vTest = _mm_cmplt_epi32(vTest, g_XMNoFraction);
	// Truncate
	__m128i vInt = _mm_cvttps_epi32(V);
	XMVECTOR vResult = _mm_cvtepi32_ps(vInt);
	__m128 vSmaller = _mm_cmplt_ps(vResult, V);
	// 0 -> 0, 0xffffffff -> -1.0f
	vSmaller = _mm_cvtepi32_ps(_mm_castps_si128(vSmaller));
	vResult = _mm_sub_ps(vResult, vSmaller);
	// All numbers less than 8388608 will use the round to int
	vResult = _mm_and_ps(vResult, _mm_castsi128_ps(vTest));
	// All others, use the ORIGINAL value
	vTest = _mm_andnot_si128(vTest, _mm_castps_si128(V));
	vResult = _mm_or_ps(vResult, _mm_castsi128_ps(vTest));
	return vResult;
#endif
}


Math::Matrix4 XM_CALLCONV transpose(const Math::Matrix4& m);

inline Math::Vector3 XM_CALLCONV pow(const Math::Vector3& v1, const Math::Vector3& v2)
{
	XMVECTOR& V1 = (XMVECTOR&)v1;
	XMVECTOR& V2 = (XMVECTOR&)v2;
#if defined(_XM_NO_INTRINSICS_)

	XMVECTORF32 Result = { { {
			powf(V1.vector4_f32[0], V2.vector4_f32[0]),
			powf(V1.vector4_f32[1], V2.vector4_f32[1]),
			powf(V1.vector4_f32[2], V2.vector4_f32[2]),
			powf(V1.vector4_f32[3], V2.vector4_f32[3])
		} } };
	return Result.v;

#elif defined(_XM_ARM_NEON_INTRINSICS_)
	XMVECTORF32 vResult = { { {
			powf(vgetq_lane_f32(V1, 0), vgetq_lane_f32(V2, 0)),
			powf(vgetq_lane_f32(V1, 1), vgetq_lane_f32(V2, 1)),
			powf(vgetq_lane_f32(V1, 2), vgetq_lane_f32(V2, 2)),
			powf(vgetq_lane_f32(V1, 3), vgetq_lane_f32(V2, 3))
		} } };
	return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
	__declspec(align(16)) float a[4];
	__declspec(align(16)) float b[4];
	_mm_store_ps(a, V1);
	_mm_store_ps(b, V2);
	XMVECTOR vResult = _mm_setr_ps(
		powf(a[0], b[0]),
		powf(a[1], b[1]),
		powf(a[2], b[2]),
		powf(a[3], b[3]));
	return vResult;
#endif
}

inline Math::Vector3 XM_CALLCONV pow(const Math::Vector3& v1, float v2)
{
	XMVECTOR& V1 = (XMVECTOR&)v1;
	XMVECTOR V2 = XMVectorReplicate(v2);
	//XMVECTOR& V2 = (XMVECTOR&)v2;
#if defined(_XM_NO_INTRINSICS_)

	XMVECTORF32 Result = { { {
			powf(V1.vector4_f32[0], V2.vector4_f32[0]),
			powf(V1.vector4_f32[1], V2.vector4_f32[1]),
			powf(V1.vector4_f32[2], V2.vector4_f32[2]),
			powf(V1.vector4_f32[3], V2.vector4_f32[3])
		} } };
	return Result.v;

#elif defined(_XM_ARM_NEON_INTRINSICS_)
	XMVECTORF32 vResult = { { {
			powf(vgetq_lane_f32(V1, 0), vgetq_lane_f32(V2, 0)),
			powf(vgetq_lane_f32(V1, 1), vgetq_lane_f32(V2, 1)),
			powf(vgetq_lane_f32(V1, 2), vgetq_lane_f32(V2, 2)),
			powf(vgetq_lane_f32(V1, 3), vgetq_lane_f32(V2, 3))
		} } };
	return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
	__declspec(align(16)) float a[4];
	__declspec(align(16)) float b[4];
	_mm_store_ps(a, V1);
	_mm_store_ps(b, V2);
	XMVECTOR vResult = _mm_setr_ps(
		powf(a[0], b[0]),
		powf(a[1], b[1]),
		powf(a[2], b[2]),
		powf(a[3], b[3]));
	return vResult;
#endif
}

inline Math::Vector4 XM_CALLCONV pow(const Math::Vector4& v1, const Math::Vector4& v2)
{
	XMVECTOR& V1 = (XMVECTOR&)v1;
	XMVECTOR& V2 = (XMVECTOR&)v2;
#if defined(_XM_NO_INTRINSICS_)

	XMVECTORF32 Result = { { {
			powf(V1.vector4_f32[0], V2.vector4_f32[0]),
			powf(V1.vector4_f32[1], V2.vector4_f32[1]),
			powf(V1.vector4_f32[2], V2.vector4_f32[2]),
			powf(V1.vector4_f32[3], V2.vector4_f32[3])
		} } };
	return Result.v;

#elif defined(_XM_ARM_NEON_INTRINSICS_)
	XMVECTORF32 vResult = { { {
			powf(vgetq_lane_f32(V1, 0), vgetq_lane_f32(V2, 0)),
			powf(vgetq_lane_f32(V1, 1), vgetq_lane_f32(V2, 1)),
			powf(vgetq_lane_f32(V1, 2), vgetq_lane_f32(V2, 2)),
			powf(vgetq_lane_f32(V1, 3), vgetq_lane_f32(V2, 3))
		} } };
	return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
	__declspec(align(16)) float a[4];
	__declspec(align(16)) float b[4];
	_mm_store_ps(a, V1);
	_mm_store_ps(b, V2);
	XMVECTOR vResult = _mm_setr_ps(
		powf(a[0], b[0]),
		powf(a[1], b[1]),
		powf(a[2], b[2]),
		powf(a[3], b[3]));
	return vResult;
#endif
}

inline Math::Vector4 XM_CALLCONV pow(const Math::Vector4& v1, float v2)
{
	XMVECTOR& V1 = (XMVECTOR&)v1;
	XMVECTOR V2 = XMVectorReplicate(v2);
#if defined(_XM_NO_INTRINSICS_)

	XMVECTORF32 Result = { { {
			powf(V1.vector4_f32[0], V2.vector4_f32[0]),
			powf(V1.vector4_f32[1], V2.vector4_f32[1]),
			powf(V1.vector4_f32[2], V2.vector4_f32[2]),
			powf(V1.vector4_f32[3], V2.vector4_f32[3])
		} } };
	return Result.v;

#elif defined(_XM_ARM_NEON_INTRINSICS_)
	XMVECTORF32 vResult = { { {
			powf(vgetq_lane_f32(V1, 0), vgetq_lane_f32(V2, 0)),
			powf(vgetq_lane_f32(V1, 1), vgetq_lane_f32(V2, 1)),
			powf(vgetq_lane_f32(V1, 2), vgetq_lane_f32(V2, 2)),
			powf(vgetq_lane_f32(V1, 3), vgetq_lane_f32(V2, 3))
		} } };
	return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
	__declspec(align(16)) float a[4];
	__declspec(align(16)) float b[4];
	_mm_store_ps(a, V1);
	_mm_store_ps(b, V2);
	XMVECTOR vResult = _mm_setr_ps(
		powf(a[0], b[0]),
		powf(a[1], b[1]),
		powf(a[2], b[2]),
		powf(a[3], b[3]));
	return vResult;
#endif
}
Math::Matrix3 XM_CALLCONV transpose(const Math::Matrix3& m);

Math::Matrix4 XM_CALLCONV inverse(const Math::Matrix4& m);

Math::Matrix4 XM_CALLCONV QuaternionToMatrix(const Math::Vector4& q);

Math::Matrix4 GetTransformMatrix(const Math::Vector3& right, const Math::Vector3& up, const Math::Vector3& forward, const Math::Vector3& position);
Math::Vector4 cross(const Math::Vector4& v1, const Math::Vector4& v2, const Math::Vector4& v3);
Math::Vector3 cross(const Math::Vector3& v1, const Math::Vector3& v2);
Math::Vector4 normalize(const Math::Vector4& v);
Math::Vector3 normalize(const Math::Vector3& v);
Math::Matrix4 GetInverseTransformMatrix(const Math::Vector3& right, const Math::Vector3& up, const Math::Vector3& forward, const Math::Vector3& position);