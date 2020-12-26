#pragma once

#include "../../Math/3rdParty/DirectXMath/Inc/DirectXMath.h"
#include "../../Math/3rdParty/DirectXMath/Inc/DirectXCollision.h"

class Camera
{
public:
	Camera();
	~Camera();

	DirectX::XMVECTOR GetPosition() const;
	DirectX::XMVECTOR GetRight()    const;
	DirectX::XMVECTOR GetUp()       const;
	DirectX::XMVECTOR GetLook()     const;

	float GetNearZ()       const; 
	float GetFarZ()        const;
	float GetAspectRatio() const;
	float GetFovY()        const;  
	float GetFovX()        const;

	float GetNearPlaneWidth()  const;
	float GetNearPlaneHeight() const;

	void SetCameraSpace(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void SetProjectionParameters(float fovY, float viewportAspect, float nz, float farz);

	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjMatrix() const;

	DirectX::BoundingFrustum GetFrustum() const;

	void WalkForward(float distance);
	void WalkSideways(float distance);

	void Pitch(float angle);
	void Yaw(float angle);
	void Roll(float angle);

	void RecalcViewMatrix();
	void RecalcProjMatrix();

private:
	DirectX::XMFLOAT3 mPosition;
	DirectX::XMFLOAT3 mLook;
	DirectX::XMFLOAT3 mUp;
	DirectX::XMFLOAT3 mRight;

	float mNearZ;			 
	float mFarZ;
	float mFovY;	

	float mNearPlaneWidth;
	float mNearPlaneHeight;

	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mProj;
};