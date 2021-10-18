#pragma once

#include <vulkan/vulkan.h>
#include <array>

namespace Vulkan
{
	class SamplerManager
	{
		enum class SamplerType: uint32_t
		{
			Linear = 0,
			Anisotropic,

			Count
		};

		static constexpr uint32_t TotalSamplers = (uint32_t)(SamplerType::Count);

	public:
		SamplerManager(VkDevice device);
		~SamplerManager();

		const VkSampler* GetSamplerVariableArray()       const;
		const size_t     GetSamplerVariableArrayLength() const;

		VkSampler GetLinearSampler()      const;
		VkSampler GetAnisotropicSampler() const;

	private:
		void InitializeSamplers();

	private:
		const VkDevice mDeviceRef;

		std::array<VkSampler, TotalSamplers> mSamplers;
	};
}