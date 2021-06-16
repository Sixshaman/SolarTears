#include "ModernFrameGraphBuilder.hpp"
#include "ModernFrameGraph.hpp"
#include <algorithm>
#include <cassert>
#include <array>

ModernFrameGraphBuilder::ModernFrameGraphBuilder(ModernFrameGraph* graphToBuild): mGraphToBuild(graphToBuild)
{
	//Create present quasi-pass
	std::string presentPassName(PresentPassName);

	mRenderPassTypes[presentPassName] = RenderPassType::Present;

	RegisterWriteSubresource(PresentPassName, BackbufferPresentPassId);
	RegisterReadSubresource(PresentPassName,  BackbufferPresentPassId);
}

ModernFrameGraphBuilder::~ModernFrameGraphBuilder()
{
}

void ModernFrameGraphBuilder::RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId)
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
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, SubresourceMetadataNode>();
	}

	SubresourceMetadataNode metadataNode;
	metadataNode.PrevPassNode         = nullptr;
	metadataNode.NextPassNode         = nullptr;
	metadataNode.SubresourceInfoIndex = AddSubresourceMetadata();
	metadataNode.FirstFrameHandle     = (uint32_t)(-1);
	metadataNode.FirstFrameViewHandle = (uint32_t)(-1);
	metadataNode.FrameCount           = 1;
	metadataNode.PassType             = RenderPassType::Graphics;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = metadataNode;
}

void ModernFrameGraphBuilder::RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId)
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
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, SubresourceMetadataNode>();
	}

	SubresourceMetadataNode metadataNode;
	metadataNode.PrevPassNode         = nullptr;
	metadataNode.NextPassNode         = nullptr;
	metadataNode.SubresourceInfoIndex = AddSubresourceMetadata();
	metadataNode.FirstFrameHandle     = (uint32_t)(-1);
	metadataNode.FirstFrameViewHandle = (uint32_t)(-1);
	metadataNode.FrameCount           = 1;
	metadataNode.PassType             = RenderPassType::Graphics;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = metadataNode;
}

void ModernFrameGraphBuilder::AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName)
{
	RenderPassName  passNameStr(passName);
	SubresourceId   subresourceIdStr(subresourceId);
	SubresourceName subresourceNameStr(subresourceName);

	auto readSubresIt  = mRenderPassesReadSubresourceIds.find(passNameStr);
	auto writeSubresIt = mRenderPassesWriteSubresourceIds.find(passNameStr);

	assert(readSubresIt != mRenderPassesReadSubresourceIds.end() || writeSubresIt != mRenderPassesWriteSubresourceIds.end());
	mRenderPassesSubresourceNameIds[passNameStr][subresourceNameStr] = subresourceIdStr;
}

void ModernFrameGraphBuilder::AssignBackbufferName(const std::string_view backbufferName)
{
	mBackbufferName = backbufferName;

	AssignSubresourceName(PresentPassName, BackbufferPresentPassId, mBackbufferName);

	std::string presentPassNameString = std::string(PresentPassName);
	std::string backbufferIdString    = std::string(BackbufferPresentPassId);

	std::unordered_map<SubresourceId, SubresourceMetadataNode>& presentPassMetadataMap = mRenderPassesSubresourceMetadatas[presentPassNameString];
	if(!presentPassMetadataMap.contains(backbufferIdString))
	{
		presentPassMetadataMap[backbufferIdString] = {};

		SubresourceMetadataNode presentAcquireMetadataNode;
		presentAcquireMetadataNode.PrevPassNode         = nullptr;
		presentAcquireMetadataNode.NextPassNode         = nullptr;
		presentAcquireMetadataNode.SubresourceInfoIndex = AddSubresourceMetadata();
		presentAcquireMetadataNode.FirstFrameHandle     = (uint32_t)(-1);
		presentAcquireMetadataNode.FirstFrameViewHandle = (uint32_t)(-1);
		presentAcquireMetadataNode.FrameCount           = GetSwapchainImageCount();
		presentAcquireMetadataNode.PassType             = RenderPassType::Graphics;

		presentPassMetadataMap[backbufferIdString] = presentAcquireMetadataNode;
	}
}

void ModernFrameGraphBuilder::Build()
{
	std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>> adjacencyList;
	BuildAdjacencyList(adjacencyList);

	std::vector<RenderPassName> sortedPassNames;
	SortRenderPassesTopological(adjacencyList, sortedPassNames);
	SortRenderPassesDependency(adjacencyList, sortedPassNames);
	mRenderPassNames = std::move(sortedPassNames);

	BuildDependencyLevels();

	ValidateSubresourceLinks();
	ValidateSubresourcePassTypes();

	BuildSubresources();
	BuildPassObjects();
	BuildBarriers();

	InitializeTraverseData();
}

const FrameGraphConfig* ModernFrameGraphBuilder::GetConfig() const
{
	return &mGraphToBuild->mFrameGraphConfig;
}

void ModernFrameGraphBuilder::BuildAdjacencyList(std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList)
{
	adjacencyList.clear();

	for (const RenderPassName& renderPassName: mRenderPassNames)
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

void ModernFrameGraphBuilder::SortRenderPassesTopological(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& unsortedPasses)
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

void ModernFrameGraphBuilder::SortRenderPassesDependency(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& topologicallySortedPasses)
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

void ModernFrameGraphBuilder::BuildDependencyLevels()
{
	mGraphToBuild->mGraphicsPassSpans.clear();

	uint32_t currentDependencyLevel = 0;
	mGraphToBuild->mGraphicsPassSpans.push_back(Span<uint32_t>{.Begin = 0, .End = 0});
	for(size_t passIndex = 0; passIndex < mRenderPassNames.size(); passIndex++)
	{
		uint32_t dependencyLevel = mRenderPassDependencyLevels[mRenderPassNames[passIndex]];
		if(currentDependencyLevel != dependencyLevel)
		{
			mGraphToBuild->mGraphicsPassSpans.push_back(Span<uint32_t>{.Begin = dependencyLevel, .End = dependencyLevel + 1});
			currentDependencyLevel = dependencyLevel;
		}
		else
		{
			mGraphToBuild->mGraphicsPassSpans.back().End++;
		}
	}

	//The loop will wrap around 0
	size_t renderPassNameIndex = mRenderPassNames.size() - 1;
	while(renderPassNameIndex < mRenderPassNames.size())
	{
		//Graphics queue only for now
		//Async compute passes should be in the end of the frame. BUT! They should be executed AT THE START of THE PREVIOUS frame. Need to reorder stuff for that
		RenderPassName renderPassName = mRenderPassNames[renderPassNameIndex];

		mRenderPassTypes[renderPassName] = RenderPassType::Compute;
		renderPassNameIndex--;
	}

	mRenderPassTypes[std::string(PresentPassName)] = RenderPassType::Present;
}

void ModernFrameGraphBuilder::ValidateSubresourceLinks()
{
	std::unordered_map<SubresourceName, RenderPassName> subresourceFirstRenderPasses; //The fisrt pass for each subresource
	std::unordered_map<SubresourceName, RenderPassName> subresourceLastRenderPasses;  //The last pass for each subresource

	//Connect previous passes
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			SubresourceName          subresourceName = subresourceNameId.first;
			SubresourceMetadataNode& subresourceNode = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);

			if(subresourceLastRenderPasses.contains(subresourceName))
			{
				RenderPassName prevPassName = subresourceLastRenderPasses.at(subresourceName);
				
				SubresourceId                   prevSubresourceId   = mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
				SubresourceMetadataNode& prevSubresourceNode = mRenderPassesSubresourceMetadatas.at(prevPassName).at(prevSubresourceId);

				prevSubresourceNode.NextPassNode = &subresourceNode;
				subresourceNode.PrevPassNode     = &prevSubresourceNode;
			}

			if(!subresourceFirstRenderPasses.contains(subresourceName))
			{
				subresourceFirstRenderPasses[subresourceName] = renderPassName;
			}

			subresourceLastRenderPasses[subresourceName] = renderPassName;
		}
	}


	//Validate backbuffer links
	RenderPassName backbufferLastPassName  = subresourceLastRenderPasses.at(mBackbufferName);
	RenderPassName backbufferFirstPassName = subresourceFirstRenderPasses.at(mBackbufferName);

	SubresourceId backbufferLastSubresourceId  = mRenderPassesSubresourceNameIds.at(backbufferLastPassName).at(mBackbufferName);
	SubresourceId backbufferFirstSubresourceId = mRenderPassesSubresourceNameIds.at(backbufferFirstPassName).at(mBackbufferName);

	SubresourceMetadataNode& backbufferFirstNode = mRenderPassesSubresourceMetadatas.at(backbufferFirstPassName).at(backbufferFirstSubresourceId);
	SubresourceMetadataNode& backbufferLastNode  = mRenderPassesSubresourceMetadatas.at(backbufferLastPassName).at(backbufferLastSubresourceId);

	SubresourceMetadataNode& backbufferPresentNode = mRenderPassesSubresourceMetadatas.at(std::string(PresentPassName)).at(std::string(BackbufferPresentPassId));

	backbufferFirstNode.PrevPassNode = &backbufferPresentNode;
	backbufferLastNode.NextPassNode  = &backbufferPresentNode;

	backbufferPresentNode.PrevPassNode = &backbufferLastNode;
	backbufferPresentNode.NextPassNode = &backbufferFirstNode;


	//Validate cyclic links (so first pass knows what state to barrier from, and the last pass knows what state to barrier to)
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			if(subresourceNameId.first == mBackbufferName) //Backbuffer is already processed
			{
				continue;
			}

			SubresourceMetadataNode& subresourceNode = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);

			if(subresourceNode.PrevPassNode == nullptr)
			{
				SubresourceName subresourceName = subresourceNameId.first;
				RenderPassName  prevPassName    = subresourceLastRenderPasses.at(subresourceName);

				SubresourceId            prevSubresourceId   = mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
				SubresourceMetadataNode& prevSubresourceNode = mRenderPassesSubresourceMetadatas.at(prevPassName).at(prevSubresourceId);

				subresourceNode.PrevPassNode     = &prevSubresourceNode;
				prevSubresourceNode.NextPassNode = &subresourceNode;
			}

			if(subresourceNode.NextPassNode == nullptr)
			{
				SubresourceName subresourceName = subresourceNameId.first;
				RenderPassName  nextPassName    = subresourceFirstRenderPasses.at(subresourceName);

				SubresourceId            nextSubresourceId       = mRenderPassesSubresourceNameIds.at(nextPassName).at(subresourceName);
				SubresourceMetadataNode& nextSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(nextPassName).at(nextSubresourceId);

				subresourceNode.NextPassNode         = &nextSubresourceMetadata;
				nextSubresourceMetadata.PrevPassNode = &subresourceNode;
			}
		}
	}
}

void ModernFrameGraphBuilder::ValidateSubresourcePassTypes()
{
	//Pass types should be known
	assert(!mRenderPassTypes.empty());

	std::string presentPassName(PresentPassName);

	for (auto& subresourceNameId: mRenderPassesSubresourceNameIds[presentPassName])
	{
		SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(presentPassName).at(subresourceNameId.second);
		metadataNode.PassType = mRenderPassTypes[presentPassName];
	}

	for (size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for (auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);
			metadataNode.PassType = mRenderPassTypes[renderPassName];
		}
	}
}

void ModernFrameGraphBuilder::FindBackbufferPasses(std::unordered_set<RenderPassName>& swapchainPassNames)
{
	swapchainPassNames.clear();

	for(const auto& renderPassName: mRenderPassNames)
	{
		const auto& nameIdMap = mRenderPassesSubresourceNameIds[renderPassName];
		for(const auto& nameId: nameIdMap)
		{
			if(nameId.first == mBackbufferName)
			{
				swapchainPassNames.insert(renderPassName);
				continue;
			}
		}
	}
}

void ModernFrameGraphBuilder::BuildPassObjects()
{
	mGraphToBuild->mRenderPasses.clear();
	mGraphToBuild->mSwapchainPassesSwapMap.clear();

	for(const std::string& passName: mRenderPassNames)
	{
		if(mRenderPassesSubresourceNameIds[passName].contains(mBackbufferName))
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
			}
		}
		else
		{
			//This pass does not use any swapchain images - can just create a single copy
			mGraphToBuild->mRenderPasses.push_back(mRenderPassCreateFunctions.at(passName)(mGraphToBuild->mDeviceRef, this, passName));
		}
	}
}

void ModernFrameGraphBuilder::BuildBarriers()
{
	//VULKAN

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

	


	//D3D12
	mGraphToBuild->mRenderPassBarriers.resize(mRenderPassNames.size());
	mGraphToBuild->mSwapchainBarrierIndices.clear();

	std::unordered_set<uint64_t> processedMetadataIds; //This is here so no barrier gets added twice
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];

		const auto& nameIdMap     = mRenderPassesSubresourceNameIds.at(renderPassName);
		const auto& idMetadataMap = mRenderPassesSubresourceMetadatas.at(renderPassName);

		std::vector<D3D12_RESOURCE_BARRIER> beforeBarriers;
		std::vector<D3D12_RESOURCE_BARRIER> afterBarriers;
		std::vector<size_t>                 beforeSwapchainBarrierIndices;
		std::vector<size_t>                 afterSwapchainBarrierIndices;
		for(auto& subresourceNameId: nameIdMap)
		{
			TextureSubresourceMetadata& subresourceMetadata     = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);
			TextureSubresourceMetadata& prevSubresourceMetadata = *subresourceMetadata.PrevPassMetadata;

			if(prevSubresourceMetadata.ResourceState == D3D12_RESOURCE_STATE_COMMON && D3D12Utils::IsStatePromoteableTo(subresourceMetadata.ResourceState)) //Promotion from common
			{
				subresourceMetadata.MetadataFlags |= TextureFlagStateAutoPromoted;
			}
			else if(prevSubresourceMetadata.QueueOwnership == subresourceMetadata.QueueOwnership) //Queue hasn't changed => ExecuteCommandLists wasn't called => No decay happened
			{
				if(prevSubresourceMetadata.ResourceState == subresourceMetadata.ResourceState) //Propagation of state promotion
				{
					subresourceMetadata.MetadataFlags |= TextureFlagStateAutoPromoted;
				}
			}
			else
			{
				bool prevWasPromoted = (prevSubresourceMetadata.MetadataFlags & TextureFlagStateAutoPromoted);
				if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevSubresourceMetadata.ResourceState)) //Read-only promoted state decays to COMMON after ECL call
				{
					if(D3D12Utils::IsStatePromoteableTo(subresourceMetadata.ResourceState)) //State promotes again
					{
						subresourceMetadata.MetadataFlags |= TextureFlagStateAutoPromoted;
					}
				}
			}

			SubresourceName subresourceName = subresourceNameId.first;
			SubresourceId   subresourceId   = subresourceNameId.second;

			TextureSubresourceMetadata subresourceMetadata = idMetadataMap.at(subresourceId);
			if(!(subresourceMetadata.MetadataFlags & TextureFlagAutoBarrier))
			{
				
				{
					/*
					*    Before barrier:
					*
					*    1.  Same queue, state unchanged:                                  No barrier needed
					*    2.  Same queue, state changed, PRESENT -> automatically promoted: No barrier needed
					*    3.  Same queue, state changed other cases:                        Need a barrier Old state -> New state
					*
					*    4.  Graphics -> Compute,  state automatically promoted:               No barrier needed
					*    5.  Graphics -> Compute,  state non-promoted, was promoted read-only: Need a barrier COMMON -> New state
					*    6.  Graphics -> Compute,  state unchanged:                            No barrier needed
					*    7.  Graphics -> Compute,  state changed other cases:                  No barrier needed, handled by the previous state's barrier
					* 
					*    8.  Compute  -> Graphics, state automatically promoted:                       No barrier needed, state will be promoted again
					* 	 9.  Compute  -> Graphics, state non-promoted, was promoted read-only:         Need a barrier COMMON -> New state   
					*    10. Compute  -> Graphics, state changed Compute/graphics -> Compute/Graphics: No barrier needed, handled by the previous state's barrier
					*    11. Compute  -> Graphics, state changed Compute/Graphics -> Graphics:         Need a barrier Old state -> New state
					*    12. Compute  -> Graphics, state unchanged:                                    No barrier needed
					*
					*    13. Graphics/Compute -> Copy, was promoted read-only: No barrier needed, state decays
					*    14. Graphics/Compute -> Copy, other cases:            No barrier needed, handled by previous state's barrier
					* 
					*    15. Copy -> Graphics/Compute, state automatically promoted: No barrier needed
					*    16. Copy -> Graphics/Compute, state non-promoted:           Need a barrier COMMON -> New state
					*/

					bool needBarrier = false;
					D3D12_RESOURCE_STATES prevPassState = subresourceMetadata.PrevPassMetadata->ResourceState;
					D3D12_RESOURCE_STATES nextPassState = subresourceMetadata.ResourceState;

					const D3D12_COMMAND_LIST_TYPE prevStateQueue = subresourceMetadata.PrevPassMetadata->QueueOwnership;
					const D3D12_COMMAND_LIST_TYPE nextStateQueue = subresourceMetadata.QueueOwnership;

					const bool prevWasPromoted = (subresourceMetadata.PrevPassMetadata->MetadataFlags & TextureFlagStateAutoPromoted);
					const bool nextIsPromoted  = (subresourceMetadata.MetadataFlags & TextureFlagStateAutoPromoted);

					if(prevStateQueue == nextStateQueue) //Rules 1, 2, 3
					{
						if(prevPassState == D3D12_RESOURCE_STATE_PRESENT) //Rule 2 or 3
						{
							needBarrier = !nextIsPromoted;
						}
						else //Rule 1 or 3
						{
							needBarrier = (prevPassState != nextPassState);
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT && nextStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE) //Rules 4, 5, 6, 7
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 5
						{
							prevPassState = D3D12_RESOURCE_STATE_COMMON; //Common state decay from promoted read-only
							needBarrier   = !nextIsPromoted;
						}
						else //Rules 4, 6, 7
						{
							needBarrier = false;
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE && nextStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT) //Rules 8, 9, 10, 11, 12
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 9
						{
							prevPassState = D3D12_RESOURCE_STATE_COMMON; //Common state decay from promoted read-only
							needBarrier   = !nextIsPromoted;
						}
						else //Rules 8, 10, 11, 12
						{
							needBarrier = !nextIsPromoted && (prevPassState != nextPassState) && !D3D12Utils::IsStateComputeFriendly(nextPassState);
						}
					}
					else if(nextStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 13, 14
					{
						needBarrier = false;
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 15, 16
					{
						needBarrier = !nextIsPromoted;
					}

					if(needBarrier)
					{
						//Swapchain image barriers are changed every frame
						ID3D12Resource2* barrieredTexture = nullptr;
						if(subresourceMetadata.ImageIndex != mGraphToBuild->mBackbufferRefIndex)
						{
							barrieredTexture = mGraphToBuild->mTextures[subresourceMetadata.ImageIndex];
						}

						D3D12_RESOURCE_BARRIER textureTransitionBarrier;
						textureTransitionBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
						textureTransitionBarrier.Transition.pResource   = barrieredTexture;
						textureTransitionBarrier.Transition.Subresource = 0;
						textureTransitionBarrier.Transition.StateBefore = prevPassState;
						textureTransitionBarrier.Transition.StateAfter  = nextPassState;
						textureTransitionBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;

						beforeBarriers.push_back(textureTransitionBarrier);

						//Mark the swapchain barriers
						if(barrieredTexture == nullptr)
						{
							beforeSwapchainBarrierIndices.push_back(beforeBarriers.size() - 1);
						}
					}
				}

				
				{
					/*
					*    After barrier:
					*
					*    1.  Same queue, state unchanged:                                           No barrier needed
					*    2.  Same queue, state changed, Promoted read-only -> PRESENT:              No barrier needed, state decays
					*    3.  Same queue, state changed, Promoted writeable/Non-promoted -> PRESENT: Need a barrier Old State -> PRESENT
					*    4.  Same queue, state changed, other cases:                                No barrier needed, will be handled by the next state's barrier
					*
					*    5.  Graphics -> Compute,  state will be automatically promoted:               No barrier needed
					*    6.  Graphics -> Compute,  state will not be promoted, was promoted read-only: No barrier needed, will be handled by the next state's barrier
					*    7.  Graphics -> Compute,  state unchanged:                                    No barrier needed
					*    8.  Graphics -> Compute,  state changed other cases:                          Need a barrier Old state -> New state
					* 
					*    9.  Compute  -> Graphics, state will be automatically promoted:                 No barrier needed
					* 	 10. Compute  -> Graphics, state will not be promoted, was promoted read-only:   No barrier needed, will be handled by the next state's barrier     
					*    11. Compute  -> Graphics, state changed Promoted read-only -> PRESENT:          No barrier needed, state will decay
					*    12. Compute  -> Graphics, state changed Compute/graphics   -> Compute/Graphics: Need a barrier Old state -> New state
					*    13. Compute  -> Graphics, state changed Compute/Graphics   -> Graphics:         No barrier needed, will be handled by the next state's barrier
					*    14. Compute  -> Graphics, state unchanged:                                      No barrier needed
					*   
					*    15. Graphics/Compute -> Copy, from Promoted read-only: No barrier needed
					*    16. Graphics/Compute -> Copy, other cases:             Need a barrier Old state -> COMMON
					* 
					*    17. Copy -> Graphics/Compute, state will be automatically promoted: No barrier needed
					*    18. Copy -> Graphics/Compute, state will not be promoted:           No barrier needed, will be handled by the next state's barrier
					*/

					bool needBarrier = false;
					D3D12_RESOURCE_STATES prevPassState = subresourceMetadata.ResourceState;
					D3D12_RESOURCE_STATES nextPassState = subresourceMetadata.NextPassMetadata->ResourceState;

					const D3D12_COMMAND_LIST_TYPE prevStateQueue = subresourceMetadata.QueueOwnership;
					const D3D12_COMMAND_LIST_TYPE nextStateQueue = subresourceMetadata.NextPassMetadata->QueueOwnership;

					const bool prevWasPromoted = (subresourceMetadata.MetadataFlags & TextureFlagStateAutoPromoted);
					const bool nextIsPromoted  = (subresourceMetadata.NextPassMetadata->MetadataFlags & TextureFlagStateAutoPromoted);

					if(prevStateQueue == nextStateQueue) //Rules 1, 2, 3, 4
					{
						if(nextPassState == D3D12_RESOURCE_STATE_PRESENT) //Rules 2, 3
						{
							needBarrier = (!prevWasPromoted || D3D12Utils::IsStateWriteable(prevPassState));
						}
						else //Rules 1, 4
						{
							needBarrier = false;
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT && nextStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE) //Rules 5, 6, 7, 8
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 6
						{
							needBarrier = false;
						}
						else //Rules 5, 7, 8
						{
							needBarrier = !nextIsPromoted && (prevPassState != nextPassState);
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE && nextStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT) //Rules 9, 10, 11, 12, 13, 14
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 10, 11
						{
							needBarrier = false;
						}
						else //Rules 9, 12, 13, 14
						{
							needBarrier = !nextIsPromoted && (prevPassState != nextPassState) && D3D12Utils::IsStateComputeFriendly(nextPassState);
						}
					}
					else if(nextStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 15, 16
					{
						needBarrier = !prevWasPromoted || D3D12Utils::IsStateWriteable(prevPassState);
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 17, 18
					{
						needBarrier = false;
					}

					if(needBarrier)
					{
						//Swapchain image barriers are changed every frame
						ID3D12Resource2* barrieredTexture = nullptr;
						if(subresourceMetadata.ImageIndex != mGraphToBuild->mBackbufferRefIndex)
						{
							barrieredTexture = mGraphToBuild->mTextures[subresourceMetadata.ImageIndex];
						}

						D3D12_RESOURCE_BARRIER textureTransitionBarrier;
						textureTransitionBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
						textureTransitionBarrier.Transition.pResource   = barrieredTexture;
						textureTransitionBarrier.Transition.Subresource = 0;
						textureTransitionBarrier.Transition.StateBefore = prevPassState;
						textureTransitionBarrier.Transition.StateAfter  = nextPassState;
						textureTransitionBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;

						afterBarriers.push_back(textureTransitionBarrier);

						//Mark the swapchain barriers
						if(barrieredTexture == nullptr)
						{
							afterSwapchainBarrierIndices.push_back(afterBarriers.size() - 1);
						}
					}
				}
			}

			processedMetadataIds.insert(subresourceMetadata.MetadataId);
		}

		uint32_t beforeBarrierBegin = (uint32_t)(mGraphToBuild->mResourceBarriers.size());
		mGraphToBuild->mResourceBarriers.insert(mGraphToBuild->mResourceBarriers.end(), beforeBarriers.begin(), beforeBarriers.end());

		uint32_t afterBarrierBegin = (uint32_t)(mGraphToBuild->mResourceBarriers.size());
		mGraphToBuild->mResourceBarriers.insert(mGraphToBuild->mResourceBarriers.end(), afterBarriers.begin(), afterBarriers.end());

		FrameGraph::BarrierSpan barrierSpan;
		barrierSpan.BeforePassBegin = beforeBarrierBegin;
		barrierSpan.BeforePassEnd   = beforeBarrierBegin + (uint32_t)(beforeBarriers.size());
		barrierSpan.AfterPassBegin  = afterBarrierBegin;
		barrierSpan.AfterPassEnd    = afterBarrierBegin + (uint32_t)(afterBarriers.size());

		mGraphToBuild->mRenderPassBarriers[i] = barrierSpan;

		for(size_t beforeSwapchainBarrierIndex: beforeSwapchainBarrierIndices)
		{
			mGraphToBuild->mSwapchainBarrierIndices.push_back(beforeBarrierBegin + (uint32_t)beforeSwapchainBarrierIndex);
		}

		for(size_t afterSwapchainBarrierIndex: afterSwapchainBarrierIndices)
		{
			mGraphToBuild->mSwapchainBarrierIndices.push_back(afterBarrierBegin + (uint32_t)afterSwapchainBarrierIndex);
		}
	}
}

void ModernFrameGraphBuilder::BuildSubresources()
{
	std::vector<TextureResourceCreateInfo> textureResourceCreateInfos;
	std::vector<TextureResourceCreateInfo> backbufferResourceCreateInfos;
	BuildResourceCreateInfos(textureResourceCreateInfos, backbufferResourceCreateInfos);
	PropagateMetadatas(textureResourceCreateInfos, backbufferResourceCreateInfos);
	
	uint32_t imageCount = PrepareResourceLocations(textureResourceCreateInfos, backbufferResourceCreateInfos);;
	CreateTextures(textureResourceCreateInfos, backbufferResourceCreateInfos, imageCount);

	std::vector<TextureSubresourceCreateInfo> subresourceCreateInfos;
	BuildSubresourceCreateInfos(textureResourceCreateInfos, subresourceCreateInfos);
	BuildSubresourceCreateInfos(backbufferResourceCreateInfos, subresourceCreateInfos);
	CreateTextureViews(subresourceCreateInfos);
}

void ModernFrameGraphBuilder::BuildResourceCreateInfos(std::vector<TextureResourceCreateInfo>& outTextureCreateInfos, std::vector<TextureResourceCreateInfo>& outBackbufferCreateInfos)
{
	std::unordered_set<SubresourceName> processedSubresourceNames;

	TextureResourceCreateInfo backbufferCreateInfo;
	backbufferCreateInfo.Name         = mBackbufferName;
	backbufferCreateInfo.MetadataHead = &mRenderPassesSubresourceMetadatas.at(std::string(PresentPassName)).at(std::string(BackbufferPresentPassId));
	
	outBackbufferCreateInfos.push_back(backbufferCreateInfo);
	processedSubresourceNames.insert(mBackbufferName);

	for(const auto& renderPassName: mRenderPassNames)
	{
		const auto& nameIdMap = mRenderPassesSubresourceNameIds[renderPassName];
		auto& metadataMap     = mRenderPassesSubresourceMetadatas[renderPassName];

		for(const auto& nameId: nameIdMap)
		{
			const SubresourceName subresourceName = nameId.first;
			const SubresourceId   subresourceId   = nameId.second;

			if(!processedSubresourceNames.contains(subresourceName))
			{
				TextureResourceCreateInfo textureCreateInfo;
				textureCreateInfo.Name         = subresourceName;
				textureCreateInfo.MetadataHead = &metadataMap.at(subresourceId);

				outTextureCreateInfos.push_back(textureCreateInfo);
			}
		}
	}
}

void ModernFrameGraphBuilder::BuildSubresourceCreateInfos(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, std::vector<TextureSubresourceCreateInfo>& outTextureViewCreateInfos)
{
	for(const TextureResourceCreateInfo& textureCreateInfo: textureCreateInfos)
	{
		const SubresourceMetadataNode* headNode    = textureCreateInfo.MetadataHead;
		const SubresourceMetadataNode* currentNode = headNode;

		std::unordered_set<uint32_t> uniqueImageViews;
		do
		{
			if(currentNode->FirstFrameViewHandle != (uint32_t)(-1) && !uniqueImageViews.contains(currentNode->FirstFrameViewHandle))
			{
				for(uint32_t frame = 0; frame < currentNode->FrameCount; frame++)
				{
					TextureSubresourceCreateInfo subresourceCreateInfo;
					subresourceCreateInfo.SubresourceInfoIndex = currentNode->SubresourceInfoIndex;
					subresourceCreateInfo.ImageIndex           = currentNode->FirstFrameHandle + frame;
					subresourceCreateInfo.ImageViewIndex       = currentNode->FirstFrameViewHandle + frame;

					outTextureViewCreateInfos.push_back(subresourceCreateInfo);
				}

				uniqueImageViews.insert(currentNode->FirstFrameViewHandle);
			}

			currentNode = currentNode->NextPassNode;

		} while(currentNode != headNode);
	}
}

void ModernFrameGraphBuilder::PropagateMetadatas(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos)
{
	for(const TextureResourceCreateInfo& backbufferCreateInfo: backbufferCreateInfos)
	{
		PropagateMetadatasInResource(backbufferCreateInfo);
	}

	for(const TextureResourceCreateInfo& textureCreateInfo: textureCreateInfos)
	{
		PropagateMetadatasInResource(textureCreateInfo);
	}
}

void ModernFrameGraphBuilder::PropagateMetadatasInResource(const TextureResourceCreateInfo& createInfo)
{
	SubresourceMetadataNode* headNode    = createInfo.MetadataHead;
	SubresourceMetadataNode* currentNode = headNode;

	do
	{
		if(!ValidateSubresourceViewParameters(currentNode))
		{
			//Reset the head node if propagation failed. This may happen if the current pass doesn't know some info about the resource and needs it to be propagated from the pass before
			headNode = currentNode;
		}

		currentNode = currentNode->NextPassNode;

	} while(currentNode != headNode);
}

uint32_t ModernFrameGraphBuilder::PrepareResourceLocations(std::vector<TextureResourceCreateInfo>& textureCreateInfos, std::vector<TextureResourceCreateInfo>& backbufferCreateInfos)
{
	uint32_t imageCount = 0;

	std::array<std::vector<TextureResourceCreateInfo>&, 2> createInfoArrays = {textureCreateInfos, backbufferCreateInfos};
	std::array<uint32_t, createInfoArrays.size()>          backbufferSpan;
	for(size_t i = 0; i < createInfoArrays.size(); i++)
	{
		imageCount += ValidateImageAndViewIndices(textureCreateInfos, imageCount);
		backbufferSpan[i] = imageCount;
	}

	mGraphToBuild->mBackbufferImageSpan.Begin = backbufferSpan[0];
	mGraphToBuild->mBackbufferImageSpan.End   = backbufferSpan[1];

	return imageCount;
}

uint32_t ModernFrameGraphBuilder::ValidateImageAndViewIndices(std::vector<TextureResourceCreateInfo>& textureResourceCreateInfos, uint32_t imageIndexOffset)
{
	uint32_t imageCount = 0;
	for(TextureResourceCreateInfo& textureCreateInfo: textureResourceCreateInfos)
	{
		ValidateImageAndViewIndicesInResource(&textureCreateInfo, imageIndexOffset + imageCount);
		imageCount += textureCreateInfo.MetadataHead->FrameCount;
	}

	return imageCount;
}

void ModernFrameGraphBuilder::ValidateImageAndViewIndicesInResource(TextureResourceCreateInfo* createInfo, uint32_t imageIndex)
{
	std::vector<SubresourceMetadataNode*> resourceMetadataNodes;

	SubresourceMetadataNode* headNode    = createInfo->MetadataHead;
	SubresourceMetadataNode* currentNode = headNode;

	do
	{
		resourceMetadataNodes.push_back(currentNode);
		currentNode = currentNode->NextPassNode;

	} while(currentNode != headNode);

	std::sort(resourceMetadataNodes.begin(), resourceMetadataNodes.end(), [](const SubresourceMetadataNode* left, const SubresourceMetadataNode* right)
	{
		return left->ViewSortKey < right->ViewSortKey;
	});

	//Only assign different image view indices if their ViewSortKey differ
	std::vector<Span<uint32_t>> differentImageViewSpans;
	differentImageViewSpans.push_back({.Begin = 0, .End = 0});

	std::vector<uint64_t> differentSortKeys;
	differentSortKeys.push_back(resourceMetadataNodes[0]->ViewSortKey);
	for(size_t nodeIndex = 1; nodeIndex < resourceMetadataNodes.size(); nodeIndex++)
	{
		uint64_t currSortKey = resourceMetadataNodes[nodeIndex]->ViewSortKey;
		if(currSortKey != differentSortKeys.back())
		{
			differentImageViewSpans.back().End = nodeIndex;
			differentImageViewSpans.push_back({.Begin = nodeIndex, .End = nodeIndex});

			differentSortKeys.push_back(currSortKey);
		}
	}

	differentImageViewSpans.back().End = resourceMetadataNodes.size();

	std::vector<uint32_t> assignedImageViewIds;
	AllocateImageViews(differentSortKeys, headNode->FrameCount, assignedImageViewIds);


	for(size_t spanIndex = 0; spanIndex < differentImageViewSpans.size(); spanIndex++)
	{
		Span<uint32_t> nodeSpan = differentImageViewSpans[spanIndex];
		for(uint32_t nodeIndex = nodeSpan.Begin; nodeIndex < nodeSpan.End; nodeIndex++)
		{
			resourceMetadataNodes[nodeIndex]->FirstFrameHandle     = imageIndex;
			resourceMetadataNodes[nodeIndex]->FirstFrameViewHandle = assignedImageViewIds[spanIndex];
		}
	}
}

void ModernFrameGraphBuilder::TopologicalSortNode(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::unordered_set<RenderPassName>& visited, std::unordered_set<RenderPassName>& onStack, const RenderPassName& renderPassName, std::vector<RenderPassName>& sortedPassNames)
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

bool ModernFrameGraphBuilder::PassesIntersect(const RenderPassName& writingPass, const RenderPassName& readingPass)
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