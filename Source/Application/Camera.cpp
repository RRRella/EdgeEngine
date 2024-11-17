#include "Camera.h"
#include "Math.h"
#include "../Utils/Source/Log.h"

// TODO: remove duplicate definition
#define DEG2RAD (DirectX::XM_PI / 180.0f)
#define RAD2DEG (180.0f / DirectX::XM_PI)
#define PI		DirectX::XM_PI
#define PI_DIV2 DirectX::XM_PIDIV2
#define CAMERA_DEBUG 1
using namespace DirectX;
Camera::Camera()
	:
	MoveSpeed(1000.0f),
	AngularSpeedDeg(20.0f),
	Drag(9.5f),
	mPitch(0.0f),
	mYaw(0.0f),
	mPosition(0,0,0),
	mVelocity(0,0,0)
{
	XMStoreFloat4x4(&mMatProj, XMMatrixIdentity());
	XMStoreFloat4x4(&mMatView, XMMatrixIdentity());
}
Camera::~Camera(void)
{}

void Camera::InitializeCamera(const CameraData& data)
{
	const auto& NEAR_PLANE = data.nearPlane;
	const auto& FAR_PLANE = data.farPlane;
	const float AspectRatio = data.width / data.height;
	const float VerticalFoV = data.fovV_Degrees * DEG2RAD;
	const float& ViewportX = data.width;
	const float& ViewportY = data.height;

	this->mProjParams.NearZ = NEAR_PLANE;
	this->mProjParams.FarZ = FAR_PLANE;
	this->mProjParams.ViewporHeight = ViewportY;
	this->mProjParams.ViewporWidth = ViewportX;
	this->mProjParams.FieldOfView = data.fovV_Degrees * DEG2RAD;
	this->mProjParams.bPerspectiveProjection = data.bPerspectiveProjection;

	SetProjectionMatrix(this->mProjParams);

	SetPosition(data.x, data.y, data.z);
	mYaw = mPitch = 0;
	Rotate(data.yaw * DEG2RAD, data.pitch * DEG2RAD, 1.0f);

	mIsInitialized = true;
}

void Camera::SetProjectionMatrix(const ProjectionMatrixParameters& params)
{
	assert(params.ViewporHeight > 0.0f);
	const float AspectRatio = params.ViewporWidth / params.ViewporHeight;

	mMatProj = params.bPerspectiveProjection
		? MakePerspectiveProjectionMatrix(params.FieldOfView, AspectRatio, params.NearZ, params.FarZ)
		: MakeOthographicProjectionMatrix(params.ViewporWidth, params.ViewporHeight, params.NearZ, params.FarZ);
}
void Camera::Update(const float dt, const CameraInput& input)
{
	Move(dt, input);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR lookAt = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	const XMVECTOR pos = XMLoadFloat3(&mPosition);
	const XMMATRIX MRot = GetRotationMatrix();
	//transform the lookat and up vector by rotation matrix
	lookAt	= XMVector3TransformCoord(lookAt, MRot);
	up		= XMVector3TransformCoord(up,	  MRot);
	//translate the lookat
	lookAt = pos + lookAt;
	//create view matrix

	XMStoreFloat4x4(&mMatView, XMMatrixLookAtLH(pos, lookAt, up));
	// move based on velocity
	XMVECTOR P = XMLoadFloat3(&mPosition);
	XMVECTOR V = XMLoadFloat3(&mVelocity);
	P += V * dt;
	XMStoreFloat3(&mPosition, P);
}
XMFLOAT3 Camera::GetPositionF() const
{
	return mPosition;
}
XMMATRIX Camera::GetViewMatrix() const
{
	return XMLoadFloat4x4(&mMatView);
}
XMMATRIX Camera::GetViewInverseMatrix() const
{
	XMMATRIX view = XMLoadFloat4x4(&mMatView);
	XMVECTOR det = XMMatrixDeterminant(view);
	XMMATRIX inverse = XMMatrixInverse(&det, view);
	return inverse;
}
XMMATRIX Camera::GetProjectionMatrix() const
{
	return  XMLoadFloat4x4(&mMatProj);
}
XMMATRIX Camera::GetRotationMatrix()
{
	return XMMatrixRotationQuaternion(mQuat);
}
XMVECTOR Camera::GetRight() const
{
	return XMVector3Rotate({ 1.0f,0.0f,0.0f }, mQuat);
}
XMVECTOR Camera::GetUp() const
{
	return XMVector3Rotate({ 0.0f,1.0f,0.0f }, mQuat);
}
XMVECTOR Camera::GetForward() const
{
	return XMVector3Rotate({ 0.0f,0.0f,1.0f }, mQuat);
}
void Camera::SetPosition(float x, float y, float z)
{
	mPosition = XMFLOAT3(x, y, z);
}
void Camera::Rotate(float yaw, float pitch, const float dt)
{
	const float delta = AngularSpeedDeg * DEG2RAD * dt;

	XMVECTOR pitchQuat = XMQuaternionRotationAxis({ 1.0f, 0.0f, 0.0f }, pitch * delta);
	XMVECTOR yawQuat = XMQuaternionRotationAxis({ 0.0f, 1.0f, 0.0f }, yaw * delta);
	XMVECTOR orientation = XMQuaternionMultiply(pitchQuat ,yawQuat);

	mPitch += pitch * delta;
	mYaw += yaw * delta;

	mQuat = XMQuaternionRotationRollPitchYawFromVector({ mPitch, mYaw, 0.0f });
	mQuat = XMQuaternionNormalize(mQuat);
}
void Camera::Reset() {}
void Camera::Rotate(const float dt, const CameraInput& input)
{
	float dy = input.DeltaMouseXY[1];
	float dx = input.DeltaMouseXY[0];

	const float delta = AngularSpeedDeg * DEG2RAD * dt;
	Rotate(dx, dy, delta);
}
void Camera::Move(const float dt, const CameraInput& input)
{
	const XMMATRIX MRotation = GetRotationMatrix();
	const XMVECTOR WorldSpaceTranslation = XMVector3TransformCoord(input.LocalTranslationVector, MRotation);
	XMVECTOR V = XMLoadFloat3(&mVelocity);
	V += (WorldSpaceTranslation * MoveSpeed - V * Drag) * dt;
	XMStoreFloat3(&mVelocity, V);
}