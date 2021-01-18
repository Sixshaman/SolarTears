#include "VulkanCSwapChain.hpp"
#include "../../../3rd party/VulkanGenericStructures/Include/VulkanGenericStructures.h"
#include "VulkanCFunctions.hpp"
#include "VulkanCUtils.hpp"
#include <unordered_set>
#include <array>
#include <algorithm>

VulkanCBindings::SwapChain::SwapChain(LoggerQueue* logger, VkInstance instance, VkDevice device): mLogger(logger), mInstanceRef(instance), mDeviceRef(device), mPresentQueueFamilyIndex((uint32_t)(-1)),
                                                                                                  mSwapchainWidth(0), mSwapchainHeight(0),
                                                                                                  mSwapChainColorSpace(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR), mSwapChainFormat(VK_FORMAT_B8G8R8A8_UNORM),
	                                                                                              mCurrentImageIndex((uint32_t)(-1)), mSurface(VK_NULL_HANDLE), mSwapchain(VK_NULL_HANDLE),
	                                                                                              mPresentQueue(VK_NULL_HANDLE)
{
	for(uint32_t i = 0; i < VulkanUtils::InFlightFrameCount; i++)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo;
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;
		semaphoreCreateInfo.flags = 0;

		ThrowIfFailed(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &mImageAcquiredSemaphores[i]));
		ThrowIfFailed(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &mImagePresentSemaphores[i]));
	}
}

VulkanCBindings::SwapChain::~SwapChain()
{
	SafeDestroyObject(vkDestroySwapchainKHR, mDeviceRef, mSwapchain);
	SafeDestroyObject(vkDestroySurfaceKHR,   mInstanceRef, mSurface);

	for(size_t i = 0; i < VulkanUtils::InFlightFrameCount; i++)
	{
		SafeDestroyObject(vkDestroySemaphore, mDeviceRef, mImageAcquiredSemaphores[i]);
		SafeDestroyObject(vkDestroySemaphore, mDeviceRef, mImagePresentSemaphores[i]);
	}
}

void VulkanCBindings::SwapChain::BindToWindow(VkPhysicalDevice physicalDevice, const InstanceParameters& instanceParameters, const DeviceParameters& deviceParameters, Window* window)
{
	SafeDestroyObject(vkDestroySurfaceKHR, mInstanceRef, mSurface);

	CreateSurface(mInstanceRef, window);
	InvalidateSurfaceCapabilities(physicalDevice, instanceParameters, deviceParameters, window);

	FindPresentQueueIndex(physicalDevice);
	GetPresentQueue(mDeviceRef, mPresentQueueFamilyIndex);

	SafeDestroyObject(vkDestroySwapchainKHR, mDeviceRef, mSwapchain);
	CreateSwapChain(physicalDevice, mDeviceRef, window);

	uint32_t swapchainImageCount = 0;
	ThrowIfFailed(vkGetSwapchainImagesKHR(mDeviceRef, mSwapchain, &swapchainImageCount, nullptr));

	assert(swapchainImageCount == SwapchainImageCount);
	ThrowIfFailed(vkGetSwapchainImagesKHR(mDeviceRef, mSwapchain, &swapchainImageCount, &mSwapchainImages[0]));
}

void VulkanCBindings::SwapChain::Recreate(VkPhysicalDevice physicalDevice, const InstanceParameters& instanceParameters, const DeviceParameters& deviceParameters, Window* window)
{
	SafeDestroyObject(vkDestroySwapchainKHR, mDeviceRef,   mSwapchain);
	SafeDestroyObject(vkDestroySurfaceKHR,   mInstanceRef, mSurface);

	CreateSurface(mInstanceRef, window);
	InvalidateSurfaceCapabilities(physicalDevice, instanceParameters, deviceParameters, window);

	//We're TOTALLY SCREWED if surface doesn't support presenting anymore for this exact queue index
	VkBool32 presentationStillSupported = VK_FALSE;
	ThrowIfFailed(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, mPresentQueueFamilyIndex, mSurface, &presentationStillSupported));
	assert(presentationStillSupported);

	CreateSwapChain(physicalDevice, mDeviceRef, window);

	uint32_t swapchainImageCount = SwapchainImageCount;
	ThrowIfFailed(vkGetSwapchainImagesKHR(mDeviceRef, mSwapchain, &swapchainImageCount, &mSwapchainImages[0]));
}

void VulkanCBindings::SwapChain::AcquireImage(VkDevice device, uint32_t frameInFlightIndex)
{
	//TODO: mGPU?
	VkAcquireNextImageInfoKHR acquireNextImageInfo;
	acquireNextImageInfo.sType      = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
	acquireNextImageInfo.pNext      = nullptr;
	acquireNextImageInfo.swapchain  = mSwapchain;
	acquireNextImageInfo.timeout    = 2000000;
	acquireNextImageInfo.semaphore  = mImageAcquiredSemaphores[frameInFlightIndex];
	acquireNextImageInfo.fence      = nullptr;
	acquireNextImageInfo.deviceMask = 0x01;

	ThrowIfFailed(vkAcquireNextImage2KHR(device, &acquireNextImageInfo, &mCurrentImageIndex));
}

VkSemaphore VulkanCBindings::SwapChain::GetImageAcquiredSemaphore(uint32_t frameInFlightIndex) const
{
	assert(frameInFlightIndex < SwapchainImageCount);

	return mImageAcquiredSemaphores[frameInFlightIndex];
}

VkSemaphore VulkanCBindings::SwapChain::GetImagePresentSemaphore(uint32_t frameInFlightIndex) const
{
	assert(frameInFlightIndex < SwapchainImageCount);

	return mImagePresentSemaphores[frameInFlightIndex];
}

void VulkanCBindings::SwapChain::Present(uint32_t frameInFlightIndex)
{
	std::array presentSemaphores = {mImagePresentSemaphores[frameInFlightIndex]};
	std::array swapchains        = {mSwapchain};
	std::array imageIndices      = {mCurrentImageIndex};

	VkPresentInfoKHR presentInfo;
	presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext              = nullptr;
	presentInfo.waitSemaphoreCount = (uint32_t)presentSemaphores.size();
	presentInfo.pWaitSemaphores    = presentSemaphores.data();
	presentInfo.swapchainCount     = (uint32_t)swapchains.size();
	presentInfo.pSwapchains        = swapchains.data();
	presentInfo.pImageIndices      = imageIndices.data();
	presentInfo.pResults           = nullptr;

	ThrowIfFailed(vkQueuePresentKHR(mPresentQueue, &presentInfo));
}

uint32_t VulkanCBindings::SwapChain::GetPresentQueueFamilyIndex() const
{
	return mPresentQueueFamilyIndex;
}

VkImage VulkanCBindings::SwapChain::GetSwapchainImage(uint32_t index) const
{
	assert(index < SwapchainImageCount);

	return mSwapchainImages[index];
}

VkImage VulkanCBindings::SwapChain::GetCurrentImage() const
{
	return mSwapchainImages[mCurrentImageIndex];
}

VkFormat VulkanCBindings::SwapChain::GetBackbufferFormat() const
{
	return mSwapChainFormat;
}

bool VulkanCBindings::SwapChain::IsBackbufferHDR() const
{
	return (mSwapChainColorSpace == VK_COLOR_SPACE_HDR10_HLG_EXT) || (mSwapChainColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) || (mSwapChainColorSpace == VK_COLOR_SPACE_DOLBYVISION_EXT);
}

void VulkanCBindings::SwapChain::CreateSurface(VkInstance instance, Window* window)
{
	CreateSurface(instance, window->NativeHandle());
}

void VulkanCBindings::SwapChain::CreateSwapChain(VkPhysicalDevice physicalDevice, VkDevice device, Window* window)
{
	SafeDestroyObject(vkDestroySwapchainKHR, device, mSwapchain);

	VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo;
	surfaceInfo.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surfaceInfo.pNext   = nullptr;
	surfaceInfo.surface = mSurface;

	std::vector<VkPresentModeKHR> presentModes;

	uint32_t presentModeCount = 0;
	ThrowIfFailed(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &presentModeCount, nullptr));

	presentModes.resize(presentModeCount);
	ThrowIfFailed(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &presentModeCount, presentModes.data()));
	
	//Use mailbox mode if it's supported, otherwise use FIFO
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if(std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end())
	{
		presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	}

	if(mSurfaceCapabilities.currentExtent.width == 0xffffffff && mSurfaceCapabilities.currentExtent.height == 0xffffffff)
	{
		mSwapchainWidth  = window->GetWidth();
		mSwapchainHeight = window->GetHeight();
	}
	else
	{
		mSwapchainWidth  = std::clamp(window->GetWidth(),  mSurfaceCapabilities.minImageExtent.width,  mSurfaceCapabilities.maxImageExtent.width);
		mSwapchainHeight = std::clamp(window->GetHeight(), mSurfaceCapabilities.minImageExtent.height, mSurfaceCapabilities.maxImageExtent.height);
	}

	//TODO: HDR?
	std::vector<VkSurfaceFormat2KHR> surfaceFormats;
	
	uint32_t surfaceFormatCount = 0;
	ThrowIfFailed(vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo, &surfaceFormatCount, nullptr));

	surfaceFormats.resize(surfaceFormatCount);
	for(size_t i = 0; i < surfaceFormats.size(); i++)
	{
		surfaceFormats[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
		surfaceFormats[i].pNext = nullptr;
	}

	ThrowIfFailed(vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo, &surfaceFormatCount, surfaceFormats.data()));
	if(surfaceFormats.size() == 1 && surfaceFormats[0].surfaceFormat.format == VK_FORMAT_UNDEFINED)
	{
		//Use BT2020, because we can
		mSwapChainColorSpace = VK_COLOR_SPACE_BT2020_LINEAR_EXT;
		mSwapChainFormat     = VK_FORMAT_B8G8R8A8_UNORM;
	}
	else
	{
		//Without HDR: Match in order BT2020 > BT709 > SRGB. Doesn't actually influence anything elsewhere because swapchain color space is only for presenting, rendering is decided in swapchain format
		auto colorSpaces = {VK_COLOR_SPACE_BT2020_LINEAR_EXT, VK_COLOR_SPACE_BT709_LINEAR_EXT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
		for(VkColorSpaceKHR colorSpace: colorSpaces)
		{
			auto colorSpaceIter = std::find_if(surfaceFormats.begin(), surfaceFormats.end(), [colorSpace](VkSurfaceFormat2KHR surfaceFormat)
			{
				return surfaceFormat.surfaceFormat.colorSpace == colorSpace && surfaceFormat.surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM;
			});

			if(colorSpaceIter != surfaceFormats.end())
			{
				mSwapChainColorSpace = colorSpaceIter->surfaceFormat.colorSpace;
				mSwapChainFormat     = colorSpaceIter->surfaceFormat.format;
				break;
			}
		}
	}

	const VkImageUsageFlags          swapchainUsages     = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	const VkSurfaceTransformFlagsKHR swapchainTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	if(!(swapchainUsages & mSurfaceCapabilities.supportedUsageFlags))
	{
		mLogger->PostLogMessage("No valid swapchain usage support!");
	}

	if(!(swapchainTransforms & mSurfaceCapabilities.supportedTransforms))
	{
		mLogger->PostLogMessage("No valid swapchain transform support!");
	}

	//TODO: VR?
	const uint32_t multiviewImageCount = 1;

	//TODO: exclusive fullscreen?
	vgs::StructureChainBlob<VkSwapchainCreateInfoKHR> swapchainCreateInfoChain;
	if(mExclussiveFullscreenCapabilities.fullScreenExclusiveSupported)
	{
		VkSurfaceFullScreenExclusiveInfoEXT exclusiveFullscreenInfo;
		exclusiveFullscreenInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
		swapchainCreateInfoChain.AppendToChain(exclusiveFullscreenInfo);

		#ifdef VK_USE_PLATFORM_WIN32_KHR
			VkSurfaceFullScreenExclusiveWin32InfoEXT exclusiveFullscreenWin32Info;
			exclusiveFullscreenWin32Info.hmonitor = window->NativeDisplay();
			swapchainCreateInfoChain.AppendToChain(exclusiveFullscreenWin32Info);
		#endif
	}

	std::array queueIndices = {mPresentQueueFamilyIndex};

	VkSwapchainCreateInfoKHR& swapchainCreateInfo = swapchainCreateInfoChain.GetChainHead();
	swapchainCreateInfo.flags                     = 0;
	swapchainCreateInfo.surface                   = mSurface;
	swapchainCreateInfo.minImageCount             = SwapchainImageCount;
	swapchainCreateInfo.imageFormat               = mSwapChainFormat;
	swapchainCreateInfo.imageColorSpace           = mSwapChainColorSpace;
	swapchainCreateInfo.imageExtent.width         = window->GetWidth();
	swapchainCreateInfo.imageExtent.height        = window->GetHeight();
	swapchainCreateInfo.imageArrayLayers          = multiviewImageCount;
	swapchainCreateInfo.imageUsage                = swapchainUsages;
	swapchainCreateInfo.imageSharingMode          = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount     = (uint32_t)queueIndices.size();
	swapchainCreateInfo.pQueueFamilyIndices       = queueIndices.data();
	swapchainCreateInfo.preTransform              = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha            = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode               = presentMode;
	swapchainCreateInfo.clipped                   = VK_TRUE;
	swapchainCreateInfo.oldSwapchain              = mSwapchain;

	VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
	ThrowIfFailed(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &newSwapchain));

	mSwapchain = newSwapchain;
}

void VulkanCBindings::SwapChain::InvalidateSurfaceCapabilities(VkPhysicalDevice physicalDevice, const InstanceParameters& instanceParameters, const DeviceParameters& deviceParameters, Window* window)
{	
	if(instanceParameters.IsSurfaceCapabilities2ExtensionEnabled())
	{
		vgs::StructureChainBlob<VkPhysicalDeviceSurfaceInfo2KHR> surfaceInfoChain;
		vgs::GenericStructureChain<VkSurfaceCapabilities2KHR>    surfaceCapabilities2Chain;

		if(deviceParameters.IsDisplayNativeHDRExtensionEnabled())
		{
			surfaceCapabilities2Chain.AppendToChain(mSurfaceHDRCapabilities);
		}

		if(deviceParameters.IsFullscreenExclusiveExtensionEnabled())
		{
			//surfaceCapabilities2Chain.AppendToChain(mExclussiveFullscreenCapabilities);

			//VkSurfaceFullScreenExclusiveInfoEXT exclusiveFullscreenInfo;
			//exclusiveFullscreenInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
			//surfaceInfoChain.AppendToChain(exclusiveFullscreenInfo);

			//#ifdef VK_USE_PLATFORM_WIN32_KHR
			//	VkSurfaceFullScreenExclusiveWin32InfoEXT exclusiveFullscreenWin32Info;
			//	exclusiveFullscreenWin32Info.hmonitor = window->NativeDisplay();
			//	surfaceInfoChain.AppendToChain(exclusiveFullscreenWin32Info);
			//#endif
		}
		else
		{
			UNREFERENCED_PARAMETER(window);
		}

		VkPhysicalDeviceSurfaceInfo2KHR& surfaceInfo = surfaceInfoChain.GetChainHead();
		surfaceInfo.surface = mSurface;

		ThrowIfFailed(vkGetPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo, &surfaceCapabilities2Chain.GetChainHead()));

		mSurfaceCapabilities = surfaceCapabilities2Chain.GetChainHead().surfaceCapabilities;
	}
	else
	{
		ThrowIfFailed(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface, &mSurfaceCapabilities));

		mSurfaceHDRCapabilities.localDimmingSupport                    = false;
		mExclussiveFullscreenCapabilities.fullScreenExclusiveSupported = false;
	}
}

void VulkanCBindings::SwapChain::FindPresentQueueIndex(VkPhysicalDevice physicalDevice)
{
	uint32_t presentQueueFamilyIndex = (uint32_t)(-1);

	std::vector<VkQueueFamilyProperties> queueFamiliesProperties;
	
	uint32_t queuePropertyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queuePropertyCount, nullptr);

	queueFamiliesProperties.resize(queuePropertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queuePropertyCount, queueFamiliesProperties.data());

	for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < (uint32_t)queueFamiliesProperties.size(); queueFamilyIndex++)
	{
		VkBool32 presentationSupported = VK_FALSE;
		ThrowIfFailed(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, mSurface, &presentationSupported));

		if(presentationSupported)
		{
			presentQueueFamilyIndex = queueFamilyIndex;
		}
	}

	assert(presentQueueFamilyIndex != (uint32_t)(-1));

	mPresentQueueFamilyIndex = presentQueueFamilyIndex;
}

void VulkanCBindings::SwapChain::GetPresentQueue(VkDevice device, uint32_t presentIndex)
{
	VkDeviceQueueInfo2 presentQueueInfo;
	presentQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	presentQueueInfo.pNext            = nullptr;
	presentQueueInfo.flags            = VkDeviceQueueCreateFlags(0); //No priorities!
	presentQueueInfo.queueIndex       = 0;                           //Use only one queue per family
	presentQueueInfo.queueFamilyIndex = presentIndex;

	vkGetDeviceQueue2(device, &presentQueueInfo, &mPresentQueue);
}

#if defined(_WIN32) && defined(VK_USE_PLATFORM_WIN32_KHR)
	void VulkanCBindings::SwapChain::CreateSurface(VkInstance instance, HWND hwnd)
	{
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo;
		surfaceCreateInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext     = nullptr;
		surfaceCreateInfo.flags     = 0;
		surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
		surfaceCreateInfo.hwnd      = hwnd;

		ThrowIfFailed(vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &mSurface));
	}
#endif