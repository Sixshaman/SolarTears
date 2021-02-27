#pragma once

#include "../../../3rd party/DirectXMath/Inc/DirectXMath.h"
#include "../../../3rd party/DirectXMath/Inc/DirectXCollision.h"

class Camera //Detached fron any positional data
{
public:
	Camera();
	~Camera();

	float GetNearZ()       const; 
	float GetFarZ()        const;
	float GetAspectRatio() const;
	float GetFovY()        const;  
	float GetFovX()        const;

	float GetNearPlaneWidth()  const;
	float GetNearPlaneHeight() const;

	void SetProjectionParameters(float fovY, float viewportAspect, float nz, float farz);

	DirectX::XMMATRIX GetProjMatrix() const;
	void RecalcProjMatrix();

	DirectX::BoundingFrustum GetFrustum() const;

private:
	float mNearZ;			 
	float mFarZ;
	float mFovY;	

	float mNearPlaneWidth;
	float mNearPlaneHeight;

	DirectX::XMFLOAT4X4 mProj;
};