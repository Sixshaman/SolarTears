#pragma once

#include <vulkan/vulkan.h>
#include "VulkanCInstanceParameters.hpp"
#include "VulkanCDeviceParameters.hpp"
#include "VulkanCUtils.hpp"
#include "../../Core/Window.hpp"

namespace VulkanCBindings
{
	class SwapChain
	{
	public:
		static constexpr uint32_t SwapchainImageCount = VulkanUtils::InFlightFrameCount;

	public:
		SwapChain(LoggerQueue* logger, VkInstance instance, VkDevice device);
		~SwapChain();

		void BindToWindow(VkPhysicalDevice physicalDevice, const InstanceParameters& instanceParameters, const DeviceParameters& deviceParameters, Window* window);
		void Recreate(VkPhysicalDevice physicalDevice, const InstanceParameters& instanceParameters, const DeviceParameters& deviceParameters, Window* window);

		void AcquireImage(VkDevice device, uint32_t frameInFlightIndex);
		void Present(uint32_t frameInFlightIndex);

		VkSemaphore GetImageAcquiredSemaphore(uint32_t frameInFlightIndex) const;
		VkSemaphore GetImagePresentSemaphore(uint32_t frameInFlightIndex)  const;

		uint32_t GetPresentQueueFamilyIndex() const;

		VkImage GetSwapchainImage(uint32_t index) const;
		VkImage GetCurrentImage()                 const;

		bool IsBackbufferHDR() const;

	private:
		void CreateSurface(VkInstance instance, Window* window);
		void CreateSwapChain(VkPhysicalDevice physicalDevice, VkDevice device, Window* window);

		void InvalidateSurfaceCapabilities(VkPhysicalDevice physicalDevice, const InstanceParameters& instanceParameters, const DeviceParameters& deviceParameters, Window* window);

		void FindPresentQueueIndex(VkPhysicalDevice physicalDevice);
		void GetPresentQueue(VkDevice device, uint32_t presentQueueIndex);

	#if defined(_WIN32) && defined(VK_USE_PLATFORM_WIN32_KHR)
		void CreateSurface(VkInstance instance, HWND hwnd);
	#endif 

	private:
		LoggerQueue* mLogger;

		const VkInstance mInstanceRef;
		const VkDevice   mDeviceRef;

		VkSurfaceKHR   mSurface;
		VkSwapchainKHR mSwapchain;

		uint32_t mCurrentImageIndex;

		uint32_t mPresentQueueFamilyIndex;

		VkQueue mPresentQueue;

		VkSurfaceCapabilitiesKHR                    mSurfaceCapabilities;
		VkSurfaceCapabilitiesFullScreenExclusiveEXT mExclussiveFullscreenCapabilities;
		VkDisplayNativeHdrSurfaceCapabilitiesAMD    mSurfaceHDRCapabilities;

		uint32_t mSwapchainWidth;
		uint32_t mSwapchainHeight;

		VkSemaphore mImageAcquiredSemaphores[VulkanUtils::InFlightFrameCount];
		VkSemaphore mImagePresentSemaphores[VulkanUtils::InFlightFrameCount];

		VkImage mSwapchainImages[SwapchainImageCount];

		VkColorSpaceKHR mSwapChainColorSpace;
		VkFormat        mSwapChainFormat;
	};
}