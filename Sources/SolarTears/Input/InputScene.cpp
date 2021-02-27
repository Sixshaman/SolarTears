#include "InputScene.hpp"
#include "ControlCodes.hpp"

InputScene::InputScene()
{
}

InputScene::~InputScene()
{
}

void InputScene::UpdateScene(uint32_t controlState, float axis2DeltaX, float axis2DeltaY, float dt)
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

		if(mInputSceneObjects[i].Axis1Callback != nullptr)
		{
			mInputSceneObjects[i].Axis1Callback(&mInputSceneObjects[i].CurrentLocation, 0.0f, 0.0f, dt);
			locationChanged = true;
		}

		if(mInputSceneObjects[i].Axis2Callback != nullptr && fabs(axis2DeltaX) > 0.0001f && fabs(axis2DeltaY) > 0.0001f)
		{
			mInputSceneObjects[i].Axis2Callback(&mInputSceneObjects[i].CurrentLocation, axis2DeltaX, axis2DeltaY, dt);
			locationChanged = true;
		}

		if(mInputSceneObjects[i].Axis3Callback != nullptr)
		{
			mInputSceneObjects[i].Axis3Callback(&mInputSceneObjects[i].CurrentLocation, 0.0f, 0.0f, dt);
			locationChanged = true;
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
