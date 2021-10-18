#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>

class PinholeCamera //Detached fron any positional data
{
public:
	PinholeCamera();
	~PinholeCamera();

	float GetNearZ()       const; 
	float GetFarZ()        const;
	float GetAspectRatio() const;
	float GetFovY()        const;  
	float GetFovX()        const;

	float GetNearPlaneWidth()  const;
	float GetNearPlaneHeight() const;

	void SetProjectionParameters(float fovY, float viewportAspect, float nz, float farz);

	const DirectX::XMFLOAT4X4& GetProjMatrix() const;
	DirectX::BoundingFrustum   GetFrustum()    const;

private:
	float mNearZ;			 
	float mFarZ;
	float mFovY;	

	float mNearPlaneWidth;
	float mNearPlaneHeight;

	DirectX::XMFLOAT4X4 mProjMatrix;
};