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
	inline constexpr void RegisterPassSubresources(std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		Pass::RegisterSubresources(metadataPayloads);
	}

	inline void RegisterPassSubresources(RenderPassType passType, std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		using RegisterSubresourcesFunc = void(*)(std::span<SubresourceMetadataPayload>);

		RegisterSubresourcesFunc RegisterSubresources = nullptr;
		CHOOSE_PASS_FUNCTION(passType, RegisterPassSubresources, RegisterSubresources);

		assert(RegisterSubresources != nullptr);
		RegisterSubresources(metadataPayloads);
	}

	template<typename Pass>
	inline constexpr void RegisterPassShaders(ShaderDatabase* shaderDatabase)
	{
		return Pass::RegisterShaders(shaderDatabase);
	}

	inline void RegisterPassShaders(RenderPassType passType, ShaderDatabase* shaderDatabase)
	{
		using RegisterShadersFunc = void(*)(ShaderDatabase*);

		RegisterShadersFunc RegisterShaders = nullptr;
		CHOOSE_PASS_FUNCTION(passType, RegisterPassShaders, RegisterShaders);

		assert(RegisterShaders != nullptr);
		RegisterShaders(shaderDatabase);
	}

	template<typename Pass>
	inline bool PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		return Pass::PropagateSubresourceInfos(metadataPayloads);
	};

	inline bool PropagateSubresourceInfos(RenderPassType passType, std::span<SubresourceMetadataPayload> metadataPayloads)
	{
		using PropagateInfosFunc = bool(*)(std::span<SubresourceMetadataPayload>);

		PropagateInfosFunc PropagateInfos = nullptr;
		CHOOSE_PASS_FUNCTION(passType, PropagateSubresourceInfos, PropagateInfos);

		assert(PropagateInfos != nullptr);
		return PropagateInfos(metadataPayloads);
	}

	template<typename Pass>
	inline VkDescriptorType GetPassSubresourceDescriptorType(uint_fast16_t subresourceIndex)
	{
		return Pass::GetSubresourceDescriptorType(subresourceIndex);
	};

	inline VkDescriptorType GetPassSubresourceDescriptorType(RenderPassType passType, uint_fast16_t subresourceIndex)
	{
		using GetDescriptorTypeFunc = VkDescriptorType(*)(uint_fast16_t);

		GetDescriptorTypeFunc GetDescriptorType = nullptr;
		CHOOSE_PASS_FUNCTION(passType, GetPassSubresourceDescriptorType, GetDescriptorType);

		assert(GetDescriptorType != nullptr);
		return GetDescriptorType(subresourceIndex);
	}

	template<typename Pass>
	inline std::unique_ptr<RenderPass> MakeUniquePass(const FrameGraphBuilder* builder, uint32_t passId)
	{
		return std::make_unique<Pass>(builder, passId);
	};

	inline std::unique_ptr<RenderPass> MakeUniquePass(RenderPassType passType, const FrameGraphBuilder* builder, uint32_t passId)
	{ 
		using MakePassFunc = std::unique_ptr<RenderPass>(*)(const FrameGraphBuilder*, uint32_t);

		MakePassFunc MakePass = nullptr;
		CHOOSE_PASS_FUNCTION(passType, MakeUniquePass, MakePass);

		assert(MakePass != nullptr);
		return MakePass(builder, passId);
	}
}