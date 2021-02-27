#include "InputScene.hpp"
#include "ControlCodes.hpp"

InputScene::InputScene()
{
}

InputScene::~InputScene()
{
}

void InputScene::UpdateScene(uint32_t controlState, float dt)
{
	for(size_t i = 0; i < mInputSceneObjects.size(); i++)
	{
		bool locationChanged = false;
		for(uint8_t ctrl = 0; ctrl < (uint8_t)(ControlCode::Count); ctrl++)
		{
			if(controlState & (1u << (uint32_t)(ctrl)))
			{
				if(mInputSceneObjects[i].KeyPressedCallbacks[ctrl] != nullptr) //Checking for null is probably faster than calling the default one
				{
					mInputSceneObjects[i].KeyPressedCallbacks[ctrl](&mInputSceneObjects[i].CurrentLocation, dt);
					locationChanged = true;
				}
			}
		}

		mInputSceneObjects[i].DirtyFlag = locationChanged;
	}
}

bool InputScene::IsLocationChanged(InputSceneControlHandle inputControlHandle)
{
	return mInputSceneObjects[inputControlHandle.Id].DirtyFlag;
}

SceneObjectLocation InputScene::GetUpdatedLocation(InputSceneControlHandle inputControlHandle)
{
	return mInputSceneObjects[inputControlHandle.Id].CurrentLocation.GetLocation();
}
