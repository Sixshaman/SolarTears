#pragma once

#include "../../Common/FrameGraph/RenderPassDispatchFuncs.hpp"
#include "Passes/VulkanGBufferPass.hpp"
#include "Passes/VulkanCopyImagePass.hpp"
#include "VulkanFrameGraphMisc.hpp"
#include <memory>
#include <vulkan/vulkan.h>

namespace Vulkan
{
	class FrameGraphBuilder;

	template<typename Pass>
	constexpr void RegisterPassSubresources(std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		return Pass::RegisterSubresources(metadataPayloads);
	}

	uint_fast16_t RegisterPassSubresources(RenderPassType passType, std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		using RegisterSubresourcesFunc = void(*)(std::span<SubresourceMetadataPayload>);

		RegisterSubresourcesFunc RegisterSubresources = nullptr;
		CHOOSE_PASS_FUNCTION(passType, RegisterPassSubresources, RegisterSubresources);

		assert(RegisterSubresources != nullptr);
		RegisterSubresources(metadataPayloads);
	}

	template<typename Pass>
	constexpr void RegisterPassShaders(ShaderDatabase* shaderDatabase)
	{
		return Pass::RegisterShaders(shaderDatabase);
	}

	constexpr void RegisterPassShaders(RenderPassType passType, ShaderDatabase* shaderDatabase)
	{
		using RegisterShadersFunc = void(*)(ShaderDatabase*);

		RegisterShadersFunc RegisterShaders = nullptr;
		CHOOSE_PASS_FUNCTION(passType, RegisterPassShaders, RegisterShaders);

		assert(RegisterShaders != nullptr);
		RegisterShaders(shaderDatabase);
	}

	template<typename Pass>
	bool PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		return Pass::PropagateSubresourceInfos(metadataPayloads);
	};

	bool PropagateSubresourceInfos(RenderPassType passType, std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		using PropagateInfosFunc = bool(*)(std::span<SubresourceMetadataPayload>);

		PropagateInfosFunc PropagateInfos = nullptr;
		CHOOSE_PASS_FUNCTION(passType, PropagateSubresourceInfos, PropagateInfos);

		assert(PropagateInfos != nullptr);
		PropagateInfos(metadataPayloads);
	}

	template<typename Pass>
	std::unique_ptr<RenderPass> MakeUniquePass(const FrameGraphBuilder* builder, uint32_t passIndex, uint32_t frame)
	{
		return std::make_unique<Pass>(builder, passIndex, frame);
	};

	std::unique_ptr<RenderPass> MakeUniquePass(RenderPassType passType, const FrameGraphBuilder* builder, uint32_t passIndex, uint32_t frame)
	{ 
		using MakePassFunc = std::unique_ptr<RenderPass>(*)(const FrameGraphBuilder*, uint32_t, uint32_t);

		MakePassFunc MakePass;
		CHOOSE_PASS_FUNCTION(passType, MakeUniquePass, MakePass);

		assert(MakePass != nullptr);
		return MakePass(builder, passIndex, frame);
	}
}