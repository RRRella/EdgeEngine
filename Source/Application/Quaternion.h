#pragma once
#include <DirectXMath.h>
#define DEG2RAD (DirectX::XM_PI / 180.0f)
#define RAD2DEG (180.0f / DirectX::XM_PI)
#define PI		DirectX::XM_PI
#define PI_DIV2 DirectX::XM_PIDIV2
class Quaternion
{
public:
	static Quaternion Identity();
	static Quaternion FromAxisAngle(const DirectX::XMVECTOR& axis, const float angle);
	static Quaternion FromAxisAngle(const DirectX::XMFLOAT3& axis, const float angle);
	static Quaternion Lerp(const Quaternion& from, const Quaternion& to, float t);
	static Quaternion Slerp(const Quaternion& from, const Quaternion& to, float t);
	static DirectX::XMFLOAT3 ToEulerRad(const Quaternion& Q);
	static DirectX::XMFLOAT3 ToEulerDeg(const Quaternion& Q);
	Quaternion(const DirectX::XMMATRIX& rotMatrix);
	Quaternion(float s, const DirectX::XMVECTOR& v);
	Quaternion  operator+(const Quaternion& q) const;
	Quaternion  operator*(const Quaternion& q) const;
	Quaternion  operator*(float c) const;
	bool        operator==(const Quaternion& q) const;
	float       Dot(const Quaternion& q) const;
	float       Len() const;
	Quaternion  Inverse() const;
	Quaternion  Conjugate() const;
	DirectX::XMMATRIX Matrix() const;
	Quaternion& Normalize();
	DirectX::XMFLOAT3 TransformVector(const DirectX::XMFLOAT3& v) const;
	DirectX::XMVECTOR TransformVector(const DirectX::XMVECTOR& v) const;
private:	// used by operator()s
	Quaternion(float s, const DirectX::XMFLOAT3& v) : S(s), V(v) {};
	Quaternion() = default;
public:
	// Q = [S, <V>]
	DirectX::XMFLOAT3 V;
	float S;
};