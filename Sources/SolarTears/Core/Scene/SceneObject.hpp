#pragma once

#include "../../Rendering/Common/Scene/RenderableSceneMisc.hpp"
#include "SceneObjectLocation.hpp"

struct SceneObject
{
	const uint64_t Id;

	SceneObjectLocation Location;

	RenderableSceneObjectHandle RenderableHandle;
};