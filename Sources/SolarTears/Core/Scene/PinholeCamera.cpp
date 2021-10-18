#include "PinholeCamera.hpp"

PinholeCamera::PinholeCamera()
{
	float fovY   = DirectX::XM_PI / 4.0f;
	float aspect = 1.0f;
	float znear  = 0.01f;
	float zfar   = 1000.0f;

	SetProjectionParameters(fovY, aspect, znear, zfar);
}

PinholeCamera::~PinholeCamera()
{
}

float PinholeCamera::GetNearZ() const
{
	return mNearZ; 
}

float PinholeCamera::GetFarZ() const
{
	return mFarZ;
}

float PinholeCamera::GetAspectRatio() const
{
	return mNearPlaneWidth / mNearPlaneHeight;
}

float PinholeCamera::GetFovY() const
{
	return mFovY;
}

float PinholeCamera::GetFovX() const
{
	float halfWidth = 0.5f * GetNearPlaneWidth();
	return 2.0f*atanf(halfWidth/mNearZ);
}

float PinholeCamera::GetNearPlaneWidth() const
{
	return mNearPlaneWidth;
}

float PinholeCamera::GetNearPlaneHeight() const
{
	return mNearPlaneHeight;
}

void PinholeCamera::SetProjectionParameters(float fovY, float viewportAspect, float zn, float zf)
{
	mFovY   = fovY;    
	mNearZ  = zn;
	mFarZ   = zf;	

	mNearPlaneHeight = 2.0f*mNearZ*tanf(0.5f * mFovY); 
	mNearPlaneWidth  = mNearPlaneHeight * viewportAspect;

	XMStoreFloat4x4(&mProjMatrix, DirectX::XMMatrixPerspectiveFovLH(mFovY, mNearPlaneWidth / mNearPlaneHeight, mNearZ, mFarZ));
}

const DirectX::XMFLOAT4X4& PinholeCamera::GetProjMatrix() const
{
	return mProjMatrix;
}

DirectX::BoundingFrustum PinholeCamera::GetFrustum() const
{
	DirectX::XMFLOAT4X4 projMatrix = GetProjMatrix();
	DirectX::BoundingFrustum frustum = DirectX::BoundingFrustum(DirectX::XMLoadFloat4x4(&projMatrix));
	return frustum;
}