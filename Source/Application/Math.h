#pragma once
#include <DirectXMath.h>
#define DEG2RAD (DirectX::XM_PI / 180.0f)
#define RAD2DEG (180.0f / DirectX::XM_PI)
#define PI		DirectX::XM_PI
#define PI_DIV2 DirectX::XM_PIDIV2
constexpr DirectX::XMFLOAT3 UpVector = DirectX::XMFLOAT3(0, 1, 0);
constexpr DirectX::XMFLOAT3 RightVector = DirectX::XMFLOAT3(1, 0, 0);
constexpr DirectX::XMFLOAT3 ForwardVector = DirectX::XMFLOAT3(0, 0, 1);
constexpr DirectX::XMFLOAT3 LeftVector = DirectX::XMFLOAT3(-1, 0, 0);
constexpr DirectX::XMFLOAT3 BackVector = DirectX::XMFLOAT3(0, 0, -1);
constexpr DirectX::XMFLOAT3 DownVector = DirectX::XMFLOAT3(0, -1, 0);
constexpr DirectX::XMFLOAT3 XAxis = DirectX::XMFLOAT3(1, 0, 0);
constexpr DirectX::XMFLOAT3 YAxis = DirectX::XMFLOAT3(0, 1, 0);
constexpr DirectX::XMFLOAT3 ZAxis = DirectX::XMFLOAT3(0, 0, 1);

DirectX::XMFLOAT4X4 MakeOthographicProjectionMatrix(float screenWidth, float screenHeight, float screenNear, float screenFar);
DirectX::XMFLOAT4X4 MakePerspectiveProjectionMatrix(float fovy, float screenAspect, float screenNear, float screenFar);
DirectX::XMFLOAT4X4 MakePerspectiveProjectionMatrixHFov(float fovx, float screenAspectInverse, float screenNear, float screenFar);