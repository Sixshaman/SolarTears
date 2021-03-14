#pragma once

#include <memory>

class Window;
class MouseKeyMap;

class MouseControl
{
public:
	MouseControl();
	~MouseControl();

	void AttachToWindow(Window* window);

	uint32_t GetControlState() const;

	float GetXDelta()     const;
	float GetYDelta()     const;
	float GetWheelDelta() const;

	void Reset();

private:
	void InitKeyMap();

private:
	std::unique_ptr<MouseKeyMap> mKeyMap;

	uint32_t mControlState;

	float mXDelta;
	float mYDelta;
	float mWheelDelta;

	int32_t mPrevMousePosX;
	int32_t mPrevMousePosY;
};