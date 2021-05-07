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
#include "../../Core/Util.hpp"
#include "../../Core/ThreadPool.hpp"
#include "../../Core/FrameCounter.hpp"
#include <VulkanGenericStructures.h>
#include <array>
#include <unordered_set>

#include "FrameGraph/Passes/VulkanGBufferPass.hpp"
#include "FrameGraph/Passes/VulkanCopyImagePass.hpp"

Vulkan::Renderer::Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter, ThreadPool* threadPool): ::Renderer(loggerQueue), mInstanceParameters(loggerQueue), mDeviceParameters(loggerQueue), mThreadPoolRef(threadPool), mFrameCounterRef(frameCounter)
{
	mDynamicLibrary = std::make_unique<FunctionsLibrary>();

	mDynamicLibrary->LoadGlobalFunctions();

	InitInstance();
	mDynamicLibrary->LoadInstanceFunctions(mInstance);

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

	mShaderManager = std::make_unique<ShaderManager>(mLoggingBoard);
}

Vulkan::Renderer::~Renderer()
{
	vkDeviceWaitIdle(mDevice); //No ThrowIfFailed in desctructors
	
	for(uint32_t i = 0; i < VulkanUtils::InFlightFrameCount; i++)
	{
		SafeDestroyObject(vkDestroyFence, mDevice, mRenderFences[i]);
	}

	mScene.reset();
	mFrameGraph.reset();
	mSwapChain.reset();
	mCommandBuffers.reset();

	SafeDestroyDevice(mDevice);
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
	
	InitializeSwapchainImages();

	CreateFrameGraph(window->GetWidth(), window->GetHeight());
}

void Vulkan::Renderer::ResizeWindowBuffers(Window* window)
{
	assert(mPhysicalDevice);

	ThrowIfFailed(vkDeviceWaitIdle(mDevice));

	mSwapChain->Recreate(mPhysicalDevice, mInstanceParameters, mDeviceParameters, window);
		
	InitializeSwapchainImages();

	CreateFrameGraph(window->GetWidth(), window->GetHeight());
}

void Vulkan::Renderer::InitScene(SceneDescription* sceneDescription)
{
	ThrowIfFailed(vkDeviceWaitIdle(mDevice));

	mScene = std::make_unique<RenderableScene>(mDevice, mFrameCounterRef, mDeviceParameters, mShaderManager.get());
	sceneDescription->BindRenderableComponent(mScene.get());

	RenderableSceneBuilder sceneBuilder(mScene.get());
	sceneDescription->BuildRenderableComponent(&sceneBuilder);

	sceneBuilder.BakeSceneFirstPart(mDeviceQueues.get(), mMemoryAllocator.get(), mShaderManager.get(), mDeviceParameters);
	sceneBuilder.BakeSceneSecondPart(mDeviceQueues.get(), mCommandBuffers.get());
}

void Vulkan::Renderer::CreateFrameGraph(uint32_t viewportWidth, uint32_t viewportHeight)
{
	mFrameGraph.reset();

	FrameGraphConfig frameGraphConfig;
	frameGraphConfig.SetScreenSize((uint16_t)viewportWidth, (uint16_t)viewportHeight);

	mFrameGraph = std::make_unique<FrameGraph>(mDevice, frameGraphConfig);

	FrameGraphBuilder frameGraphBuilder(mFrameGraph.get(), mScene.get(), &mInstanceParameters, &mDeviceParameters, mShaderManager.get());

	GBufferPass::Register(&frameGraphBuilder, "GBuffer");
	CopyImagePass::Register(&frameGraphBuilder, "CopyImage");

	frameGraphBuilder.AssignSubresourceName("GBuffer",   GBufferPass::ColorBufferImageId, "ColorBuffer");
	frameGraphBuilder.AssignSubresourceName("CopyImage", CopyImagePass::SrcImageId,       "ColorBuffer");
	frameGraphBuilder.AssignSubresourceName("CopyImage", CopyImagePass::DstImageId,       "Backbuffer");

	frameGraphBuilder.AssignBackbufferName("Backbuffer");

	frameGraphBuilder.Build(mDeviceQueues.get(), mCommandBuffers.get(), mMemoryAllocator.get(), mSwapChain.get());
}

void Vulkan::Renderer::RenderScene()
{
	const uint32_t currentFrameResourceIndex = mFrameCounterRef->GetFrameCount() % VulkanUtils::InFlightFrameCount;
	const uint32_t currentSwapchainIndex     = currentFrameResourceIndex;

	VkFence frameFence = mRenderFences[currentFrameResourceIndex];
	std::array frameFences = {frameFence};
	ThrowIfFailed(vkWaitForFences(mDevice, (uint32_t)(frameFences.size()), frameFences.data(), VK_TRUE, 3000000000));

	ThrowIfFailed(vkResetFences(mDevice, (uint32_t)(frameFences.size()), frameFences.data()));

	VkSemaphore preTraverseSemaphore = mSwapChain->GetImageAcquiredSemaphore(currentFrameResourceIndex);
	mSwapChain->AcquireImage(mDevice, currentFrameResourceIndex);

	VkSemaphore postTraverseSemaphore = VK_NULL_HANDLE;
	mFrameGraph->Traverse(mThreadPoolRef, mCommandBuffers.get(), mScene.get(), mDeviceQueues.get(), frameFence, currentFrameResourceIndex, currentSwapchainIndex, preTraverseSemaphore, &postTraverseSemaphore);

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
	for(uint32_t i = 0; i < VulkanUtils::InFlightFrameCount; i++)
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
	VkCommandBuffer commandBuffer = mCommandBuffers->GetMainThreadGraphicsCommandBuffer(0);
	VkCommandPool   commandPool   = mCommandBuffers->GetMainThreadComputeCommandPool(0);

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