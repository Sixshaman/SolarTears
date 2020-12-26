#include "VulkanCRenderer.hpp"
#include "VulkanCFunctions.hpp"
#include "VulkanCFunctionsLibrary.hpp"
#include "VulkanCSwapChain.hpp"
#include "VulkanCMemory.hpp"
#include "VulkanCWorkerCommandBuffers.hpp"
#include "VulkanCShaders.hpp"
#include "FrameGraph/VulkanCRenderPass.hpp"
#include "FrameGraph/VulkanCFrameGraph.hpp"
#include "FrameGraph/VulkanCFrameGraphBuilder.hpp"
#include "Scene/VulkanCScene.hpp"
#include "Scene/VulkanCSceneBuilder.hpp"
#include "../../Core/Util.hpp"
#include "../../Core/ThreadPool.hpp"
#include "3rdParty/VulkanGenericStructures.h"
#include <array>
#include <unordered_set>

#include "FrameGraph/Passes/VulkanCGBufferRenderPass.hpp"
#include "FrameGraph/Passes/VulkanCCopyImagePass.hpp"

VulkanCBindings::Renderer::Renderer(LoggerQueue* loggerQueue, ThreadPool* threadPool): ::Renderer(loggerQueue), mInstanceParameters(loggerQueue), mDeviceParameters(loggerQueue), mThreadPool(threadPool),
                                                                                       mGraphicsQueueFamilyIndex((uint32_t)-1), mComputeQueueFamilyIndex((uint32_t)-1), mTransferQueueFamilyIndex((uint32_t)-1),
	                                                                                   mCurrentFrameIndex(0)
{
	mDynamicLibrary = std::make_unique<VulkanCBindings::FunctionsLibrary>();

	mDynamicLibrary->LoadGlobalFunctions();

	InitInstance();
	mDynamicLibrary->LoadInstanceFunctions(mInstance);

	SelectPhysicalDevice(&mPhysicalDevice);

	mDeviceParameters.InvalidateDeviceParameters(mPhysicalDevice);
	FindDeviceQueueIndices(mPhysicalDevice, &mGraphicsQueueFamilyIndex, &mComputeQueueFamilyIndex, &mTransferQueueFamilyIndex);

	std::unordered_set<uint32_t> deviceFamilyQueues = {mGraphicsQueueFamilyIndex, mComputeQueueFamilyIndex, mTransferQueueFamilyIndex};
	CreateLogicalDevice(mPhysicalDevice, deviceFamilyQueues);
	mDynamicLibrary->LoadDeviceFunctions(mDevice);

	GetDeviceQueues(mGraphicsQueueFamilyIndex, mComputeQueueFamilyIndex, mTransferQueueFamilyIndex);

	mSwapChain = std::make_unique<VulkanCBindings::SwapChain>(mLoggingBoard, mInstance, mDevice);

	CreateCommandBuffersAndPools();
	CreateFences();

	mMemoryAllocator = std::make_unique<VulkanCBindings::MemoryManager>(mLoggingBoard, mPhysicalDevice, mDeviceParameters);
}

VulkanCBindings::Renderer::~Renderer()
{
	vkDeviceWaitIdle(mDevice); //No ThrowIfFailed in desctructors
	
	for(uint32_t i = 0; i < VulkanUtils::InFlightFrameCount; i++)
	{
		SafeDestroyObject(vkDestroyFence, mDevice, mRenderFences[i]);
	}

	mScene.reset();
	mCommandBuffers.reset();
	mSwapChain.reset();

	SafeDestroyDevice(mDevice);
	SafeDestroyInstance(mInstance);
}

void VulkanCBindings::Renderer::AttachToWindow(Window* window)
{
	assert(mPhysicalDevice != VK_NULL_HANDLE);

	ThrowIfFailed(vkDeviceWaitIdle(mDevice));
	
	mSwapChain->BindToWindow(mPhysicalDevice, mInstanceParameters, mDeviceParameters, window);

	//Assume it only has to be done once. The present queue index should NEVER change once we run the program
	if(mSwapChain->GetPresentQueueFamilyIndex() != mGraphicsQueueFamilyIndex && mSwapChain->GetPresentQueueFamilyIndex() != mComputeQueueFamilyIndex && mSwapChain->GetPresentQueueFamilyIndex() != mTransferQueueFamilyIndex)
	{
		//Device wasn't created with separate present queue in mind. Recreate the device
		SafeDestroyDevice(mDevice);

		std::unordered_set<uint32_t> deviceFamilyQueues = { mGraphicsQueueFamilyIndex, mComputeQueueFamilyIndex, mTransferQueueFamilyIndex, mSwapChain->GetPresentQueueFamilyIndex()};
		CreateLogicalDevice(mPhysicalDevice, deviceFamilyQueues);

		//Recreate the swapchain (without changing the present queue index)
		mSwapChain->Recreate(mPhysicalDevice, mInstanceParameters, mDeviceParameters, window);
	}
	
	InitializeSwapchainImages();
}

void VulkanCBindings::Renderer::ResizeWindowBuffers(Window* window)
{
	assert(mPhysicalDevice);

	ThrowIfFailed(vkDeviceWaitIdle(mDevice));

	mSwapChain->Recreate(mPhysicalDevice, mInstanceParameters, mDeviceParameters, window);
		
	InitializeSwapchainImages();
}

void VulkanCBindings::Renderer::InitSceneAndFrameGraph(SceneDescription* scene)
{
	ThrowIfFailed(vkDeviceWaitIdle(mDevice));


	//Scene
	ShaderManager shaderManager(mLoggingBoard);
	mScene = std::make_unique<VulkanCBindings::RenderableScene>(mDevice, mDeviceParameters, &shaderManager);

	VulkanCBindings::RenderableSceneBuilder sceneBuilder(mScene.get(), mTransferQueueFamilyIndex, mComputeQueueFamilyIndex, mGraphicsQueueFamilyIndex);
	scene->BindRenderableComponent(&sceneBuilder);

	sceneBuilder.BakeSceneFirstPart(mMemoryAllocator.get(), &shaderManager, mDeviceParameters);


	//Frame graph
	mFrameGraph = std::make_unique<VulkanCBindings::FrameGraph>(mDevice);

	VulkanCBindings::FrameGraphBuilder frameGraphBuilder(mFrameGraph.get(), mScene.get(), &mDeviceParameters, &shaderManager);

	GBufferPass::Register(&frameGraphBuilder, "GBuffer");
	CopyImagePass::Register(&frameGraphBuilder, "CopyImage");

	frameGraphBuilder.AssignSubresourceName("GBuffer",   GBufferPass::ColorBufferImageId, "ColorBuffer");
	frameGraphBuilder.AssignSubresourceName("CopyImage", CopyImagePass::SrcImageId,       "ColorBuffer");
	frameGraphBuilder.AssignSubresourceName("CopyImage", CopyImagePass::DstImageId,       "Backbuffer");

	frameGraphBuilder.AssignBackbufferName("Backbuffer");

	frameGraphBuilder.Build();

	//Baking
	ThrowIfFailed(vkResetCommandPool(mDevice, mMainTransferCommandPools[0], 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo transferCmdBufferBeginInfo;
	transferCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	transferCmdBufferBeginInfo.pNext            = nullptr;
	transferCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	transferCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(mMainTransferCommandBuffers[0], &transferCmdBufferBeginInfo));
	
	sceneBuilder.BakeSceneSecondPart(mMainTransferCommandBuffers[0]);

	ThrowIfFailed(vkEndCommandBuffer(mMainTransferCommandBuffers[0]));

	std::array commandBuffersToExecute = {mMainTransferCommandBuffers[0]};

	//TODO: mGPU?
	//TODO: Performance query?
	VkSubmitInfo queueSubmitInfo;
	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext                = nullptr;
	queueSubmitInfo.waitSemaphoreCount   = 0;
	queueSubmitInfo.pWaitSemaphores      = nullptr;
	queueSubmitInfo.pWaitDstStageMask    = nullptr;
	queueSubmitInfo.commandBufferCount   = (uint32_t)commandBuffersToExecute.size();
	queueSubmitInfo.pCommandBuffers      = commandBuffersToExecute.data();
	queueSubmitInfo.signalSemaphoreCount = 0;
	queueSubmitInfo.pSignalSemaphores    = nullptr;

	std::array submitInfos = {queueSubmitInfo};
	ThrowIfFailed(vkQueueSubmit(mTransferQueue, (uint32_t)submitInfos.size(), submitInfos.data(), nullptr));

	ThrowIfFailed(vkQueueWaitIdle(mTransferQueue));
}

void VulkanCBindings::Renderer::RenderScene()
{
	const uint32_t currentFrameResourceIndex = mCurrentFrameIndex % VulkanUtils::InFlightFrameCount;

	std::array frameFences = {mRenderFences[currentFrameResourceIndex]};
	ThrowIfFailed(vkWaitForFences(mDevice, (uint32_t)(frameFences.size()), frameFences.data(), VK_TRUE, 3000000000));

	ThrowIfFailed(vkResetFences(mDevice, (uint32_t)(frameFences.size()), frameFences.data()));
	ThrowIfFailed(vkResetCommandPool(mDevice, mMainGraphicsCommandPools[currentFrameResourceIndex], 0));

	mSwapChain->AcquireImage(mDevice, currentFrameResourceIndex);

	VkImage currSwapchainImage = mSwapChain->GetCurrentImage();

	VkCommandBufferBeginInfo graphicsCmdBufferBeginInfo;
	graphicsCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	graphicsCmdBufferBeginInfo.pNext            = nullptr;
	graphicsCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	graphicsCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(mMainGraphicsCommandBuffers[currentFrameResourceIndex], &graphicsCmdBufferBeginInfo);

	VkImageMemoryBarrier swapchainAcquireBarrier;
	swapchainAcquireBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	swapchainAcquireBarrier.pNext                           = nullptr;
	swapchainAcquireBarrier.srcAccessMask                   = 0;
	swapchainAcquireBarrier.dstAccessMask                   = 0;
	swapchainAcquireBarrier.oldLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	swapchainAcquireBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	swapchainAcquireBarrier.srcQueueFamilyIndex             = mSwapChain->GetPresentQueueFamilyIndex();
	swapchainAcquireBarrier.dstQueueFamilyIndex             = mGraphicsQueueFamilyIndex;
	swapchainAcquireBarrier.image                           = currSwapchainImage;
	swapchainAcquireBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	swapchainAcquireBarrier.subresourceRange.baseMipLevel   = 0;
	swapchainAcquireBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
	swapchainAcquireBarrier.subresourceRange.baseArrayLayer = 0;
	swapchainAcquireBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

	std::array<VkMemoryBarrier, 0>         memorySwapchainAcquireBarriers;
	std::array<VkBufferMemoryBarrier, 0>   bufferSwapchainAcquireBarriers;
	std::array                             imageSwapchainAcquireBarriers = {swapchainAcquireBarrier};
	vkCmdPipelineBarrier(mMainGraphicsCommandBuffers[currentFrameResourceIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		                 (uint32_t)memorySwapchainAcquireBarriers.size(), memorySwapchainAcquireBarriers.data(),
		                 (uint32_t)bufferSwapchainAcquireBarriers.size(), bufferSwapchainAcquireBarriers.data(),
		                 (uint32_t)imageSwapchainAcquireBarriers.size(),  imageSwapchainAcquireBarriers.data());

	VkImageSubresourceRange clearRange;
	clearRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	clearRange.baseMipLevel   = 0;
	clearRange.levelCount     = VK_REMAINING_MIP_LEVELS;
	clearRange.baseArrayLayer = 0;
	clearRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

	VkClearColorValue clearColor;
	std::array<float, 4> clearColorData = {1.0f, 0.0f, 0.0f, 1.0f};
	memcpy(clearColor.float32, clearColorData.data(), clearColorData.size() * sizeof(float));

	std::array clearRanges = {clearRange};
	vkCmdClearColorImage(mMainComputeCommandBuffers[currentFrameResourceIndex], currSwapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, (uint32_t)clearRanges.size(), clearRanges.data());

	VkImageMemoryBarrier swapchainPresentBarrier;
	swapchainPresentBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	swapchainPresentBarrier.pNext                           = nullptr;
	swapchainPresentBarrier.srcAccessMask                   = 0;
	swapchainPresentBarrier.dstAccessMask                   = 0;
	swapchainPresentBarrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	swapchainPresentBarrier.newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	swapchainPresentBarrier.srcQueueFamilyIndex             = mGraphicsQueueFamilyIndex;
	swapchainPresentBarrier.dstQueueFamilyIndex             = mSwapChain->GetPresentQueueFamilyIndex();
	swapchainPresentBarrier.image                           = currSwapchainImage;
	swapchainPresentBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	swapchainPresentBarrier.subresourceRange.baseMipLevel   = 0;
	swapchainPresentBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
	swapchainPresentBarrier.subresourceRange.baseArrayLayer = 0;
	swapchainPresentBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

	std::array<VkMemoryBarrier, 0>       memorySwapchainPresentBarriers;
	std::array<VkBufferMemoryBarrier, 0> bufferSwapchainPresentBarriers;
	std::array                           imageSwapchainPresentBarriers = {swapchainPresentBarrier};
	vkCmdPipelineBarrier(mMainGraphicsCommandBuffers[currentFrameResourceIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		                 (uint32_t)memorySwapchainPresentBarriers.size(), memorySwapchainPresentBarriers.data(),
		                 (uint32_t)bufferSwapchainPresentBarriers.size(), bufferSwapchainPresentBarriers.data(),
		                 (uint32_t)imageSwapchainPresentBarriers.size(), imageSwapchainPresentBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(mMainGraphicsCommandBuffers[currentFrameResourceIndex]));

	std::array waitSemaphores      = {mSwapChain->GetImageAcquiredSemaphore(currentFrameResourceIndex)};
	std::array signalSemaphores    = {mSwapChain->GetImagePresentSemaphore(currentFrameResourceIndex)};
	std::array waitSemaphoreStages = {VkPipelineStageFlags(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)};

	std::array commandBuffersToExecute = {mMainGraphicsCommandBuffers[currentFrameResourceIndex]};

	VkSubmitInfo graphicsQueueSubmitInfo;
	graphicsQueueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	graphicsQueueSubmitInfo.pNext                = nullptr;
	graphicsQueueSubmitInfo.waitSemaphoreCount   = (uint32_t)(waitSemaphores.size());
	graphicsQueueSubmitInfo.pWaitSemaphores      = waitSemaphores.data();
	graphicsQueueSubmitInfo.pWaitDstStageMask    = waitSemaphoreStages.data();
	graphicsQueueSubmitInfo.commandBufferCount   = (uint32_t)commandBuffersToExecute.size();
	graphicsQueueSubmitInfo.pCommandBuffers      = commandBuffersToExecute.data();
	graphicsQueueSubmitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
	graphicsQueueSubmitInfo.pSignalSemaphores    = signalSemaphores.data();

	std::array submitInfos = {graphicsQueueSubmitInfo};
	ThrowIfFailed(vkQueueSubmit(mGraphicsQueue, (uint32_t)(submitInfos.size()), submitInfos.data(), frameFences[0]));

	mSwapChain->Present(currentFrameResourceIndex);

	mCurrentFrameIndex++;
}

uint64_t VulkanCBindings::Renderer::GetFrameNumber() const
{
	return mCurrentFrameIndex;
}

void VulkanCBindings::Renderer::InitInstance()
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
	appInfo.apiVersion         = VK_MAKE_VERSION(1, 1, 0);

	VkInstanceCreateInfo& instanceCreateInfo   = instanceCreateInfoChain.GetChainHead();
	instanceCreateInfo.flags                   = 0;
	instanceCreateInfo.pApplicationInfo        = &appInfo;
	instanceCreateInfo.enabledLayerCount       = (uint32_t)enabledLayersCStrs.size();
	instanceCreateInfo.ppEnabledLayerNames     = enabledLayersCStrs.data();
	instanceCreateInfo.enabledExtensionCount   = (uint32_t)enabledExtensionsCStrs.size();
	instanceCreateInfo.ppEnabledExtensionNames = enabledExtensionsCStrs.data();

	ThrowIfFailed(vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance));
}

void VulkanCBindings::Renderer::SelectPhysicalDevice(VkPhysicalDevice* outPhysicalDevice)
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

void VulkanCBindings::Renderer::CreateLogicalDevice(VkPhysicalDevice physicalDevice, const std::unordered_set<uint32_t>& queueFamilies)
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

void VulkanCBindings::Renderer::FindDeviceQueueIndices(VkPhysicalDevice physicalDevice, uint32_t* outGraphicsQueueIndex, uint32_t* outComputeQueueIndex, uint32_t* outTransferQueueIndex)
{
	//Use dedicated queues
	uint32_t graphicsQueueFamilyIndex = (uint32_t)(-1);
	uint32_t computeQueueFamilyIndex  = (uint32_t)(-1);
	uint32_t transferQueueFamilyIndex = (uint32_t)(-1);

	std::vector<VkQueueFamilyProperties> queueFamiliesProperties;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

	queueFamiliesProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamiliesProperties.data());
	for(uint32_t queueFamilyIndex = 0; queueFamilyIndex < (uint32_t)queueFamiliesProperties.size(); queueFamilyIndex++)
	{
		//First found graphics queue
		if((graphicsQueueFamilyIndex == (uint32_t)(-1)) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0))
		{
			graphicsQueueFamilyIndex = queueFamilyIndex;
		}
		
		//First found dedicated compute queue
		if((computeQueueFamilyIndex == (uint32_t)(-1)) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
		{
			computeQueueFamilyIndex = queueFamilyIndex;
		}

		//First found dedicated transfer queue
		if((transferQueueFamilyIndex == (uint32_t)(-1)) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
		{
			transferQueueFamilyIndex = queueFamilyIndex;
		}
	}

	//Some of the queues are not found, use non-dedicated ones instead
	if(computeQueueFamilyIndex == (uint32_t)(-1))
	{
		computeQueueFamilyIndex = graphicsQueueFamilyIndex;
	}

	if(transferQueueFamilyIndex == (uint32_t)(-1))
	{
		transferQueueFamilyIndex = computeQueueFamilyIndex;
	}

	assert(graphicsQueueFamilyIndex != (uint32_t)(-1));
	assert(computeQueueFamilyIndex  != (uint32_t)(-1));
	assert(transferQueueFamilyIndex != (uint32_t)(-1));

	if(outGraphicsQueueIndex != nullptr)
	{
		*outGraphicsQueueIndex = graphicsQueueFamilyIndex;
	}

	if(outComputeQueueIndex != nullptr)
	{
		*outComputeQueueIndex = computeQueueFamilyIndex;
	}

	if(outTransferQueueIndex != nullptr)
	{
		*outTransferQueueIndex = transferQueueFamilyIndex;
	}
}

void VulkanCBindings::Renderer::GetDeviceQueues(uint32_t graphicsIndex, uint32_t computeIndex, uint32_t transferIndex)
{
	VkDeviceQueueInfo2 graphicsQueueInfo;
	graphicsQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	graphicsQueueInfo.pNext            = nullptr;
	graphicsQueueInfo.flags            = 0; //I still don't use ANY queue priorities
	graphicsQueueInfo.queueIndex       = 0; //Use only one queue per family
	graphicsQueueInfo.queueFamilyIndex = graphicsIndex;

	vkGetDeviceQueue2(mDevice, &graphicsQueueInfo, &mGraphicsQueue);


	VkDeviceQueueInfo2 computeQueueInfo;
	computeQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	computeQueueInfo.pNext            = nullptr;
	computeQueueInfo.flags            = 0; //I still don't use ANY queue priorities
	computeQueueInfo.queueIndex       = 0; //Use only one queue per family
	computeQueueInfo.queueFamilyIndex = computeIndex;

	vkGetDeviceQueue2(mDevice, &computeQueueInfo, &mComputeQueue);


	VkDeviceQueueInfo2 transferQueueInfo;
	transferQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	transferQueueInfo.pNext            = nullptr;
	transferQueueInfo.flags            = 0; //I still don't use ANY queue priorities
	transferQueueInfo.queueIndex       = 0; //Use only one queue per family
	transferQueueInfo.queueFamilyIndex = transferIndex;

	vkGetDeviceQueue2(mDevice, &transferQueueInfo, &mTransferQueue);
}

void VulkanCBindings::Renderer::CreateCommandBuffersAndPools()
{
	mCommandBuffers = std::make_unique<VulkanCBindings::WorkerCommandBuffers>(mDevice, mThreadPool->GetWorkerThreadCount(), mGraphicsQueueFamilyIndex, mComputeQueueFamilyIndex, mTransferQueueFamilyIndex);

	for(uint32_t frame = 0; frame < VulkanUtils::InFlightFrameCount; frame++)
	{
		mMainGraphicsCommandPools[frame] = mCommandBuffers->GetMainThreadGraphicsCommandPool(frame);
		mMainComputeCommandPools[frame]  = mCommandBuffers->GetMainThreadComputeCommandPool(frame);
		mMainTransferCommandPools[frame] = mCommandBuffers->GetMainThreadTransferCommandPool(frame);

		mMainGraphicsCommandBuffers[frame] = mCommandBuffers->GetMainThreadGraphicsCommandBuffer(frame);
		mMainComputeCommandBuffers[frame]  = mCommandBuffers->GetMainThreadComputeCommandBuffer(frame);
		mMainTransferCommandBuffers[frame] = mCommandBuffers->GetMainThreadTransferCommandBuffer(frame);
	}
}

void VulkanCBindings::Renderer::CreateFences()
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

void VulkanCBindings::Renderer::InitializeSwapchainImages()
{
	ThrowIfFailed(vkResetCommandPool(mDevice, mMainGraphicsCommandPools[0], 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo graphicsCmdBufferBeginInfo;
	graphicsCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	graphicsCmdBufferBeginInfo.pNext            = nullptr;
	graphicsCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	graphicsCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(mMainGraphicsCommandBuffers[0], &graphicsCmdBufferBeginInfo);

	std::array<VkImageMemoryBarrier, VulkanCBindings::SwapChain::SwapchainImageCount> imageBarriers;
	for(uint32_t i = 0; i < VulkanCBindings::SwapChain::SwapchainImageCount; i++)
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
	vkCmdPipelineBarrier(mMainGraphicsCommandBuffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		                 (uint32_t)memoryBarriers.size(), memoryBarriers.data(),
		                 (uint32_t)bufferBarriers.size(), bufferBarriers.data(),
		                 (uint32_t)imageBarriers.size(), imageBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(mMainGraphicsCommandBuffers[0]));

	std::array commandBuffersToExecute = {mMainGraphicsCommandBuffers[0]};

	//TODO: mGPU?
	//TODO: Performance query?
	VkSubmitInfo queueSubmitInfo;
	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext                = nullptr;
	queueSubmitInfo.waitSemaphoreCount   = 0;
	queueSubmitInfo.pWaitSemaphores      = nullptr;
	queueSubmitInfo.pWaitDstStageMask    = nullptr;
	queueSubmitInfo.commandBufferCount   = (uint32_t)commandBuffersToExecute.size();
	queueSubmitInfo.pCommandBuffers      = commandBuffersToExecute.data();
	queueSubmitInfo.signalSemaphoreCount = 0;
	queueSubmitInfo.pSignalSemaphores    = nullptr;

	std::array submitInfos = {queueSubmitInfo};
	ThrowIfFailed(vkQueueSubmit(mGraphicsQueue, (uint32_t)submitInfos.size(), submitInfos.data(), nullptr));

	ThrowIfFailed(vkQueueWaitIdle(mGraphicsQueue));
}
