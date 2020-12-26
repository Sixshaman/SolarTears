#include "Camera.hpp"

Camera::Camera()
{
	DirectX::XMVECTOR pos     = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	DirectX::XMVECTOR look    = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
	DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

	float fovY   = DirectX::XM_PI / 4.0f;
	float aspect = 1.0f;
	float znear  = 0.01f;
	float zfar   = 1000.0f;

	SetCameraSpace(pos, look, worldUp);
	SetProjectionParameters(fovY, aspect, znear, zfar);
}

Camera::~Camera()
{
}

DirectX::XMVECTOR Camera::GetPosition() const
{
	return DirectX::XMLoadFloat3(&mPosition);
}

DirectX::XMVECTOR Camera::GetRight() const
{
	return DirectX::XMLoadFloat3(&mRight);
}

DirectX::XMVECTOR Camera::GetUp() const
{
	return DirectX::XMLoadFloat3(&mUp);
}

DirectX::XMVECTOR Camera::GetLook() const
{
	return DirectX::XMLoadFloat3(&mLook);
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

void Camera::SetCameraSpace(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp)
{
	DirectX::XMVECTOR Look  = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target, pos)); //Look = (target-pos)/|target - pos|
	DirectX::XMVECTOR Right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, Look)); //Right = (worldUp x Look) / |(worldUp x Look)|
	DirectX::XMVECTOR Up    = DirectX::XMVector3Cross(Look, Right);                                //Up = Look x Right

	XMStoreFloat3(&mPosition, pos);
	XMStoreFloat3(&mLook,     Look);
	XMStoreFloat3(&mRight,    Right);
	XMStoreFloat3(&mUp,       Up);
}

DirectX::XMMATRIX Camera::GetViewMatrix() const
{
	return XMLoadFloat4x4(&mView);
}

DirectX::XMMATRIX Camera::GetProjMatrix() const
{
	return XMLoadFloat4x4(&mProj);
}

DirectX::BoundingFrustum Camera::GetFrustum() const
{
	DirectX::BoundingFrustum frustum = DirectX::BoundingFrustum(GetProjMatrix());

	DirectX::XMMATRIX view = GetViewMatrix();

	//TODO: faster inverse (without actually inverting the matrix)
	DirectX::XMVECTOR viewMatrixDeterminant = DirectX::XMVectorSplatOne();
	DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&viewMatrixDeterminant, view);

	frustum.Transform(frustum, invView);
	return frustum;
}

void Camera::WalkSideways(float distance)
{
	DirectX::XMVECTOR currentPosition = DirectX::XMLoadFloat3(&mPosition);
	DirectX::XMVECTOR moveAmount      = DirectX::XMVectorReplicate(distance);

	DirectX::XMVECTOR right = DirectX::XMLoadFloat3(&mRight);

	XMStoreFloat3(&mPosition, DirectX::XMVectorMultiplyAdd(moveAmount, right, currentPosition)); //mPosition += d*mRight
}

void Camera::WalkForward(float distance)
{
	DirectX::XMVECTOR currentPosition = DirectX::XMLoadFloat3(&mPosition);
	DirectX::XMVECTOR moveAmount      = DirectX::XMVectorReplicate(distance);

	DirectX::XMVECTOR look = DirectX::XMLoadFloat3(&mLook);

	XMStoreFloat3(&mPosition, DirectX::XMVectorMultiplyAdd(moveAmount, look, currentPosition)); //mPosition += d*mLook
}

void Camera::Pitch(float angle)
{
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&mRight), angle);

	if(!(mLook.y > 0.995f && angle < 0) && !(mLook.y < -0.995f && angle > 0))
	{
		XMStoreFloat3(&mUp,   DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), R));
		XMStoreFloat3(&mLook, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), R));
	}
}

void Camera::Yaw(float angle)
{
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationY(angle);

	XMStoreFloat3(&mRight, XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp,    XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook,  XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), R));
}

void Camera::Roll(float angle)
{
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationNormal(DirectX::XMLoadFloat3(&mLook), angle);

	XMStoreFloat3(&mUp,    DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp),    R));
	XMStoreFloat3(&mRight, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), R));
}

void Camera::RecalcViewMatrix()
{
	DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&mRight);
	DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&mUp);
	DirectX::XMVECTOR L = DirectX::XMLoadFloat3(&mLook);
	DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&mPosition);

	L = DirectX::XMVector3Normalize(L);
	U = DirectX::XMVector3Cross(L, R);
	R = DirectX::XMVector3Cross(U, L);

	float x = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, R));
	float y = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, U));
	float z = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, L));

	XMStoreFloat3(&mRight, R); //mRight = R
	XMStoreFloat3(&mUp, U);	   //mUp = U
	XMStoreFloat3(&mLook, L);  //mLook = L

	mView(0, 0) = mRight.x; //ux
	mView(1, 0) = mRight.y; //uy
	mView(2, 0) = mRight.z; //uz
	mView(3, 0) = x;		//-Q*u

	mView(0, 1) = mUp.x; //vx
	mView(1, 1) = mUp.y; //vy
	mView(2, 1) = mUp.z; //vz
	mView(3, 1) = y;	 //-Q*v

	mView(0, 2) = mLook.x; //wx
	mView(1, 2) = mLook.y; //wy
	mView(2, 2) = mLook.z; //wz
	mView(3, 2) = z;	   //-Q*w

	mView(0, 3) = 0.0f; //0
	mView(1, 3) = 0.0f; //0
	mView(2, 3) = 0.0f; //0
	mView(3, 3) = 1.0f;	//1
}

void Camera::RecalcProjMatrix()
{
	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(mFovY, mNearPlaneWidth / mNearPlaneHeight, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
}