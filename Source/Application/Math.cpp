#include "Math.h"

using namespace DirectX;

DirectX::XMFLOAT4X4 MakeOthographicProjectionMatrix(float screenWidth, float screenHeight, float screenNear, float screenFar)
{
	XMFLOAT4X4 m;
	XMStoreFloat4x4(&m, XMMatrixOrthographicLH(screenWidth, screenHeight, screenNear, screenFar));
	return m;
}

DirectX::XMFLOAT4X4 MakePerspectiveProjectionMatrix(float fovy, float screenAspect, float screenNear, float screenFar)
{
	XMFLOAT4X4 m;
	XMStoreFloat4x4(&m, XMMatrixPerspectiveFovLH(fovy, screenAspect, screenNear, screenFar));
	return m;
}

DirectX::XMFLOAT4X4 MakePerspectiveProjectionMatrixHFov(float fovx, float screenAspectInverse, float screenNear, float screenFar)
{	// horizonital FOV
	XMFLOAT4X4 m;

	const float& FarZ = screenFar;
	const float& NearZ = screenNear;
	const float& r = screenAspectInverse;

	const float Width = 1.0f / tanf(fovx * 0.5f);
	const float Height = Width / r;
	const float fRange = FarZ / (FarZ - NearZ);

	XMMATRIX M;
	M.r[0].m128_f32[0] = Width;
	M.r[0].m128_f32[1] = 0.0f;
	M.r[0].m128_f32[2] = 0.0f;
	M.r[0].m128_f32[3] = 0.0f;

	M.r[1].m128_f32[0] = 0.0f;
	M.r[1].m128_f32[1] = Height;
	M.r[1].m128_f32[2] = 0.0f;
	M.r[1].m128_f32[3] = 0.0f;

	M.r[2].m128_f32[0] = 0.0f;
	M.r[2].m128_f32[1] = 0.0f;
	M.r[2].m128_f32[2] = fRange;
	M.r[2].m128_f32[3] = 1.0f;

	M.r[3].m128_f32[0] = 0.0f;
	M.r[3].m128_f32[1] = 0.0f;
	M.r[3].m128_f32[2] = -fRange * NearZ;
	M.r[3].m128_f32[3] = 0.0f;
	XMStoreFloat4x4(&m, M);
	return m;
}