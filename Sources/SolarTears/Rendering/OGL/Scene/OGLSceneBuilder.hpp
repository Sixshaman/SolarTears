#pragma once

#include <vector>
#include <string>
#include <memory>
#include "OGLScene.hpp"
#include "../OGLUtils.hpp"
#include "../../Common/Scene/ClassicRenderableSceneBuilder.hpp"

namespace OpenGL
{
	class RenderableSceneBuilder : public ClassicRenderableSceneBuilder
	{
	public:
		RenderableSceneBuilder(RenderableScene* sceneToBuild);
		~RenderableSceneBuilder();

	private:
		RenderableScene* mOGLSceneToBuild;
	};
}