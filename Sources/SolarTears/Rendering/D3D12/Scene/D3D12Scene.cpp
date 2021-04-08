#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../../Core/Scene/SceneDescription.hpp"
#include "../../RenderableSceneMisc.hpp"
#include <array>

D3D12::RenderableScene::RenderableScene(const FrameCounter* frameCounter) :RenderableSceneBase(D3D12Utils::InFlightFrameCount), mFrameCounterRef(frameCounter)
{
	mScheduledSceneUpdates.push_back(RenderableSceneBase::ScheduledSceneUpdate());
	mObjectDataScheduledUpdateIndices.push_back(0);
	mScenePerObjectData.push_back(RenderableSceneBase::PerObjectData());
}

D3D12::RenderableScene::~RenderableScene()
{
}

void D3D12::RenderableScene::FinalizeSceneUpdating()
{
}