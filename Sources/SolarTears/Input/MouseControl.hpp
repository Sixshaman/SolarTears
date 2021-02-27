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

	uint32_t GetControlState();

	float GetXDelta();
	float GetYDelta();
	float GetWheelDelta();

private:
	void InitKeyMap();

private:
	std::unique_ptr<MouseKeyMap> mKeyMap;

	uint32_t mControlState;
};