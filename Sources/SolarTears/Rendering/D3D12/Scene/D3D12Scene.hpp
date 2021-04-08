#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include "../../RenderableSceneBase.hpp"
#include "../../../Core/FrameCounter.hpp"

namespace D3D12
{
	class RenderableScene: public RenderableSceneBase
	{
		friend class RenderableSceneBuilder;

		struct SceneSubobject
		{
			uint32_t IndexCount;
			uint32_t FirstIndex;
			int32_t  VertexOffset;
			uint32_t TextureDescriptorSetIndex;

			//Material data will be set VIA ROOT CONSTANTS
		};

		struct MeshSubobjectRange
		{
			uint32_t FirstSubobjectIndex;
			uint32_t AfterLastSubobjectIndex;
		};

	public:
		RenderableScene(const FrameCounter* frameCounter);
		~RenderableScene();

		void FinalizeSceneUpdating() override;

	private:
		//Created from inside
		const FrameCounter* mFrameCounterRef;

	private:
		//Created from outside
	};
}