#include "VulkanRenderer.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanFunctionsLibrary.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanMemory.hpp"
#include "VulkanWorkerCommandBuffers.hpp"
#include "VulkanShaders.hpp"
#include "VulkanDeviceQueues.hpp"
#include "FrameGraph/VulkanRenderPass.hpp"
#include "FrameGraph/VulkanFrameGraph.hpp"
#include "FrameGraph/VulkanFrameGraphBuilder.hpp"
#include "Scene/VulkanScene.hpp"
#include "Scene/VulkanSceneBuilder.hpp"
#include "VulkanSamplers.hpp"
#include "VulkanDescriptors.hpp"
#include "VulkanSharedDescriptorDatabaseBuilder.hpp"
#include "VulkanPassDescriptorDatabaseBuilder.hpp"
#include "../Common/RenderingUtils.hpp"
#include "../../Core/Util.hpp"
#include "../../Core/ThreadPool.hpp"
#include "../../Core/FrameCounter.hpp"
#include <VulkanGenericStructures.h>
#include <array>
#include <unordered_set>

Vulkan::Renderer::Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter, ThreadPool* threadPool): ::Renderer(loggerQueue), mInstanceParameters(loggerQueue), mDeviceParameters(loggerQueue), mThreadPoolRef(threadPool), mFrameCounterRef(frameCounter)
{
	mDynamicLibrary = std::make_unique<FunctionsLibrary>();
	mDynamicLibrary->LoadGlobalFunctions();

	InitInstance();
	mDynamicLibrary->LoadInstanceFunctions(mInstance);

	InitDebuggingEnvironment();

	SelectPhysicalDevice(&mPhysicalDevice);

	mDeviceParameters.InvalidateDeviceParameters(mPhysicalDevice);

	mDeviceQueues = std::make_unique<DeviceQueues>(mPhysicalDevice);

	std::unordered_set<uint32_t> deviceFamilyQueues = {mDeviceQueues->GetGraphicsQueueFamilyIndex(), mDeviceQueues->GetComputeQueueFamilyIndex(), mDeviceQueues->GetTransferQueueFamilyIndex()};
	CreateLogicalDevice(mPhysicalDevice, deviceFamilyQueues);
	mDynamicLibrary->LoadDeviceFunctions(mDevice);

	mDeviceQueues->InitQueueHandles(mDevice);

	mSwapChain = std::make_unique<SwapChain>(mLoggingBoard, mInstance, mDevice);

	mCommandBuffers = std::make_unique<WorkerCommandBuffers>(mDevice, mThreadPoolRef->GetWorkerThreadCount(), mDeviceQueues.get());

	CreateFences();

	mMemoryAllocator = std::make_unique<MemoryManager>(mLoggingBoard, mPhysicalDevice, mDeviceParameters);

	mDescriptorDatabase = std::make_unique<DescriptorDatabase>(mDevice);
	mSamplerManager     = std::make_unique<SamplerManager>(mDevice);
}

Vulkan::Renderer::~Renderer()
{
	vkDeviceWaitIdle(mDevice); //No ThrowIfFailed in desctructors
	
	for(uint32_t i = 0; i < Utils::InFlightFrameCount; i++)
	{
		SafeDestroyObject(vkDestroyFence, mDevice, mRenderFences[i]);
	}

	mScene.reset();
	mFrameGraph.reset();
	mSwapChain.reset();
	mCommandBuffers.reset();

	SafeDestroyDevice(mDevice);

#if (defined(DEBUG) || defined(_DEBUG)) && defined(VK_EXT_debug_report)
	SafeDestroyObject(vkDestroyDebugUtilsMessengerEXT, mInstance, mDebugMessenger);
#endif

	SafeDestroyInstance(mInstance);
}

void Vulkan::Renderer::AttachToWindow(Window* window)
{
	assert(mPhysicalDevice != VK_NULL_HANDLE);

	ThrowIfFailed(vkDeviceWaitIdle(mDevice));
	
	mSwapChain->BindToWindow(mPhysicalDevice, mInstanceParameters, mDeviceParameters, window);

	//Assume it only has to be done once. The present queue index should NEVER change once we run the program
	if(mSwapChain->GetPresentQueueFamilyIndex() != mDeviceQueues->GetGraphicsQueueFamilyIndex() 
	&& mSwapChain->GetPresentQueueFamilyIndex() != mDeviceQueues->GetComputeQueueFamilyIndex() 
	&& mSwapChain->GetPresentQueueFamilyIndex() != mDeviceQueues->GetTransferQueueFamilyIndex())
	{
		//Device wasn't created with separate present queue in mind. Recreate the device
		SafeDestroyDevice(mDevice);

		std::unordered_set<uint32_t> deviceFamilyQueues = {mDeviceQueues->GetGraphicsQueueFamilyIndex(), mDeviceQueues->GetComputeQueueFamilyIndex(), mDeviceQueues->GetTransferQueueFamilyIndex(), mSwapChain->GetPresentQueueFamilyIndex()};
		CreateLogicalDevice(mPhysicalDevice, deviceFamilyQueues);

		mDeviceQueues->InitQueueHandles(mDevice);

		//Recreate the swapchain (without changing the present queue index)
		mSwapChain->Recreate(mPhysicalDevice, mInstanceParameters, mDeviceParameters, window);
	}

	mCommandBuffers->CreatePresentCommandBuffers(mSwapChain.get());

	InitializeSwapchainImages();
}

void Vulkan::Renderer::ResizeWindowBuffers(Window* window)
{
	assert(mPhysicalDevice);

	ThrowIfFailed(vkDeviceWaitIdle(mDevice));

	mSwapChain->Recreate(mPhysicalDevice, mInstanceParameters, mDeviceParameters, window);
		
	InitializeSwapchainImages();
}

BaseRenderableScene* Vulkan::Renderer::InitScene(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations, std::unordered_map<std::string_view, RenderableSceneObjectHandle>& outObjectHandles)
{
	ThrowIfFailed(vkDeviceWaitIdle(mDevice));

	mScene = std::make_unique<RenderableScene>(mDevice, mFrameCounterRef, mDeviceParameters);
	RenderableSceneBuilder sceneBuilder(mScene.get(), mMemoryAllocator.get(), mDeviceQueues.get(), mCommandBuffers.get(), &mDeviceParameters);

	sceneBuilder.Build(sceneDescription, sceneMeshInitialLocations, outObjectHandles);

	SharedDescriptorDatabaseBuilder sharedDatabaseBuilder(mDescriptorDatabase.get());
	sharedDatabaseBuilder.RecreateSharedSets(mScene.get(), mSamplerManager.get());

	return mScene.get();
}

void Vulkan::Renderer::InitFrameGraph(FrameGraphConfig&& frameGraphConfig, FrameGraphDescription&& frameGraphDescription)
{
	mFrameGraph = std::make_unique<FrameGraph>(mDevice, std::move(frameGraphConfig));

	FrameGraphBuildInfo frameGraphBuildInfo = 
	{
		.InstanceParams  = &mInstanceParameters,
		.DeviceParams    = &mDeviceParameters,
		.MemoryAllocator = mMemoryAllocator.get(),
		.Queues          = mDeviceQueues.get(),
		.CommandBuffers  = mCommandBuffers.get()
	};

	FrameGraphBuilder frameGraphBuilder(mLoggingBoard, mFrameGraph.get(), mSwapChain.get());
	frameGraphBuilder.Build(std::move(frameGraphDescription), frameGraphBuildInfo);

	PassDescriptorDatabaseBuilder   passDatabaseBuilder(mDescriptorDatabase.get());
	SharedDescriptorDatabaseBuilder sharedDatabaseBuilder(mDescriptorDatabase.get());
	frameGraphBuilder.ValidateDescriptors(mDescriptorDatabase.get(), &sharedDatabaseBuilder, &passDatabaseBuilder);

	passDatabaseBuilder.RecreatePassSets(&frameGraphBuilder);
	sharedDatabaseBuilder.RecreateSharedSets(mScene.get(), mSamplerManager.get());
}

void Vulkan::Renderer::Render()
{
	const uint32_t currentFrameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;
	const uint32_t currentSwapchainIndex     = currentFrameResourceIndex;

	VkFence frameFence = mRenderFences[currentFrameResourceIndex];
	std::array frameFences = {frameFence};
	ThrowIfFailed(vkWaitForFences(mDevice, (uint32_t)(frameFences.size()), frameFences.data(), VK_TRUE, 3000000000));

	ThrowIfFailed(vkResetFences(mDevice, (uint32_t)(frameFences.size()), frameFences.data()));

	VkSemaphore preTraverseSemaphore = mSwapChain->GetImageAcquiredSemaphore(currentFrameResourceIndex);
	mSwapChain->AcquireImage(mDevice, currentFrameResourceIndex);

	VkSemaphore postTraverseSemaphore = VK_NULL_HANDLE;
	mFrameGraph->Traverse(mThreadPoolRef, mCommandBuffers.get(), mScene.get(), mDeviceQueues.get(), mSwapChain.get(), frameFence, currentFrameResourceIndex, currentSwapchainIndex, preTraverseSemaphore, &postTraverseSemaphore);

	mSwapChain->Present(postTraverseSemaphore);
}

void Vulkan::Renderer::InitInstance()
{
	std::vector<std::string> enabledLayers;
	mInstanceParameters.InvalidateInstanceLayers(enabledLayers);

	std::vector<std::string>                      enabledExtensions;
	vgs::StructureChainBlob<VkInstanceCreateInfo> instanceCreateInfoChain;
	mInstanceParameters.InvalidateInstanceExtensions(enabledExtensions, instanceCreateInfoChain);

	//Convert from std::string to const char
	std::vector<const char*> enabledLayersCStrs;
	std::vector<const char*> enabledExtensionsCStrs;

	for(const std::string& layer: enabledLayers)
	{
		enabledLayersCStrs.push_back(layer.c_str());
	}

	for(const std::string& extension: enabledExtensions)
	{
		enabledExtensionsCStrs.push_back(extension.c_str());
	}

	VkApplicationInfo appInfo;
	appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext              = nullptr;
	appInfo.pApplicationName   = "SolarTears";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1); 
	appInfo.pEngineName        = "SolarTears";
	appInfo.engineVersion      = VK_MAKE_VERSION(0, 0, 1);
	appInfo.apiVersion         = VK_API_VERSION_1_1;

	VkInstanceCreateInfo& instanceCreateInfo   = instanceCreateInfoChain.GetChainHead();
	instanceCreateInfo.flags                   = 0;
	instanceCreateInfo.pApplicationInfo        = &appInfo;
	instanceCreateInfo.enabledLayerCount       = (uint32_t)enabledLayersCStrs.size();
	instanceCreateInfo.ppEnabledLayerNames     = enabledLayersCStrs.data();
	instanceCreateInfo.enabledExtensionCount   = (uint32_t)enabledExtensionsCStrs.size();
	instanceCreateInfo.ppEnabledExtensionNames = enabledExtensionsCStrs.data();

	ThrowIfFailed(vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance));
}

void Vulkan::Renderer::InitDebuggingEnvironment()
{
	mDebugMessenger = nullptr;

#if (defined(DEBUG) || defined(_DEBUG)) && defined(VK_EXT_debug_utils)
	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo;
	debugMessengerCreateInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugMessengerCreateInfo.pNext           = nullptr;
	debugMessengerCreateInfo.flags           = 0;
	debugMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugMessengerCreateInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugMessengerCreateInfo.pfnUserCallback = VulkanUtils::DebugReportCallback;
	debugMessengerCreateInfo.pUserData       = nullptr;

	ThrowIfFailed(vkCreateDebugUtilsMessengerEXT(mInstance, &debugMessengerCreateInfo, nullptr, &mDebugMessenger));
#endif
}

void Vulkan::Renderer::SelectPhysicalDevice(VkPhysicalDevice* outPhysicalDevice)
{
	assert(outPhysicalDevice != nullptr);

	//TODO: device selection? mGPU?
	std::vector<VkPhysicalDevice> physicalDevices;
	
	uint32_t physicalDeviceCount = 0;
	ThrowIfFailed(vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, nullptr));

	assert(physicalDeviceCount > 0);
	physicalDevices.resize(physicalDeviceCount);
	ThrowIfFailed(vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, physicalDevices.data()));

	*outPhysicalDevice = physicalDevices[0];
}

void Vulkan::Renderer::CreateLogicalDevice(VkPhysicalDevice physicalDevice, const std::unordered_set<uint32_t>& queueFamilies)
{
	SelectPhysicalDevice(&mPhysicalDevice);

	mDeviceParameters.InvalidateDeviceParameters(mPhysicalDevice);
	mLoggingBoard->PostLogMessage(std::string("GPU: ") + mDeviceParameters.GetDeviceProperties().deviceName);

	std::vector<std::string> enabledDeviceExtensions;
	vgs::StructureChainBlob<VkDeviceCreateInfo> deviceCreateInfoChain;
	mDeviceParameters.InvalidateDeviceExtensions(physicalDevice, enabledDeviceExtensions, deviceCreateInfoChain);

	std::vector<const char*> enabledExtensionsCStrs;
	for(const std::string& extension: enabledDeviceExtensions)
	{
		enabledExtensionsCStrs.push_back(extension.c_str());
	}

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	queueCreateInfos.reserve(queueFamilies.size());
	for(uint32_t familyIndex: queueFamilies)
	{
		//Always query 1 queue with priority 1.0f
		std::array<float, 1> queuePriorities = {1.0f};

		//No queue extensions are used. I don't need any global priorities and stuff
		VkDeviceQueueCreateInfo queueCreateInfo;
		queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.pNext            = nullptr;
		queueCreateInfo.flags            = 0;
		queueCreateInfo.queueFamilyIndex = familyIndex;
		queueCreateInfo.queueCount       = (uint32_t)queuePriorities.size();
		queueCreateInfo.pQueuePriorities = queuePriorities.data();

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo& deviceCreateInfo     = deviceCreateInfoChain.GetChainHead();
	deviceCreateInfo.flags                   = 0;
	deviceCreateInfo.queueCreateInfoCount    = (uint32_t)queueCreateInfos.size();
	deviceCreateInfo.pQueueCreateInfos       = queueCreateInfos.data();
	deviceCreateInfo.enabledLayerCount       = 0;
	deviceCreateInfo.ppEnabledLayerNames     = nullptr; //Deprecated
	deviceCreateInfo.enabledExtensionCount   = (uint32_t)enabledExtensionsCStrs.size();
	deviceCreateInfo.ppEnabledExtensionNames = enabledExtensionsCStrs.data();
	deviceCreateInfo.pEnabledFeatures        = nullptr;

	ThrowIfFailed(vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice));
}

void Vulkan::Renderer::CreateFences()
{
	for(uint32_t i = 0; i < Utils::InFlightFrameCount; i++)
	{
		VkFenceCreateInfo fenceCreateInfo;
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.pNext = nullptr;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		ThrowIfFailed(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mRenderFences[i]));
	}
}

void Vulkan::Renderer::InitializeSwapchainImages()
{
	VkCommandBuffer commandBuffer = mCommandBuffers->GetMainThreadPresentCommandBuffer(0);
	VkCommandPool   commandPool   = mCommandBuffers->GetMainThreadPresentCommandPool(0);

	ThrowIfFailed(vkResetCommandPool(mDevice, commandPool, 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo graphicsCmdBufferBeginInfo;
	graphicsCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	graphicsCmdBufferBeginInfo.pNext            = nullptr;
	graphicsCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	graphicsCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(commandBuffer, &graphicsCmdBufferBeginInfo);

	std::array<VkImageMemoryBarrier, SwapChain::SwapchainImageCount> imageBarriers;
	for(uint32_t i = 0; i < SwapChain::SwapchainImageCount; i++)
	{
		imageBarriers[i].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarriers[i].pNext                           = nullptr;
		imageBarriers[i].srcAccessMask                   = 0;
		imageBarriers[i].dstAccessMask                   = 0;
		imageBarriers[i].oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarriers[i].newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		imageBarriers[i].srcQueueFamilyIndex             = mSwapChain->GetPresentQueueFamilyIndex();
		imageBarriers[i].dstQueueFamilyIndex             = mSwapChain->GetPresentQueueFamilyIndex();
		imageBarriers[i].image                           = mSwapChain->GetSwapchainImage(i);
		imageBarriers[i].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarriers[i].subresourceRange.baseMipLevel   = 0;
		imageBarriers[i].subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imageBarriers[i].subresourceRange.baseArrayLayer = 0;
		imageBarriers[i].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
	}

	std::array<VkMemoryBarrier, 0>       memoryBarriers;
	std::array<VkBufferMemoryBarrier, 0> bufferBarriers;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		                 (uint32_t)memoryBarriers.size(), memoryBarriers.data(),
		                 (uint32_t)bufferBarriers.size(), bufferBarriers.data(),
		                 (uint32_t)imageBarriers.size(), imageBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(commandBuffer));

	mDeviceQueues->GraphicsQueueSubmit(commandBuffer);
	mDeviceQueues->GraphicsQueueWait();
}