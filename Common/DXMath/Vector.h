//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#pragma once

#include "Scalar.h"

namespace Math
{
	class Vector4;
	class Matrix3;
	// A 3-vector with an unspecified fourth component.  Depending on the context, the W can be 0 or 1, but both are implicit.
	// The actual value of the fourth component is undefined for performance reasons.
	class Vector3
	{
		friend class Matrix3;
		friend class Vector4;
	public:

		INLINE Vector3() {}
		INLINE Vector3(float x, float y, float z) : m_vec(XMVectorSet(x, y, z, 0)) { }
		INLINE Vector3(const XMFLOAT3& v) : m_vec(XMLoadFloat3(&v)) {}
		INLINE Vector3(const Vector3& v) : m_vec(v) {}
		INLINE Vector3(const Scalar& s) : m_vec(s) {}
		INLINE  Vector3(const Vector4& v);
		INLINE void operator=(const Vector4& v);
		INLINE  Vector3(const FXMVECTOR& vec) : m_vec(vec) {}

		INLINE operator XMVECTOR() const { return m_vec; }

		INLINE Scalar GetX() const { return Scalar(XMVectorSplatX(m_vec)); }
		INLINE Scalar GetY() const { return Scalar(XMVectorSplatY(m_vec)); }
		INLINE Scalar GetZ() const { return Scalar(XMVectorSplatZ(m_vec)); }
		INLINE void SetX(const Scalar& x) { m_vec = XMVectorPermute<4, 1, 2, 3>(m_vec, x); }
		INLINE void SetY(const Scalar& y) { m_vec = XMVectorPermute<0, 5, 2, 3>(m_vec, y); }
		INLINE void SetZ(const Scalar& z) { m_vec = XMVectorPermute<0, 1, 6, 3>(m_vec, z); }

		INLINE Vector3 operator- () const { return Vector3(XMVectorNegate(m_vec)); }
		INLINE Vector3 operator+ (const Vector3& v2) const { return Vector3(XMVectorAdd(m_vec, v2)); }
		INLINE Vector3 operator- (const Vector3& v2) const { return Vector3(XMVectorSubtract(m_vec, v2)); }
		INLINE Vector3 operator+ (const Scalar& v2) const { return Vector3(XMVectorAdd(m_vec, v2)); }
		INLINE Vector3 operator- (const Scalar& v2) const { return Vector3(XMVectorSubtract(m_vec, v2)); }
		INLINE Vector3 operator* (const Vector3& v2) const { return Vector3(XMVectorMultiply(m_vec, v2)); }
		INLINE Vector3 operator/ (const Vector3& v2) const { return Vector3(XMVectorDivide(m_vec, v2)); }
		INLINE Vector3 operator* (const Scalar&  v2) const { return *this * Vector3(v2); }
		INLINE Vector3 operator/ (const Scalar&  v2) const { return *this / Vector3(v2); }
		INLINE Vector3 operator* (float  v2) const { return *this * Scalar(v2); }
		INLINE Vector3 operator/ (float  v2) const { return *this / Scalar(v2); }

		INLINE Vector3& operator += (const Vector3& v) { *this = *this + v; return *this; }
		INLINE Vector3& operator += (float v) { *this = *this + v; return *this; }
		INLINE Vector3& operator -= (const Vector3& v) { *this = *this - v; return *this; }
		INLINE Vector3& operator -= (float v) { *this = *this - v; return *this; }
		INLINE Vector3& operator *= (const Vector3& v) { *this = *this * v; return *this; }
		INLINE Vector3& operator *= (float v) { *this = *this * v; return *this; }
		INLINE Vector3& operator /= (const Vector3& v) { *this = *this / v; return *this; }
		INLINE Vector3& operator /= (float v) { *this = *this / v; return *this; }

		INLINE friend Vector3 operator* (const Scalar&  v1, const Vector3& v2) { return Vector3(v1) * v2; }
		INLINE friend Vector3 operator/ (const Scalar&  v1, const Vector3& v2) { return Vector3(v1) / v2; }
		INLINE friend Vector3 operator* (float   v1, const Vector3& v2) { return Scalar(v1) * v2; }
		INLINE friend Vector3 operator/ (float   v1, const Vector3& v2) { return Scalar(v1) / v2; }

		INLINE operator XMFLOAT4()
		{
			XMFLOAT4 f;
			XMStoreFloat4(&f, m_vec);
			return f;
		}
		INLINE operator XMFLOAT3()
		{
			XMFLOAT3 f;
			XMStoreFloat3(&f, m_vec);
			return f;
		}
	protected:
		XMVECTOR m_vec;
	};

	// A 4-vector, completely defined.
	class Vector4
	{
	public:
		friend class Vector3;
		INLINE Vector4() {}
		INLINE Vector4(float x, float y, float z, float w) : m_vec(XMVectorSet(x, y, z, w)) {}
		INLINE Vector4(const Vector3& xyz, float w) : m_vec(XMVectorSetW(xyz, w)) {  }
		INLINE Vector4(const Vector4& v) : m_vec(v) {}
		INLINE Vector4(const Scalar& s) : m_vec(s) { }
		INLINE Vector4(const Vector3& xyz) : m_vec(SetWToOne(xyz)) { }
		INLINE void operator=(const Vector3& xyz)
		{
			m_vec = SetWToOne(xyz);
		}
		INLINE Vector4(const FXMVECTOR& vec) : m_vec(vec) {}
		INLINE Vector4(const XMFLOAT4& flt) : m_vec(XMLoadFloat4(&flt)) {}

		INLINE operator XMVECTOR() const { return m_vec; }

		INLINE Scalar GetX() const { return Scalar(XMVectorSplatX(m_vec)); }
		INLINE Scalar GetY() const { return Scalar(XMVectorSplatY(m_vec)); }
		INLINE Scalar GetZ() const { return Scalar(XMVectorSplatZ(m_vec)); }
		INLINE Scalar GetW() const { return Scalar(XMVectorSplatW(m_vec)); }
		INLINE void SetX(const Scalar& x) { m_vec = XMVectorPermute<4, 1, 2, 3>(m_vec, x); }
		INLINE void SetY(const Scalar& y) { m_vec = XMVectorPermute<0, 5, 2, 3>(m_vec, y); }
		INLINE void SetZ(const Scalar& z) { m_vec = XMVectorPermute<0, 1, 6, 3>(m_vec, z); }
		INLINE void SetW(const Scalar& w) { m_vec = XMVectorPermute<0, 1, 2, 7>(m_vec, w); }

		INLINE Vector4 operator- () const { return Vector4(XMVectorNegate(m_vec)); }
		INLINE Vector4 operator+ (const Vector4& v2) const { return Vector4(XMVectorAdd(m_vec, v2)); }
		INLINE Vector4 operator- (const Vector4& v2) const { return Vector4(XMVectorSubtract(m_vec, v2)); }
		INLINE Vector4 operator* (const Vector4& v2) const { return Vector4(XMVectorMultiply(m_vec, v2)); }
		INLINE Vector4 operator/ (const Vector4& v2) const { return Vector4(XMVectorDivide(m_vec, v2)); }
		INLINE Vector4 operator* (const Scalar&  v2) const { return *this * Vector4(v2); }
		INLINE Vector4 operator/ (const Scalar&  v2) const { return *this / Vector4(v2); }
		INLINE Vector4 operator+ (const Scalar&  v2) const { return *this + Vector4(v2); }
		INLINE Vector4 operator- (const Scalar&  v2) const { return *this - Vector4(v2); }
		INLINE Vector4 operator* (float   v2) const { return *this * Scalar(v2); }
		INLINE Vector4 operator/ (float   v2) const { return *this / Scalar(v2); }
		INLINE Vector4 operator+ (float   v2) const { return *this + Scalar(v2); }
		INLINE Vector4 operator- (float   v2) const { return *this - Scalar(v2); }

		INLINE Vector4& operator*= (float   v2) { *this = *this * Scalar(v2); return *this; }
		INLINE Vector4& operator/= (float   v2) { *this = *this / Scalar(v2); return *this; }
		INLINE Vector4& operator+= (float   v2) { *this = *this + Scalar(v2); return *this; }
		INLINE Vector4& operator-= (float   v2) { *this = *this - Scalar(v2); return *this; }
		INLINE Vector4& operator += (const Vector4& v) { *this = *this + v; return *this; }
		INLINE Vector4& operator -= (const Vector4& v) { *this = *this - v; return *this; }
		INLINE Vector4& operator *= (const Vector4& v) { *this = *this * v; return *this; }
		INLINE Vector4& operator /= (const Vector4& v) { *this = *this / v; return *this; }

		INLINE friend Vector4 operator* (const Scalar&  v1, const Vector4& v2) { return Vector4(v1) * v2; }
		INLINE friend Vector4 operator/ (const Scalar&  v1, const Vector4& v2) { return Vector4(v1) / v2; }
		INLINE friend Vector4 operator* (float   v1, const Vector4& v2) { return Scalar(v1) * v2; }
		INLINE friend Vector4 operator/ (float   v1, const Vector4& v2) { return Scalar(v1) / v2; }
		INLINE operator XMFLOAT4()
		{
			XMFLOAT4 f;
			XMStoreFloat4(&f, m_vec);
			return f;
		}
		INLINE operator XMFLOAT3()
		{
			XMFLOAT3 f;
			XMStoreFloat3(&f, m_vec);
			return f;
		}

		INLINE operator Vector3()
		{
			return Vector3(m_vec);
		}
	protected:
		XMVECTOR m_vec;
	};

	INLINE Vector3::Vector3(const Vector4& v)
	{
		m_vec = SetWToZero(v);
	}
	INLINE void Vector3::operator=(const Vector4& v)
	{
		m_vec = SetWToZero(v);
	}

} // namespace Math