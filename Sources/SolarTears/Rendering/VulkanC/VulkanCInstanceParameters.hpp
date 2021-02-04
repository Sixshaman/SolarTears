#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include "../../Logging/LoggerQueue.hpp"

namespace vgs
{
	template<typename T> class StructureChainBlob;
}

namespace VulkanCBindings
{
	class InstanceParameters
	{
		struct InstanceExtensionFlags
		{
			bool DebugUtilsExtensionEnabled;
			bool DisplayExtensionPresent;
			bool DisplayProperties2ExtensionPresent;
			bool DirectModeDisplayExtensionPresent;
			bool SurfaceCapabilities2ExtensionPresent;
			bool HeadlessSurfaceExtensionPresent;
		};

		static const std::unordered_map<std::string, size_t> global_config_instance_extension_flags_offsets;

	public:
		InstanceParameters(LoggerQueue* loggingBoard);
		~InstanceParameters();

		void InvalidateInstanceLayers(std::vector<std::string>& outEnabledLayers);

		void InvalidateInstanceExtensions(std::vector<std::string>& outEnabledExtensions, vgs::StructureChainBlob<VkInstanceCreateInfo>& outStructureChain);

		bool IsDebugUtilsExtensionEnabled()           const;
		bool IsDisplayExtensionEnabled()              const;
		bool IsDisplayProperties2ExtensionEnabled()   const;
		bool IsDirectModeDisplayExtensionEnabled()    const;
		bool IsSurfaceCapabilities2ExtensionEnabled() const;
		bool IsHeadlessSurfaceExtensionEnabled()      const;

	private:
		LoggerQueue* mLoggingBoard;

		InstanceExtensionFlags mEnabledExtensionFlags;
	};
}