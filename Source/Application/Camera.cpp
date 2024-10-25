#include "Camera.h"
#include "Math.h"
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

void Camera::InitializeCamera(const CameraData& data, int ViewportX, int ViewportY)
{
	const auto& NEAR_PLANE = data.nearPlane;
	const auto& FAR_PLANE = data.farPlane;
	const float AspectRatio = static_cast<float>(ViewportX) / ViewportY;
	const float VerticalFoV = data.fovV_Degrees * DEG2RAD;

	SetProjectionMatrix(VerticalFoV, AspectRatio, NEAR_PLANE, FAR_PLANE);

	SetPosition(data.x, data.y, data.z);
	mYaw = mPitch = 0;
	Rotate(data.yaw * DEG2RAD, data.pitch * DEG2RAD, 1.0f);
}

void Camera::SetProjectionMatrix(float fovy, float screenAspect, float screenNear, float screenFar)
{
	XMStoreFloat4x4(&mMatProj, XMMatrixPerspectiveFovLH(fovy, screenAspect, screenNear, screenFar));
}
void Camera::Update(const float dt, const CameraInput& input)
{
	Rotate(dt, input);
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
FrustumPlaneset Camera::GetViewFrustumPlanes() const
{
	return FrustumPlaneset::ExtractFromMatrix(GetViewMatrix() * GetProjectionMatrix());
}
XMMATRIX Camera::GetRotationMatrix() const
{
	return XMMatrixRotationRollPitchYaw(mPitch, mYaw, 0.0f);
}
void Camera::SetPosition(float x, float y, float z)
{
	mPosition = XMFLOAT3(x, y, z);
}
void Camera::Rotate(float yaw, float pitch, const float dt)
{
	mYaw   += yaw   * dt;
	mPitch += pitch * dt;
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
