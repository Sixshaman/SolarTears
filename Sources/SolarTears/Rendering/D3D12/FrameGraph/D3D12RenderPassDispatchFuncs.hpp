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

	void RegisterPassSubresources(RenderPassType passType, std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		using RegisterSubresourcesFunc = void(*)(std::span<SubresourceMetadataPayload>);

		RegisterSubresourcesFunc RegisterSubresources = nullptr;
		CHOOSE_PASS_FUNCTION(passType, RegisterPassSubresources, RegisterSubresources);

		assert(RegisterSubresources != nullptr);
		RegisterSubresources(metadataPayloads);
	}

	template<typename Pass>
	constexpr bool PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		return Pass::PropagateSubresourceInfos(metadataPayloads);
	};

	bool PropagateSubresourceInfos(RenderPassType passType, std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		using PropagateInfosFunc = bool(*)(std::span<SubresourceMetadataPayload>);

		PropagateInfosFunc PropagateInfos = nullptr;
		CHOOSE_PASS_FUNCTION(passType, PropagateSubresourceInfos, PropagateInfos);

		assert(PropagateInfos != nullptr);
		return PropagateInfos(metadataPayloads);
	}

	template<typename Pass>
	std::unique_ptr<RenderPass> MakeUniquePass(const FrameGraphBuilder* frameGraphBuilder, uint32_t passId)
	{
		return std::make_unique<Pass>(frameGraphBuilder, passId);
	};

	std::unique_ptr<RenderPass> MakeUniquePass(RenderPassType passType, const FrameGraphBuilder* builder, uint32_t passId)
	{ 
		using MakePassFunc = std::unique_ptr<RenderPass>(*)(const FrameGraphBuilder*, uint32_t);

		MakePassFunc MakePass = nullptr;
		CHOOSE_PASS_FUNCTION(passType, MakeUniquePass, MakePass);

		assert(MakePass != nullptr);
		return MakePass(builder, passId);
	}
}