#include "InputSceneBuilder.hpp"

InputSceneBuilder::InputSceneBuilder(InputScene* sceneToBuild): mSceneToBuild(sceneToBuild)
{
}

InputSceneBuilder::~InputSceneBuilder()
{
}

InputSceneControlHandle InputSceneBuilder::AddInputObject(const SceneObjectLocation& BaseSceneObjectLocation)
{
	InputScene::InputSceneObject inputObject = {.CurrentLocation = InputControlLocation(BaseSceneObjectLocation)};
	std::fill(inputObject.KeyPressedCallbacks, inputObject.KeyPressedCallbacks + (size_t)(ControlCode::Count), nullptr);
	inputObject.DirtyFlag = true;

	mSceneToBuild->mInputSceneObjects.emplace_back(inputObject);

	InputSceneControlHandle sceneControlHandle;
	sceneControlHandle.Id = (uint32_t)(mSceneToBuild->mInputSceneObjects.size() - 1);
	return sceneControlHandle;
}

void InputSceneBuilder::SetInputObjectKeyCallback(InputSceneControlHandle controlHandle, ControlCode controlCode, InputControlPressedCallback controlCallback)
{
	mSceneToBuild->mInputSceneObjects[controlHandle.Id].KeyPressedCallbacks[(uint8_t)(controlCode)] = controlCallback;
}