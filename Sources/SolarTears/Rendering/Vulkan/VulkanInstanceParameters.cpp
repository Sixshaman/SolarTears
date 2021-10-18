#include "VulkanInstanceParameters.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include <VulkanGenericStructures.h>
#include <unordered_set>
#include <array>

namespace Vulkan
{
	//Enable KHRONOS validation
	static const std::vector<std::string> global_config_instance_layers =
	{
	#if (defined(DEBUG) || defined(_DEBUG))
		"VK_LAYER_KHRONOS_validation",
	#endif // (DEBUG) || defined(_DEBUG)
	};

	//Add/remove extensions here
	static const std::vector<std::string> global_config_instance_extensions =
	{
	#if defined(DEBUG) || defined(_DEBUG)
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,         //DEBUG UTILS
		VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, //VVvvvvvvvvvalidation!
	#endif

		VK_KHR_DISPLAY_EXTENSION_NAME,                    //Display enumeration!
		VK_KHR_GET_DISPLAY_PROPERTIES_2_EXTENSION_NAME,   //Display properties (extended)!
		VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,        //Exclusive display control!

		VK_KHR_SURFACE_EXTENSION_NAME,                    //Surfaces, because I need to render to something!
		VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, //Extended surface capabilities!
		VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME,           //Window-independent surfaces!!!

		//Sufaces
	#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
		VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
	#elif defined(VK_USE_PLATFORM_XCB_KHR)
		VK_KHR_XCB_SURFACE_EXTENSION_NAME,
	#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
		VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME,
	#endif
	};

#if defined(DEBUG) || defined(_DEBUG)
	static const std::array global_enabled_validation_features = {VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT};
#endif

	//TODO: designated structure initializers
	static const std::unordered_map<std::string, vgs::StructureBlob> global_config_instance_extension_structures =
	{
#if defined(DEBUG) || defined(_DEBUG)
		//Set the callback that will be executed during vkCreateInstance()/vkDestroyInstance()
		{
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			vgs::StructureBlob
			(
				VkDebugUtilsMessengerCreateInfoEXT
				{
					.flags           = 0,
					.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
					.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
					.pfnUserCallback = VulkanUtils::DebugReportCallback,
					.pUserData       = nullptr
				}
			)
		},

		{
			VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,
			vgs::StructureBlob
			(
				//Enable:  GPU-based validation, synchronization validation
				//Disable: nothing
				VkValidationFeaturesEXT
				{
					.enabledValidationFeatureCount  = 2,
					.pEnabledValidationFeatures     = global_enabled_validation_features.data(),
					.disabledValidationFeatureCount = (uint32_t)(global_enabled_validation_features.size()),
					.pDisabledValidationFeatures    = nullptr
				}
			)
		}
#endif
	};
}

const std::unordered_map<std::string, size_t> Vulkan::InstanceParameters::global_config_instance_extension_flags_offsets =
{
	{
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		offsetof(InstanceExtensionFlags, DebugUtilsExtensionEnabled)
	},

	{
		VK_KHR_DISPLAY_EXTENSION_NAME,
		offsetof(InstanceExtensionFlags, DisplayExtensionPresent)
	},

	{
		VK_KHR_GET_DISPLAY_PROPERTIES_2_EXTENSION_NAME,
		offsetof(InstanceExtensionFlags, DisplayProperties2ExtensionPresent)
	},

	{
		VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
		offsetof(InstanceExtensionFlags, DirectModeDisplayExtensionPresent)
	},

	{
		VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
		offsetof(InstanceExtensionFlags, SurfaceCapabilities2ExtensionPresent)
	},

	{
		VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME,
		offsetof(InstanceExtensionFlags, HeadlessSurfaceExtensionPresent)
	}
};

Vulkan::InstanceParameters::InstanceParameters(LoggerQueue* loggingBoard): mLoggingBoard(loggingBoard)
{
	assert(global_config_instance_extension_flags_offsets.size() * sizeof(bool) == sizeof(InstanceExtensionFlags));

	memset(&mEnabledExtensionFlags, 0, sizeof(InstanceExtensionFlags));
}

Vulkan::InstanceParameters::~InstanceParameters()
{
}

void Vulkan::InstanceParameters::InvalidateInstanceLayers(std::vector<std::string>& outEnabledLayers)
{
	outEnabledLayers.clear();
	std::vector<VkLayerProperties> availableLayers;
	
	uint32_t layerCount = 0;
	ThrowIfFailed(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

	availableLayers.resize(layerCount);
	ThrowIfFailed(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()));

	for(const std::string& requiredLayer: global_config_instance_layers)
	{
		auto layerIt = std::find_if(availableLayers.begin(), availableLayers.end(), [requiredLayer](const VkLayerProperties& layerProperties)
		{
			return layerProperties.layerName == requiredLayer;
		});

		if(layerIt != availableLayers.end())
		{
			outEnabledLayers.push_back(requiredLayer.c_str());
		}
		else
		{
			mLoggingBoard->PostLogMessage("Vulkan initialization: layer " + requiredLayer + " is not supported");
		}
	}
}

void Vulkan::InstanceParameters::InvalidateInstanceExtensions(std::vector<std::string>& outEnabledExtensions, vgs::StructureChainBlob<VkInstanceCreateInfo>& outStructureChain)
{
	outEnabledExtensions.clear();

	memset(&mEnabledExtensionFlags, 0, sizeof(InstanceExtensionFlags));
	
	//Enable debug callback and extended validation if these are available
	std::vector<VkExtensionProperties> availableExtensions;

	uint32_t extensionCount = 0;
	ThrowIfFailed(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

	availableExtensions.resize(extensionCount);
	ThrowIfFailed(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()));

	std::unordered_set<std::string> availableExtensionNames;
	for(const VkExtensionProperties& extension: availableExtensions)
	{
		availableExtensionNames.insert(extension.extensionName);
	}

	for(const std::string& extension: global_config_instance_extensions)
	{
		if(availableExtensionNames.contains(extension))
		{
			outEnabledExtensions.push_back(extension);

			auto offsetIt = global_config_instance_extension_flags_offsets.find(extension);
			if(offsetIt != global_config_instance_extension_flags_offsets.end())
			{
				assert(offsetIt->second + sizeof(bool) <= sizeof(InstanceExtensionFlags));

				bool* extFlagPtr = (bool*)(((std::byte*)&mEnabledExtensionFlags) + offsetIt->second);
				*extFlagPtr      = true; 
			}
		}
		else
		{
			mLoggingBoard->PostLogMessage("Vulkan initialization: extension " + extension + " is not supported");
		}
	}

	for(auto it = global_config_instance_extension_structures.begin(); it != global_config_instance_extension_structures.end(); it++)
	{
		if(availableExtensionNames.contains(it->first))
		{
			outStructureChain.AppendToChainGeneric(it->second);
		}
	}
}

bool Vulkan::InstanceParameters::IsDebugUtilsExtensionEnabled() const
{
	return mEnabledExtensionFlags.DebugUtilsExtensionEnabled;
}

bool Vulkan::InstanceParameters::IsDisplayExtensionEnabled() const
{
	return mEnabledExtensionFlags.DisplayExtensionPresent;
}

bool Vulkan::InstanceParameters::IsDisplayProperties2ExtensionEnabled() const
{
	return mEnabledExtensionFlags.DisplayProperties2ExtensionPresent;
}

bool Vulkan::InstanceParameters::IsDirectModeDisplayExtensionEnabled() const
{
	return mEnabledExtensionFlags.DirectModeDisplayExtensionPresent;
}

bool Vulkan::InstanceParameters::IsSurfaceCapabilities2ExtensionEnabled() const
{
	return mEnabledExtensionFlags.SurfaceCapabilities2ExtensionPresent;
}

bool Vulkan::InstanceParameters::IsHeadlessSurfaceExtensionEnabled() const
{
	return mEnabledExtensionFlags.HeadlessSurfaceExtensionPresent;
}
