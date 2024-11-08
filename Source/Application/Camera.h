#pragma once
#include <DirectXMath.h>
#include <array>
#include "Math.h"

struct CameraData
{
	float x, y, z; // position
	float width;
	float height;
	float nearPlane;
	float farPlane;
	float fovV_Degrees;
	float yaw, pitch; // in degrees
	bool bPerspectiveProjection;
};

struct ProjectionMatrixParameters
{
	float ViewporWidth;
	float ViewporHeight;
	float NearZ;
	float FarZ;
	float FieldOfView;
	bool bPerspectiveProjection;
};

struct CameraInput
{
	CameraInput() = delete;
	CameraInput(const DirectX::XMVECTOR& v) : LocalTranslationVector(v), DeltaMouseXY{ 0, 0 } {}
	const DirectX::XMVECTOR& LocalTranslationVector;
	std::array<float, 2> DeltaMouseXY;
};
class Camera
{
public:
	Camera();
	~Camera(void);
	void InitializeCamera(const CameraData& data);
	void SetProjectionMatrix(const ProjectionMatrixParameters& params);
	
	void Update(const float dt, const CameraInput& input);
	DirectX::XMFLOAT3 GetPositionF() const;
	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetViewInverseMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;
	
	void SetPosition(float x, float y, float z);
	void Rotate(float yaw, float pitch, const float dt);
	void Reset();

	void Move(const float dt, const CameraInput& input);
	void Rotate(const float dt, const CameraInput& input);
	DirectX::XMMATRIX GetRotationMatrix() const;

	DirectX::XMFLOAT3 mPosition;
	DirectX::XMFLOAT3 mVelocity;

	float mYaw = 0.0f;
	float mPitch = 0.0f;

	ProjectionMatrixParameters mProjParams;

	float Drag;            // 15.0f
	float AngularSpeedDeg; // 40.0f
	float MoveSpeed;       // 1000.0f

	DirectX::XMFLOAT4X4 mMatView;
	DirectX::XMFLOAT4X4 mMatProj;
};