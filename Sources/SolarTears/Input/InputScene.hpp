#pragma once

#include "InputSceneMisc.hpp"
#include "InputControlLocation.hpp"
#include "ControlCodes.hpp"
#include <vector>

class InputScene
{
	friend class InputSceneBuilder;

	//Todo: cache-friendliness (?)
	struct InputSceneObject
	{
		InputControlLocation        CurrentLocation;
		InputControlPressedCallback KeyPressedCallbacks[(uint8_t)ControlCode::Count];
		bool                        DirtyFlag;
	};

public:
	InputScene();
	~InputScene();

	//Updates the location of all objects controlled by various inputs
	void UpdateScene(uint32_t controlState, float dt);

	bool                IsLocationChanged(InputSceneControlHandle inputControlHandle);
	SceneObjectLocation GetUpdatedLocation(InputSceneControlHandle inputControlHandle);

private:
	std::vector<InputSceneObject> mInputSceneObjects;
};