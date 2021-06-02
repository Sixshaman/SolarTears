#include "ModernFrameGraphBuilder.hpp"
#include "ModernFrameGraph.hpp"
#include <algorithm>
#include <cassert>

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
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, TextureSubresourceMetadata>();
	}

	TextureSubresourceMetadata subresourceMetadata;
	subresourceMetadata.PrevPassMetadata = nullptr;
	subresourceMetadata.NextPassMetadata = nullptr;
	subresourceMetadata.ImageIndex       = (uint32_t)(-1);
	subresourceMetadata.ImageViewIndex   = (uint32_t)(-1);
	subresourceMetadata.ViewType         = SubresourceViewType::Unknown;
	subresourceMetadata.PassType         = RenderPassType::Graphics;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = subresourceMetadata;
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
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, TextureSubresourceMetadata>();
	}

	TextureSubresourceMetadata subresourceMetadata;
	subresourceMetadata.PrevPassMetadata = nullptr;
	subresourceMetadata.NextPassMetadata = nullptr;
	subresourceMetadata.ImageIndex       = (uint32_t)(-1);
	subresourceMetadata.ImageViewIndex   = (uint32_t)(-1);
	subresourceMetadata.ViewType         = SubresourceViewType::Unknown;
	subresourceMetadata.PassType         = RenderPassType::Graphics;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = subresourceMetadata;
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

	TextureSubresourceMetadata presentAcquirePassMetadata;
	presentAcquirePassMetadata.PrevPassMetadata = nullptr;
	presentAcquirePassMetadata.NextPassMetadata = nullptr;
	presentAcquirePassMetadata.ImageIndex       = (uint32_t)(-1);
	presentAcquirePassMetadata.ImageViewIndex   = (uint32_t)(-1);
	presentAcquirePassMetadata.ViewType         = SubresourceViewType::Unknown;
	presentAcquirePassMetadata.PassType         = RenderPassType::Present;

	mRenderPassesSubresourceMetadatas[std::string(PresentPassName)][std::string(BackbufferPresentPassId)] = presentAcquirePassMetadata;
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


	//VULKAN
	std::unordered_set<RenderPassName> backbufferPassNames; //All passes that use the backbuffer
	BuildSubresources(memoryAllocator, swapchainImages, backbufferPassNames, swapChain->GetBackbufferFormat(), defaultQueueIndex);
	BuildPassObjects(backbufferPassNames);

	BuildBarriers();

	BarrierImages(deviceQueues, workerCommandBuffers, defaultQueueIndex);



	//D3D12
	ValidateCommonPromotion();

	std::unordered_set<RenderPassName> backbufferPassNames; //All passes that use the backbuffer
	BuildSubresources(device, memoryAllocator, swapchainImages, backbufferPassNames);
	BuildPassObjects(device, backbufferPassNames);

	BuildBarriers();
}

void ModernFrameGraphBuilder::BuildAdjacencyList(std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList)
{
	adjacencyList.clear();

	for (const RenderPassName& renderPassName : mRenderPassNames)
	{
		std::unordered_set<RenderPassName> renderPassAdjacentPasses;
		for(const RenderPassName& otherPassName : mRenderPassNames)
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
	mGraphToBuild->mGraphicsPassSpans.push_back(ModernFrameGraph::DependencyLevelSpan{.DependencyLevelBegin = 0, .DependencyLevelEnd = 0});
	for(size_t passIndex = 0; passIndex < mRenderPassNames.size(); passIndex++)
	{
		uint32_t dependencyLevel = mRenderPassDependencyLevels[mRenderPassNames[passIndex]];
		if(currentDependencyLevel != dependencyLevel)
		{
			mGraphToBuild->mGraphicsPassSpans.push_back(ModernFrameGraph::DependencyLevelSpan{.DependencyLevelBegin = dependencyLevel, .DependencyLevelEnd = dependencyLevel + 1});
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
			SubresourceName             subresourceName     = subresourceNameId.first;
			TextureSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);

			if(subresourceLastRenderPasses.contains(subresourceName))
			{
				RenderPassName prevPassName = subresourceLastRenderPasses.at(subresourceName);
				
				SubresourceId               prevSubresourceId       = mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
				TextureSubresourceMetadata& prevSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(prevPassName).at(prevSubresourceId);

				prevSubresourceMetadata.NextPassMetadata = &subresourceMetadata;
				subresourceMetadata.PrevPassMetadata     = &prevSubresourceMetadata;
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

	TextureSubresourceMetadata& backbufferFirstSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(backbufferFirstPassName).at(backbufferFirstSubresourceId);
	TextureSubresourceMetadata& backbufferLastSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(backbufferLastPassName).at(backbufferLastSubresourceId);

	TextureSubresourceMetadata& backbufferPresentSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(std::string(PresentPassName)).at(std::string(BackbufferPresentPassId));

	backbufferFirstSubresourceMetadata.PrevPassMetadata = &backbufferPresentSubresourceMetadata;
	backbufferLastSubresourceMetadata.NextPassMetadata  = &backbufferPresentSubresourceMetadata;

	backbufferPresentSubresourceMetadata.PrevPassMetadata = &backbufferLastSubresourceMetadata;
	backbufferPresentSubresourceMetadata.NextPassMetadata = &backbufferFirstSubresourceMetadata;


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

			TextureSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);

			if(subresourceMetadata.PrevPassMetadata == nullptr)
			{
				SubresourceName subresourceName = subresourceNameId.first;

				RenderPassName prevPassName = subresourceLastRenderPasses.at(subresourceName);

				SubresourceId               prevSubresourceId       = mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
				TextureSubresourceMetadata& prevSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(prevPassName).at(prevSubresourceId);

                //The barriers should be circular
				subresourceMetadata.PrevPassMetadata     = &prevSubresourceMetadata; //Barrier from the last state at the beginning of frame
				prevSubresourceMetadata.NextPassMetadata = &subresourceMetadata;
			}

			if(subresourceMetadata.NextPassMetadata == nullptr)
			{
				SubresourceName subresourceName = subresourceNameId.first;

				RenderPassName nextPassName = subresourceFirstRenderPasses.at(subresourceName);

				SubresourceId               nextSubresourceId       = mRenderPassesSubresourceNameIds.at(nextPassName).at(subresourceName);
				TextureSubresourceMetadata& nextSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(nextPassName).at(nextSubresourceId);

                //The barriers should be circular
				subresourceMetadata.NextPassMetadata     = &nextSubresourceMetadata; //Barrier from the last state at the beginning of frame
				nextSubresourceMetadata.PrevPassMetadata = &subresourceMetadata;
			}
		}
	}
}

void ModernFrameGraphBuilder::ValidateSubresourcePassTypes()
{
	//Pass types should be known
	assert(!mRenderPassTypes.empty());

	std::string presentPassName(PresentPassName);

	for (auto& subresourceNameId : mRenderPassesSubresourceNameIds[presentPassName])
	{
		TextureSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(presentPassName).at(subresourceNameId.second);
		subresourceMetadata.PassType = mRenderPassTypes[presentPassName];
	}

	for (size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for (auto& subresourceNameId : mRenderPassesSubresourceNameIds[renderPassName])
		{
			TextureSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);
			subresourceMetadata.PassType = mRenderPassTypes[renderPassName];
		}
	}
}

void ModernFrameGraphBuilder::BuildSubresources(std::unordered_set<RenderPassName>& swapchainPassNames)
{
	std::unordered_map<SubresourceName, TextureResourceCreateInfo>    textureResourceCreateInfos;
	std::unordered_map<SubresourceName, BackbufferResourceCreateInfo> backbufferResourceCreateInfos;
	BuildResourceCreateInfos(textureResourceCreateInfos, backbufferResourceCreateInfos, swapchainPassNames);

	PropagateMetadatasFromImageViews(textureResourceCreateInfos, backbufferResourceCreateInfos);

	CreateImages(textureResourceCreateInfos);
	CreateImageViews(textureResourceCreateInfos);

	SetSwapchainImages(backbufferResourceCreateInfos, swapchainImages);
	CreateSwapchainImageViews(backbufferResourceCreateInfos, swapchainFormat);
}

void ModernFrameGraphBuilder::MergeImageViewInfos(std::unordered_map<SubresourceName, TextureResourceCreateInfo>& inoutImageResourceCreateInfos)
{
	//Fix all "arbitrary" image view infos
	for(auto& nameWithCreateInfo: inoutImageResourceCreateInfos)
	{
		for(int i = 0; i < nameWithCreateInfo.second.ImageViewInfos.size(); i++)
		{
			if(nameWithCreateInfo.second.ImageViewInfos[i].Format == SubresourceFormat::Unknown)
			{
				//Assign the closest non-UNDEFINED format
				for(int j = i + 1; j < (int)nameWithCreateInfo.second.ImageViewInfos.size(); j++)
				{
					if(nameWithCreateInfo.second.ImageViewInfos[j].Format != SubresourceFormat::Unknown)
					{
						nameWithCreateInfo.second.ImageViewInfos[i].Format = nameWithCreateInfo.second.ImageViewInfos[j].Format;
						break;
					}
				}

				//Still UNDEFINED? Try to find the format in elements before
				if(nameWithCreateInfo.second.ImageViewInfos[i].Format == SubresourceFormat::Unknown)
				{
					for(int j = i - 1; j >= 0; j++)
					{
						if(nameWithCreateInfo.second.ImageViewInfos[j].Format != SubresourceFormat::Unknown)
						{
							nameWithCreateInfo.second.ImageViewInfos[i].Format = nameWithCreateInfo.second.ImageViewInfos[j].Format;
							break;
						}
					}
				}
			}

			assert(nameWithCreateInfo.second.ImageViewInfos[i].Format != SubresourceFormat::Unknown);
		}

		//Merge all of the same image views into one
		std::sort(nameWithCreateInfo.second.ImageViewInfos.begin(), nameWithCreateInfo.second.ImageViewInfos.end(), [](const TextureViewInfo& left, const TextureViewInfo& right)
		{
			return (uint32_t)left.Format < (uint32_t)right.Format;
		});

		std::vector<TextureViewInfo> newImageViewInfos;

		SubresourceFormat currentFormat = SubresourceFormat::Unknown;
		for(size_t i = 0; i < nameWithCreateInfo.second.ImageViewInfos.size(); i++)
		{
			if(currentFormat != nameWithCreateInfo.second.ImageViewInfos[i].Format)
			{
				TextureViewInfo textureViewInfo;
				textureViewInfo.Format = nameWithCreateInfo.second.ImageViewInfos[i].Format;
				textureViewInfo.ViewAddresses.push_back(
				{
					.PassName = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[0].PassName, 
					.SubresId = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[0].SubresId
				});

				newImageViewInfos.push_back(textureViewInfo);
			}
			else
			{
				newImageViewInfos.back().ViewAddresses.push_back({.PassName = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[0].PassName, .SubresId = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[0].SubresId});
			}

			currentFormat = nameWithCreateInfo.second.ImageViewInfos[i].Format;
		}

		nameWithCreateInfo.second.ImageViewInfos = newImageViewInfos;
	}
}

void ModernFrameGraphBuilder::PropagateMetadatasFromImageViews(const std::unordered_map<SubresourceName, TextureResourceCreateInfo>& imageCreateInfos, const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferCreateInfos)
{
	for(auto& nameWithCreateInfo: imageCreateInfos)
	{
		for(size_t i = 0; i < nameWithCreateInfo.second.ImageViewInfos.size(); i++)
		{
			SubresourceFormat format = nameWithCreateInfo.second.ImageViewInfos[i].Format;
			for(size_t j = 0; j < nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses.size(); j++)
			{
				const SubresourceAddress& address = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[j];
				mRenderPassesSubresourceMetadatas.at(address.PassName).at(address.SubresId).MainFormat = format;
			}
		}
	}

	for(auto& nameWithCreateInfo: backbufferCreateInfos)
	{
		for(size_t i = 0; i < nameWithCreateInfo.second.ImageViewInfos.size(); i++)
		{
			SubresourceFormat format = nameWithCreateInfo.second.ImageViewInfos[i].Format;
			for(size_t j = 0; j < nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses.size(); j++)
			{
				const SubresourceAddress& address = nameWithCreateInfo.second.ImageViewInfos[i].ViewAddresses[j];
				mRenderPassesSubresourceMetadatas.at(address.PassName).at(address.SubresId).MainFormat = format;
			}
		}
	}
}

void ModernFrameGraphBuilder::BuildResourceCreateInfos(std::unordered_map<SubresourceName, TextureResourceCreateInfo>& outImageCreateInfos, std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& outBackbufferCreateInfos, std::unordered_set<RenderPassName>& swapchainPassNames)
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
				TextureSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas[renderPassName][subresourceId];

				BackbufferResourceCreateInfo backbufferCreateInfo;

				TextureViewInfo textureViewInfo;
				textureViewInfo.Format = metadata.Format;
				textureViewInfo.ViewAddresses.push_back({.PassName = renderPassName, .SubresId = subresourceId});

				backbufferCreateInfo.ImageViewInfos.push_back(textureViewInfo);

				outBackbufferCreateInfos[subresourceName] = backbufferCreateInfo;

				swapchainPassNames.insert(renderPassName);
			}
			else
			{
				TextureSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas[renderPassName][subresourceId];
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
				if(metadata.Format != SubresourceFormat::Unknown)
				{
					if(imageResourceCreateInfo.ImageCreateInfo.format == VK_FORMAT_UNDEFINED)
					{
						//Not all passes specify the format
						imageResourceCreateInfo.ImageCreateInfo.format = metadata.Format;
					}
				}

				//It's fine if format is Unknow. It means it can be arbitrary and we'll fix it later based on other image view infos for this resource
				TextureViewInfo textureViewInfo;
				textureViewInfo.Format = metadata.Format;
				textureViewInfo.ViewAddresses.push_back({.PassName = renderPassName, .SubresId = subresourceId});

				imageResourceCreateInfo.ImageViewInfos.push_back(textureViewInfo);

				imageResourceCreateInfo.ImageCreateInfo.usage |= metadata.UsageFlags;
			}
		}
	}

	MergeImageViewInfos(outImageCreateInfos);
}

void ModernFrameGraphBuilder::CreateImageViews(const std::unordered_map<SubresourceName, ImageResourceCreateInfo>& imageResourceCreateInfos)
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

void ModernFrameGraphBuilder::CreateSwapchainImageViews(const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferResourceCreateInfos, SubresourceFormat swapchainFormat)
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