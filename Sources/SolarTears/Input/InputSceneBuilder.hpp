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

private:
	InputScene* mSceneToBuild;
};