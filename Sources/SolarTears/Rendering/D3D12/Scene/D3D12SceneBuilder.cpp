#include "D3D12SceneBuilder.hpp"
#include "../D3D12Utils.hpp"
#include "../../../../3rd party/DirectXTK12/Inc/DDSTextureLoader.h"
#include <array>
#include <cassert>

D3D12::RenderableSceneBuilder::RenderableSceneBuilder(RenderableScene* sceneToBuild): mSceneToBuild(sceneToBuild)
{
	assert(sceneToBuild != nullptr);
}

D3D12::RenderableSceneBuilder::~RenderableSceneBuilder()
{
}