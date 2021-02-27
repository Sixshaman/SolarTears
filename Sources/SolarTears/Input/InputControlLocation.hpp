#pragma once

#include "../Core/Scene/SceneObjectLocation.hpp"

class InputControlLocation
{
public:
	InputControlLocation(const SceneObjectLocation& sceneObjectLocation);
	~InputControlLocation();

	void Walk(float d);
	void Strafe(float d);
	void Float(float d);

	SceneObjectLocation GetLocation() const;

private:
	SceneObjectLocation mCurrentLocation;

	DirectX::XMFLOAT3 mPitchAxis; //Object's own X axis in world space coordinates
	DirectX::XMFLOAT3 mYawAxis;   //Object's own Y axis in world space coordinates
	DirectX::XMFLOAT3 mRollAxis;  //Object's own Z axis in world space coordinates
};