#pragma once

#include "InputScene.hpp"
#include "InputSceneMisc.hpp"
#include "ControlCodes.hpp"

class InputSceneBuilder
{
public:
	InputSceneBuilder(InputScene* sceneToBuild);
	~InputSceneBuilder();

	InputSceneControlHandle AddInputObject(const SceneObjectLocation& BaseSceneObjectLocation);

	void SetInputObjectKeyCallback(InputSceneControlHandle controlHandle, ControlCode controlCode, InputControlPressedCallback controlCallback);

	void SetInputObjectAxis1Callback(InputSceneControlHandle controlHandle, InputAxisMoveCallback axisCallback);
	void SetInputObjectAxis2Callback(InputSceneControlHandle controlHandle, InputAxisMoveCallback axisCallback);
	void SetInputObjectAxis3Callback(InputSceneControlHandle controlHandle, InputAxisMoveCallback axisCallback);

private:
	InputScene* mSceneToBuild;
};