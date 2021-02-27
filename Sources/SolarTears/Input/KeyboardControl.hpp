#pragma once

#include <memory>

class Window;
class KeyboardKeyMap;

class KeyboardControl
{
public:
	KeyboardControl();
	~KeyboardControl();

	void AttachToWindow(Window* window);

	uint32_t GetControlState();

private:
	void InitKeyMap();

private:
	std::unique_ptr<KeyboardKeyMap> mKeyMap;

	uint32_t mControlState;
};