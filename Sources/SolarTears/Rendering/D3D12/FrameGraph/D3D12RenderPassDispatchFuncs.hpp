#pragma once

#include "../../Common/FrameGraph/RenderPassDispatchFuncs.hpp"
#include "Passes/D3D12GBufferPass.hpp"
#include "Passes/D3D12CopyImagePass.hpp"
#include "D3D12FrameGraphMisc.hpp"
#include <memory>

namespace D3D12
{
	class FrameGraphBuilder;

	template<typename Pass>
	constexpr void RegisterPassSubresources(std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		return Pass::RegisterSubresources(metadataPayloads);
	}

	constexpr uint_fast16_t RegisterPassSubresources(RenderPassType passType, std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		using RegisterSubresourcesFunc = void(*)(std::span<SubresourceMetadataPayload>);

		RegisterSubresourcesFunc RegisterSubresources = nullptr;
		CHOOSE_PASS_FUNCTION(passType, RegisterPassSubresources, RegisterSubresources);

		assert(RegisterSubresources != nullptr);
		RegisterSubresources(metadataPayloads);
	}

	template<typename Pass>
	std::unique_ptr<RenderPass> MakeUniquePass(const FrameGraphBuilder* builder, const std::string& passName, uint32_t frame)
	{
		return std::make_unique<Pass>(builder, passName, frame);
	};

	std::unique_ptr<RenderPass> MakeUniquePass(RenderPassType passType, const FrameGraphBuilder* builder, const std::string& passName, uint32_t frame)
	{ 
		using MakePassFunc = std::unique_ptr<RenderPass>(*)(const FrameGraphBuilder*, const std::string&, uint32_t);

		MakePassFunc MakePass;
		CHOOSE_PASS_FUNCTION(passType, MakeUniquePass, MakePass);

		assert(MakePass != nullptr);
		return MakePass(builder, passName, frame);
	}
}