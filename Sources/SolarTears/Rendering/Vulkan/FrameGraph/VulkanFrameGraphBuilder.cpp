#include "VulkanCFrameGraphBuilder.hpp"
#include "VulkanCFrameGraph.hpp"
#include "../VulkanCInstanceParameters.hpp"
#include "../VulkanCWorkerCommandBuffers.hpp"
#include "../VulkanCFunctions.hpp"
#include "../VulkanCMemory.hpp"
#include "../VulkanCSwapChain.hpp"
#include "../VulkanCDeviceQueues.hpp"
#include <algorithm>

VulkanCBindings::FrameGraphBuilder::FrameGraphBuilder(FrameGraph* graphToBuild, const RenderableScene* scene, const InstanceParameters* instanceParameters, const DeviceParameters* deviceParameters, const ShaderManager* shaderManager): mGraphToBuild(graphToBuild), mScene(scene), mInstanceParameters(instanceParameters), mDeviceParameters(deviceParameters), mShaderManager(shaderManager)
{
	mSubresourceMetadataCounter = 0;

	//Create present quasi-pass
	std::string acquirePassName(AcquirePassName);
	std::string presentPassName(PresentPassName);

	mRenderPassTypes[acquirePassName] = RenderPassType::PRESENT;
	mRenderPassTypes[presentPassName] = RenderPassType::PRESENT;

	RegisterWriteSubresource(AcquirePassName, BackbufferAcquirePassId);
	RegisterReadSubresource(PresentPassName, BackbufferPresentPassId);
}

VulkanCBindings::FrameGraphBuilder::~FrameGraphBuilder()
{
}

void VulkanCBindings::FrameGraphBuilder::RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc, RenderPassType passType)
{
	RenderPassName renderPassName(passName);
	assert(!mRenderPassCreateFunctions.contains(renderPassName));

	mRenderPassNames.push_back(renderPassName);
	mRenderPassCreateFunctions[renderPassName] = createFunc;

	mRenderPassTypes[renderPassName] = passType;
}

void VulkanCBindings::FrameGraphBuilder::RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	RenderPassName renderPassName(passName);

	if(!mRenderPassesReadSubresourceIds.contains(renderPassName))
	{
		mRenderPassesReadSubresourceIds[renderPassName] = std::unordered_set<SubresourceId>();
	}

	SubresourceId subresId(subresourceId);
	assert(!mRenderPassesReadSubresourceIds[renderPassName].contains(subresId));

	mRenderPassesReadSubresourceIds[renderPassName].insert(subresId);

	if(!mRenderPassesSubresourceMetadatas.contains(renderPassName))
	{
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, ImageSubresourceMetadata>();
	}

	ImageSubresourceMetadata subresourceMetadata;
	subresourceMetadata.MetadataId           = mSubresourceMetadataCounter;
	subresourceMetadata.PrevPassMetadata     = nullptr;
	subresourceMetadata.NextPassMetadata     = nullptr;
	subresourceMetadata.MetadataFlags        = 0;
	subresourceMetadata.QueueFamilyOwnership = (uint32_t)(-1);
	subresourceMetadata.Format               = VK_FORMAT_UNDEFINED;
	subresourceMetadata.ImageIndex           = (uint32_t)(-1);
	subresourceMetadata.ImageViewIndex       = (uint32_t)(-1);
	subresourceMetadata.Layout               = VK_IMAGE_LAYOUT_GENERAL;
	subresourceMetadata.UsageFlags           = 0;
	subresourceMetadata.AspectFlags          = 0;
	subresourceMetadata.PipelineStageFlags   = 0;
	subresourceMetadata.AccessFlags          = 0;

	mSubresourceMetadataCounter++;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = subresourceMetadata;
}

void VulkanCBindings::FrameGraphBuilder::RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	RenderPassName renderPassName(passName);

	if(!mRenderPassesWriteSubresourceIds.contains(renderPassName))
	{
		mRenderPassesWriteSubresourceIds[renderPassName] = std::unordered_set<SubresourceId>();
	}

	SubresourceId subresId(subresourceId);
	assert(!mRenderPassesWriteSubresourceIds[renderPassName].contains(subresId));

	mRenderPassesWriteSubresourceIds[renderPassName].insert(subresId);

	if(!mRenderPassesSubresourceMetadatas.contains(renderPassName))
	{
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, ImageSubresourceMetadata>();
	}

	ImageSubresourceMetadata subresourceMetadata;
	subresourceMetadata.MetadataId           = mSubresourceMetadataCounter;
	subresourceMetadata.PrevPassMetadata     = nullptr;
	subresourceMetadata.NextPassMetadata     = nullptr;
	subresourceMetadata.MetadataFlags        = 0;
	subresourceMetadata.QueueFamilyOwnership = (uint32_t)(-1);
	subresourceMetadata.Format               = VK_FORMAT_UNDEFINED;
	subresourceMetadata.ImageIndex           = (uint32_t)(-1);
	subresourceMetadata.ImageViewIndex       = (uint32_t)(-1);
	subresourceMetadata.Layout               = VK_IMAGE_LAYOUT_UNDEFINED;
	subresourceMetadata.UsageFlags           = 0;
	subresourceMetadata.AspectFlags          = 0;
	subresourceMetadata.PipelineStageFlags   = 0;
	subresourceMetadata.AccessFlags          = 0;

	mSubresourceMetadataCounter++;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = subresourceMetadata;
}

void VulkanCBindings::FrameGraphBuilder::AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName)
{
	RenderPassName  passNameStr(passName);
	SubresourceId   subresourceIdStr(subresourceId);
	SubresourceName subresourceNameStr(subresourceName);

	auto readSubresIt  = mRenderPassesReadSubresourceIds.find(passNameStr);
	auto writeSubresIt = mRenderPassesWriteSubresourceIds.find(passNameStr);

	assert(readSubresIt != mRenderPassesReadSubresourceIds.end() || writeSubresIt != mRenderPassesWriteSubresourceIds.end());
	mRenderPassesSubresourceNameIds[passNameStr][subresourceNameStr] = subresourceIdStr;
}

void VulkanCBindings::FrameGraphBuilder::AssignBackbufferName(const std::string_view backbufferName)
{
	mBackbufferName = backbufferName;

	AssignSubresourceName(AcquirePassName, BackbufferAcquirePassId, mBackbufferName);
	AssignSubresourceName(PresentPassName, BackbufferPresentPassId, mBackbufferName);

	ImageSubresourceMetadata presentAcquirePassMetadata;
	presentAcquirePassMetadata.MetadataId           = mSubresourceMetadataCounter;
	presentAcquirePassMetadata.PrevPassMetadata     = nullptr;
	presentAcquirePassMetadata.NextPassMetadata     = nullptr;
	presentAcquirePassMetadata.MetadataFlags        = 0;
	presentAcquirePassMetadata.QueueFamilyOwnership = (uint32_t)(-1);
	presentAcquirePassMetadata.Format               = VK_FORMAT_UNDEFINED;
	presentAcquirePassMetadata.ImageIndex           = (uint32_t)(-1);
	presentAcquirePassMetadata.ImageViewIndex       = (uint32_t)(-1);
	presentAcquirePassMetadata.Layout               = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	presentAcquirePassMetadata.UsageFlags           = 0;
	presentAcquirePassMetadata.AspectFlags          = VK_IMAGE_ASPECT_COLOR_BIT;
	presentAcquirePassMetadata.PipelineStageFlags   = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	presentAcquirePassMetadata.AccessFlags          = 0;

	mSubresourceMetadataCounter++;

	mRenderPassesSubresourceMetadatas[std::string(AcquirePassName)][std::string(BackbufferAcquirePassId)] = presentAcquirePassMetadata;
	mRenderPassesSubresourceMetadatas[std::string(PresentPassName)][std::string(BackbufferPresentPassId)] = presentAcquirePassMetadata;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceFormat(const std::string_view passName, const std::string_view subresourceId, VkFormat format)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].Format = format;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId, VkImageLayout layout)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].Layout = layout;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId, VkImageUsageFlags usage)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].UsageFlags = usage;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId, VkImageAspectFlags aspect)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].AspectFlags = aspect;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId, VkPipelineStageFlags stage)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].PipelineStageFlags = stage;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId, VkAccessFlags accessFlags)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].AccessFlags = accessFlags;
}

void VulkanCBindings::FrameGraphBuilder::EnableSubresourceAutoBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBaarrier)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	if(autoBaarrier)
	{
		mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].MetadataFlags |= ImageFlagAutoBarrier;
	}
	else
	{
		mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].MetadataFlags &= ~ImageFlagAutoBarrier;
	}
}

const VulkanCBindings::RenderableScene* VulkanCBindings::FrameGraphBuilder::GetScene() const
{
	return mScene;
}

const VulkanCBindings::DeviceParameters* VulkanCBindings::FrameGraphBuilder::GetDeviceParameters() const
{
	return mDeviceParameters;
}

const VulkanCBindings::ShaderManager* VulkanCBindings::FrameGraphBuilder::GetShaderManager() const
{
	return mShaderManager;
}

const FrameGraphConfig* VulkanCBindings::FrameGraphBuilder::GetConfig() const
{
	return &mGraphToBuild->mFrameGraphConfig;
}

VkImage VulkanCBindings::FrameGraphBuilder::GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.ImageIndex == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		return mGraphToBuild->mImages[metadata.ImageIndex];
	}
}

VkImageView VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresource(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.ImageViewIndex == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		return mGraphToBuild->mImageViews[metadata.ImageViewIndex];
	}
}

VkFormat VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceFormat(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.Format;
}

VkImageLayout VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.Layout;
}

VkImageUsageFlags VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.UsageFlags;
}

VkPipelineStageFlags VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.PipelineStageFlags;
}

VkImageAspectFlags VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.AspectFlags;
}

VkAccessFlags VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.AccessFlags;
}

VkImageLayout VulkanCBindings::FrameGraphBuilder::GetPreviousPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.PrevPassMetadata != nullptr)
	{
		return metadata.PrevPassMetadata->Layout;
	}
	else
	{
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}
}

VkImageUsageFlags VulkanCBindings::FrameGraphBuilder::GetPreviousPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.PrevPassMetadata != nullptr)
	{
		return metadata.PrevPassMetadata->UsageFlags;
	}
	else
	{
		return 0;
	}
}

VkPipelineStageFlags VulkanCBindings::FrameGraphBuilder::GetPreviousPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.PrevPassMetadata != nullptr)
	{
		return metadata.PrevPassMetadata->PipelineStageFlags;
	}
	else
	{
		return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
}

VkImageAspectFlags VulkanCBindings::FrameGraphBuilder::GetPreviousPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.PrevPassMetadata)
	{
		return metadata.PrevPassMetadata->AspectFlags;
	}
	else
	{
		return 0;
	}
}

VkAccessFlags VulkanCBindings::FrameGraphBuilder::GetPreviousPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.PrevPassMetadata)
	{
		return metadata.PrevPassMetadata->AccessFlags;
	}
	else
	{
		return 0;
	}
}

VkImageLayout VulkanCBindings::FrameGraphBuilder::GetNextPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.NextPassMetadata != nullptr)
	{
		return metadata.NextPassMetadata->Layout;
	}
	else
	{
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}
}

VkImageUsageFlags VulkanCBindings::FrameGraphBuilder::GetNextPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.NextPassMetadata != nullptr)
	{
		return metadata.NextPassMetadata->UsageFlags;
	}
	else
	{
		return 0;
	}
}

VkPipelineStageFlags VulkanCBindings::FrameGraphBuilder::GetNextPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.NextPassMetadata != nullptr)
	{
		return metadata.NextPassMetadata->PipelineStageFlags;
	}
	else
	{
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
}

VkImageAspectFlags VulkanCBindings::FrameGraphBuilder::GetNextPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.NextPassMetadata)
	{
		return metadata.NextPassMetadata->AspectFlags;
	}
	else
	{
		return 0;
	}
}

VkAccessFlags VulkanCBindings::FrameGraphBuilder::GetNextPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.NextPassMetadata)
	{
		return metadata.NextPassMetadata->AccessFlags;
	}
	else
	{
		return 0;
	}
}

void VulkanCBindings::FrameGraphBuilder::Build(const DeviceQueues* deviceQueues, const WorkerCommandBuffers* workerCommandBuffers, const MemoryManager* memoryAllocator, const SwapChain* swapChain)
{
	std::vector<VkImage> swapchainImages(swapChain->SwapchainImageCount);
	for(uint32_t i = 0; i < swapChain->SwapchainImageCount; i++)
	{
		swapchainImages[i] = swapChain->GetSwapchainImage(i);
	}

	std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>> adjacencyList;
	BuildAdjacencyList(adjacencyList);

	std::vector<RenderPassName> sortedPassNames;
	SortRenderPassesTopological(adjacencyList, sortedPassNames);
	SortRenderPassesDependency(adjacencyList, sortedPassNames);
	mRenderPassNames = std::move(sortedPassNames);

	BuildDependencyLevels(deviceQueues, swapChain);

	ValidateSubresourceLinks();
	ValidateSubresourceQueues();

	uint32_t defaultQueueIndex = deviceQueues->GetGraphicsQueueFamilyIndex(); //Default image queue ownership

	std::unordered_set<RenderPassName> backbufferPassNames; //All passes that use the backbuffer
	BuildSubresources(memoryAllocator, swapchainImages, backbufferPassNames, swapChain->GetBackbufferFormat(), defaultQueueIndex);
	BuildPassObjects(backbufferPassNames);

	BuildDescriptors();
	BuildBarriers();

	BarrierImages(deviceQueues, workerCommandBuffers, defaultQueueIndex);
}

void VulkanCBindings::FrameGraphBuilder::BuildAdjacencyList(std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList)
{
	adjacencyList.clear();

	for(const RenderPassName& renderPassName: mRenderPassNames)
	{
		std::unordered_set<RenderPassName> renderPassAdjacentPasses;
		for(const RenderPassName& otherPassName: mRenderPassNames)
		{
			if(renderPassName == otherPassName)
			{
				continue;
			}

			if(PassesIntersect(renderPassName, otherPassName))
			{
				renderPassAdjacentPasses.insert(otherPassName);
			}
		}

		adjacencyList[renderPassName] = std::move(renderPassAdjacentPasses);
	}
}

void VulkanCBindings::FrameGraphBuilder::SortRenderPassesTopological(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& unsortedPasses)
{
	unsortedPasses.clear();
	unsortedPasses.reserve(mRenderPassNames.size());

	std::unordered_set<RenderPassName> visited;
	std::unordered_set<RenderPassName> onStack;

	for(const RenderPassName& renderPassName: mRenderPassNames)
	{
		TopologicalSortNode(adjacencyList, visited, onStack, renderPassName, unsortedPasses);
	}

	for(size_t i = 0; i < unsortedPasses.size() / 2; i++) //Reverse the order
	{
		std::swap(unsortedPasses[i], unsortedPasses[unsortedPasses.size() - i - 1]);
	}
}

void VulkanCBindings::FrameGraphBuilder::SortRenderPassesDependency(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& topologicallySortedPasses)
{
	mRenderPassDependencyLevels.clear();

	for(const RenderPassName& renderPassName: topologicallySortedPasses)
	{
		mRenderPassDependencyLevels[renderPassName] = 0;
	}

	for(const RenderPassName& renderPassName: topologicallySortedPasses)
	{
		uint32_t& mainPassDependencyLevel = mRenderPassDependencyLevels[renderPassName];

		const auto& adjacentPassNames = adjacencyList.at(renderPassName);
		for(const RenderPassName& adjacentPassName: adjacentPassNames)
		{
			uint32_t& adjacentPassDependencyLevel = mRenderPassDependencyLevels[adjacentPassName];

			if(adjacentPassDependencyLevel < mainPassDependencyLevel + 1)
			{
				adjacentPassDependencyLevel = mainPassDependencyLevel + 1;
			}
		}
	}

	std::stable_sort(topologicallySortedPasses.begin(), topologicallySortedPasses.end(), [this](const RenderPassName& left, const RenderPassName& right)
	{
		return mRenderPassDependencyLevels[left] < mRenderPassDependencyLevels[right];
	});
}

void VulkanCBindings::FrameGraphBuilder::BuildDependencyLevels(const DeviceQueues* deviceQueues, const SwapChain* swapChain)
{
	mGraphToBuild->mGraphicsPassSpans.clear();

	uint32_t currentDependencyLevel = 0;
	mGraphToBuild->mGraphicsPassSpans.push_back(FrameGraph::DependencyLevelSpan{.DependencyLevelBegin = 0, .DependencyLevelEnd = 0});
	for(size_t passIndex = 0; passIndex < mRenderPassNames.size(); passIndex++)
	{
		uint32_t dependencyLevel = mRenderPassDependencyLevels[mRenderPassNames[passIndex]];
		if(currentDependencyLevel != dependencyLevel)
		{
			mGraphToBuild->mGraphicsPassSpans.push_back(FrameGraph::DependencyLevelSpan{.DependencyLevelBegin = dependencyLevel, .DependencyLevelEnd = dependencyLevel + 1});
			currentDependencyLevel = dependencyLevel;
		}
		else
		{
			mGraphToBuild->mGraphicsPassSpans.back().DependencyLevelEnd++;
		}
	}

	mGraphToBuild->mFrameRecordedGraphicsCommandBuffers.resize(mGraphToBuild->mGraphicsPassSpans.size());

	//The loop will wrap around 0
	size_t renderPassNameIndex = mRenderPassNames.size() - 1;
	while(renderPassNameIndex < mRenderPassNames.size())
	{
		//Graphics queue only for now
		//Async compute passes should be in the end of the frame. BUT! They should be executed AT THE START of THE PREVIOUS frame. Need to reorder stuff for that
		RenderPassName renderPassName = mRenderPassNames[renderPassNameIndex];

		mRenderPassQueueFamilies[renderPassName] = deviceQueues->GetGraphicsQueueFamilyIndex();
		renderPassNameIndex--;
	}

	mRenderPassQueueFamilies[std::string(AcquirePassName)] = swapChain->GetPresentQueueFamilyIndex();
	mRenderPassQueueFamilies[std::string(PresentPassName)] = swapChain->GetPresentQueueFamilyIndex();
}

void VulkanCBindings::FrameGraphBuilder::TopologicalSortNode(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::unordered_set<RenderPassName>& visited, std::unordered_set<RenderPassName>& onStack, const RenderPassName& renderPassName, std::vector<RenderPassName>& sortedPassNames)
{
	if(visited.contains(renderPassName))
	{
		return;
	}

	onStack.insert(renderPassName);
	visited.insert(renderPassName);

	const auto& adjacentPassNames = adjacencyList.at(renderPassName);
	for(const RenderPassName& adjacentPassName: adjacentPassNames)
	{
		TopologicalSortNode(adjacencyList, visited, onStack, adjacentPassName, sortedPassNames);
	}

	onStack.erase(renderPassName);
	sortedPassNames.push_back(renderPassName);
}

void VulkanCBindings::FrameGraphBuilder::ValidateSubresourceLinks()
{
	std::unordered_map<SubresourceName, RenderPassName> subresourceLastRenderPasses; 

	//Build present pass links
	subresourceLastRenderPasses[mBackbufferName] = std::string(AcquirePassName);

	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			SubresourceName           subresourceName     = subresourceNameId.first;
			ImageSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);

			if(subresourceLastRenderPasses.contains(subresourceName))
			{
				RenderPassName prevPassName = subresourceLastRenderPasses.at(subresourceName);
				
				SubresourceId             prevSubresourceId       = mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
				ImageSubresourceMetadata& prevSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(prevPassName).at(prevSubresourceId);

				prevSubresourceMetadata.NextPassMetadata = &subresourceMetadata;
				subresourceMetadata.PrevPassMetadata     = &prevSubresourceMetadata;
			}

			subresourceLastRenderPasses[subresourceName] = renderPassName;
		}
	}

	RenderPassName backbufferLastPassName = subresourceLastRenderPasses.at(mBackbufferName);

	SubresourceId             backbufferLastSubresourceId          = mRenderPassesSubresourceNameIds.at(backbufferLastPassName).at(mBackbufferName);
	ImageSubresourceMetadata& backbufferLastSubresourceMetadata    = mRenderPassesSubresourceMetadatas.at(backbufferLastPassName).at(backbufferLastSubresourceId);
	ImageSubresourceMetadata& backbufferPresentSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(std::string(PresentPassName)).at(std::string(BackbufferPresentPassId));

	backbufferLastSubresourceMetadata.NextPassMetadata    = &backbufferPresentSubresourceMetadata;
	backbufferPresentSubresourceMetadata.PrevPassMetadata = &backbufferLastSubresourceMetadata;

	//Validate cyclic links (so first pass knows what state to barrier from)
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			ImageSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);
			if(subresourceMetadata.PrevPassMetadata == nullptr)
			{
				SubresourceName subresourceName = subresourceNameId.first;

				RenderPassName prevPassName = subresourceLastRenderPasses.at(subresourceName);

				SubresourceId             prevSubresourceId       = mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
				ImageSubresourceMetadata& prevSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(prevPassName).at(prevSubresourceId);

				subresourceMetadata.PrevPassMetadata     = &prevSubresourceMetadata; //Barrier from the last state at the beginning of frame
				prevSubresourceMetadata.NextPassMetadata = nullptr;                  //No barriers after the frame ends (except for present barrier)
			}
		}
	}
}

void VulkanCBindings::FrameGraphBuilder::ValidateSubresourceQueues()
{
	//Queue families should be known
	assert(!mRenderPassQueueFamilies.empty());

	std::string acquirePassName(AcquirePassName);
	std::string presentPassName(PresentPassName);

	for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[acquirePassName])
	{
		ImageSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(acquirePassName).at(subresourceNameId.second);
		subresourceMetadata.QueueFamilyOwnership      = mRenderPassQueueFamilies[acquirePassName];
	}

	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			ImageSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);
			subresourceMetadata.QueueFamilyOwnership      = mRenderPassQueueFamilies[renderPassName];
		}
	}

	for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[presentPassName])
	{
		ImageSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(presentPassName).at(subresourceNameId.second);
		subresourceMetadata.QueueFamilyOwnership      = mRenderPassQueueFamilies[presentPassName];
	}
}

void VulkanCBindings::FrameGraphBuilder::BuildSubresources(const MemoryManager* memoryAllocator, const std::vector<VkImage>& swapchainImages, std::unordered_set<RenderPassName>& swapchainPassNames, VkFormat swapchainFormat, uint32_t defaultQueueIndex)
{
	std::vector<uint32_t> defaultQueueIndices = {defaultQueueIndex};

	std::unordered_map<SubresourceName, ImageResourceCreateInfo>      imageResourceCreateInfos;
	std::unordered_map<SubresourceName, BackbufferResourceCreateInfo> backbufferResourceCreateInfos;
	BuildResourceCreateInfos(defaultQueueIndices, imageResourceCreateInfos, backbufferResourceCreateInfos, swapchainPassNames);

	PropagateMetadatasFromImageViews(imageResourceCreateInfos, backbufferResourceCreateInfos);

	CreateImages(imageResourceCreateInfos, memoryAllocator);
	CreateImageViews(imageResourceCreateInfos);

	SetSwapchainImages(backbufferResourceCreateInfos, swapchainImages);
	CreateSwapchainImageViews(backbufferResourceCreateInfos, swapchainFormat);
}

void VulkanCBindings::FrameGraphBuilder::BuildPassObjects(const std::unordered_set<std::string>& swapchainPassNames)
{
	mGraphToBuild->mRenderPasses.clear();
	mGraphToBuild->mSwapchainRenderPasses.clear();
	mGraphToBuild->mSwapchainPassesSwapMap.clear();

	for(const std::string& passName: mRenderPassNames)
	{
		if(swapchainPassNames.contains(passName))
		{
			//This pass uses swapchain images - create a copy of render pass for each swapchain image
			mGraphToBuild->mRenderPasses.emplace_back(nullptr);

			//Fill in primary swap link (what to swap)
			mGraphToBuild->mSwapchainPassesSwapMap.push_back((uint32_t)(mGraphToBuild->mRenderPasses.size() - 1));

			//Correctness is guaranteed as long as mLastSwapchainImageIndex is the last index in mSwapchainImageRefs
			const uint32_t swapchainImageCount = (uint32_t)mGraphToBuild->mSwapchainImageRefs.size();			
			for(uint32_t swapchainImageIndex = 0; swapchainImageIndex < mGraphToBuild->mSwapchainImageRefs.size(); swapchainImageIndex++)
			{
				//Create a separate render pass for each of swapchain images
				mGraphToBuild->SwitchSwapchainImages(swapchainImageIndex);
				mGraphToBuild->mSwapchainRenderPasses.push_back(mRenderPassCreateFunctions.at(passName)(mGraphToBuild->mDeviceRef, this, passName));

				//Fill in secondary swap link (what to swap to)
				mGraphToBuild->mSwapchainPassesSwapMap.push_back((uint32_t)(mGraphToBuild->mSwapchainRenderPasses.size() - 1));

				mGraphToBuild->mLastSwapchainImageIndex = swapchainImageIndex;
			}
		}
		else
		{
			//This pass does not use any swapchain images - can just create a single copy
			mGraphToBuild->mRenderPasses.push_back(mRenderPassCreateFunctions.at(passName)(mGraphToBuild->mDeviceRef, this, passName));
		}
	}

	//Prepare for the first use
	uint32_t swapchainImageCount = (uint32_t)mGraphToBuild->mSwapchainImageRefs.size();
	for(uint32_t i = 0; i < (uint32_t)mGraphToBuild->mSwapchainPassesSwapMap.size(); i += (swapchainImageCount + 1u))
	{
		uint32_t passIndexToSwitch = mGraphToBuild->mSwapchainPassesSwapMap[i];
		mGraphToBuild->mRenderPasses[passIndexToSwitch].swap(mGraphToBuild->mSwapchainRenderPasses[mGraphToBuild->mSwapchainPassesSwapMap[i + mGraphToBuild->mLastSwapchainImageIndex + 1]]);
	}
}

void VulkanCBindings::FrameGraphBuilder::BuildBarriers()
{
	mGraphToBuild->mImageRenderPassBarriers.resize(mRenderPassNames.size());
	mGraphToBuild->mSwapchainBarrierIndices.clear();

	std::unordered_set<uint64_t> processedMetadataIds; //This is here so no barrier gets added twice
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];

		const auto& nameIdMap     = mRenderPassesSubresourceNameIds.at(renderPassName);
		const auto& idMetadataMap = mRenderPassesSubresourceMetadatas.at(renderPassName);

		std::vector<VkImageMemoryBarrier> beforeBarriers;
		std::vector<VkImageMemoryBarrier> afterBarriers;
		std::vector<size_t>               beforeSwapchainBarriers;
		std::vector<size_t>               afterSwapchainBarriers;
		for(auto& subresourceNameId: nameIdMap)
		{
			SubresourceName subresourceName = subresourceNameId.first;
			SubresourceId   subresourceId   = subresourceNameId.second;

			ImageSubresourceMetadata subresourceMetadata = idMetadataMap.at(subresourceId);
			if(!(subresourceMetadata.MetadataFlags & ImageFlagAutoBarrier))
			{
				if(subresourceMetadata.PrevPassMetadata != nullptr)
				{
					if(processedMetadataIds.contains(subresourceMetadata.PrevPassMetadata->MetadataId)) //Was already processed as "after pass"
					{
						continue;
					}

					const bool accessFlagsDiffer   = (subresourceMetadata.AccessFlags          != subresourceMetadata.PrevPassMetadata->AccessFlags);
					const bool layoutsDiffer       = (subresourceMetadata.Layout               != subresourceMetadata.PrevPassMetadata->Layout);
					const bool queueFamiliesDiffer = (subresourceMetadata.QueueFamilyOwnership != subresourceMetadata.PrevPassMetadata->QueueFamilyOwnership);
					if(accessFlagsDiffer || layoutsDiffer || queueFamiliesDiffer)
					{
						//Swapchain image barriers are changed every frame
						VkImage barrieredImage = nullptr;
						if(subresourceMetadata.ImageIndex != mGraphToBuild->mBackbufferRefIndex)
						{
							barrieredImage = mGraphToBuild->mImages[subresourceMetadata.ImageIndex];
						}

						VkImageMemoryBarrier imageMemoryBarrier;
						imageMemoryBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.pNext                           = nullptr;
						imageMemoryBarrier.srcAccessMask                   = subresourceMetadata.PrevPassMetadata->AccessFlags;
						imageMemoryBarrier.dstAccessMask                   = subresourceMetadata.AccessFlags;
						imageMemoryBarrier.oldLayout                       = subresourceMetadata.PrevPassMetadata->Layout;
						imageMemoryBarrier.newLayout                       = subresourceMetadata.Layout;
						imageMemoryBarrier.srcQueueFamilyIndex             = subresourceMetadata.PrevPassMetadata->QueueFamilyOwnership;
						imageMemoryBarrier.dstQueueFamilyIndex             = subresourceMetadata.QueueFamilyOwnership;
						imageMemoryBarrier.image                           = barrieredImage;
						imageMemoryBarrier.subresourceRange.aspectMask     = subresourceMetadata.AspectFlags;
						imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
						imageMemoryBarrier.subresourceRange.layerCount     = 1;
						imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
						imageMemoryBarrier.subresourceRange.levelCount     = 1;

						beforeBarriers.push_back(imageMemoryBarrier);

						//Mark the swapchain barriers
						if(barrieredImage == nullptr)
						{
							beforeSwapchainBarriers.push_back(beforeBarriers.size() - 1);
						}
					}
				}

				if(subresourceMetadata.NextPassMetadata != nullptr)
				{
					const bool accessFlagsDiffer   = (subresourceMetadata.AccessFlags          != subresourceMetadata.NextPassMetadata->AccessFlags);
					const bool layoutsDiffer       = (subresourceMetadata.Layout               != subresourceMetadata.NextPassMetadata->Layout);
					const bool queueFamiliesDiffer = (subresourceMetadata.QueueFamilyOwnership != subresourceMetadata.NextPassMetadata->QueueFamilyOwnership);
					if(accessFlagsDiffer || layoutsDiffer || queueFamiliesDiffer)
					{
						//Swapchain image barriers are changed every frame
						VkImage barrieredImage = nullptr;
						if(subresourceMetadata.ImageIndex != mGraphToBuild->mBackbufferRefIndex)
						{
							barrieredImage = mGraphToBuild->mImages[subresourceMetadata.ImageIndex];
						}

						VkImageMemoryBarrier imageMemoryBarrier;
						imageMemoryBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.pNext                           = nullptr;
						imageMemoryBarrier.srcAccessMask                   = subresourceMetadata.AccessFlags;
						imageMemoryBarrier.dstAccessMask                   = subresourceMetadata.NextPassMetadata->AccessFlags;
						imageMemoryBarrier.oldLayout                       = subresourceMetadata.Layout;
						imageMemoryBarrier.newLayout                       = subresourceMetadata.NextPassMetadata->Layout;
						imageMemoryBarrier.srcQueueFamilyIndex             = subresourceMetadata.QueueFamilyOwnership;
						imageMemoryBarrier.dstQueueFamilyIndex             = subresourceMetadata.NextPassMetadata->QueueFamilyOwnership;
						imageMemoryBarrier.image                           = mGraphToBuild->mImages[subresourceMetadata.ImageIndex];
						imageMemoryBarrier.subresourceRange.aspectMask     = subresourceMetadata.NextPassMetadata->AspectFlags;
						imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
						imageMemoryBarrier.subresourceRange.layerCount     = 1;
						imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
						imageMemoryBarrier.subresourceRange.levelCount     = 1;

						afterBarriers.push_back(imageMemoryBarrier);

						//Mark the swapchain barriers
						if(barrieredImage == nullptr)
						{
							afterSwapchainBarriers.push_back(afterBarriers.size() - 1);
						}
					}
				}
			}

			processedMetadataIds.insert(subresourceMetadata.MetadataId);
		}

		uint32_t beforeBarrierBegin = (uint32_t)(mGraphToBuild->mImageBarriers.size());
		mGraphToBuild->mImageBarriers.insert(mGraphToBuild->mImageBarriers.end(), beforeBarriers.begin(), beforeBarriers.end());

		uint32_t afterBarrierBegin = (uint32_t)(mGraphToBuild->mImageBarriers.size());
		mGraphToBuild->mImageBarriers.insert(mGraphToBuild->mImageBarriers.end(), afterBarriers.begin(), afterBarriers.end());

		FrameGraph::BarrierSpan barrierSpan;
		barrierSpan.BeforePassBegin = beforeBarrierBegin;
		barrierSpan.BeforePassEnd   = beforeBarrierBegin + (uint32_t)(beforeBarriers.size());
		barrierSpan.AfterPassBegin  = afterBarrierBegin;
		barrierSpan.AfterPassEnd    = afterBarrierBegin + (uint32_t)(afterBarriers.size());

		//That's for now
		barrierSpan.beforeFlagsBegin = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		barrierSpan.beforeFlagsEnd   = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		barrierSpan.afterFlagsBegin  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		barrierSpan.afterFlagsEnd    = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		mGraphToBuild->mImageRenderPassBarriers[i] = barrierSpan;

		for(size_t beforeSwapchainBarrierIndex: beforeSwapchainBarriers)
		{
			mGraphToBuild->mSwapchainBarrierIndices.push_back(beforeBarrierBegin + (uint32_t)beforeSwapchainBarrierIndex);
		}

		for(size_t afterSwapchainBarrierIndex: afterSwapchainBarriers)
		{
			mGraphToBuild->mSwapchainBarrierIndices.push_back(afterBarrierBegin + (uint32_t)afterSwapchainBarrierIndex);
		}
	}
}

void VulkanCBindings::FrameGraphBuilder::BuildDescriptors()
{
	//TODO (in case of passes using storage images or input attachments)
	//For now I don't have any passes to test it
}

void VulkanCBindings::FrameGraphBuilder::BarrierImages(const DeviceQueues* deviceQueues, const WorkerCommandBuffers* workerCommandBuffers, uint32_t defaultQueueIndex)
{
	//Transit to the last pass as if rendering just ended. That ensures correct barriers at the start of the frame
	std::vector<VkImageMemoryBarrier> imageMemoryBarriers;

	std::unordered_map<SubresourceName, ImageSubresourceMetadata> imageLastMetadatas;
	for(size_t i = mRenderPassNames.size() - 1; i < mRenderPassNames.size(); i--) //The loop wraps around 0
	{
		const RenderPassName renderPassName = mRenderPassNames[i];

		const auto& nameIdMap = mRenderPassesSubresourceNameIds[renderPassName];
		for(const auto& nameId: nameIdMap)
		{
			if(!imageLastMetadatas.contains(nameId.first) && nameId.first != mBackbufferName)
			{
				ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas[renderPassName][nameId.second];
				imageLastMetadatas[nameId.first] = metadata;
			}
		}
	}

	std::vector<VkImageMemoryBarrier> imageBarriers;
	for(const auto& nameMetadata: imageLastMetadatas)
	{
		VkImageMemoryBarrier imageBarrier;
		imageBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.pNext                           = nullptr;
		imageBarrier.srcAccessMask                   = 0;
		imageBarrier.dstAccessMask                   = nameMetadata.second.AccessFlags;
		imageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier.newLayout                       = nameMetadata.second.Layout;
		imageBarrier.srcQueueFamilyIndex             = defaultQueueIndex; //Resources were created with that queue ownership
		imageBarrier.dstQueueFamilyIndex             = nameMetadata.second.QueueFamilyOwnership;
		imageBarrier.image                           = mGraphToBuild->mImages[nameMetadata.second.ImageIndex];
		imageBarrier.subresourceRange.aspectMask     = nameMetadata.second.AspectFlags;
		imageBarrier.subresourceRange.baseMipLevel   = 0;
		imageBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

		imageBarriers.push_back(imageBarrier);
	}

	VkCommandPool   graphicsCommandPool   = workerCommandBuffers->GetMainThreadGraphicsCommandPool(0);
	VkCommandBuffer graphicsCommandBuffer = workerCommandBuffers->GetMainThreadGraphicsCommandBuffer(0);

	ThrowIfFailed(vkResetCommandPool(mGraphToBuild->mDeviceRef, graphicsCommandPool, 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo graphicsCmdBufferBeginInfo;
	graphicsCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	graphicsCmdBufferBeginInfo.pNext            = nullptr;
	graphicsCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	graphicsCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(graphicsCommandBuffer, &graphicsCmdBufferBeginInfo));

	std::array<VkMemoryBarrier,       0> memoryBarriers;
	std::array<VkBufferMemoryBarrier, 0> bufferBarriers;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		                 (uint32_t)memoryBarriers.size(), memoryBarriers.data(),
		                 (uint32_t)bufferBarriers.size(), bufferBarriers.data(),
		                 (uint32_t)imageBarriers.size(),  imageBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(graphicsCommandBuffer));

	deviceQueues->GraphicsQueueSubmit(graphicsCommandBuffer);
	deviceQueues->GraphicsQueueWait();
}

void VulkanCBindings::FrameGraphBuilder::BuildResourceCreateInfos(const std::vector<uint32_t>& defaultQueueIndices, std::unordered_map<SubresourceName, ImageResourceCreateInfo>& outImageCreateInfos, std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& outBackbufferCreateInfos, std::unordered_set<RenderPassName>& swapchainPassNames)
{
	for(const auto& renderPassName: mRenderPassNames)
	{
		const auto& nameIdMap = mRenderPassesSubresourceNameIds[renderPassName];
		for(const auto& nameId: nameIdMap)
		{
			const SubresourceName subresourceName = nameId.first;
			const SubresourceId   subresourceId   = nameId.second;

			if(subresourceName == mBackbufferName)
			{
				ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas[renderPassName][subresourceId];

				BackbufferResourceCreateInfo backbufferCreateInfo;

				ImageViewInfo imageViewInfo;
				imageViewInfo.Format     = metadata.Format;
				imageViewInfo.AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageViewInfo.ViewAddresses.push_back({.PassName = renderPassName, .SubresId = subresourceId});

				backbufferCreateInfo.ImageViewInfos.push_back(imageViewInfo);

				outBackbufferCreateInfos[subresourceName] = backbufferCreateInfo;

				swapchainPassNames.insert(renderPassName);
			}
			else
			{
				ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas[renderPassName][subresourceId];
				if(!outImageCreateInfos.contains(subresourceName))
				{
					ImageResourceCreateInfo imageResourceCreateInfo;

					//TODO: Multisampling
					VkImageCreateInfo imageCreateInfo;
					imageCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
					imageCreateInfo.pNext                 = nullptr;
					imageCreateInfo.flags                 = 0;
					imageCreateInfo.imageType             = VK_IMAGE_TYPE_2D;
					imageCreateInfo.format                = metadata.Format;
					imageCreateInfo.extent.width          = mGraphToBuild->mFrameGraphConfig.GetViewportWidth();
					imageCreateInfo.extent.height         = mGraphToBuild->mFrameGraphConfig.GetViewportHeight();
					imageCreateInfo.extent.depth          = 1;
					imageCreateInfo.mipLevels             = 1;
					imageCreateInfo.arrayLayers           = 1;
					imageCreateInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
					imageCreateInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
					imageCreateInfo.usage                 = metadata.UsageFlags;
					imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
					imageCreateInfo.queueFamilyIndexCount = (uint32_t)(defaultQueueIndices.size());
					imageCreateInfo.pQueueFamilyIndices   = defaultQueueIndices.data();
					imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

					imageResourceCreateInfo.ImageCreateInfo = imageCreateInfo;

					outImageCreateInfos[subresourceName] = imageResourceCreateInfo;
				}

				auto& imageResourceCreateInfo = outImageCreateInfos[subresourceName];
				if(metadata.Format != VK_FORMAT_UNDEFINED)
				{
					if(imageResourceCreateInfo.ImageCreateInfo.format == VK_FORMAT_UNDEFINED)
					{
						//Not all passes specify the format
						imageResourceCreateInfo.ImageCreateInfo.format = metadata.Format;
					}
				}

				//It's fine if format is UNDEFINED or AspectFlags is 0. It means it can be arbitrary and we'll fix it later based on other image view infos for this resource
				ImageViewInfo imageViewInfo;
				imageViewInfo.Format     = metadata.Format;
				imageViewInfo.AspectMask = metadata.AspectFlags;
				imageViewInfo.ViewAddresses.push_back({.PassName = renderPassName, .SubresId = subresourceId});

				imageResourceCreateInfo.ImageViewInfos.push_back(imageViewInfo);

				imageResourceCreateInfo.ImageCreateInfo.usage |= metadata.UsageFlags;
			}
		}
	}

	MergeImageViewInfos(outImageCreateInfos);
}

void VulkanCBindings::FrameGraphBuilder::MergeImageViewInfos(std::unordered_map<SubresourceName, ImageResourceCreateInfo>& inoutImageResourceCreateInfos)
{
	//Fix all "arbitrary" image view infos
	for(auto& nameWithCreateInfo: inoutImageResourceCreateInfos)
	{
		for(int i = 0; i < nameWithCreateInfo.second.ImageViewInfos.size(); i++)
		{
			if(nameWithCreateInfo.second.ImageViewInfos[i].Format == VK_FORMAT_UNDEFINED)
			{
				//Assign the closest non-UNDEFINED format
				for(int j = i + 1; j < (int)nameWithCreateInfo.second.ImageViewInfos.size(); j++)
				{
					if(nameWithCreateInfo.second.ImageViewInfos[j].Format != VK_FORMAT_UNDEFINED)
					{
						nameWithCreateInfo.second.ImageViewInfos[i].Format = nameWithCreateInfo.second.ImageViewInfos[j].Format;
						break;
					}
				}

				//Still UNDEFINED? Try to find the format in elements before
				if(nameWithCreateInfo.second.ImageViewInfos[i].Format == VK_FORMAT_UNDEFINED)
				{
					for(int j = i - 1; j >= 0; j++)
					{
						if(nameWithCreateInfo.second.ImageViewInfos[j].Format != VK_FORMAT_UNDEFINED)
						{
							nameWithCreateInfo.second.ImageViewInfos[i].Format = nameWithCreateInfo.second.ImageViewInfos[j].Format;
							break;
						}
					}
				}
			}


			if(nameWithCreateInfo.second.ImageViewInfos[i].AspectMask == 0)
			{
				//Assign the closest non-0 aspect mask
				for(int j = i + 1; j < (int)nameWithCreateInfo.second.ImageViewInfos.size(); j++)
				{
					if(nameWithCreateInfo.second.ImageViewInfos[j].AspectMask != 0)
					{
						nameWithCreateInfo.second.ImageViewInfos[i].AspectMask = nameWithCreateInfo.second.ImageViewInfos[j].AspectMask;
						break;
					}
				}

				//Still 0? Try to find the mask in elements before
				if(nameWithCreateInfo.second.ImageViewInfos[i].AspectMask == 0)
				{
					for(int j = i - 1; j >= 0; j++)
					{
						if(nameWithCreateInfo.second.ImageViewInfos[j].AspectMask != 0)
						{
							nameWithCreateInfo.second.ImageViewInfos[i].AspectMask = nameWithCreateInfo.second.ImageViewInfos[j].AspectMask;
							break;
						}
					}
				}
			}

			assert(nameWithCreateInfo.second.ImageViewInfos[i].Format     != VK_FORMAT_UNDEFINED);
			assert(nameWithCreateInfo.second.ImageViewInfos[i].AspectMask != 0);
		}

		//Merge all of the same image views into one
		std::sort(nameWithCreateInfo.second.ImageViewInfos.begin(), nameWithCreateInfo.second.ImageViewInfos.end(), [](const ImageViewInfo& left, const ImageViewInfo& right)
		{
			if(left.Format == right.Format)
			{
				return (uint64_t)left.AspectMask < (uint64_t)right.AspectMask;
			}
			else
			{
				return (uint64_t)left.Format < (uint64_t)right.Format;
			}
		});

		std::vector<ImageViewInfo> newImageViewInfos;

		VkFormat           currentFormat      = VK_FORMAT_UNDEFINED;
		VkImageAspectFlags currentAspectFlags = 0;
		for(size_t i = 0; i < nameWithCreateInfo.second.ImageViewInfos.size(); i++)
		{
			if(currentFormat != nameWithCreateInfo.second.ImageViewInfos[i].Format || currentAspectFlags != nameWithCreateInfo.second.ImageViewInfos[i].AspectMask)
			{
				ImageViewInfo imageViewInfo;
				imageViewInfo.Format     = nameWithCreateInfo.second.ImageViewInfos[i].Format;
				imageViewInfo.AspectMask = nameWithCreateInfo.second.ImageViewInfos[i].AspectMask;
				imageViewInfo.ViewAddresses.push_back({.PassName = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[0].PassName, .SubresId = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[0].SubresId});

				newImageViewInfos.push_back(imageViewInfo);
			}
			else
			{
				newImageViewInfos.back().ViewAddresses.push_back({.PassName = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[0].PassName, .SubresId = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[0].SubresId});
			}

			currentFormat      = nameWithCreateInfo.second.ImageViewInfos[i].Format;
			currentAspectFlags = nameWithCreateInfo.second.ImageViewInfos[i].AspectMask;
		}

		nameWithCreateInfo.second.ImageViewInfos = newImageViewInfos;
	}
}

void VulkanCBindings::FrameGraphBuilder::PropagateMetadatasFromImageViews(const std::unordered_map<SubresourceName, ImageResourceCreateInfo>& imageCreateInfos, const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferCreateInfos)
{
	for(auto& nameWithCreateInfo: imageCreateInfos)
	{
		for(size_t i = 0; i < nameWithCreateInfo.second.ImageViewInfos.size(); i++)
		{
			VkFormat           format      = nameWithCreateInfo.second.ImageViewInfos[i].Format;
			VkImageAspectFlags aspectFlags = nameWithCreateInfo.second.ImageViewInfos[i].AspectMask;

			for(size_t j = 0; j < nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses.size(); j++)
			{
				const SubresourceAddress& address = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[j];
				mRenderPassesSubresourceMetadatas.at(address.PassName).at(address.SubresId).Format      = format;
				mRenderPassesSubresourceMetadatas.at(address.PassName).at(address.SubresId).AspectFlags = aspectFlags;
			}
		}
	}

	for(auto& nameWithCreateInfo: backbufferCreateInfos)
	{
		for(size_t i = 0; i < nameWithCreateInfo.second.ImageViewInfos.size(); i++)
		{
			VkFormat           format      = nameWithCreateInfo.second.ImageViewInfos[i].Format;
			VkImageAspectFlags aspectFlags = nameWithCreateInfo.second.ImageViewInfos[i].AspectMask;

			for(size_t j = 0; j < nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses.size(); j++)
			{
				const SubresourceAddress& address = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[j];
				mRenderPassesSubresourceMetadatas.at(address.PassName).at(address.SubresId).Format      = format;
				mRenderPassesSubresourceMetadatas.at(address.PassName).at(address.SubresId).AspectFlags = aspectFlags;
			}
		}
	}
}

void VulkanCBindings::FrameGraphBuilder::SetSwapchainImages(const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferResourceCreateInfos, const std::vector<VkImage>& swapchainImages)
{
	mGraphToBuild->mSwapchainImageRefs.clear();

	mGraphToBuild->mImages.push_back(VK_NULL_HANDLE);
	mGraphToBuild->mBackbufferRefIndex = (uint32_t)(mGraphToBuild->mImages.size() - 1);

	for(size_t i = 0; i < swapchainImages.size(); i++)
	{
		mGraphToBuild->mSwapchainImageRefs.push_back(swapchainImages[i]);
	}
	
	for(const auto& nameWithCreateInfo: backbufferResourceCreateInfos)
	{
		//IMAGE VIEWS
		for(const auto& imageViewInfo: nameWithCreateInfo.second.ImageViewInfos)
		{
			for(const auto& viewAddress: imageViewInfo.ViewAddresses)
			{
				mRenderPassesSubresourceMetadatas[viewAddress.PassName][viewAddress.SubresId].ImageIndex = mGraphToBuild->mBackbufferRefIndex;
			}
		}
	}
}

void VulkanCBindings::FrameGraphBuilder::CreateImages(const std::unordered_map<SubresourceName, ImageResourceCreateInfo>& imageResourceCreateInfos, const MemoryManager* memoryAllocator)
{
	mGraphToBuild->mImages.clear();

	for(const auto& nameWithCreateInfo: imageResourceCreateInfos)
	{
		VkImage image = VK_NULL_HANDLE;
		ThrowIfFailed(vkCreateImage(mGraphToBuild->mDeviceRef, &nameWithCreateInfo.second.ImageCreateInfo, nullptr, &image));

		mGraphToBuild->mImages.push_back(image);
		
		//IMAGE VIEWS
		for(const auto& imageViewInfo: nameWithCreateInfo.second.ImageViewInfos)
		{
			for(const auto& viewAddress: imageViewInfo.ViewAddresses)
			{
				mRenderPassesSubresourceMetadatas[viewAddress.PassName][viewAddress.SubresId].ImageIndex = (uint32_t)(mGraphToBuild->mImages.size() - 1);
			}
		}
		
		SetDebugObjectName(image, nameWithCreateInfo.first); //Only does something in debug
	}

	SafeDestroyObject(vkFreeMemory, mGraphToBuild->mDeviceRef, mGraphToBuild->mImageMemory);

	std::vector<VkDeviceSize> textureMemoryOffsets;
	mGraphToBuild->mImageMemory = memoryAllocator->AllocateImagesMemory(mGraphToBuild->mDeviceRef, mGraphToBuild->mImages, textureMemoryOffsets);

	std::vector<VkBindImageMemoryInfo> bindImageMemoryInfos(mGraphToBuild->mImages.size());
	for(size_t i = 0; i < mGraphToBuild->mImages.size(); i++)
	{
		//TODO: mGPU?
		bindImageMemoryInfos[i].sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
		bindImageMemoryInfos[i].pNext        = nullptr;
		bindImageMemoryInfos[i].memory       = mGraphToBuild->mImageMemory;
		bindImageMemoryInfos[i].memoryOffset = textureMemoryOffsets[i];
		bindImageMemoryInfos[i].image        = mGraphToBuild->mImages[i];
	}

	ThrowIfFailed(vkBindImageMemory2(mGraphToBuild->mDeviceRef, (uint32_t)(bindImageMemoryInfos.size()), bindImageMemoryInfos.data()));
}

void VulkanCBindings::FrameGraphBuilder::CreateImageViews(const std::unordered_map<SubresourceName, ImageResourceCreateInfo>& imageResourceCreateInfos)
{
	mGraphToBuild->mImageViews.clear();

	for(const auto& nameWithCreateInfo: imageResourceCreateInfos)
	{
		//Image is the same for all image views
		const SubresourceAddress firstViewAddress = nameWithCreateInfo.second.ImageViewInfos[0].ViewAddresses[0];
		uint32_t imageIndex                       = mRenderPassesSubresourceMetadatas[firstViewAddress.PassName][firstViewAddress.SubresId].ImageIndex;
		VkImage image                             = mGraphToBuild->mImages[imageIndex];

		for(size_t j = 0; j < nameWithCreateInfo.second.ImageViewInfos.size(); j++)
		{
			const ImageViewInfo& imageViewInfo = nameWithCreateInfo.second.ImageViewInfos[j];

			VkImageView imageView = VK_NULL_HANDLE;
			CreateImageView(imageViewInfo, image, nameWithCreateInfo.second.ImageCreateInfo.format, &imageView);

			mGraphToBuild->mImageViews.push_back(imageView);

			uint32_t imageViewIndex = (uint32_t)(mGraphToBuild->mImageViews.size() - 1);
			for(size_t k = 0; k < imageViewInfo.ViewAddresses.size(); k++)
			{
				mRenderPassesSubresourceMetadatas[imageViewInfo.ViewAddresses[k].PassName][imageViewInfo.ViewAddresses[k].SubresId].ImageViewIndex = imageViewIndex;
			}
		}
	}
}

void VulkanCBindings::FrameGraphBuilder::CreateSwapchainImageViews(const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferResourceCreateInfos, VkFormat swapchainFormat)
{
	mGraphToBuild->mSwapchainImageViews.clear();
	mGraphToBuild->mSwapchainViewsSwapMap.clear();

	for(const auto& nameWithCreateInfo: backbufferResourceCreateInfos)
	{
		for(size_t j = 0; j < nameWithCreateInfo.second.ImageViewInfos.size(); j++)
		{
			const ImageViewInfo& imageViewInfo = nameWithCreateInfo.second.ImageViewInfos[j];

			//Base image view is NULL (it will be assigned from mSwapchainImageViews each frame)
			mGraphToBuild->mImageViews.push_back(VK_NULL_HANDLE);

			uint32_t imageViewIndex = (uint32_t)(mGraphToBuild->mImageViews.size() - 1);
			for(size_t k = 0; k < imageViewInfo.ViewAddresses.size(); k++)
			{
				mRenderPassesSubresourceMetadatas[imageViewInfo.ViewAddresses[k].PassName][imageViewInfo.ViewAddresses[k].SubresId].ImageViewIndex = imageViewIndex;
			}

			//Create per-frame views and the entry in mSwapchainViewsSwapMap describing how to swap views each frame
			mGraphToBuild->mSwapchainViewsSwapMap.push_back(imageViewIndex);
			for(size_t swapchainImageIndex = 0; swapchainImageIndex < mGraphToBuild->mSwapchainImageRefs.size(); swapchainImageIndex++)
			{
				VkImage image = mGraphToBuild->mSwapchainImageRefs[swapchainImageIndex];

				VkImageView imageView = VK_NULL_HANDLE;
				CreateImageView(imageViewInfo, image, swapchainFormat, &imageView);

				mGraphToBuild->mSwapchainImageViews.push_back(imageView);
				mGraphToBuild->mSwapchainViewsSwapMap.push_back((uint32_t)(mGraphToBuild->mSwapchainImageViews.size() - 1));
			}
		}
	}

	mGraphToBuild->mLastSwapchainImageIndex = (uint32_t)(mGraphToBuild->mSwapchainImageRefs.size() - 1);
	
	std::swap(mGraphToBuild->mImages[mGraphToBuild->mBackbufferRefIndex], mGraphToBuild->mSwapchainImageRefs[mGraphToBuild->mLastSwapchainImageIndex]);
	for(uint32_t i = 0; i < (uint32_t)mGraphToBuild->mSwapchainViewsSwapMap.size(); i += ((uint32_t)mGraphToBuild->mSwapchainImageRefs.size() + 1u))
	{
		uint32_t imageViewIndex     = mGraphToBuild->mSwapchainViewsSwapMap[i];
		uint32_t imageViewSwapIndex = mGraphToBuild->mSwapchainViewsSwapMap[i + mGraphToBuild->mLastSwapchainImageIndex];

		std::swap(mGraphToBuild->mImageViews[imageViewIndex], mGraphToBuild->mSwapchainImageViews[imageViewSwapIndex]);
	}
}

void VulkanCBindings::FrameGraphBuilder::CreateImageView(const ImageViewInfo& imageViewInfo, VkImage image, VkFormat defaultFormat, VkImageView* outImageView)
{
	assert(outImageView != nullptr);

	VkFormat format = imageViewInfo.Format;
	if(format == VK_FORMAT_UNDEFINED)
	{
		format = defaultFormat;
	}

	assert(format != VK_FORMAT_UNDEFINED);

	//TODO: mutable format usage
	//TODO: fragment density map
	VkImageViewCreateInfo imageViewCreateInfo;
	imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext                           = nullptr;
	imageViewCreateInfo.flags                           = 0;
	imageViewCreateInfo.image                           = image;
	imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format                          = format;
	imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.aspectMask     = imageViewInfo.AspectMask;

	imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
	imageViewCreateInfo.subresourceRange.levelCount     = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount     = 1;

	ThrowIfFailed(vkCreateImageView(mGraphToBuild->mDeviceRef, &imageViewCreateInfo, nullptr, outImageView));
}

bool VulkanCBindings::FrameGraphBuilder::PassesIntersect(const RenderPassName& writingPass, const RenderPassName& readingPass)
{
	if(mRenderPassesSubresourceNameIds[writingPass].size() <= mRenderPassesSubresourceNameIds[readingPass].size())
	{
		for(const auto& subresourceNameId: mRenderPassesSubresourceNameIds[writingPass])
		{
			if(!mRenderPassesWriteSubresourceIds[writingPass].contains(subresourceNameId.second))
			{
				//Is not a written subresource
				continue;
			}

			auto readIt = mRenderPassesSubresourceNameIds[readingPass].find(subresourceNameId.first);
			if(readIt != mRenderPassesSubresourceNameIds[readingPass].end() && mRenderPassesReadSubresourceIds[readingPass].contains(readIt->second))
			{
				//Is a read subresource
				return true;
			}
		}
	}
	else
	{
		for(const auto& subresourceNameId: mRenderPassesSubresourceNameIds[readingPass])
		{
			if(!mRenderPassesReadSubresourceIds[readingPass].contains(subresourceNameId.second))
			{
				//Is not a read subresource
				continue;
			}

			auto writeIt = mRenderPassesSubresourceNameIds[writingPass].find(subresourceNameId.first);
			if(writeIt != mRenderPassesSubresourceNameIds[writingPass].end() && mRenderPassesReadSubresourceIds[writingPass].contains(writeIt->second))
			{
				//Is a written subresource
				return true;
			}
		}
	}

	return false;
}

void VulkanCBindings::FrameGraphBuilder::SetDebugObjectName(VkImage image, const SubresourceName& name) const
{
#if defined(VK_EXT_debug_utils) && (defined(DEBUG) || defined(_DEBUG))
	
	if(mInstanceParameters->IsDebugUtilsExtensionEnabled())
	{
		VkDebugUtilsObjectNameInfoEXT imageNameInfo;
		imageNameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		imageNameInfo.pNext        = nullptr;
		imageNameInfo.objectType   = VK_OBJECT_TYPE_IMAGE;
		imageNameInfo.objectHandle = reinterpret_cast<uint64_t>(image);
		imageNameInfo.pObjectName  = name.c_str();

		ThrowIfFailed(vkSetDebugUtilsObjectNameEXT(mGraphToBuild->mDeviceRef, &imageNameInfo));
	}
#else
	UNREFERENCED_PARAMETER(image);
	UNREFERENCED_PARAMETER(name);
#endif
}