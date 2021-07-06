#pragma once

#include "RenderableSceneBuilderBase.hpp"

class ClassicRenderableScene;

class ClassicRenderableSceneBuilder: public RenderableSceneBuilderBase
{
public:
	ClassicRenderableSceneBuilder(ClassicRenderableScene* sceneToBuild);
	~ClassicRenderableSceneBuilder();

protected:
	ClassicRenderableScene* mSceneToBuild;
};