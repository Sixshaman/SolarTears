#include "OGLSceneBuilder.hpp"

OpenGL::RenderableSceneBuilder::RenderableSceneBuilder(RenderableScene* sceneToBuild): ClassicRenderableSceneBuilder(sceneToBuild), mOGLSceneToBuild(sceneToBuild)
{
}

OpenGL::RenderableSceneBuilder::~RenderableSceneBuilder()
{
}