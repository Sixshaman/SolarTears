#include "Camera.hpp"

Camera::Camera()
{
	float fovY   = DirectX::XM_PI / 4.0f;
	float aspect = 1.0f;
	float znear  = 0.01f;
	float zfar   = 1000.0f;

	SetProjectionParameters(fovY, aspect, znear, zfar);
}

Camera::~Camera()
{
}

float Camera::GetNearZ() const
{
	return mNearZ; 
}

float Camera::GetFarZ() const
{
	return mFarZ;
}

float Camera::GetAspectRatio() const
{
	return mNearPlaneWidth / mNearPlaneHeight;
}

float Camera::GetFovY() const
{
	return mFovY;
}

float Camera::GetFovX() const
{
	float halfWidth = 0.5f* GetNearPlaneWidth();
	return 2.0f*atanf(halfWidth/mNearZ);
}

float Camera::GetNearPlaneWidth() const
{
	return mNearPlaneWidth;
}

float Camera::GetNearPlaneHeight() const
{
	return mNearPlaneHeight;
}

void Camera::SetProjectionParameters(float fovY, float viewportAspect, float zn, float zf)
{
	mFovY   = fovY;    
	mNearZ  = zn;
	mFarZ   = zf;	

	mNearPlaneHeight = 2.0f*mNearZ*tanf(0.5f * mFovY); 
	mNearPlaneWidth  = mNearPlaneHeight * viewportAspect;
}

DirectX::XMMATRIX Camera::GetProjMatrix() const
{
	return XMLoadFloat4x4(&mProj);
}

DirectX::BoundingFrustum Camera::GetFrustum() const
{
	DirectX::BoundingFrustum frustum = DirectX::BoundingFrustum(GetProjMatrix());
	return frustum;
}

void Camera::RecalcProjMatrix()
{
	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(mFovY, mNearPlaneWidth / mNearPlaneHeight, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
}