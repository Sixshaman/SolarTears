#include "ClassicRenderableScene.hpp"
#include "../RenderingUtils.hpp"
#include "../../../Core/FrameCounter.hpp"

ClassicRenderableScene::ClassicRenderableScene(): RenderableSceneBase(1)
{
	mScheduledSceneUpdates.push_back(RenderableSceneBase::ScheduledSceneUpdate());
	mObjectDataScheduledUpdateIndices.push_back(0);
	mScenePerObjectData.push_back(RenderableSceneBase::PerObjectData());
}

ClassicRenderableScene::~ClassicRenderableScene()
{
}