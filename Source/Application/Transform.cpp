#include "Transform.h"


Transform::Transform(const XMFLOAT3& position, const XMVECTOR& rotation, const XMFLOAT3& scale)
	: _position(position)
	, _rotation(rotation)
	, _scale(scale)
	
{}
Transform::~Transform() {}
Transform & Transform::operator=(const Transform & t)
{
	this->_position = t._position;
	this->_rotation = t._rotation;
	this->_scale    = t._scale;
	return *this;
}
void Transform::Translate(const XMFLOAT3& translation)
{
	XMVECTOR POSITION = XMLoadFloat3(&_position);
	XMVECTOR TRANSLATION = XMLoadFloat3(&translation);
	POSITION += TRANSLATION;
	XMStoreFloat3(&_position, POSITION);
}
void Transform::Translate(float x, float y, float z)
{
	XMVECTOR POSITION = XMLoadFloat3(&_position);
	XMVECTOR TRANSLATION = XMLoadFloat3(&XMFLOAT3(x, y, z));
	POSITION += TRANSLATION;
	XMStoreFloat3(&_position, POSITION);
}
void Transform::Scale(const XMFLOAT3& scl)
{
	_scale = scl;
}
void Transform::RotateAroundPointAndAxis(const XMVECTOR& axis, float angle, const XMVECTOR& point)
{ 
	XMVECTOR R = XMLoadFloat3(&_position);
	R -= point; // R = _position - point;
	const XMVECTOR rot = XMQuaternionRotationAxis(axis, angle);
	R = XMVector3Rotate(R, rot);
	R = point + R;
	XMStoreFloat3(&_position, R);
}
XMMATRIX Transform::WorldTransformationMatrix() const
{
	XMVECTOR scale = XMLoadFloat3(&_scale);
	XMVECTOR translation = XMLoadFloat3(&_position);
	XMVECTOR rotOrigin = XMVectorZero();
	return XMMatrixAffineTransformation(scale, rotOrigin, _rotation, translation);
}
DirectX::XMMATRIX Transform::WorldTransformationMatrix_NoScale() const
{
	XMVECTOR scale = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
	XMVECTOR translation = XMLoadFloat3(&_position);
	XMVECTOR rotOrigin = XMVectorZero();
	return XMMatrixAffineTransformation(scale, rotOrigin, _rotation, translation);
}
XMMATRIX Transform::RotationMatrix() const
{
	return XMMatrixRotationQuaternion(_rotation);
}
// builds normal matrix from world matrix, ignoring translation
// and using inverse-transpose of rotation/scale matrix
DirectX::XMMATRIX Transform::NormalMatrix(const XMMATRIX& world)
{
	XMMATRIX nrm = world;
	nrm.r[3].m128_f32[0] = nrm.r[3].m128_f32[1] = nrm.r[3].m128_f32[2] = 0;
	nrm.r[3].m128_f32[3] = 1;
	XMVECTOR Det = XMMatrixDeterminant(nrm);
	nrm = XMMatrixInverse(&Det, nrm);
	nrm = XMMatrixTranspose(nrm);
	return nrm;
}