#include "VulkanCFrameGraph.hpp"
#include "../VulkanCWorkerCommandBuffers.hpp"
#include "../../../Core/ThreadPool.hpp"

VulkanCBindings::FrameGraph::FrameGraph(VkDevice device): mDeviceRef(device)
{
}

VulkanCBindings::FrameGraph::~FrameGraph()
{
}

void VulkanCBindings::FrameGraph::Traverse(WorkerCommandBuffers* commandBuffers, RenderableScene* scene, ThreadPool* threadPool, uint32_t currentFrameResourceIndex)
{
	//TODO: multithreading (thread pools are too hard for me rn, I have to level up my multithreading/concurrency skills)
	UNREFERENCED_PARAMETER(threadPool);

	uint32_t renderPassIndex = 0;

	VkCommandBuffer graphicsCommandBuffer = commandBuffers->GetMainThreadGraphicsCommandBuffer(currentFrameResourceIndex);
	while(renderPassIndex < mGraphicsPassCount)
	{
		mRenderPasses[renderPassIndex]->RecordExecution(graphicsCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

		renderPassIndex++;
	}

	VkCommandBuffer computeCommandBuffer = commandBuffers->GetMainThreadComputeCommandBuffer(currentFrameResourceIndex);
	while((renderPassIndex - mGraphicsPassCount) < mComputePassCount)
	{
		mRenderPasses[renderPassIndex]->RecordExecution(computeCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

		renderPassIndex++;
	}

	VkCommandBuffer transferCommandBuffer = commandBuffers->GetMainThreadTransferCommandBuffer(currentFrameResourceIndex);
	while((renderPassIndex - mGraphicsPassCount - mComputePassCount) < mTransferPassCount)
	{
		mRenderPasses[renderPassIndex]->RecordExecution(transferCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

		renderPassIndex++;
	}
}