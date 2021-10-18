#include "VulkanFunctionsLibrary.hpp"
#include "VulkanFunctions.hpp"
#include <cassert>

Vulkan::FunctionsLibrary::FunctionsLibrary(): mLibrary(nullptr)
{
#ifdef WIN32
	mLibrary = LoadLibraryA("vulkan-1.dll");
	assert(mLibrary != 0);

	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)(GetProcAddress(mLibrary, "vkGetInstanceProcAddr"));
#endif // WIN32
}

Vulkan::FunctionsLibrary::~FunctionsLibrary()
{
#ifdef WIN32
	FreeLibrary(mLibrary);
#endif // WIN32
}

void Vulkan::FunctionsLibrary::LoadGlobalFunctions()
{
	vkCreateInstance                       = (PFN_vkCreateInstance)(vkGetInstanceProcAddr(nullptr,                       "vkCreateInstance"));
	vkEnumerateInstanceVersion             = (PFN_vkEnumerateInstanceVersion)(vkGetInstanceProcAddr(nullptr,             "vkEnumerateInstanceVersion"));
	vkEnumerateInstanceLayerProperties     = (PFN_vkEnumerateInstanceLayerProperties)(vkGetInstanceProcAddr(nullptr,     "vkEnumerateInstanceLayerProperties"));
	vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceExtensionProperties"));
}

void Vulkan::FunctionsLibrary::LoadInstanceFunctions(VkInstance instance)
{
	vkDestroyInstance                                                 = (PFN_vkDestroyInstance)(vkGetInstanceProcAddr(instance,                                                 "vkDestroyInstance"));
	vkEnumeratePhysicalDevices                                        = (PFN_vkEnumeratePhysicalDevices)(vkGetInstanceProcAddr(instance,                                        "vkEnumeratePhysicalDevices"));
	vkGetDeviceProcAddr                                               = (PFN_vkGetDeviceProcAddr)(vkGetInstanceProcAddr(instance,                                               "vkGetDeviceProcAddr"));
	vkGetPhysicalDeviceProperties                                     = (PFN_vkGetPhysicalDeviceProperties)(vkGetInstanceProcAddr(instance,                                     "vkGetPhysicalDeviceProperties"));
	vkGetPhysicalDeviceQueueFamilyProperties                          = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)(vkGetInstanceProcAddr(instance,                          "vkGetPhysicalDeviceQueueFamilyProperties"));
	vkGetPhysicalDeviceMemoryProperties                               = (PFN_vkGetPhysicalDeviceMemoryProperties)(vkGetInstanceProcAddr(instance,                               "vkGetPhysicalDeviceMemoryProperties"));
	vkGetPhysicalDeviceFeatures                                       = (PFN_vkGetPhysicalDeviceFeatures)(vkGetInstanceProcAddr(instance,                                       "vkGetPhysicalDeviceFeatures"));
	vkGetPhysicalDeviceFormatProperties                               = (PFN_vkGetPhysicalDeviceFormatProperties)(vkGetInstanceProcAddr(instance,                               "vkGetPhysicalDeviceFormatProperties"));
	vkGetPhysicalDeviceImageFormatProperties                          = (PFN_vkGetPhysicalDeviceImageFormatProperties)(vkGetInstanceProcAddr(instance,                          "vkGetPhysicalDeviceImageFormatProperties"));
	vkCreateDevice                                                    = (PFN_vkCreateDevice)(vkGetInstanceProcAddr(instance,                                                    "vkCreateDevice"));
	vkEnumerateDeviceLayerProperties                                  = (PFN_vkEnumerateDeviceLayerProperties)(vkGetInstanceProcAddr(instance,                                  "vkEnumerateDeviceLayerProperties"));
	vkEnumerateDeviceExtensionProperties                              = (PFN_vkEnumerateDeviceExtensionProperties)(vkGetInstanceProcAddr(instance,                              "vkEnumerateDeviceExtensionProperties"));
	vkGetPhysicalDeviceSparseImageFormatProperties                    = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties)(vkGetInstanceProcAddr(instance,                    "vkGetPhysicalDeviceSparseImageFormatProperties"));
	vkGetPhysicalDeviceFeatures2                                      = (PFN_vkGetPhysicalDeviceFeatures2)(vkGetInstanceProcAddr(instance,                                      "vkGetPhysicalDeviceFeatures2"));
	vkGetPhysicalDeviceProperties2                                    = (PFN_vkGetPhysicalDeviceProperties2)(vkGetInstanceProcAddr(instance,                                    "vkGetPhysicalDeviceProperties2"));
	vkGetPhysicalDeviceFormatProperties2                              = (PFN_vkGetPhysicalDeviceFormatProperties2)(vkGetInstanceProcAddr(instance,                              "vkGetPhysicalDeviceFormatProperties2"));
	vkGetPhysicalDeviceImageFormatProperties2                         = (PFN_vkGetPhysicalDeviceImageFormatProperties2)(vkGetInstanceProcAddr(instance,                         "vkGetPhysicalDeviceImageFormatProperties2"));
	vkGetPhysicalDeviceQueueFamilyProperties2                         = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2)(vkGetInstanceProcAddr(instance,                         "vkGetPhysicalDeviceQueueFamilyProperties2"));
	vkGetPhysicalDeviceMemoryProperties2                              = (PFN_vkGetPhysicalDeviceMemoryProperties2)(vkGetInstanceProcAddr(instance,                              "vkGetPhysicalDeviceMemoryProperties2"));
	vkGetPhysicalDeviceSparseImageFormatProperties2                   = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2)(vkGetInstanceProcAddr(instance,                   "vkGetPhysicalDeviceSparseImageFormatProperties2"));
	vkGetPhysicalDeviceExternalBufferProperties                       = (PFN_vkGetPhysicalDeviceExternalBufferProperties)(vkGetInstanceProcAddr(instance,                       "vkGetPhysicalDeviceExternalBufferProperties"));
	vkGetPhysicalDeviceExternalSemaphoreProperties                    = (PFN_vkGetPhysicalDeviceExternalSemaphoreProperties)(vkGetInstanceProcAddr(instance,                    "vkGetPhysicalDeviceExternalSemaphoreProperties"));
	vkGetPhysicalDeviceExternalFenceProperties                        = (PFN_vkGetPhysicalDeviceExternalFenceProperties)(vkGetInstanceProcAddr(instance,                        "vkGetPhysicalDeviceExternalFenceProperties"));
	vkEnumeratePhysicalDeviceGroups                                   = (PFN_vkEnumeratePhysicalDeviceGroups)(vkGetInstanceProcAddr(instance,                                   "vkEnumeratePhysicalDeviceGroups"));

#if defined(VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME)
	vkAcquireDrmDisplayEXT                                            = (PFN_vkAcquireDrmDisplayEXT)(vkGetInstanceProcAddr(instance,                                            "vkAcquireDrmDisplayEXT"));
	vkGetDrmDisplayEXT                                                = (PFN_vkGetDrmDisplayEXT)(vkGetInstanceProcAddr(instance,                                                "vkGetDrmDisplayEXT"));
#endif

#if defined(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)
	vkGetPhysicalDeviceCalibrateableTimeDomainsEXT                    = (PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)(vkGetInstanceProcAddr(instance,                    "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT"));
#endif

#if defined(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)
	vkCreateDebugReportCallbackEXT                                    = (PFN_vkCreateDebugReportCallbackEXT)(vkGetInstanceProcAddr(instance,                                    "vkCreateDebugReportCallbackEXT"));
	vkDestroyDebugReportCallbackEXT                                   = (PFN_vkDestroyDebugReportCallbackEXT)(vkGetInstanceProcAddr(instance,                                   "vkDestroyDebugReportCallbackEXT"));
	vkDebugReportMessageEXT                                           = (PFN_vkDebugReportMessageEXT)(vkGetInstanceProcAddr(instance,                                           "vkDebugReportMessageEXT"));
#endif

#if defined(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
	vkCreateDebugUtilsMessengerEXT                                    = (PFN_vkCreateDebugUtilsMessengerEXT)(vkGetInstanceProcAddr(instance,                                    "vkCreateDebugUtilsMessengerEXT"));
	vkDestroyDebugUtilsMessengerEXT                                   = (PFN_vkDestroyDebugUtilsMessengerEXT)(vkGetInstanceProcAddr(instance,                                   "vkDestroyDebugUtilsMessengerEXT"));
	vkSubmitDebugUtilsMessageEXT                                      = (PFN_vkSubmitDebugUtilsMessageEXT)(vkGetInstanceProcAddr(instance,                                      "vkSubmitDebugUtilsMessageEXT"));
#endif

#if defined(VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME)
	vkReleaseDisplayEXT                                               = (PFN_vkReleaseDisplayEXT)(vkGetInstanceProcAddr(instance,                                               "vkReleaseDisplayEXT"));
#endif

#if defined(VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME)
	vkGetPhysicalDeviceSurfaceCapabilities2EXT                        = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT)(vkGetInstanceProcAddr(instance,                        "vkGetPhysicalDeviceSurfaceCapabilities2EXT"));
#endif

#if defined(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME)
	vkCreateHeadlessSurfaceEXT                                        = (PFN_vkCreateHeadlessSurfaceEXT)(vkGetInstanceProcAddr(instance,                                        "vkCreateHeadlessSurfaceEXT"));
#endif

#if defined(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME)
	vkGetPhysicalDeviceMultisamplePropertiesEXT                       = (PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT)(vkGetInstanceProcAddr(instance,                       "vkGetPhysicalDeviceMultisamplePropertiesEXT"));
#endif

#if defined(VK_EXT_TOOLING_INFO_EXTENSION_NAME)
	vkGetPhysicalDeviceToolPropertiesEXT                              = (PFN_vkGetPhysicalDeviceToolPropertiesEXT)(vkGetInstanceProcAddr(instance,                              "vkGetPhysicalDeviceToolPropertiesEXT"));
#endif

#if defined(VK_KHR_DEVICE_GROUP_EXTENSION_NAME)
	vkGetPhysicalDevicePresentRectanglesKHR                           = (PFN_vkGetPhysicalDevicePresentRectanglesKHR)(vkGetInstanceProcAddr(instance,                           "vkGetPhysicalDevicePresentRectanglesKHR"));
#endif

#if defined(VK_KHR_DISPLAY_EXTENSION_NAME)
	vkGetPhysicalDeviceDisplayPropertiesKHR                           = (PFN_vkGetPhysicalDeviceDisplayPropertiesKHR)(vkGetInstanceProcAddr(instance,                           "vkGetPhysicalDeviceDisplayPropertiesKHR"));
	vkGetPhysicalDeviceDisplayPlanePropertiesKHR                      = (PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR)(vkGetInstanceProcAddr(instance,                      "vkGetPhysicalDeviceDisplayPlanePropertiesKHR"));
	vkGetDisplayPlaneSupportedDisplaysKHR                             = (PFN_vkGetDisplayPlaneSupportedDisplaysKHR)(vkGetInstanceProcAddr(instance,                             "vkGetDisplayPlaneSupportedDisplaysKHR"));
	vkGetDisplayModePropertiesKHR                                     = (PFN_vkGetDisplayModePropertiesKHR)(vkGetInstanceProcAddr(instance,                                     "vkGetDisplayModePropertiesKHR"));
	vkCreateDisplayModeKHR                                            = (PFN_vkCreateDisplayModeKHR)(vkGetInstanceProcAddr(instance,                                            "vkCreateDisplayModeKHR"));
	vkGetDisplayPlaneCapabilitiesKHR                                  = (PFN_vkGetDisplayPlaneCapabilitiesKHR)(vkGetInstanceProcAddr(instance,                                  "vkGetDisplayPlaneCapabilitiesKHR"));
	vkCreateDisplayPlaneSurfaceKHR                                    = (PFN_vkCreateDisplayPlaneSurfaceKHR)(vkGetInstanceProcAddr(instance,                                    "vkCreateDisplayPlaneSurfaceKHR"));
#endif

#if defined(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)
	vkGetPhysicalDeviceFragmentShadingRatesKHR                        = (PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR)(vkGetInstanceProcAddr(instance,                        "vkGetPhysicalDeviceFragmentShadingRatesKHR"));
#endif

#if defined(VK_KHR_GET_DISPLAY_PROPERTIES_2_EXTENSION_NAME)
	vkGetPhysicalDeviceDisplayProperties2KHR                          = (PFN_vkGetPhysicalDeviceDisplayProperties2KHR)(vkGetInstanceProcAddr(instance,                          "vkGetPhysicalDeviceDisplayProperties2KHR"));
	vkGetPhysicalDeviceDisplayPlaneProperties2KHR                     = (PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR)(vkGetInstanceProcAddr(instance,                     "vkGetPhysicalDeviceDisplayPlaneProperties2KHR"));
	vkGetDisplayModeProperties2KHR                                    = (PFN_vkGetDisplayModeProperties2KHR)(vkGetInstanceProcAddr(instance,                                    "vkGetDisplayModeProperties2KHR"));
	vkGetDisplayPlaneCapabilities2KHR                                 = (PFN_vkGetDisplayPlaneCapabilities2KHR)(vkGetInstanceProcAddr(instance,                                 "vkGetDisplayPlaneCapabilities2KHR"));
#endif

#if defined(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME)
	vkGetPhysicalDeviceSurfaceCapabilities2KHR                        = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR)(vkGetInstanceProcAddr(instance,                        "vkGetPhysicalDeviceSurfaceCapabilities2KHR"));
	vkGetPhysicalDeviceSurfaceFormats2KHR                             = (PFN_vkGetPhysicalDeviceSurfaceFormats2KHR)(vkGetInstanceProcAddr(instance,                             "vkGetPhysicalDeviceSurfaceFormats2KHR"));
#endif

#if defined(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME)
	vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR   = (PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR)(vkGetInstanceProcAddr(instance,   "vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR"));
	vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR           = (PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR)(vkGetInstanceProcAddr(instance,           "vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR"));
#endif

#if defined(VK_KHR_SURFACE_EXTENSION_NAME)
	vkDestroySurfaceKHR                                               = (PFN_vkDestroySurfaceKHR)(vkGetInstanceProcAddr(instance,                                               "vkDestroySurfaceKHR"));
	vkGetPhysicalDeviceSurfaceSupportKHR                              = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)(vkGetInstanceProcAddr(instance,                              "vkGetPhysicalDeviceSurfaceSupportKHR"));
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR                         = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(vkGetInstanceProcAddr(instance,                         "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
	vkGetPhysicalDeviceSurfaceFormatsKHR                              = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)(vkGetInstanceProcAddr(instance,                              "vkGetPhysicalDeviceSurfaceFormatsKHR"));
	vkGetPhysicalDeviceSurfacePresentModesKHR                         = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)(vkGetInstanceProcAddr(instance,                         "vkGetPhysicalDeviceSurfacePresentModesKHR"));
#endif

#if defined(VK_NV_COOPERATIVE_MATRIX_EXTENSION_NAME)
	vkGetPhysicalDeviceCooperativeMatrixPropertiesNV                  = (PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV)(vkGetInstanceProcAddr(instance,                  "vkGetPhysicalDeviceCooperativeMatrixPropertiesNV"));
#endif

#if defined(VK_NV_COVERAGE_REDUCTION_MODE_EXTENSION_NAME)
	vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV = (PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV)(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV"));
#endif

#if defined(VK_NV_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME)
	vkGetPhysicalDeviceExternalImageFormatPropertiesNV                = (PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV)(vkGetInstanceProcAddr(instance,                "vkGetPhysicalDeviceExternalImageFormatPropertiesNV"));
#endif

#if defined(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME) && defined(VK_ENABLE_BETA_EXTENSIONS)
	vkGetPhysicalDeviceVideoCapabilitiesKHR                           = (PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR)(vkGetInstanceProcAddr(instance,                           "vkGetPhysicalDeviceVideoCapabilitiesKHR"));
	vkGetPhysicalDeviceVideoFormatPropertiesKHR                       = (PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR)(vkGetInstanceProcAddr(instance,                       "vkGetPhysicalDeviceVideoFormatPropertiesKHR"));
#endif

#if defined(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_ANDROID_KHR)
	vkCreateAndroidSurfaceKHR                                         = (PFN_vkCreateAndroidSurfaceKHR)(vkGetInstanceProcAddr(instance,                                         "vkCreateAndroidSurfaceKHR"));
#endif

#if defined(VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_DIRECTFB_EXT)
	vkCreateDirectFBSurfaceEXT                                        = (PFN_vkCreateDirectFBSurfaceEXT)(vkGetInstanceProcAddr(instance,                                        "vkCreateDirectFBSurfaceEXT"));
	vkGetPhysicalDeviceDirectFBPresentationSupportEXT                 = (PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT)(vkGetInstanceProcAddr(instance,                 "vkGetPhysicalDeviceDirectFBPresentationSupportEXT"));
#endif

#if defined(VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_FUCHSIA)
	vkCreateImagePipeSurfaceFUCHSIA                                   = (PFN_vkCreateImagePipeSurfaceFUCHSIA)(vkGetInstanceProcAddr(instance,                                   "vkCreateImagePipeSurfaceFUCHSIA"));
#endif

#if defined(VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_GGP)
	vkCreateStreamDescriptorSurfaceGGP                                = (PFN_vkCreateStreamDescriptorSurfaceGGP)(vkGetInstanceProcAddr(instance,                                "vkCreateStreamDescriptorSurfaceGGP"));
#endif

#if defined(VK_MVK_IOS_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_IOS_MVK)
	vkCreateIOSSurfaceMVK                                             = (PFN_vkCreateIOSSurfaceMVK)(vkGetInstanceProcAddr(instance,                                             "vkCreateIOSSurfaceMVK"));
#endif

#if defined(VK_MVK_MACOS_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_MACOS_MVK)
	vkCreateMacOSSurfaceMVK                                           = (PFN_vkCreateMacOSSurfaceMVK)(vkGetInstanceProcAddr(instance,                                           "vkCreateMacOSSurfaceMVK"));
#endif

#if defined(VK_EXT_METAL_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_METAL_EXT)
	vkCreateMetalSurfaceEXT                                           = (PFN_vkCreateMetalSurfaceEXT)(vkGetInstanceProcAddr(instance,                                           "vkCreateMetalSurfaceEXT"));
#endif

#if defined(VK_QNX_SCREEN_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_SCREEN_QNX)
	vkCreateScreenSurfaceQNX                                          = (PFN_vkCreateScreenSurfaceQNX)(vkGetInstanceProcAddr(instance,                                          "vkCreateScreenSurfaceQNX"));
	vkGetPhysicalDeviceScreenPresentationSupportQNX                   = (PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX)(vkGetInstanceProcAddr(instance,                   "vkGetPhysicalDeviceScreenPresentationSupportQNX"));
#endif

#if defined(VK_NN_VI_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_VI_NN)
	vkCreateViSurfaceNN                                               = (PFN_vkCreateViSurfaceNN)(vkGetInstanceProcAddr(instance,                                               "vkCreateViSurfaceNN"));
#endif

#if defined(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
	vkCreateWaylandSurfaceKHR                                         = (PFN_vkCreateWaylandSurfaceKHR)(vkGetInstanceProcAddr(instance,                                         "vkCreateWaylandSurfaceKHR"));
	vkGetPhysicalDeviceWaylandPresentationSupportKHR                  = (PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)(vkGetInstanceProcAddr(instance,                  "vkGetPhysicalDeviceWaylandPresentationSupportKHR"));
#endif

#if defined(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_WIN32_KHR)
	vkGetPhysicalDeviceSurfacePresentModes2EXT                        = (PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT)(vkGetInstanceProcAddr(instance,                        "vkGetPhysicalDeviceSurfacePresentModes2EXT"));
#endif

#if defined(VK_KHR_WIN32_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_WIN32_KHR)
	vkCreateWin32SurfaceKHR                                           = (PFN_vkCreateWin32SurfaceKHR)(vkGetInstanceProcAddr(instance,                                           "vkCreateWin32SurfaceKHR"));
	vkGetPhysicalDeviceWin32PresentationSupportKHR                    = (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)(vkGetInstanceProcAddr(instance,                    "vkGetPhysicalDeviceWin32PresentationSupportKHR"));
#endif

#if defined(VK_NV_ACQUIRE_WINRT_DISPLAY_EXTENSION_NAME) && defined(VK_USE_PLATFORM_WIN32_KHR)
	vkAcquireWinrtDisplayNV                                           = (PFN_vkAcquireWinrtDisplayNV)(vkGetInstanceProcAddr(instance,                                           "vkAcquireWinrtDisplayNV"));
	vkGetWinrtDisplayNV                                               = (PFN_vkGetWinrtDisplayNV)(vkGetInstanceProcAddr(instance,                                               "vkGetWinrtDisplayNV"));
#endif

#if defined(VK_KHR_XCB_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_XCB_KHR)
	vkCreateXcbSurfaceKHR                                             = (PFN_vkCreateXcbSurfaceKHR)(vkGetInstanceProcAddr(instance,                                             "vkCreateXcbSurfaceKHR"));
	vkGetPhysicalDeviceXcbPresentationSupportKHR                      = (PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR)(vkGetInstanceProcAddr(instance,                      "vkGetPhysicalDeviceXcbPresentationSupportKHR"));
#endif

#if defined(VK_KHR_XLIB_SURFACE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_XLIB_KHR)
	vkCreateXlibSurfaceKHR                                            = (PFN_vkCreateXlibSurfaceKHR)(vkGetInstanceProcAddr(instance,                                            "vkCreateXlibSurfaceKHR"));
	vkGetPhysicalDeviceXlibPresentationSupportKHR                     = (PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR)(vkGetInstanceProcAddr(instance,                     "vkGetPhysicalDeviceXlibPresentationSupportKHR"));
#endif

#if defined(VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME) && defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
	vkAcquireXlibDisplayEXT                                           = (PFN_vkAcquireXlibDisplayEXT)(vkGetInstanceProcAddr(instance,                                           "vkAcquireXlibDisplayEXT"));
	vkGetRandROutputDisplayEXT                                        = (PFN_vkGetRandROutputDisplayEXT)(vkGetInstanceProcAddr(instance,                                        "vkGetRandROutputDisplayEXT"));
#endif
}

void Vulkan::FunctionsLibrary::LoadDeviceFunctions(VkDevice device)
{
	vkDestroyDevice                                   = (PFN_vkDestroyDevice)(vkGetDeviceProcAddr(device,                                   "vkDestroyDevice"));
	vkGetDeviceQueue                                  = (PFN_vkGetDeviceQueue)(vkGetDeviceProcAddr(device,                                  "vkGetDeviceQueue"));
	vkQueueSubmit                                     = (PFN_vkQueueSubmit)(vkGetDeviceProcAddr(device,                                     "vkQueueSubmit"));
	vkQueueWaitIdle                                   = (PFN_vkQueueWaitIdle)(vkGetDeviceProcAddr(device,                                   "vkQueueWaitIdle"));
	vkDeviceWaitIdle                                  = (PFN_vkDeviceWaitIdle)(vkGetDeviceProcAddr(device,                                  "vkDeviceWaitIdle"));
	vkAllocateMemory                                  = (PFN_vkAllocateMemory)(vkGetDeviceProcAddr(device,                                  "vkAllocateMemory"));
	vkFreeMemory                                      = (PFN_vkFreeMemory)(vkGetDeviceProcAddr(device,                                      "vkFreeMemory"));
	vkMapMemory                                       = (PFN_vkMapMemory)(vkGetDeviceProcAddr(device,                                       "vkMapMemory"));
	vkUnmapMemory                                     = (PFN_vkUnmapMemory)(vkGetDeviceProcAddr(device,                                     "vkUnmapMemory"));
	vkFlushMappedMemoryRanges                         = (PFN_vkFlushMappedMemoryRanges)(vkGetDeviceProcAddr(device,                         "vkFlushMappedMemoryRanges"));
	vkInvalidateMappedMemoryRanges                    = (PFN_vkInvalidateMappedMemoryRanges)(vkGetDeviceProcAddr(device,                    "vkInvalidateMappedMemoryRanges"));
	vkGetDeviceMemoryCommitment                       = (PFN_vkGetDeviceMemoryCommitment)(vkGetDeviceProcAddr(device,                       "vkGetDeviceMemoryCommitment"));
	vkGetBufferMemoryRequirements                     = (PFN_vkGetBufferMemoryRequirements)(vkGetDeviceProcAddr(device,                     "vkGetBufferMemoryRequirements"));
	vkBindBufferMemory                                = (PFN_vkBindBufferMemory)(vkGetDeviceProcAddr(device,                                "vkBindBufferMemory"));
	vkGetImageMemoryRequirements                      = (PFN_vkGetImageMemoryRequirements)(vkGetDeviceProcAddr(device,                      "vkGetImageMemoryRequirements"));
	vkBindImageMemory                                 = (PFN_vkBindImageMemory)(vkGetDeviceProcAddr(device,                                 "vkBindImageMemory"));
	vkGetImageSparseMemoryRequirements                = (PFN_vkGetImageSparseMemoryRequirements)(vkGetDeviceProcAddr(device,                "vkGetImageSparseMemoryRequirements"));
	vkQueueBindSparse                                 = (PFN_vkQueueBindSparse)(vkGetDeviceProcAddr(device,                                 "vkQueueBindSparse"));
	vkCreateFence                                     = (PFN_vkCreateFence)(vkGetDeviceProcAddr(device,                                     "vkCreateFence"));
	vkDestroyFence                                    = (PFN_vkDestroyFence)(vkGetDeviceProcAddr(device,                                    "vkDestroyFence"));
	vkResetFences                                     = (PFN_vkResetFences)(vkGetDeviceProcAddr(device,                                     "vkResetFences"));
	vkGetFenceStatus                                  = (PFN_vkGetFenceStatus)(vkGetDeviceProcAddr(device,                                  "vkGetFenceStatus"));
	vkWaitForFences                                   = (PFN_vkWaitForFences)(vkGetDeviceProcAddr(device,                                   "vkWaitForFences"));
	vkCreateSemaphore                                 = (PFN_vkCreateSemaphore)(vkGetDeviceProcAddr(device,                                 "vkCreateSemaphore"));
	vkDestroySemaphore                                = (PFN_vkDestroySemaphore)(vkGetDeviceProcAddr(device,                                "vkDestroySemaphore"));
	vkCreateEvent                                     = (PFN_vkCreateEvent)(vkGetDeviceProcAddr(device,                                     "vkCreateEvent"));
	vkDestroyEvent                                    = (PFN_vkDestroyEvent)(vkGetDeviceProcAddr(device,                                    "vkDestroyEvent"));
	vkGetEventStatus                                  = (PFN_vkGetEventStatus)(vkGetDeviceProcAddr(device,                                  "vkGetEventStatus"));
	vkSetEvent                                        = (PFN_vkSetEvent)(vkGetDeviceProcAddr(device,                                        "vkSetEvent"));
	vkResetEvent                                      = (PFN_vkResetEvent)(vkGetDeviceProcAddr(device,                                      "vkResetEvent"));
	vkCreateQueryPool                                 = (PFN_vkCreateQueryPool)(vkGetDeviceProcAddr(device,                                 "vkCreateQueryPool"));
	vkDestroyQueryPool                                = (PFN_vkDestroyQueryPool)(vkGetDeviceProcAddr(device,                                "vkDestroyQueryPool"));
	vkGetQueryPoolResults                             = (PFN_vkGetQueryPoolResults)(vkGetDeviceProcAddr(device,                             "vkGetQueryPoolResults"));
	vkResetQueryPool                                  = (PFN_vkResetQueryPool)(vkGetDeviceProcAddr(device,                                  "vkResetQueryPool"));
	vkCreateBuffer                                    = (PFN_vkCreateBuffer)(vkGetDeviceProcAddr(device,                                    "vkCreateBuffer"));
	vkDestroyBuffer                                   = (PFN_vkDestroyBuffer)(vkGetDeviceProcAddr(device,                                   "vkDestroyBuffer"));
	vkCreateBufferView                                = (PFN_vkCreateBufferView)(vkGetDeviceProcAddr(device,                                "vkCreateBufferView"));
	vkDestroyBufferView                               = (PFN_vkDestroyBufferView)(vkGetDeviceProcAddr(device,                               "vkDestroyBufferView"));
	vkCreateImage                                     = (PFN_vkCreateImage)(vkGetDeviceProcAddr(device,                                     "vkCreateImage"));
	vkDestroyImage                                    = (PFN_vkDestroyImage)(vkGetDeviceProcAddr(device,                                    "vkDestroyImage"));
	vkGetImageSubresourceLayout                       = (PFN_vkGetImageSubresourceLayout)(vkGetDeviceProcAddr(device,                       "vkGetImageSubresourceLayout"));
	vkCreateImageView                                 = (PFN_vkCreateImageView)(vkGetDeviceProcAddr(device,                                 "vkCreateImageView"));
	vkDestroyImageView                                = (PFN_vkDestroyImageView)(vkGetDeviceProcAddr(device,                                "vkDestroyImageView"));
	vkCreateShaderModule                              = (PFN_vkCreateShaderModule)(vkGetDeviceProcAddr(device,                              "vkCreateShaderModule"));
	vkDestroyShaderModule                             = (PFN_vkDestroyShaderModule)(vkGetDeviceProcAddr(device,                             "vkDestroyShaderModule"));
	vkCreatePipelineCache                             = (PFN_vkCreatePipelineCache)(vkGetDeviceProcAddr(device,                             "vkCreatePipelineCache"));
	vkDestroyPipelineCache                            = (PFN_vkDestroyPipelineCache)(vkGetDeviceProcAddr(device,                            "vkDestroyPipelineCache"));
	vkGetPipelineCacheData                            = (PFN_vkGetPipelineCacheData)(vkGetDeviceProcAddr(device,                            "vkGetPipelineCacheData"));
	vkMergePipelineCaches                             = (PFN_vkMergePipelineCaches)(vkGetDeviceProcAddr(device,                             "vkMergePipelineCaches"));
	vkCreateGraphicsPipelines                         = (PFN_vkCreateGraphicsPipelines)(vkGetDeviceProcAddr(device,                         "vkCreateGraphicsPipelines"));
	vkCreateComputePipelines                          = (PFN_vkCreateComputePipelines)(vkGetDeviceProcAddr(device,                          "vkCreateComputePipelines"));
	vkDestroyPipeline                                 = (PFN_vkDestroyPipeline)(vkGetDeviceProcAddr(device,                                 "vkDestroyPipeline"));
	vkCreatePipelineLayout                            = (PFN_vkCreatePipelineLayout)(vkGetDeviceProcAddr(device,                            "vkCreatePipelineLayout"));
	vkDestroyPipelineLayout                           = (PFN_vkDestroyPipelineLayout)(vkGetDeviceProcAddr(device,                           "vkDestroyPipelineLayout"));
	vkCreateSampler                                   = (PFN_vkCreateSampler)(vkGetDeviceProcAddr(device,                                   "vkCreateSampler"));
	vkDestroySampler                                  = (PFN_vkDestroySampler)(vkGetDeviceProcAddr(device,                                  "vkDestroySampler"));
	vkCreateDescriptorSetLayout                       = (PFN_vkCreateDescriptorSetLayout)(vkGetDeviceProcAddr(device,                       "vkCreateDescriptorSetLayout"));
	vkDestroyDescriptorSetLayout                      = (PFN_vkDestroyDescriptorSetLayout)(vkGetDeviceProcAddr(device,                      "vkDestroyDescriptorSetLayout"));
	vkCreateDescriptorPool                            = (PFN_vkCreateDescriptorPool)(vkGetDeviceProcAddr(device,                            "vkCreateDescriptorPool"));
	vkDestroyDescriptorPool                           = (PFN_vkDestroyDescriptorPool)(vkGetDeviceProcAddr(device,                           "vkDestroyDescriptorPool"));
	vkResetDescriptorPool                             = (PFN_vkResetDescriptorPool)(vkGetDeviceProcAddr(device,                             "vkResetDescriptorPool"));
	vkAllocateDescriptorSets                          = (PFN_vkAllocateDescriptorSets)(vkGetDeviceProcAddr(device,                          "vkAllocateDescriptorSets"));
	vkFreeDescriptorSets                              = (PFN_vkFreeDescriptorSets)(vkGetDeviceProcAddr(device,                              "vkFreeDescriptorSets"));
	vkUpdateDescriptorSets                            = (PFN_vkUpdateDescriptorSets)(vkGetDeviceProcAddr(device,                            "vkUpdateDescriptorSets"));
	vkCreateFramebuffer                               = (PFN_vkCreateFramebuffer)(vkGetDeviceProcAddr(device,                               "vkCreateFramebuffer"));
	vkDestroyFramebuffer                              = (PFN_vkDestroyFramebuffer)(vkGetDeviceProcAddr(device,                              "vkDestroyFramebuffer"));
	vkCreateRenderPass                                = (PFN_vkCreateRenderPass)(vkGetDeviceProcAddr(device,                                "vkCreateRenderPass"));
	vkDestroyRenderPass                               = (PFN_vkDestroyRenderPass)(vkGetDeviceProcAddr(device,                               "vkDestroyRenderPass"));
	vkGetRenderAreaGranularity                        = (PFN_vkGetRenderAreaGranularity)(vkGetDeviceProcAddr(device,                        "vkGetRenderAreaGranularity"));
	vkCreateCommandPool                               = (PFN_vkCreateCommandPool)(vkGetDeviceProcAddr(device,                               "vkCreateCommandPool"));
	vkDestroyCommandPool                              = (PFN_vkDestroyCommandPool)(vkGetDeviceProcAddr(device,                              "vkDestroyCommandPool"));
	vkResetCommandPool                                = (PFN_vkResetCommandPool)(vkGetDeviceProcAddr(device,                                "vkResetCommandPool"));
	vkAllocateCommandBuffers                          = (PFN_vkAllocateCommandBuffers)(vkGetDeviceProcAddr(device,                          "vkAllocateCommandBuffers"));
	vkFreeCommandBuffers                              = (PFN_vkFreeCommandBuffers)(vkGetDeviceProcAddr(device,                              "vkFreeCommandBuffers"));
	vkBeginCommandBuffer                              = (PFN_vkBeginCommandBuffer)(vkGetDeviceProcAddr(device,                              "vkBeginCommandBuffer"));
	vkEndCommandBuffer                                = (PFN_vkEndCommandBuffer)(vkGetDeviceProcAddr(device,                                "vkEndCommandBuffer"));
	vkResetCommandBuffer                              = (PFN_vkResetCommandBuffer)(vkGetDeviceProcAddr(device,                              "vkResetCommandBuffer"));
	vkCmdBindPipeline                                 = (PFN_vkCmdBindPipeline)(vkGetDeviceProcAddr(device,                                 "vkCmdBindPipeline"));
	vkCmdSetViewport                                  = (PFN_vkCmdSetViewport)(vkGetDeviceProcAddr(device,                                  "vkCmdSetViewport"));
	vkCmdSetScissor                                   = (PFN_vkCmdSetScissor)(vkGetDeviceProcAddr(device,                                   "vkCmdSetScissor"));
	vkCmdSetLineWidth                                 = (PFN_vkCmdSetLineWidth)(vkGetDeviceProcAddr(device,                                 "vkCmdSetLineWidth"));
	vkCmdSetDepthBias                                 = (PFN_vkCmdSetDepthBias)(vkGetDeviceProcAddr(device,                                 "vkCmdSetDepthBias"));
	vkCmdSetBlendConstants                            = (PFN_vkCmdSetBlendConstants)(vkGetDeviceProcAddr(device,                            "vkCmdSetBlendConstants"));
	vkCmdSetDepthBounds                               = (PFN_vkCmdSetDepthBounds)(vkGetDeviceProcAddr(device,                               "vkCmdSetDepthBounds"));
	vkCmdSetStencilCompareMask                        = (PFN_vkCmdSetStencilCompareMask)(vkGetDeviceProcAddr(device,                        "vkCmdSetStencilCompareMask"));
	vkCmdSetStencilWriteMask                          = (PFN_vkCmdSetStencilWriteMask)(vkGetDeviceProcAddr(device,                          "vkCmdSetStencilWriteMask"));
	vkCmdSetStencilReference                          = (PFN_vkCmdSetStencilReference)(vkGetDeviceProcAddr(device,                          "vkCmdSetStencilReference"));
	vkCmdBindDescriptorSets                           = (PFN_vkCmdBindDescriptorSets)(vkGetDeviceProcAddr(device,                           "vkCmdBindDescriptorSets"));
	vkCmdBindIndexBuffer                              = (PFN_vkCmdBindIndexBuffer)(vkGetDeviceProcAddr(device,                              "vkCmdBindIndexBuffer"));
	vkCmdBindVertexBuffers                            = (PFN_vkCmdBindVertexBuffers)(vkGetDeviceProcAddr(device,                            "vkCmdBindVertexBuffers"));
	vkCmdDraw                                         = (PFN_vkCmdDraw)(vkGetDeviceProcAddr(device,                                         "vkCmdDraw"));
	vkCmdDrawIndexed                                  = (PFN_vkCmdDrawIndexed)(vkGetDeviceProcAddr(device,                                  "vkCmdDrawIndexed"));
	vkCmdDrawIndirect                                 = (PFN_vkCmdDrawIndirect)(vkGetDeviceProcAddr(device,                                 "vkCmdDrawIndirect"));
	vkCmdDrawIndexedIndirect                          = (PFN_vkCmdDrawIndexedIndirect)(vkGetDeviceProcAddr(device,                          "vkCmdDrawIndexedIndirect"));
	vkCmdDispatch                                     = (PFN_vkCmdDispatch)(vkGetDeviceProcAddr(device,                                     "vkCmdDispatch"));
	vkCmdDispatchIndirect                             = (PFN_vkCmdDispatchIndirect)(vkGetDeviceProcAddr(device,                             "vkCmdDispatchIndirect"));
	vkCmdCopyBuffer                                   = (PFN_vkCmdCopyBuffer)(vkGetDeviceProcAddr(device,                                   "vkCmdCopyBuffer"));
	vkCmdCopyImage                                    = (PFN_vkCmdCopyImage)(vkGetDeviceProcAddr(device,                                    "vkCmdCopyImage"));
	vkCmdBlitImage                                    = (PFN_vkCmdBlitImage)(vkGetDeviceProcAddr(device,                                    "vkCmdBlitImage"));
	vkCmdCopyBufferToImage                            = (PFN_vkCmdCopyBufferToImage)(vkGetDeviceProcAddr(device,                            "vkCmdCopyBufferToImage"));
	vkCmdCopyImageToBuffer                            = (PFN_vkCmdCopyImageToBuffer)(vkGetDeviceProcAddr(device,                            "vkCmdCopyImageToBuffer"));
	vkCmdUpdateBuffer                                 = (PFN_vkCmdUpdateBuffer)(vkGetDeviceProcAddr(device,                                 "vkCmdUpdateBuffer"));
	vkCmdFillBuffer                                   = (PFN_vkCmdFillBuffer)(vkGetDeviceProcAddr(device,                                   "vkCmdFillBuffer"));
	vkCmdClearColorImage                              = (PFN_vkCmdClearColorImage)(vkGetDeviceProcAddr(device,                              "vkCmdClearColorImage"));
	vkCmdClearDepthStencilImage                       = (PFN_vkCmdClearDepthStencilImage)(vkGetDeviceProcAddr(device,                       "vkCmdClearDepthStencilImage"));
	vkCmdClearAttachments                             = (PFN_vkCmdClearAttachments)(vkGetDeviceProcAddr(device,                             "vkCmdClearAttachments"));
	vkCmdResolveImage                                 = (PFN_vkCmdResolveImage)(vkGetDeviceProcAddr(device,                                 "vkCmdResolveImage"));
	vkCmdSetEvent                                     = (PFN_vkCmdSetEvent)(vkGetDeviceProcAddr(device,                                     "vkCmdSetEvent"));
	vkCmdResetEvent                                   = (PFN_vkCmdResetEvent)(vkGetDeviceProcAddr(device,                                   "vkCmdResetEvent"));
	vkCmdWaitEvents                                   = (PFN_vkCmdWaitEvents)(vkGetDeviceProcAddr(device,                                   "vkCmdWaitEvents"));
	vkCmdPipelineBarrier                              = (PFN_vkCmdPipelineBarrier)(vkGetDeviceProcAddr(device,                              "vkCmdPipelineBarrier"));
	vkCmdBeginQuery                                   = (PFN_vkCmdBeginQuery)(vkGetDeviceProcAddr(device,                                   "vkCmdBeginQuery"));
	vkCmdEndQuery                                     = (PFN_vkCmdEndQuery)(vkGetDeviceProcAddr(device,                                     "vkCmdEndQuery"));
	vkCmdResetQueryPool                               = (PFN_vkCmdResetQueryPool)(vkGetDeviceProcAddr(device,                               "vkCmdResetQueryPool"));
	vkCmdWriteTimestamp                               = (PFN_vkCmdWriteTimestamp)(vkGetDeviceProcAddr(device,                               "vkCmdWriteTimestamp"));
	vkCmdCopyQueryPoolResults                         = (PFN_vkCmdCopyQueryPoolResults)(vkGetDeviceProcAddr(device,                         "vkCmdCopyQueryPoolResults"));
	vkCmdPushConstants                                = (PFN_vkCmdPushConstants)(vkGetDeviceProcAddr(device,                                "vkCmdPushConstants"));
	vkCmdBeginRenderPass                              = (PFN_vkCmdBeginRenderPass)(vkGetDeviceProcAddr(device,                              "vkCmdBeginRenderPass"));
	vkCmdNextSubpass                                  = (PFN_vkCmdNextSubpass)(vkGetDeviceProcAddr(device,                                  "vkCmdNextSubpass"));
	vkCmdEndRenderPass                                = (PFN_vkCmdEndRenderPass)(vkGetDeviceProcAddr(device,                                "vkCmdEndRenderPass"));
	vkCmdExecuteCommands                              = (PFN_vkCmdExecuteCommands)(vkGetDeviceProcAddr(device,                              "vkCmdExecuteCommands"));
	vkTrimCommandPool                                 = (PFN_vkTrimCommandPool)(vkGetDeviceProcAddr(device,                                 "vkTrimCommandPool"));
	vkGetDeviceGroupPeerMemoryFeatures                = (PFN_vkGetDeviceGroupPeerMemoryFeatures)(vkGetDeviceProcAddr(device,                "vkGetDeviceGroupPeerMemoryFeatures"));
	vkBindBufferMemory2                               = (PFN_vkBindBufferMemory2)(vkGetDeviceProcAddr(device,                               "vkBindBufferMemory2"));
	vkBindImageMemory2                                = (PFN_vkBindImageMemory2)(vkGetDeviceProcAddr(device,                                "vkBindImageMemory2"));
	vkCmdSetDeviceMask                                = (PFN_vkCmdSetDeviceMask)(vkGetDeviceProcAddr(device,                                "vkCmdSetDeviceMask"));
	vkCmdDispatchBase                                 = (PFN_vkCmdDispatchBase)(vkGetDeviceProcAddr(device,                                 "vkCmdDispatchBase"));
	vkCreateDescriptorUpdateTemplate                  = (PFN_vkCreateDescriptorUpdateTemplate)(vkGetDeviceProcAddr(device,                  "vkCreateDescriptorUpdateTemplate"));
	vkDestroyDescriptorUpdateTemplate                 = (PFN_vkDestroyDescriptorUpdateTemplate)(vkGetDeviceProcAddr(device,                 "vkDestroyDescriptorUpdateTemplate"));
	vkUpdateDescriptorSetWithTemplate                 = (PFN_vkUpdateDescriptorSetWithTemplate)(vkGetDeviceProcAddr(device,                 "vkUpdateDescriptorSetWithTemplate"));
	vkGetBufferMemoryRequirements2                    = (PFN_vkGetBufferMemoryRequirements2)(vkGetDeviceProcAddr(device,                    "vkGetBufferMemoryRequirements2"));
	vkGetImageMemoryRequirements2                     = (PFN_vkGetImageMemoryRequirements2)(vkGetDeviceProcAddr(device,                     "vkGetImageMemoryRequirements2"));
	vkGetImageSparseMemoryRequirements2               = (PFN_vkGetImageSparseMemoryRequirements2)(vkGetDeviceProcAddr(device,               "vkGetImageSparseMemoryRequirements2"));
	vkCreateSamplerYcbcrConversion                    = (PFN_vkCreateSamplerYcbcrConversion)(vkGetDeviceProcAddr(device,                    "vkCreateSamplerYcbcrConversion"));
	vkDestroySamplerYcbcrConversion                   = (PFN_vkDestroySamplerYcbcrConversion)(vkGetDeviceProcAddr(device,                   "vkDestroySamplerYcbcrConversion"));
	vkGetDeviceQueue2                                 = (PFN_vkGetDeviceQueue2)(vkGetDeviceProcAddr(device,                                 "vkGetDeviceQueue2"));
	vkGetDescriptorSetLayoutSupport                   = (PFN_vkGetDescriptorSetLayoutSupport)(vkGetDeviceProcAddr(device,                   "vkGetDescriptorSetLayoutSupport"));
	vkCreateRenderPass2                               = (PFN_vkCreateRenderPass2)(vkGetDeviceProcAddr(device,                               "vkCreateRenderPass2"));
	vkCmdBeginRenderPass2                             = (PFN_vkCmdBeginRenderPass2)(vkGetDeviceProcAddr(device,                             "vkCmdBeginRenderPass2"));
	vkCmdNextSubpass2                                 = (PFN_vkCmdNextSubpass2)(vkGetDeviceProcAddr(device,                                 "vkCmdNextSubpass2"));
	vkCmdEndRenderPass2                               = (PFN_vkCmdEndRenderPass2)(vkGetDeviceProcAddr(device,                               "vkCmdEndRenderPass2"));
	vkGetSemaphoreCounterValue                        = (PFN_vkGetSemaphoreCounterValue)(vkGetDeviceProcAddr(device,                        "vkGetSemaphoreCounterValue"));
	vkWaitSemaphores                                  = (PFN_vkWaitSemaphores)(vkGetDeviceProcAddr(device,                                  "vkWaitSemaphores"));
	vkSignalSemaphore                                 = (PFN_vkSignalSemaphore)(vkGetDeviceProcAddr(device,                                 "vkSignalSemaphore"));
	vkCmdDrawIndirectCount                            = (PFN_vkCmdDrawIndirectCount)(vkGetDeviceProcAddr(device,                            "vkCmdDrawIndirectCount"));
	vkCmdDrawIndexedIndirectCount                     = (PFN_vkCmdDrawIndexedIndirectCount)(vkGetDeviceProcAddr(device,                     "vkCmdDrawIndexedIndirectCount"));
	vkGetBufferOpaqueCaptureAddress                   = (PFN_vkGetBufferOpaqueCaptureAddress)(vkGetDeviceProcAddr(device,                   "vkGetBufferOpaqueCaptureAddress"));
	vkGetBufferDeviceAddress                          = (PFN_vkGetBufferDeviceAddress)(vkGetDeviceProcAddr(device,                          "vkGetBufferDeviceAddress"));
	vkGetDeviceMemoryOpaqueCaptureAddress             = (PFN_vkGetDeviceMemoryOpaqueCaptureAddress)(vkGetDeviceProcAddr(device,             "vkGetDeviceMemoryOpaqueCaptureAddress"));

#if defined(VK_AMD_BUFFER_MARKER_EXTENSION_NAME)
	vkCmdWriteBufferMarkerAMD                         = (PFN_vkCmdWriteBufferMarkerAMD)(vkGetDeviceProcAddr(device,                         "vkCmdWriteBufferMarkerAMD"));
#endif

#if defined(VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME)
	vkSetLocalDimmingAMD                              = (PFN_vkSetLocalDimmingAMD)(vkGetDeviceProcAddr(device,                              "vkSetLocalDimmingAMD"));
#endif

#if defined(VK_AMD_SHADER_INFO_EXTENSION_NAME)
	vkGetShaderInfoAMD                                = (PFN_vkGetShaderInfoAMD)(vkGetDeviceProcAddr(device,                                "vkGetShaderInfoAMD"));
#endif

#if defined(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)
	vkGetCalibratedTimestampsEXT                      = (PFN_vkGetCalibratedTimestampsEXT)(vkGetDeviceProcAddr(device,                      "vkGetCalibratedTimestampsEXT"));
#endif

#if defined(VK_EXT_COLOR_WRITE_ENABLE_EXTENSION_NAME)
	vkCmdSetColorWriteEnableEXT                       = (PFN_vkCmdSetColorWriteEnableEXT)(vkGetDeviceProcAddr(device,                       "vkCmdSetColorWriteEnableEXT"));
#endif

#if defined(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME)
	vkCmdBeginConditionalRenderingEXT                 = (PFN_vkCmdBeginConditionalRenderingEXT)(vkGetDeviceProcAddr(device,                 "vkCmdBeginConditionalRenderingEXT"));
	vkCmdEndConditionalRenderingEXT                   = (PFN_vkCmdEndConditionalRenderingEXT)(vkGetDeviceProcAddr(device,                   "vkCmdEndConditionalRenderingEXT"));
#endif

#if defined(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)
	vkDebugMarkerSetObjectNameEXT                     = (PFN_vkDebugMarkerSetObjectNameEXT)(vkGetDeviceProcAddr(device,                     "vkDebugMarkerSetObjectNameEXT"));
	vkDebugMarkerSetObjectTagEXT                      = (PFN_vkDebugMarkerSetObjectTagEXT)(vkGetDeviceProcAddr(device,                      "vkDebugMarkerSetObjectTagEXT"));
	vkCmdDebugMarkerBeginEXT                          = (PFN_vkCmdDebugMarkerBeginEXT)(vkGetDeviceProcAddr(device,                          "vkCmdDebugMarkerBeginEXT"));
	vkCmdDebugMarkerEndEXT                            = (PFN_vkCmdDebugMarkerEndEXT)(vkGetDeviceProcAddr(device,                            "vkCmdDebugMarkerEndEXT"));
	vkCmdDebugMarkerInsertEXT                         = (PFN_vkCmdDebugMarkerInsertEXT)(vkGetDeviceProcAddr(device,                         "vkCmdDebugMarkerInsertEXT"));
#endif

#if defined(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
	vkSetDebugUtilsObjectNameEXT                      = (PFN_vkSetDebugUtilsObjectNameEXT)(vkGetDeviceProcAddr(device,                      "vkSetDebugUtilsObjectNameEXT"));
	vkSetDebugUtilsObjectTagEXT                       = (PFN_vkSetDebugUtilsObjectTagEXT)(vkGetDeviceProcAddr(device,                       "vkSetDebugUtilsObjectTagEXT"));
	vkQueueBeginDebugUtilsLabelEXT                    = (PFN_vkQueueBeginDebugUtilsLabelEXT)(vkGetDeviceProcAddr(device,                    "vkQueueBeginDebugUtilsLabelEXT"));
	vkQueueEndDebugUtilsLabelEXT                      = (PFN_vkQueueEndDebugUtilsLabelEXT)(vkGetDeviceProcAddr(device,                      "vkQueueEndDebugUtilsLabelEXT"));
	vkQueueInsertDebugUtilsLabelEXT                   = (PFN_vkQueueInsertDebugUtilsLabelEXT)(vkGetDeviceProcAddr(device,                   "vkQueueInsertDebugUtilsLabelEXT"));
	vkCmdBeginDebugUtilsLabelEXT                      = (PFN_vkCmdBeginDebugUtilsLabelEXT)(vkGetDeviceProcAddr(device,                      "vkCmdBeginDebugUtilsLabelEXT"));
	vkCmdEndDebugUtilsLabelEXT                        = (PFN_vkCmdEndDebugUtilsLabelEXT)(vkGetDeviceProcAddr(device,                        "vkCmdEndDebugUtilsLabelEXT"));
	vkCmdInsertDebugUtilsLabelEXT                     = (PFN_vkCmdInsertDebugUtilsLabelEXT)(vkGetDeviceProcAddr(device,                     "vkCmdInsertDebugUtilsLabelEXT"));
#endif

#if defined(VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME)
	vkCmdSetDiscardRectangleEXT                       = (PFN_vkCmdSetDiscardRectangleEXT)(vkGetDeviceProcAddr(device,                       "vkCmdSetDiscardRectangleEXT"));
#endif

#if defined(VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME)
	vkDisplayPowerControlEXT                          = (PFN_vkDisplayPowerControlEXT)(vkGetDeviceProcAddr(device,                          "vkDisplayPowerControlEXT"));
	vkRegisterDeviceEventEXT                          = (PFN_vkRegisterDeviceEventEXT)(vkGetDeviceProcAddr(device,                          "vkRegisterDeviceEventEXT"));
	vkRegisterDisplayEventEXT                         = (PFN_vkRegisterDisplayEventEXT)(vkGetDeviceProcAddr(device,                         "vkRegisterDisplayEventEXT"));
	vkGetSwapchainCounterEXT                          = (PFN_vkGetSwapchainCounterEXT)(vkGetDeviceProcAddr(device,                          "vkGetSwapchainCounterEXT"));
#endif

#if defined(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME)
	vkCmdSetPatchControlPointsEXT                     = (PFN_vkCmdSetPatchControlPointsEXT)(vkGetDeviceProcAddr(device,                     "vkCmdSetPatchControlPointsEXT"));
	vkCmdSetRasterizerDiscardEnableEXT                = (PFN_vkCmdSetRasterizerDiscardEnableEXT)(vkGetDeviceProcAddr(device,                "vkCmdSetRasterizerDiscardEnableEXT"));
	vkCmdSetDepthBiasEnableEXT                        = (PFN_vkCmdSetDepthBiasEnableEXT)(vkGetDeviceProcAddr(device,                        "vkCmdSetDepthBiasEnableEXT"));
	vkCmdSetLogicOpEXT                                = (PFN_vkCmdSetLogicOpEXT)(vkGetDeviceProcAddr(device,                                "vkCmdSetLogicOpEXT"));
	vkCmdSetPrimitiveRestartEnableEXT                 = (PFN_vkCmdSetPrimitiveRestartEnableEXT)(vkGetDeviceProcAddr(device,                 "vkCmdSetPrimitiveRestartEnableEXT"));
#endif

#if defined(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME)
	vkCmdSetCullModeEXT                               = (PFN_vkCmdSetCullModeEXT)(vkGetDeviceProcAddr(device,                               "vkCmdSetCullModeEXT"));
	vkCmdSetFrontFaceEXT                              = (PFN_vkCmdSetFrontFaceEXT)(vkGetDeviceProcAddr(device,                              "vkCmdSetFrontFaceEXT"));
	vkCmdSetPrimitiveTopologyEXT                      = (PFN_vkCmdSetPrimitiveTopologyEXT)(vkGetDeviceProcAddr(device,                      "vkCmdSetPrimitiveTopologyEXT"));
	vkCmdSetViewportWithCountEXT                      = (PFN_vkCmdSetViewportWithCountEXT)(vkGetDeviceProcAddr(device,                      "vkCmdSetViewportWithCountEXT"));
	vkCmdSetScissorWithCountEXT                       = (PFN_vkCmdSetScissorWithCountEXT)(vkGetDeviceProcAddr(device,                       "vkCmdSetScissorWithCountEXT"));
	vkCmdBindVertexBuffers2EXT                        = (PFN_vkCmdBindVertexBuffers2EXT)(vkGetDeviceProcAddr(device,                        "vkCmdBindVertexBuffers2EXT"));
	vkCmdSetDepthTestEnableEXT                        = (PFN_vkCmdSetDepthTestEnableEXT)(vkGetDeviceProcAddr(device,                        "vkCmdSetDepthTestEnableEXT"));
	vkCmdSetDepthWriteEnableEXT                       = (PFN_vkCmdSetDepthWriteEnableEXT)(vkGetDeviceProcAddr(device,                       "vkCmdSetDepthWriteEnableEXT"));
	vkCmdSetDepthCompareOpEXT                         = (PFN_vkCmdSetDepthCompareOpEXT)(vkGetDeviceProcAddr(device,                         "vkCmdSetDepthCompareOpEXT"));
	vkCmdSetDepthBoundsTestEnableEXT                  = (PFN_vkCmdSetDepthBoundsTestEnableEXT)(vkGetDeviceProcAddr(device,                  "vkCmdSetDepthBoundsTestEnableEXT"));
	vkCmdSetStencilTestEnableEXT                      = (PFN_vkCmdSetStencilTestEnableEXT)(vkGetDeviceProcAddr(device,                      "vkCmdSetStencilTestEnableEXT"));
	vkCmdSetStencilOpEXT                              = (PFN_vkCmdSetStencilOpEXT)(vkGetDeviceProcAddr(device,                              "vkCmdSetStencilOpEXT"));
#endif

#if defined(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME)
	vkGetMemoryHostPointerPropertiesEXT               = (PFN_vkGetMemoryHostPointerPropertiesEXT)(vkGetDeviceProcAddr(device,               "vkGetMemoryHostPointerPropertiesEXT"));
#endif

#if defined(VK_EXT_HDR_METADATA_EXTENSION_NAME)
	vkSetHdrMetadataEXT                               = (PFN_vkSetHdrMetadataEXT)(vkGetDeviceProcAddr(device,                               "vkSetHdrMetadataEXT"));
#endif

#if defined(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME)
	vkGetImageDrmFormatModifierPropertiesEXT          = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)(vkGetDeviceProcAddr(device,          "vkGetImageDrmFormatModifierPropertiesEXT"));
#endif

#if defined(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME)
	vkCmdSetLineStippleEXT                            = (PFN_vkCmdSetLineStippleEXT)(vkGetDeviceProcAddr(device,                            "vkCmdSetLineStippleEXT"));
#endif

#if defined(VK_EXT_MULTI_DRAW_EXTENSION_NAME)
	vkCmdDrawMultiEXT                                 = (PFN_vkCmdDrawMultiEXT)(vkGetDeviceProcAddr(device,                                 "vkCmdDrawMultiEXT"));
	vkCmdDrawMultiIndexedEXT                          = (PFN_vkCmdDrawMultiIndexedEXT)(vkGetDeviceProcAddr(device,                          "vkCmdDrawMultiIndexedEXT"));
#endif

#if defined(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME)
	vkSetDeviceMemoryPriorityEXT                      = (PFN_vkSetDeviceMemoryPriorityEXT)(vkGetDeviceProcAddr(device,                      "vkSetDeviceMemoryPriorityEXT"));
#endif

#if defined(VK_EXT_PRIVATE_DATA_EXTENSION_NAME)
	vkCreatePrivateDataSlotEXT                        = (PFN_vkCreatePrivateDataSlotEXT)(vkGetDeviceProcAddr(device,                        "vkCreatePrivateDataSlotEXT"));
	vkDestroyPrivateDataSlotEXT                       = (PFN_vkDestroyPrivateDataSlotEXT)(vkGetDeviceProcAddr(device,                       "vkDestroyPrivateDataSlotEXT"));
	vkSetPrivateDataEXT                               = (PFN_vkSetPrivateDataEXT)(vkGetDeviceProcAddr(device,                               "vkSetPrivateDataEXT"));
	vkGetPrivateDataEXT                               = (PFN_vkGetPrivateDataEXT)(vkGetDeviceProcAddr(device,                               "vkGetPrivateDataEXT"));
#endif

#if defined(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME)
	vkCmdSetSampleLocationsEXT                        = (PFN_vkCmdSetSampleLocationsEXT)(vkGetDeviceProcAddr(device,                        "vkCmdSetSampleLocationsEXT"));
#endif

#if defined(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME)
	vkCmdBindTransformFeedbackBuffersEXT              = (PFN_vkCmdBindTransformFeedbackBuffersEXT)(vkGetDeviceProcAddr(device,              "vkCmdBindTransformFeedbackBuffersEXT"));
	vkCmdBeginTransformFeedbackEXT                    = (PFN_vkCmdBeginTransformFeedbackEXT)(vkGetDeviceProcAddr(device,                    "vkCmdBeginTransformFeedbackEXT"));
	vkCmdEndTransformFeedbackEXT                      = (PFN_vkCmdEndTransformFeedbackEXT)(vkGetDeviceProcAddr(device,                      "vkCmdEndTransformFeedbackEXT"));
	vkCmdBeginQueryIndexedEXT                         = (PFN_vkCmdBeginQueryIndexedEXT)(vkGetDeviceProcAddr(device,                         "vkCmdBeginQueryIndexedEXT"));
	vkCmdEndQueryIndexedEXT                           = (PFN_vkCmdEndQueryIndexedEXT)(vkGetDeviceProcAddr(device,                           "vkCmdEndQueryIndexedEXT"));
	vkCmdDrawIndirectByteCountEXT                     = (PFN_vkCmdDrawIndirectByteCountEXT)(vkGetDeviceProcAddr(device,                     "vkCmdDrawIndirectByteCountEXT"));
#endif

#if defined(VK_EXT_VALIDATION_CACHE_EXTENSION_NAME)
	vkCreateValidationCacheEXT                        = (PFN_vkCreateValidationCacheEXT)(vkGetDeviceProcAddr(device,                        "vkCreateValidationCacheEXT"));
	vkDestroyValidationCacheEXT                       = (PFN_vkDestroyValidationCacheEXT)(vkGetDeviceProcAddr(device,                       "vkDestroyValidationCacheEXT"));
	vkGetValidationCacheDataEXT                       = (PFN_vkGetValidationCacheDataEXT)(vkGetDeviceProcAddr(device,                       "vkGetValidationCacheDataEXT"));
	vkMergeValidationCachesEXT                        = (PFN_vkMergeValidationCachesEXT)(vkGetDeviceProcAddr(device,                        "vkMergeValidationCachesEXT"));
#endif

#if defined(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME)
	vkCmdSetVertexInputEXT                            = (PFN_vkCmdSetVertexInputEXT)(vkGetDeviceProcAddr(device,                            "vkCmdSetVertexInputEXT"));
#endif

#if defined(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME)
	vkGetRefreshCycleDurationGOOGLE                   = (PFN_vkGetRefreshCycleDurationGOOGLE)(vkGetDeviceProcAddr(device,                   "vkGetRefreshCycleDurationGOOGLE"));
	vkGetPastPresentationTimingGOOGLE                 = (PFN_vkGetPastPresentationTimingGOOGLE)(vkGetDeviceProcAddr(device,                 "vkGetPastPresentationTimingGOOGLE"));
#endif

#if defined(VK_HUAWEI_INVOCATION_MASK_EXTENSION_NAME)
	vkCmdBindInvocationMaskHUAWEI                     = (PFN_vkCmdBindInvocationMaskHUAWEI)(vkGetDeviceProcAddr(device,                     "vkCmdBindInvocationMaskHUAWEI"));
#endif

#if defined(VK_HUAWEI_SUBPASS_SHADING_EXTENSION_NAME)
	vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI   = (PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI)(vkGetDeviceProcAddr(device,   "vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI"));
	vkCmdSubpassShadingHUAWEI                         = (PFN_vkCmdSubpassShadingHUAWEI)(vkGetDeviceProcAddr(device,                         "vkCmdSubpassShadingHUAWEI"));
#endif

#if defined(VK_INTEL_PERFORMANCE_QUERY_EXTENSION_NAME)
	vkInitializePerformanceApiINTEL                   = (PFN_vkInitializePerformanceApiINTEL)(vkGetDeviceProcAddr(device,                   "vkInitializePerformanceApiINTEL"));
	vkUninitializePerformanceApiINTEL                 = (PFN_vkUninitializePerformanceApiINTEL)(vkGetDeviceProcAddr(device,                 "vkUninitializePerformanceApiINTEL"));
	vkCmdSetPerformanceMarkerINTEL                    = (PFN_vkCmdSetPerformanceMarkerINTEL)(vkGetDeviceProcAddr(device,                    "vkCmdSetPerformanceMarkerINTEL"));
	vkCmdSetPerformanceStreamMarkerINTEL              = (PFN_vkCmdSetPerformanceStreamMarkerINTEL)(vkGetDeviceProcAddr(device,              "vkCmdSetPerformanceStreamMarkerINTEL"));
	vkCmdSetPerformanceOverrideINTEL                  = (PFN_vkCmdSetPerformanceOverrideINTEL)(vkGetDeviceProcAddr(device,                  "vkCmdSetPerformanceOverrideINTEL"));
	vkAcquirePerformanceConfigurationINTEL            = (PFN_vkAcquirePerformanceConfigurationINTEL)(vkGetDeviceProcAddr(device,            "vkAcquirePerformanceConfigurationINTEL"));
	vkReleasePerformanceConfigurationINTEL            = (PFN_vkReleasePerformanceConfigurationINTEL)(vkGetDeviceProcAddr(device,            "vkReleasePerformanceConfigurationINTEL"));
	vkQueueSetPerformanceConfigurationINTEL           = (PFN_vkQueueSetPerformanceConfigurationINTEL)(vkGetDeviceProcAddr(device,           "vkQueueSetPerformanceConfigurationINTEL"));
	vkGetPerformanceParameterINTEL                    = (PFN_vkGetPerformanceParameterINTEL)(vkGetDeviceProcAddr(device,                    "vkGetPerformanceParameterINTEL"));
#endif

#if defined(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
	vkDestroyAccelerationStructureKHR                 = (PFN_vkDestroyAccelerationStructureKHR)(vkGetDeviceProcAddr(device,                 "vkDestroyAccelerationStructureKHR"));
	vkCmdCopyAccelerationStructureKHR                 = (PFN_vkCmdCopyAccelerationStructureKHR)(vkGetDeviceProcAddr(device,                 "vkCmdCopyAccelerationStructureKHR"));
	vkCopyAccelerationStructureKHR                    = (PFN_vkCopyAccelerationStructureKHR)(vkGetDeviceProcAddr(device,                    "vkCopyAccelerationStructureKHR"));
	vkCmdCopyAccelerationStructureToMemoryKHR         = (PFN_vkCmdCopyAccelerationStructureToMemoryKHR)(vkGetDeviceProcAddr(device,         "vkCmdCopyAccelerationStructureToMemoryKHR"));
	vkCopyAccelerationStructureToMemoryKHR            = (PFN_vkCopyAccelerationStructureToMemoryKHR)(vkGetDeviceProcAddr(device,            "vkCopyAccelerationStructureToMemoryKHR"));
	vkCmdCopyMemoryToAccelerationStructureKHR         = (PFN_vkCmdCopyMemoryToAccelerationStructureKHR)(vkGetDeviceProcAddr(device,         "vkCmdCopyMemoryToAccelerationStructureKHR"));
	vkCopyMemoryToAccelerationStructureKHR            = (PFN_vkCopyMemoryToAccelerationStructureKHR)(vkGetDeviceProcAddr(device,            "vkCopyMemoryToAccelerationStructureKHR"));
	vkCmdWriteAccelerationStructuresPropertiesKHR     = (PFN_vkCmdWriteAccelerationStructuresPropertiesKHR)(vkGetDeviceProcAddr(device,     "vkCmdWriteAccelerationStructuresPropertiesKHR"));
	vkWriteAccelerationStructuresPropertiesKHR        = (PFN_vkWriteAccelerationStructuresPropertiesKHR)(vkGetDeviceProcAddr(device,        "vkWriteAccelerationStructuresPropertiesKHR"));
	vkGetDeviceAccelerationStructureCompatibilityKHR  = (PFN_vkGetDeviceAccelerationStructureCompatibilityKHR)(vkGetDeviceProcAddr(device,  "vkGetDeviceAccelerationStructureCompatibilityKHR"));
	vkCreateAccelerationStructureKHR                  = (PFN_vkCreateAccelerationStructureKHR)(vkGetDeviceProcAddr(device,                  "vkCreateAccelerationStructureKHR"));
	vkCmdBuildAccelerationStructuresKHR               = (PFN_vkCmdBuildAccelerationStructuresKHR)(vkGetDeviceProcAddr(device,               "vkCmdBuildAccelerationStructuresKHR"));
	vkCmdBuildAccelerationStructuresIndirectKHR       = (PFN_vkCmdBuildAccelerationStructuresIndirectKHR)(vkGetDeviceProcAddr(device,       "vkCmdBuildAccelerationStructuresIndirectKHR"));
	vkBuildAccelerationStructuresKHR                  = (PFN_vkBuildAccelerationStructuresKHR)(vkGetDeviceProcAddr(device,                  "vkBuildAccelerationStructuresKHR"));
	vkGetAccelerationStructureDeviceAddressKHR        = (PFN_vkGetAccelerationStructureDeviceAddressKHR)(vkGetDeviceProcAddr(device,        "vkGetAccelerationStructureDeviceAddressKHR"));
	vkGetAccelerationStructureBuildSizesKHR           = (PFN_vkGetAccelerationStructureBuildSizesKHR)(vkGetDeviceProcAddr(device,           "vkGetAccelerationStructureBuildSizesKHR"));
#endif

#if defined(VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME)
	vkCmdCopyBuffer2KHR                               = (PFN_vkCmdCopyBuffer2KHR)(vkGetDeviceProcAddr(device,                               "vkCmdCopyBuffer2KHR"));
	vkCmdCopyImage2KHR                                = (PFN_vkCmdCopyImage2KHR)(vkGetDeviceProcAddr(device,                                "vkCmdCopyImage2KHR"));
	vkCmdBlitImage2KHR                                = (PFN_vkCmdBlitImage2KHR)(vkGetDeviceProcAddr(device,                                "vkCmdBlitImage2KHR"));
	vkCmdCopyBufferToImage2KHR                        = (PFN_vkCmdCopyBufferToImage2KHR)(vkGetDeviceProcAddr(device,                        "vkCmdCopyBufferToImage2KHR"));
	vkCmdCopyImageToBuffer2KHR                        = (PFN_vkCmdCopyImageToBuffer2KHR)(vkGetDeviceProcAddr(device,                        "vkCmdCopyImageToBuffer2KHR"));
	vkCmdResolveImage2KHR                             = (PFN_vkCmdResolveImage2KHR)(vkGetDeviceProcAddr(device,                             "vkCmdResolveImage2KHR"));
#endif

#if defined(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
	vkCreateDeferredOperationKHR                      = (PFN_vkCreateDeferredOperationKHR)(vkGetDeviceProcAddr(device,                      "vkCreateDeferredOperationKHR"));
	vkDestroyDeferredOperationKHR                     = (PFN_vkDestroyDeferredOperationKHR)(vkGetDeviceProcAddr(device,                     "vkDestroyDeferredOperationKHR"));
	vkGetDeferredOperationMaxConcurrencyKHR           = (PFN_vkGetDeferredOperationMaxConcurrencyKHR)(vkGetDeviceProcAddr(device,           "vkGetDeferredOperationMaxConcurrencyKHR"));
	vkGetDeferredOperationResultKHR                   = (PFN_vkGetDeferredOperationResultKHR)(vkGetDeviceProcAddr(device,                   "vkGetDeferredOperationResultKHR"));
	vkDeferredOperationJoinKHR                        = (PFN_vkDeferredOperationJoinKHR)(vkGetDeviceProcAddr(device,                        "vkDeferredOperationJoinKHR"));
#endif

#if defined(VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME)
	vkCmdPushDescriptorSetWithTemplateKHR             = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)(vkGetDeviceProcAddr(device,             "vkCmdPushDescriptorSetWithTemplateKHR"));
#endif

#if defined(VK_KHR_DEVICE_GROUP_EXTENSION_NAME)
	vkGetDeviceGroupPresentCapabilitiesKHR            = (PFN_vkGetDeviceGroupPresentCapabilitiesKHR)(vkGetDeviceProcAddr(device,            "vkGetDeviceGroupPresentCapabilitiesKHR"));
	vkGetDeviceGroupSurfacePresentModesKHR            = (PFN_vkGetDeviceGroupSurfacePresentModesKHR)(vkGetDeviceProcAddr(device,            "vkGetDeviceGroupSurfacePresentModesKHR"));
	vkAcquireNextImage2KHR                            = (PFN_vkAcquireNextImage2KHR)(vkGetDeviceProcAddr(device,                            "vkAcquireNextImage2KHR"));
#endif

#if defined(VK_KHR_DISPLAY_SWAPCHAIN_EXTENSION_NAME)
	vkCreateSharedSwapchainsKHR                       = (PFN_vkCreateSharedSwapchainsKHR)(vkGetDeviceProcAddr(device,                       "vkCreateSharedSwapchainsKHR"));
#endif

#if defined(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME)
	vkGetFenceFdKHR                                   = (PFN_vkGetFenceFdKHR)(vkGetDeviceProcAddr(device,                                   "vkGetFenceFdKHR"));
	vkImportFenceFdKHR                                = (PFN_vkImportFenceFdKHR)(vkGetDeviceProcAddr(device,                                "vkImportFenceFdKHR"));
#endif

#if defined(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)
	vkGetMemoryFdKHR                                  = (PFN_vkGetMemoryFdKHR)(vkGetDeviceProcAddr(device,                                  "vkGetMemoryFdKHR"));
	vkGetMemoryFdPropertiesKHR                        = (PFN_vkGetMemoryFdPropertiesKHR)(vkGetDeviceProcAddr(device,                        "vkGetMemoryFdPropertiesKHR"));
#endif

#if defined(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME)
	vkGetSemaphoreFdKHR                               = (PFN_vkGetSemaphoreFdKHR)(vkGetDeviceProcAddr(device,                               "vkGetSemaphoreFdKHR"));
	vkImportSemaphoreFdKHR                            = (PFN_vkImportSemaphoreFdKHR)(vkGetDeviceProcAddr(device,                            "vkImportSemaphoreFdKHR"));
#endif

#if defined(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)
	vkCmdSetFragmentShadingRateKHR                    = (PFN_vkCmdSetFragmentShadingRateKHR)(vkGetDeviceProcAddr(device,                    "vkCmdSetFragmentShadingRateKHR"));
#endif

#if defined(VK_KHR_MAINTENANCE_4_EXTENSION_NAME)
	vkGetDeviceBufferMemoryRequirementsKHR            = (PFN_vkGetDeviceBufferMemoryRequirementsKHR)(vkGetDeviceProcAddr(device,            "vkGetDeviceBufferMemoryRequirementsKHR"));
	vkGetDeviceImageMemoryRequirementsKHR             = (PFN_vkGetDeviceImageMemoryRequirementsKHR)(vkGetDeviceProcAddr(device,             "vkGetDeviceImageMemoryRequirementsKHR"));
	vkGetDeviceImageSparseMemoryRequirementsKHR       = (PFN_vkGetDeviceImageSparseMemoryRequirementsKHR)(vkGetDeviceProcAddr(device,       "vkGetDeviceImageSparseMemoryRequirementsKHR"));
#endif

#if defined(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME)
	vkAcquireProfilingLockKHR                         = (PFN_vkAcquireProfilingLockKHR)(vkGetDeviceProcAddr(device,                         "vkAcquireProfilingLockKHR"));
	vkReleaseProfilingLockKHR                         = (PFN_vkReleaseProfilingLockKHR)(vkGetDeviceProcAddr(device,                         "vkReleaseProfilingLockKHR"));
#endif

#if defined(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME)
	vkGetPipelineExecutablePropertiesKHR              = (PFN_vkGetPipelineExecutablePropertiesKHR)(vkGetDeviceProcAddr(device,              "vkGetPipelineExecutablePropertiesKHR"));
	vkGetPipelineExecutableStatisticsKHR              = (PFN_vkGetPipelineExecutableStatisticsKHR)(vkGetDeviceProcAddr(device,              "vkGetPipelineExecutableStatisticsKHR"));
	vkGetPipelineExecutableInternalRepresentationsKHR = (PFN_vkGetPipelineExecutableInternalRepresentationsKHR)(vkGetDeviceProcAddr(device, "vkGetPipelineExecutableInternalRepresentationsKHR"));
#endif

#if defined(VK_KHR_PRESENT_WAIT_EXTENSION_NAME)
	vkWaitForPresentKHR                               = (PFN_vkWaitForPresentKHR)(vkGetDeviceProcAddr(device,                               "vkWaitForPresentKHR"));
#endif

#if defined(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)
	vkCmdPushDescriptorSetKHR                         = (PFN_vkCmdPushDescriptorSetKHR)(vkGetDeviceProcAddr(device,                         "vkCmdPushDescriptorSetKHR"));
#endif

#if defined(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
	vkCmdTraceRaysKHR                                 = (PFN_vkCmdTraceRaysKHR)(vkGetDeviceProcAddr(device,                                 "vkCmdTraceRaysKHR"));
	vkGetRayTracingShaderGroupHandlesKHR              = (PFN_vkGetRayTracingShaderGroupHandlesKHR)(vkGetDeviceProcAddr(device,              "vkGetRayTracingShaderGroupHandlesKHR"));
	vkGetRayTracingCaptureReplayShaderGroupHandlesKHR = (PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR)(vkGetDeviceProcAddr(device, "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR"));
	vkCreateRayTracingPipelinesKHR                    = (PFN_vkCreateRayTracingPipelinesKHR)(vkGetDeviceProcAddr(device,                    "vkCreateRayTracingPipelinesKHR"));
	vkCmdTraceRaysIndirectKHR                         = (PFN_vkCmdTraceRaysIndirectKHR)(vkGetDeviceProcAddr(device,                         "vkCmdTraceRaysIndirectKHR"));
	vkGetRayTracingShaderGroupStackSizeKHR            = (PFN_vkGetRayTracingShaderGroupStackSizeKHR)(vkGetDeviceProcAddr(device,            "vkGetRayTracingShaderGroupStackSizeKHR"));
	vkCmdSetRayTracingPipelineStackSizeKHR            = (PFN_vkCmdSetRayTracingPipelineStackSizeKHR)(vkGetDeviceProcAddr(device,            "vkCmdSetRayTracingPipelineStackSizeKHR"));
#endif

#if defined(VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME)
	vkGetSwapchainStatusKHR                           = (PFN_vkGetSwapchainStatusKHR)(vkGetDeviceProcAddr(device,                           "vkGetSwapchainStatusKHR"));
#endif

#if defined(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
	vkCreateSwapchainKHR                              = (PFN_vkCreateSwapchainKHR)(vkGetDeviceProcAddr(device,                              "vkCreateSwapchainKHR"));
	vkDestroySwapchainKHR                             = (PFN_vkDestroySwapchainKHR)(vkGetDeviceProcAddr(device,                             "vkDestroySwapchainKHR"));
	vkGetSwapchainImagesKHR                           = (PFN_vkGetSwapchainImagesKHR)(vkGetDeviceProcAddr(device,                           "vkGetSwapchainImagesKHR"));
	vkAcquireNextImageKHR                             = (PFN_vkAcquireNextImageKHR)(vkGetDeviceProcAddr(device,                             "vkAcquireNextImageKHR"));
	vkQueuePresentKHR                                 = (PFN_vkQueuePresentKHR)(vkGetDeviceProcAddr(device,                                 "vkQueuePresentKHR"));
#endif

#if defined(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
	vkCmdSetEvent2KHR                                 = (PFN_vkCmdSetEvent2KHR)(vkGetDeviceProcAddr(device,                                 "vkCmdSetEvent2KHR"));
	vkCmdResetEvent2KHR                               = (PFN_vkCmdResetEvent2KHR)(vkGetDeviceProcAddr(device,                               "vkCmdResetEvent2KHR"));
	vkCmdWaitEvents2KHR                               = (PFN_vkCmdWaitEvents2KHR)(vkGetDeviceProcAddr(device,                               "vkCmdWaitEvents2KHR"));
	vkCmdPipelineBarrier2KHR                          = (PFN_vkCmdPipelineBarrier2KHR)(vkGetDeviceProcAddr(device,                          "vkCmdPipelineBarrier2KHR"));
	vkQueueSubmit2KHR                                 = (PFN_vkQueueSubmit2KHR)(vkGetDeviceProcAddr(device,                                 "vkQueueSubmit2KHR"));
	vkCmdWriteTimestamp2KHR                           = (PFN_vkCmdWriteTimestamp2KHR)(vkGetDeviceProcAddr(device,                           "vkCmdWriteTimestamp2KHR"));
	vkCmdWriteBufferMarker2AMD                        = (PFN_vkCmdWriteBufferMarker2AMD)(vkGetDeviceProcAddr(device,                        "vkCmdWriteBufferMarker2AMD"));
	vkGetQueueCheckpointData2NV                       = (PFN_vkGetQueueCheckpointData2NV)(vkGetDeviceProcAddr(device,                       "vkGetQueueCheckpointData2NV"));
#endif

#if defined(VK_NVX_BINARY_IMPORT_EXTENSION_NAME)
	vkCreateCuModuleNVX                               = (PFN_vkCreateCuModuleNVX)(vkGetDeviceProcAddr(device,                               "vkCreateCuModuleNVX"));
	vkCreateCuFunctionNVX                             = (PFN_vkCreateCuFunctionNVX)(vkGetDeviceProcAddr(device,                             "vkCreateCuFunctionNVX"));
	vkDestroyCuModuleNVX                              = (PFN_vkDestroyCuModuleNVX)(vkGetDeviceProcAddr(device,                              "vkDestroyCuModuleNVX"));
	vkDestroyCuFunctionNVX                            = (PFN_vkDestroyCuFunctionNVX)(vkGetDeviceProcAddr(device,                            "vkDestroyCuFunctionNVX"));
	vkCmdCuLaunchKernelNVX                            = (PFN_vkCmdCuLaunchKernelNVX)(vkGetDeviceProcAddr(device,                            "vkCmdCuLaunchKernelNVX"));
#endif

#if defined(VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME)
	vkGetImageViewHandleNVX                           = (PFN_vkGetImageViewHandleNVX)(vkGetDeviceProcAddr(device,                           "vkGetImageViewHandleNVX"));
	vkGetImageViewAddressNVX                          = (PFN_vkGetImageViewAddressNVX)(vkGetDeviceProcAddr(device,                          "vkGetImageViewAddressNVX"));
#endif

#if defined(VK_NV_CLIP_SPACE_W_SCALING_EXTENSION_NAME)
	vkCmdSetViewportWScalingNV                        = (PFN_vkCmdSetViewportWScalingNV)(vkGetDeviceProcAddr(device,                        "vkCmdSetViewportWScalingNV"));
#endif

#if defined(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME)
	vkCmdSetCheckpointNV                              = (PFN_vkCmdSetCheckpointNV)(vkGetDeviceProcAddr(device,                              "vkCmdSetCheckpointNV"));
	vkGetQueueCheckpointDataNV                        = (PFN_vkGetQueueCheckpointDataNV)(vkGetDeviceProcAddr(device,                        "vkGetQueueCheckpointDataNV"));
#endif

#if defined(VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME)
	vkCmdExecuteGeneratedCommandsNV                   = (PFN_vkCmdExecuteGeneratedCommandsNV)(vkGetDeviceProcAddr(device,                   "vkCmdExecuteGeneratedCommandsNV"));
	vkCmdPreprocessGeneratedCommandsNV                = (PFN_vkCmdPreprocessGeneratedCommandsNV)(vkGetDeviceProcAddr(device,                "vkCmdPreprocessGeneratedCommandsNV"));
	vkCmdBindPipelineShaderGroupNV                    = (PFN_vkCmdBindPipelineShaderGroupNV)(vkGetDeviceProcAddr(device,                    "vkCmdBindPipelineShaderGroupNV"));
	vkGetGeneratedCommandsMemoryRequirementsNV        = (PFN_vkGetGeneratedCommandsMemoryRequirementsNV)(vkGetDeviceProcAddr(device,        "vkGetGeneratedCommandsMemoryRequirementsNV"));
	vkCreateIndirectCommandsLayoutNV                  = (PFN_vkCreateIndirectCommandsLayoutNV)(vkGetDeviceProcAddr(device,                  "vkCreateIndirectCommandsLayoutNV"));
	vkDestroyIndirectCommandsLayoutNV                 = (PFN_vkDestroyIndirectCommandsLayoutNV)(vkGetDeviceProcAddr(device,                 "vkDestroyIndirectCommandsLayoutNV"));
#endif

#if defined(VK_NV_EXTERNAL_MEMORY_RDMA_EXTENSION_NAME)
	vkGetMemoryRemoteAddressNV                        = (PFN_vkGetMemoryRemoteAddressNV)(vkGetDeviceProcAddr(device,                        "vkGetMemoryRemoteAddressNV"));
#endif

#if defined(VK_NV_FRAGMENT_SHADING_RATE_ENUMS_EXTENSION_NAME)
	vkCmdSetFragmentShadingRateEnumNV                 = (PFN_vkCmdSetFragmentShadingRateEnumNV)(vkGetDeviceProcAddr(device,                 "vkCmdSetFragmentShadingRateEnumNV"));
#endif

#if defined(VK_NV_MESH_SHADER_EXTENSION_NAME)
	vkCmdDrawMeshTasksNV                              = (PFN_vkCmdDrawMeshTasksNV)(vkGetDeviceProcAddr(device,                              "vkCmdDrawMeshTasksNV"));
	vkCmdDrawMeshTasksIndirectNV                      = (PFN_vkCmdDrawMeshTasksIndirectNV)(vkGetDeviceProcAddr(device,                      "vkCmdDrawMeshTasksIndirectNV"));
	vkCmdDrawMeshTasksIndirectCountNV                 = (PFN_vkCmdDrawMeshTasksIndirectCountNV)(vkGetDeviceProcAddr(device,                 "vkCmdDrawMeshTasksIndirectCountNV"));
#endif

#if defined(VK_NV_RAY_TRACING_EXTENSION_NAME)
	vkCompileDeferredNV                               = (PFN_vkCompileDeferredNV)(vkGetDeviceProcAddr(device,                               "vkCompileDeferredNV"));
	vkCreateAccelerationStructureNV                   = (PFN_vkCreateAccelerationStructureNV)(vkGetDeviceProcAddr(device,                   "vkCreateAccelerationStructureNV"));
	vkDestroyAccelerationStructureNV                  = (PFN_vkDestroyAccelerationStructureNV)(vkGetDeviceProcAddr(device,                  "vkDestroyAccelerationStructureNV"));
	vkGetAccelerationStructureMemoryRequirementsNV    = (PFN_vkGetAccelerationStructureMemoryRequirementsNV)(vkGetDeviceProcAddr(device,    "vkGetAccelerationStructureMemoryRequirementsNV"));
	vkBindAccelerationStructureMemoryNV               = (PFN_vkBindAccelerationStructureMemoryNV)(vkGetDeviceProcAddr(device,               "vkBindAccelerationStructureMemoryNV"));
	vkCmdCopyAccelerationStructureNV                  = (PFN_vkCmdCopyAccelerationStructureNV)(vkGetDeviceProcAddr(device,                  "vkCmdCopyAccelerationStructureNV"));
	vkCmdWriteAccelerationStructuresPropertiesNV      = (PFN_vkCmdWriteAccelerationStructuresPropertiesNV)(vkGetDeviceProcAddr(device,      "vkCmdWriteAccelerationStructuresPropertiesNV"));
	vkCmdBuildAccelerationStructureNV                 = (PFN_vkCmdBuildAccelerationStructureNV)(vkGetDeviceProcAddr(device,                 "vkCmdBuildAccelerationStructureNV"));
	vkCmdTraceRaysNV                                  = (PFN_vkCmdTraceRaysNV)(vkGetDeviceProcAddr(device,                                  "vkCmdTraceRaysNV"));
	vkGetAccelerationStructureHandleNV                = (PFN_vkGetAccelerationStructureHandleNV)(vkGetDeviceProcAddr(device,                "vkGetAccelerationStructureHandleNV"));
	vkCreateRayTracingPipelinesNV                     = (PFN_vkCreateRayTracingPipelinesNV)(vkGetDeviceProcAddr(device,                     "vkCreateRayTracingPipelinesNV"));
#endif

#if defined(VK_NV_SCISSOR_EXCLUSIVE_EXTENSION_NAME)
	vkCmdSetExclusiveScissorNV                        = (PFN_vkCmdSetExclusiveScissorNV)(vkGetDeviceProcAddr(device,                        "vkCmdSetExclusiveScissorNV"));
#endif

#if defined(VK_NV_SHADING_RATE_IMAGE_EXTENSION_NAME)
	vkCmdBindShadingRateImageNV                       = (PFN_vkCmdBindShadingRateImageNV)(vkGetDeviceProcAddr(device,                       "vkCmdBindShadingRateImageNV"));
	vkCmdSetViewportShadingRatePaletteNV              = (PFN_vkCmdSetViewportShadingRatePaletteNV)(vkGetDeviceProcAddr(device,              "vkCmdSetViewportShadingRatePaletteNV"));
	vkCmdSetCoarseSampleOrderNV                       = (PFN_vkCmdSetCoarseSampleOrderNV)(vkGetDeviceProcAddr(device,                       "vkCmdSetCoarseSampleOrderNV"));
#endif

#if defined(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME) && defined(VK_ENABLE_BETA_EXTENSIONS)
	vkCmdDecodeVideoKHR                               = (PFN_vkCmdDecodeVideoKHR)(vkGetDeviceProcAddr(device,                               "vkCmdDecodeVideoKHR"));
#endif

#if defined(VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME) && defined(VK_ENABLE_BETA_EXTENSIONS)
	vkCmdEncodeVideoKHR                               = (PFN_vkCmdEncodeVideoKHR)(vkGetDeviceProcAddr(device,                               "vkCmdEncodeVideoKHR"));
#endif

#if defined(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME) && defined(VK_ENABLE_BETA_EXTENSIONS)
	vkCreateVideoSessionKHR                           = (PFN_vkCreateVideoSessionKHR)(vkGetDeviceProcAddr(device,                           "vkCreateVideoSessionKHR"));
	vkDestroyVideoSessionKHR                          = (PFN_vkDestroyVideoSessionKHR)(vkGetDeviceProcAddr(device,                          "vkDestroyVideoSessionKHR"));
	vkCreateVideoSessionParametersKHR                 = (PFN_vkCreateVideoSessionParametersKHR)(vkGetDeviceProcAddr(device,                 "vkCreateVideoSessionParametersKHR"));
	vkUpdateVideoSessionParametersKHR                 = (PFN_vkUpdateVideoSessionParametersKHR)(vkGetDeviceProcAddr(device,                 "vkUpdateVideoSessionParametersKHR"));
	vkDestroyVideoSessionParametersKHR                = (PFN_vkDestroyVideoSessionParametersKHR)(vkGetDeviceProcAddr(device,                "vkDestroyVideoSessionParametersKHR"));
	vkGetVideoSessionMemoryRequirementsKHR            = (PFN_vkGetVideoSessionMemoryRequirementsKHR)(vkGetDeviceProcAddr(device,            "vkGetVideoSessionMemoryRequirementsKHR"));
	vkBindVideoSessionMemoryKHR                       = (PFN_vkBindVideoSessionMemoryKHR)(vkGetDeviceProcAddr(device,                       "vkBindVideoSessionMemoryKHR"));
	vkCmdBeginVideoCodingKHR                          = (PFN_vkCmdBeginVideoCodingKHR)(vkGetDeviceProcAddr(device,                          "vkCmdBeginVideoCodingKHR"));
	vkCmdControlVideoCodingKHR                        = (PFN_vkCmdControlVideoCodingKHR)(vkGetDeviceProcAddr(device,                        "vkCmdControlVideoCodingKHR"));
	vkCmdEndVideoCodingKHR                            = (PFN_vkCmdEndVideoCodingKHR)(vkGetDeviceProcAddr(device,                            "vkCmdEndVideoCodingKHR"));
#endif

#if defined(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME) && defined(VK_USE_PLATFORM_ANDROID_KHR)
	vkGetAndroidHardwareBufferPropertiesANDROID       = (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)(vkGetDeviceProcAddr(device,       "vkGetAndroidHardwareBufferPropertiesANDROID"));
	vkGetMemoryAndroidHardwareBufferANDROID           = (PFN_vkGetMemoryAndroidHardwareBufferANDROID)(vkGetDeviceProcAddr(device,           "vkGetMemoryAndroidHardwareBufferANDROID"));
#endif

#if defined(VK_ANDROID_NATIVE_BUFFER_EXTENSION_NAME) && defined(VK_USE_PLATFORM_ANDROID_KHR)
	vkGetSwapchainGrallocUsageANDROID                 = (PFN_vkGetSwapchainGrallocUsageANDROID)(vkGetDeviceProcAddr(device,                 "vkGetSwapchainGrallocUsageANDROID"));
	vkGetSwapchainGrallocUsage2ANDROID                = (PFN_vkGetSwapchainGrallocUsage2ANDROID)(vkGetDeviceProcAddr(device,                "vkGetSwapchainGrallocUsage2ANDROID"));
	vkAcquireImageANDROID                             = (PFN_vkAcquireImageANDROID)(vkGetDeviceProcAddr(device,                             "vkAcquireImageANDROID"));
	vkQueueSignalReleaseImageANDROID                  = (PFN_vkQueueSignalReleaseImageANDROID)(vkGetDeviceProcAddr(device,                  "vkQueueSignalReleaseImageANDROID"));
#endif

#if defined(VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME) && defined(VK_USE_PLATFORM_FUCHSIA)
	vkCreateBufferCollectionFUCHSIA                   = (PFN_vkCreateBufferCollectionFUCHSIA)(vkGetDeviceProcAddr(device,                   "vkCreateBufferCollectionFUCHSIA"));
	vkSetBufferCollectionBufferConstraintsFUCHSIA     = (PFN_vkSetBufferCollectionBufferConstraintsFUCHSIA)(vkGetDeviceProcAddr(device,     "vkSetBufferCollectionBufferConstraintsFUCHSIA"));
	vkSetBufferCollectionImageConstraintsFUCHSIA      = (PFN_vkSetBufferCollectionImageConstraintsFUCHSIA)(vkGetDeviceProcAddr(device,      "vkSetBufferCollectionImageConstraintsFUCHSIA"));
	vkDestroyBufferCollectionFUCHSIA                  = (PFN_vkDestroyBufferCollectionFUCHSIA)(vkGetDeviceProcAddr(device,                  "vkDestroyBufferCollectionFUCHSIA"));
	vkGetBufferCollectionPropertiesFUCHSIA            = (PFN_vkGetBufferCollectionPropertiesFUCHSIA)(vkGetDeviceProcAddr(device,            "vkGetBufferCollectionPropertiesFUCHSIA"));
#endif

#if defined(VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME) && defined(VK_USE_PLATFORM_FUCHSIA)
	vkGetMemoryZirconHandleFUCHSIA                    = (PFN_vkGetMemoryZirconHandleFUCHSIA)(vkGetDeviceProcAddr(device,                    "vkGetMemoryZirconHandleFUCHSIA"));
	vkGetMemoryZirconHandlePropertiesFUCHSIA          = (PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA)(vkGetDeviceProcAddr(device,          "vkGetMemoryZirconHandlePropertiesFUCHSIA"));
#endif

#if defined(VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_FUCHSIA)
	vkGetSemaphoreZirconHandleFUCHSIA                 = (PFN_vkGetSemaphoreZirconHandleFUCHSIA)(vkGetDeviceProcAddr(device,                 "vkGetSemaphoreZirconHandleFUCHSIA"));
	vkImportSemaphoreZirconHandleFUCHSIA              = (PFN_vkImportSemaphoreZirconHandleFUCHSIA)(vkGetDeviceProcAddr(device,              "vkImportSemaphoreZirconHandleFUCHSIA"));
#endif

#if defined(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME) && defined(VK_USE_PLATFORM_WIN32_KHR)
	vkGetDeviceGroupSurfacePresentModes2EXT           = (PFN_vkGetDeviceGroupSurfacePresentModes2EXT)(vkGetDeviceProcAddr(device,           "vkGetDeviceGroupSurfacePresentModes2EXT"));
	vkAcquireFullScreenExclusiveModeEXT               = (PFN_vkAcquireFullScreenExclusiveModeEXT)(vkGetDeviceProcAddr(device,               "vkAcquireFullScreenExclusiveModeEXT"));
	vkReleaseFullScreenExclusiveModeEXT               = (PFN_vkReleaseFullScreenExclusiveModeEXT)(vkGetDeviceProcAddr(device,               "vkReleaseFullScreenExclusiveModeEXT"));
#endif

#if defined(VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME) && defined(VK_USE_PLATFORM_WIN32_KHR)
	vkGetFenceWin32HandleKHR                          = (PFN_vkGetFenceWin32HandleKHR)(vkGetDeviceProcAddr(device,                          "vkGetFenceWin32HandleKHR"));
	vkImportFenceWin32HandleKHR                       = (PFN_vkImportFenceWin32HandleKHR)(vkGetDeviceProcAddr(device,                       "vkImportFenceWin32HandleKHR"));
#endif

#if defined(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) && defined(VK_USE_PLATFORM_WIN32_KHR)
	vkGetMemoryWin32HandleKHR                         = (PFN_vkGetMemoryWin32HandleKHR)(vkGetDeviceProcAddr(device,                         "vkGetMemoryWin32HandleKHR"));
	vkGetMemoryWin32HandlePropertiesKHR               = (PFN_vkGetMemoryWin32HandlePropertiesKHR)(vkGetDeviceProcAddr(device,               "vkGetMemoryWin32HandlePropertiesKHR"));
#endif

#if defined(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME) && defined(VK_USE_PLATFORM_WIN32_KHR)
	vkGetSemaphoreWin32HandleKHR                      = (PFN_vkGetSemaphoreWin32HandleKHR)(vkGetDeviceProcAddr(device,                      "vkGetSemaphoreWin32HandleKHR"));
	vkImportSemaphoreWin32HandleKHR                   = (PFN_vkImportSemaphoreWin32HandleKHR)(vkGetDeviceProcAddr(device,                   "vkImportSemaphoreWin32HandleKHR"));
#endif

#if defined(VK_NV_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) && defined(VK_USE_PLATFORM_WIN32_KHR)
	vkGetMemoryWin32HandleNV                          = (PFN_vkGetMemoryWin32HandleNV)(vkGetDeviceProcAddr(device,                          "vkGetMemoryWin32HandleNV"));
#endif
}
