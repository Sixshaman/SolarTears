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
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, SubresourceMetadataNode>();
	}

	SubresourceMetadataNode metadataNode;
	metadataNode.PrevPassNode         = nullptr;
	metadataNode.NextPassNode         = nullptr;
	metadataNode.SubresourceInfoIndex = AddSubresourceMetadata();
	metadataNode.ImageIndex           = (uint32_t)(-1);
	metadataNode.ImageViewIndex       = (uint32_t)(-1);
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
	metadataNode.ImageIndex           = (uint32_t)(-1);
	metadataNode.ImageViewIndex       = (uint32_t)(-1);
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
		presentAcquireMetadataNode.ImageIndex           = (uint32_t)(-1);
		presentAcquireMetadataNode.ImageViewIndex       = (uint32_t)(-1);
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

	std::unordered_set<RenderPassName> backbufferPassNames; //All passes that use the backbuffer
	FindBackbufferPasses(backbufferPassNames);

	BuildSubresources();
	BuildPassObjects(backbufferPassNames);



	//VULKAN
	BuildBarriers();
	BarrierImages(deviceQueues, workerCommandBuffers, defaultQueueIndex);

	//D3D12
	ValidateCommonPromotion();
	BuildBarriers();
}

const FrameGraphConfig* ModernFrameGraphBuilder::GetConfig() const
{
	return &mGraphToBuild->mFrameGraphConfig;
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

void ModernFrameGraphBuilder::BuildSubresources()
{
	std::vector<TextureResourceCreateInfo> textureResourceCreateInfos;
	std::vector<TextureResourceCreateInfo> backbufferResourceCreateInfos;
	BuildResourceCreateInfos(textureResourceCreateInfos, backbufferResourceCreateInfos);
	PropagateMetadatas(textureResourceCreateInfos, backbufferResourceCreateInfos);

	std::vector<uint32_t> viewSubresourceInfoIndices;
	ValidateImageIndices(textureResourceCreateInfos, viewSubresourceInfoIndices);

	CreateTextures(textureResourceCreateInfos);
	SetSwapchainImages(backbufferResourceCreateInfos, viewSubresourceInfoIndices);

	CreateTextureViews(textureResourceCreateInfos, viewSubresourceInfoIndices);
	CreateSwapchainImageViews(backbufferResourceCreateInfos);
}

void ModernFrameGraphBuilder::BuildResourceCreateInfos(std::vector<TextureResourceCreateInfo>& outTextureCreateInfos, std::vector<TextureResourceCreateInfo>& outBackbufferCreateInfos, std::vector<uint32_t>& outViewSubresourceInfoIndices)
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
	const SubresourceMetadataNode* headNode    = createInfo.MetadataHead;
	const SubresourceMetadataNode* currentNode = headNode;

	do
	{
		if(!PropagateSubresourceParameters(currentNode->SubresourceInfoIndex, currentNode->NextPassNode->SubresourceInfoIndex))
		{
			//Reset the head node if propagation failed. This may happen if the current pass doesn't know some info about the resource and needs it to be propagated from the pass before
			headNode = currentNode;
		}

		currentNode = currentNode->NextPassNode;

	} while(currentNode != headNode);
}

void ModernFrameGraphBuilder::ValidateImageIndices(std::vector<TextureResourceCreateInfo>& textureResourceCreateInfos, std::vector<uint32_t>& inoutViewSubresourceInfoIndices)
{
	for(uint32_t imageIndex = 0; imageIndex < (uint32_t)textureResourceCreateInfos.size(); imageIndex++)
	{
		ValidateImageIndicesInResource(&textureResourceCreateInfos[imageIndex], imageIndex, inoutViewSubresourceInfoIndices);
	}
}

void ModernFrameGraphBuilder::ValidateImageIndicesInResource(TextureResourceCreateInfo* createInfo, uint32_t imageIndex, std::vector<uint32_t>& inoutViewSubresourceInfoIndices)
{
	std::vector<SubresourceMetadataNode*> resourceMetadataNodes;
	std::vector<uint32_t>                 subresourceInfoIndices;

	SubresourceMetadataNode* headNode    = createInfo->MetadataHead;
	SubresourceMetadataNode* currentNode = headNode;

	do
	{
		resourceMetadataNodes.push_back(currentNode);
		subresourceInfoIndices.push_back(currentNode->SubresourceInfoIndex);

		currentNode = currentNode->NextPassNode;

	} while(currentNode != headNode);

	std::vector<uint32_t> uniqueSubresourceInfoIndices;
	std::vector<uint32_t> indexRemap;
	BuildUniqueSubresourceList(subresourceInfoIndices, uniqueSubresourceInfoIndices, indexRemap);

	uint32_t imageViewIndexStart = (uint32_t)inoutViewSubresourceInfoIndices.size();
	for (size_t i = 0; i < uniqueSubresourceInfoIndices.size(); i++)
	{
		inoutViewSubresourceInfoIndices.push_back(uniqueSubresourceInfoIndices[i]);
	}

	createInfo->SubresourceSpan.FirstSubresourceInfoIndex = imageViewIndexStart;
	createInfo->SubresourceSpan.LastSubresourceInfoIndex  = (uint32_t)inoutViewSubresourceInfoIndices.size();

	for(size_t i = 0; i < subresourceInfoIndices.size(); i++)
	{
		resourceMetadataNodes[i]->ImageIndex     = imageIndex;
		resourceMetadataNodes[i]->ImageViewIndex = imageViewIndexStart + indexRemap[i];
	}
}

void ModernFrameGraphBuilder::SetSwapchainImages(std::vector<TextureResourceCreateInfo>& backbufferResourceCreateInfos, std::vector<uint32_t>& inoutViewSubresourceInfoIndices)
{
	mGraphToBuild->mBackbufferImageIndex = AllocateBackbufferResources();
	for(TextureResourceCreateInfo& backbufferCreateInfo: backbufferResourceCreateInfos)
	{
		ValidateImageIndicesInResource(&backbufferCreateInfo, mGraphToBuild->mBackbufferImageIndex, inoutViewSubresourceInfoIndices);
	}
}

void ModernFrameGraphBuilder::CreateSwapchainImageViews(std::vector<TextureResourceCreateInfo>& backbufferResourceCreateInfos)
{
	for(const TextureResourceCreateInfo& backbufferResourceCreateInfo: backbufferResourceCreateInfos)
	{
		for(size_t j = 0; j < nameWithCreateInfo.second.ImageViewInfos.size(); j++)
		{
			const ImageViewInfo& imageViewInfo = nameWithCreateInfo.second.ImageViewInfos[j];

			//Base image view is NULL (it will be assigned from mSwapchainImageViews each frame)
			mGraphToBuild->mImageViews.push_back(VK_NULL_HANDLE);

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