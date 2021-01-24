#include "VulkanCFrameGraphBuilder.hpp"
#include "VulkanCFrameGraph.hpp"
#include "../VulkanCFunctions.hpp"
#include "../VulkanCMemory.hpp"
#include "../VulkanCDeviceQueues.hpp"
#include <algorithm>

VulkanCBindings::FrameGraphBuilder::FrameGraphBuilder(FrameGraph* graphToBuild, const RenderableScene* scene, const DeviceParameters* deviceParameters, const ShaderManager* shaderManager): mGraphToBuild(graphToBuild), mScene(scene), mDeviceParameters(deviceParameters), mShaderManager(shaderManager)
{
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
	subresourceMetadata.PrevPassMetadata   = nullptr;
	subresourceMetadata.NextPassMetadata   = nullptr;
	subresourceMetadata.Format             = VK_FORMAT_UNDEFINED;
	subresourceMetadata.ImageIndex         = (uint32_t)(-1);
	subresourceMetadata.ImageViewIndex     = (uint32_t)(-1);
	subresourceMetadata.Layout             = VK_IMAGE_LAYOUT_GENERAL;
	subresourceMetadata.UsageFlags         = 0;
	subresourceMetadata.AspectFlags        = 0;
	subresourceMetadata.PipelineStageFlags = 0;
	subresourceMetadata.AccessFlags        = 0;

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
	subresourceMetadata.PrevPassMetadata   = nullptr;
	subresourceMetadata.NextPassMetadata   = nullptr;
	subresourceMetadata.Format             = VK_FORMAT_UNDEFINED;
	subresourceMetadata.ImageIndex         = (uint32_t)(-1);
	subresourceMetadata.ImageViewIndex     = (uint32_t)(-1);
	subresourceMetadata.Layout             = VK_IMAGE_LAYOUT_UNDEFINED;
	subresourceMetadata.UsageFlags         = 0;
	subresourceMetadata.AspectFlags        = 0;
	subresourceMetadata.PipelineStageFlags = 0;
	subresourceMetadata.AccessFlags        = 0;

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

const VulkanCBindings::FrameGraphConfig* VulkanCBindings::FrameGraphBuilder::GetConfig() const
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

void VulkanCBindings::FrameGraphBuilder::Build(const DeviceQueues* deviceQueues, const MemoryManager* memoryAllocator, const std::vector<VkImage>& swapchainImages, VkFormat swapchainFormat)
{
	std::vector<std::unordered_set<size_t>> adjacencyList;
	BuildAdjacencyList(adjacencyList);
	SortRenderPasses(adjacencyList);

	ValidateSubresourceLinks();

	std::unordered_set<RenderPassName> swapchainPassNames;
	BuildSubresources(deviceQueues, memoryAllocator, swapchainImages, swapchainPassNames, swapchainFormat);
	BuildPassObjects(swapchainPassNames);
}

void VulkanCBindings::FrameGraphBuilder::BuildAdjacencyList(std::vector<std::unordered_set<size_t>>& adjacencyList)
{
	adjacencyList.clear();

	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		adjacencyList.emplace_back();

		for(size_t j = 0; j < mRenderPassNames.size(); j++)
		{
			if(i == j)
			{
				continue;
			}

			if(PassesIntersect(mRenderPassNames[i], mRenderPassNames[j]))
			{
				adjacencyList[i].insert(j);
			}
		}
	}
}

void VulkanCBindings::FrameGraphBuilder::SortRenderPasses(const std::vector<std::unordered_set<size_t>>& adjacencyList)
{
	std::vector<size_t> sortedPassIndices;

	std::vector<uint_fast8_t> visited(mRenderPassNames.size(), false);
	std::vector<uint_fast8_t> onStack(mRenderPassNames.size(), false);

	for(size_t passIndex = 0; passIndex < mRenderPassNames.size(); passIndex++)
	{
		TopologicalSortNode(adjacencyList, visited, onStack, passIndex, sortedPassIndices);
	}

	std::vector<RenderPassName> sortedRenderPassNames(mRenderPassNames.size());
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		sortedRenderPassNames[mRenderPassNames.size() - i - 1] = mRenderPassNames[sortedPassIndices[i]];
	}

	mRenderPassNames = std::move(sortedRenderPassNames);

	mRenderPassIndices.clear();
	for(uint32_t i = 0; i < mRenderPassNames.size(); i++)
	{
		mRenderPassIndices[mRenderPassNames[i]] = i;
	}
}

void VulkanCBindings::FrameGraphBuilder::TopologicalSortNode(const std::vector<std::unordered_set<size_t>>& adjacencyList, std::vector<uint_fast8_t>& visited, std::vector<uint_fast8_t>& onStack, size_t passIndex, std::vector<size_t>& sortedPassIndices)
{
	if(visited[passIndex])
	{
		return;
	}

	onStack[passIndex] = true;
	visited[passIndex] = true;

	for(size_t adjacentPassIndex: adjacencyList[passIndex])
	{
		TopologicalSortNode(adjacencyList, visited, onStack, adjacentPassIndex, sortedPassIndices);
	}

	onStack[passIndex] = false;
	sortedPassIndices.push_back(passIndex);
}

void VulkanCBindings::FrameGraphBuilder::ValidateSubresourceLinks()
{
	std::unordered_map<SubresourceName, RenderPassName> subresourceLastRenderPasses; 

	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			SubresourceName           subresourceName     = subresourceNameId.first;
			ImageSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas[renderPassName][subresourceNameId.second];

			if(subresourceLastRenderPasses.contains(subresourceName))
			{
				RenderPassName prevPassName = subresourceLastRenderPasses.at(subresourceName);
				
				SubresourceId prevSubresourceId = mRenderPassesSubresourceNameIds[prevPassName][subresourceName];
				ImageSubresourceMetadata& prevSubresourceMetadata = mRenderPassesSubresourceMetadatas[prevPassName][prevSubresourceId];

				prevSubresourceMetadata.NextPassMetadata = &subresourceMetadata;
				subresourceMetadata.PrevPassMetadata = &prevSubresourceMetadata;
			}

			subresourceLastRenderPasses[subresourceName] = renderPassName;
		}
	}
}

void VulkanCBindings::FrameGraphBuilder::BuildSubresources(const DeviceQueues* deviceQueues, const MemoryManager* memoryAllocator, const std::vector<VkImage>& swapchainImages, std::unordered_set<RenderPassName>& swapchainPassNames, VkFormat swapchainFormat)
{
	std::unordered_map<SubresourceName, ImageResourceCreateInfo>      imageResourceCreateInfos;
	std::unordered_map<SubresourceName, BackbufferResourceCreateInfo> backbufferResourceCreateInfos;
	BuildResourceCreateInfos(deviceQueues, imageResourceCreateInfos, backbufferResourceCreateInfos, swapchainPassNames);

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

	mGraphToBuild->mGraphicsPassCount = 0;
	mGraphToBuild->mComputePassCount  = 0;

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

	//Simple strategy: all compute passes in the end are executed on compute queue
	//ACTUAL async compute is very dependent on the frame graph structure and requires much more precise resource management
	size_t renderPassNameIndex = mRenderPassNames.size() - 1;
	while(renderPassNameIndex < mRenderPassNames.size() && mRenderPassTypes[mRenderPassNames[renderPassNameIndex]] == RenderPassType::COMPUTE)
	{
		mGraphToBuild->mComputePassCount++;
		renderPassNameIndex--;
	}

	//Btw: the loop will wrap around 0
	while(renderPassNameIndex < mRenderPassNames.size() && (mRenderPassTypes[mRenderPassNames[renderPassNameIndex]] == RenderPassType::GRAPHICS || mRenderPassTypes[mRenderPassNames[renderPassNameIndex]] == RenderPassType::COMPUTE || mRenderPassTypes[mRenderPassNames[renderPassNameIndex]] == RenderPassType::TRANSFER))
	{
		mGraphToBuild->mGraphicsPassCount++;
		renderPassNameIndex--;
	}

	//Prepare for the first use
	uint32_t swapchainImageCount = (uint32_t)mGraphToBuild->mSwapchainImageRefs.size();
	for(uint32_t i = 0; i < (uint32_t)mGraphToBuild->mSwapchainPassesSwapMap.size(); i += (swapchainImageCount + 1u))
	{
		uint32_t passIndexToSwitch = mGraphToBuild->mSwapchainPassesSwapMap[i];
		mGraphToBuild->mRenderPasses[passIndexToSwitch].swap(mGraphToBuild->mSwapchainRenderPasses[mGraphToBuild->mSwapchainPassesSwapMap[i + mGraphToBuild->mLastSwapchainImageIndex + 1]]);
	}
}

void VulkanCBindings::FrameGraphBuilder::BuildResourceCreateInfos(const DeviceQueues* deviceQueues, std::unordered_map<SubresourceName, ImageResourceCreateInfo>& outImageCreateInfos, std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& outBackbufferCreateInfos, std::unordered_set<RenderPassName>& swapchainPassNames)
{
	std::unordered_map<RenderPassType, uint32_t> familyIndicesForPassTypes;
	familyIndicesForPassTypes[RenderPassType::GRAPHICS] = deviceQueues->GetGraphicsQueueFamilyIndex();
	familyIndicesForPassTypes[RenderPassType::COMPUTE]  = deviceQueues->GetComputeQueueFamilyIndex();
	familyIndicesForPassTypes[RenderPassType::TRANSFER] = deviceQueues->GetTransferQueueFamilyIndex();

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

					imageResourceCreateInfo.QueueOwnerIndices = std::to_array({familyIndicesForPassTypes.at(mRenderPassTypes.at(renderPassName))});


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
					imageCreateInfo.queueFamilyIndexCount = (uint32_t)(imageResourceCreateInfo.QueueOwnerIndices.size());
					imageCreateInfo.pQueueFamilyIndices   = imageResourceCreateInfo.QueueOwnerIndices.data();
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
				imageResourceCreateInfo.QueueOwnerIndices[0]   = familyIndicesForPassTypes.at(mRenderPassTypes.at(renderPassName));
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