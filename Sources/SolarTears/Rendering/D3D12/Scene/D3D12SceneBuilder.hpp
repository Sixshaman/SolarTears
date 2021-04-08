#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>
#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../RenderableSceneBuilderBase.hpp"

namespace D3D12
{
	class RenderableSceneBuilder: public RenderableSceneBuilderBase
	{
	public:
		RenderableSceneBuilder(RenderableScene* sceneToBuild);
		~RenderableSceneBuilder();

	public:
		static constexpr size_t   GetVertexSize();

		static constexpr DXGI_FORMAT GetVertexPositionFormat();
		static constexpr DXGI_FORMAT GetVertexNormalFormat();
		static constexpr DXGI_FORMAT GetVertexTexcoordFormat();

		static constexpr uint32_t GetVertexPositionOffset();
		static constexpr uint32_t GetVertexNormalOffset();
		static constexpr uint32_t GetVertexTexcoordOffset();

	private:
		RenderableScene* mSceneToBuild; 
	};
}

#include "D3D12SceneBuilder.inl"