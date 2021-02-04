#include "VulkanCInstanceParameters.hpp"
#include "../../../3rd party/VulkanGenericStructures/Include/VulkanGenericStructures.h"
#include "VulkanCUtils.hpp"
#include "VulkanCFunctions.hpp"
#include <unordered_set>
#include <array>

namespace VulkanCBindings
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
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,        //Debug reports!
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

	//TODO: designated structure initializers
	static const std::unordered_map<std::string, vgs::StructureBlob> global_config_instance_extension_structures =
	{
		{
			VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
			vgs::StructureBlob
			(
				VkDebugReportCallbackCreateInfoEXT
				{
					.flags       = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
					.pfnCallback = VulkanUtils::DebugReportCCallback,
					.pUserData   = nullptr
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
					.pEnabledValidationFeatures     = std::array<VkValidationFeatureEnableEXT, 2>({VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT}).data(),
					.disabledValidationFeatureCount = 0,
					.pDisabledValidationFeatures    = nullptr
				}
			)
		}
	};
}

const std::unordered_map<std::string, size_t> VulkanCBindings::InstanceParameters::global_config_instance_extension_flags_offsets =
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

VulkanCBindings::InstanceParameters::InstanceParameters(LoggerQueue* loggingBoard): mLoggingBoard(loggingBoard)
{
	assert(global_config_instance_extension_flags_offsets.size() * sizeof(bool) == sizeof(InstanceExtensionFlags));

	memset(&mEnabledExtensionFlags, 0, sizeof(InstanceExtensionFlags));
}

VulkanCBindings::InstanceParameters::~InstanceParameters()
{
}

void VulkanCBindings::InstanceParameters::InvalidateInstanceLayers(std::vector<std::string>& outEnabledLayers)
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

void VulkanCBindings::InstanceParameters::InvalidateInstanceExtensions(std::vector<std::string>& outEnabledExtensions, vgs::StructureChainBlob<VkInstanceCreateInfo>& outStructureChain)
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

bool VulkanCBindings::InstanceParameters::IsDebugUtilsExtensionEnabled() const
{
	return mEnabledExtensionFlags.DebugUtilsExtensionEnabled;
}

bool VulkanCBindings::InstanceParameters::IsDisplayExtensionEnabled() const
{
	return mEnabledExtensionFlags.DisplayExtensionPresent;
}

bool VulkanCBindings::InstanceParameters::IsDisplayProperties2ExtensionEnabled() const
{
	return mEnabledExtensionFlags.DisplayProperties2ExtensionPresent;
}

bool VulkanCBindings::InstanceParameters::IsDirectModeDisplayExtensionEnabled() const
{
	return mEnabledExtensionFlags.DirectModeDisplayExtensionPresent;
}

bool VulkanCBindings::InstanceParameters::IsSurfaceCapabilities2ExtensionEnabled() const
{
	return mEnabledExtensionFlags.SurfaceCapabilities2ExtensionPresent;
}

bool VulkanCBindings::InstanceParameters::IsHeadlessSurfaceExtensionEnabled() const
{
	return mEnabledExtensionFlags.HeadlessSurfaceExtensionPresent;
}
