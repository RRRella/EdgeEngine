#pragma once
#include <DirectXMath.h>
#include <utility>
#include "Math.h"

using namespace DirectX;


struct Transform
{
public:
	
	Transform(  const XMFLOAT3& position = XMFLOAT3(0.0f, 0.0f, 0.0f),
				const XMVECTOR& rotation = XMVECTOR{ 1.0f,0.0f, 0.0f, 0.0f },
	            const XMFLOAT3& scale    = XMFLOAT3(1.0f, 1.0f, 1.0f));
	~Transform();
	Transform& operator=(const Transform&);
	
	void SetXRotationDeg(float xDeg)              { _rotation = XMQuaternionRotationAxis(XMLoadFloat3(&RightVector), xDeg * DEG2RAD); }
	void SetYRotationDeg(float yDeg)              { _rotation = XMQuaternionRotationAxis(XMLoadFloat3(&UpVector), yDeg * DEG2RAD); }
	void SetZRotationDeg(float zDeg)              { _rotation = XMQuaternionRotationAxis(XMLoadFloat3(&ForwardVector), zDeg * DEG2RAD); }
	void SetScale(float x, float y, float z)      { _scale = XMFLOAT3(x, y, z); }
	void SetScale(const XMFLOAT3& scl)            { _scale = scl; }
	void SetScale(const DirectX::XMVECTOR& scl) { XMStoreFloat3(&_scale, scl); }
	void SetUniformScale(float s)                 { _scale = XMFLOAT3(s, s, s); }
	void SetPosition(float x, float y, float z)   { _position = XMFLOAT3(x, y, z); }
	void SetPosition(const XMFLOAT3& pos){ _position = pos; }
	
	void Translate(const XMFLOAT3& translation);
	void Translate(float x, float y, float z);
	void Scale    (const XMFLOAT3& scl);
	
	void RotateAroundPointAndAxis(const XMVECTOR& axis, float angle, const XMVECTOR& point);

	void RotateAroundAxisRadians (const XMVECTOR& axis, float angle) 
	{ 
		RotateInWorldSpace(XMQuaternionRotationAxis(axis, angle));
	}

	void RotateAroundAxisDegrees(const XMVECTOR& axis, float angle) 
	{ 
		RotateInWorldSpace(XMQuaternionRotationAxis(axis, angle * DEG2RAD));
	}

	void RotateAroundAxisRadians(const XMFLOAT3& axis, float angle) 
	{ 
		RotateInWorldSpace(XMQuaternionRotationAxis(XMLoadFloat3(&axis), angle));
	}

	void RotateAroundAxisDegrees(const XMFLOAT3& axis, float angle) 
	{ 
		RotateInWorldSpace(XMQuaternionRotationAxis(XMLoadFloat3(&axis), angle * DEG2RAD));
	}

	void RotateAroundLocalAxisDegrees(const XMVECTOR& axis, float angle)
	{
		RotateInLocalSpace(XMQuaternionRotationAxis(axis, (angle * DEG2RAD)));
	}

	void RotateAroundLocalXAxisDegrees(float angle)  
	{ 
		RotateInLocalSpace(XMQuaternionRotationAxis(XMLoadFloat3(&XAxis), (angle * DEG2RAD)));
	}

	void RotateAroundLocalYAxisDegrees(float angle)  
	{ 
		RotateInLocalSpace(XMQuaternionRotationAxis(XMLoadFloat3(&YAxis), (angle * DEG2RAD)));
	}

	void RotateAroundLocalZAxisDegrees(float angle)  
	{ 
		RotateInLocalSpace(XMQuaternionRotationAxis(XMLoadFloat3(&ZAxis), (angle * DEG2RAD)));
	}

	void RotateAroundGlobalXAxisDegrees(float angle) 
	{ 
		RotateAroundAxisDegrees(XAxis, angle);
	}

	void RotateAroundGlobalYAxisDegrees(float angle) 
	{ 
		RotateAroundAxisDegrees(YAxis, angle);
	}

	void RotateAroundGlobalZAxisDegrees(float angle) 
	{ 
		RotateAroundAxisDegrees(ZAxis, angle);
	}

	void RotateInWorldSpace(const XMVECTOR& q) 
	{ 
		_rotation = XMQuaternionMultiply(q , _rotation);
		_rotation = XMQuaternionNormalize(_rotation);
	}
	void RotateInLocalSpace(const XMVECTOR& q) 
	{ 
		_rotation = XMQuaternionMultiply(_rotation, q);
		_rotation = XMQuaternionNormalize(_rotation);
	}

	void ResetPosition() { _position = XMFLOAT3(0, 0, 0); }
	void ResetRotation() { _rotation = XMVECTOR{ 0.0f,0.0f, 0.0f, 1.0f }; }
	void ResetScale() { _scale = XMFLOAT3(1, 1, 1); }
	void Reset() { ResetScale(); ResetRotation(); ResetPosition(); }
	
	XMMATRIX WorldTransformationMatrix() const;
	XMMATRIX WorldTransformationMatrix_NoScale() const;
	XMMATRIX RotationMatrix() const;
	static XMMATRIX NormalMatrix(const XMMATRIX& world);
	
	XMFLOAT3       _position;
	XMVECTOR       _rotation;
	XMFLOAT3       _scale;
};