#include "ModernFrameGraphBuilder.hpp"
#include "ModernFrameGraph.hpp"
#include <algorithm>
#include <cassert>
#include <array>
#include <numeric>

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
	metadataNode.Flags                = 0;
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
	metadataNode.Flags                = 0;
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
		presentAcquireMetadataNode.Flags                = 0;
		presentAcquireMetadataNode.PassType             = RenderPassType::Graphics;

		presentPassMetadataMap[backbufferIdString] = presentAcquireMetadataNode;
	}
}

void ModernFrameGraphBuilder::EnableSubresourceAutoBeforeBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBarrier)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(autoBarrier)
	{
		metadataNode.Flags |= TextureFlagAutoBeforeBarrier;
	}
	else
	{
		metadataNode.Flags &= ~TextureFlagAutoBeforeBarrier;
	}
}

void ModernFrameGraphBuilder::EnableSubresourceAutoAfterBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBarrier)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(autoBarrier)
	{
		metadataNode.Flags |= TextureFlagAutoAfterBarrier;
	}
	else
	{
		metadataNode.Flags &= ~TextureFlagAutoAfterBarrier;
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
	CalculatePassPeriods();

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

const Span<uint32_t> ModernFrameGraphBuilder::GetBackbufferImageSpan() const
{
	return mGraphToBuild->mBackbufferImageSpan;
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

void ModernFrameGraphBuilder::CalculatePassPeriods()
{
	for(const RenderPassName& passName: mRenderPassNames)
	{
		uint32_t passPeriod = 1;

		const auto& nameIdMap   = mRenderPassesSubresourceNameIds.at(passName);
		const auto& metadataMap = mRenderPassesSubresourceMetadatas.at(passName);
		for(const auto& subresourceNameId: nameIdMap)
		{
			if(subresourceNameId.first != mBackbufferName) //Swapchain images don't count as render pass own period
			{
				const SubresourceMetadataNode& metadataNode = metadataMap.at(subresourceNameId.second);
				passPeriod = std::lcm(metadataNode.FrameCount, passPeriod);
			}
		}

		mRenderPassOwnPeriods[passName] = passPeriod;
	}
}

void ModernFrameGraphBuilder::BuildPassObjects()
{
	for(const std::string& passName: mRenderPassNames)
	{
		uint32_t passSwapchainImageCount = 1;
		if(mRenderPassesSubresourceNameIds[passName].contains(mBackbufferName))
		{
			passSwapchainImageCount = GetSwapchainImageCount();
		}

		uint32_t passPeriod = mRenderPassOwnPeriods.at(passName);

		ModernFrameGraph::RenderPassSpanInfo passSpanInfo;
		passSpanInfo.PassSpanBegin = NextPassSpanId();
		passSpanInfo.OwnFrames     = passPeriod;

		mGraphToBuild->mPassFrameSpans.push_back(passSpanInfo);
		for(uint32_t swapchainImageIndex = 0; swapchainImageIndex < passSwapchainImageCount; swapchainImageIndex++)
		{
			for(uint32_t passFrame = 0; passFrame < passPeriod; passFrame++)
			{
				uint32_t frame = passPeriod * swapchainImageIndex + passFrame;
				AddRenderPass(passName, frame);
			}
		}
	}
}

void ModernFrameGraphBuilder::BuildBarriers()
{
	//COMMON
	mGraphToBuild->mRenderPassBarriers.resize(mRenderPassNames.size());
	mGraphToBuild->mMultiframeBarrierInfos.clear();

	uint32_t currentBarrierCount = 0;
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];

		const auto& nameIdMap     = mRenderPassesSubresourceNameIds.at(renderPassName);
		const auto& idMetadataMap = mRenderPassesSubresourceMetadatas.at(renderPassName);

		std::vector<SubresourceMetadataNode*> passMetadatasForBeforeBarriers;
		std::vector<SubresourceMetadataNode*> passMetadatasForAfterBarriers;
		for(auto& subresourceNameId: nameIdMap)
		{
			SubresourceMetadataNode& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);
			
			if(!(subresourceMetadata.Flags & TextureFlagAutoBeforeBarrier))
			{
				passMetadatasForBeforeBarriers.push_back(&subresourceMetadata);
			}

			if(!(subresourceMetadata.Flags & TextureFlagAutoAfterBarrier))
			{
				passMetadatasForAfterBarriers.push_back(&subresourceMetadata);
			}
		}

		ModernFrameGraph::BarrierSpan barrierSpan;
		barrierSpan.BeforePassBegin = currentBarrierCount;

		for(SubresourceMetadataNode* subresourceMetadata: passMetadatasForBeforeBarriers)
		{
			SubresourceMetadataNode* prevSubresourceMetadata = subresourceMetadata->PrevPassNode;
			
			uint32_t barrierId = AddBeforePassBarrier(subresourceMetadata->FirstFrameHandle, prevSubresourceMetadata->PassType, prevSubresourceMetadata->SubresourceInfoIndex, subresourceMetadata->PassType, subresourceMetadata->SubresourceInfoIndex);
			if(barrierId != (uint32_t)(-1))
			{
				currentBarrierCount++;

				if(subresourceMetadata->FrameCount > 1)
				{
					ModernFrameGraph::MultiframeBarrierInfo multiframeBarrierInfo;
					multiframeBarrierInfo.BarrierIndex  = barrierId;
					multiframeBarrierInfo.BaseTexIndex  = subresourceMetadata->FirstFrameHandle;
					multiframeBarrierInfo.TexturePeriod = subresourceMetadata->FrameCount;

					mGraphToBuild->mMultiframeBarrierInfos.push_back(multiframeBarrierInfo);
				}
			}
		}

		barrierSpan.BeforePassEnd  = currentBarrierCount;
		barrierSpan.AfterPassBegin = currentBarrierCount;

		for(SubresourceMetadataNode* subresourceMetadata: passMetadatasForBeforeBarriers)
		{
			SubresourceMetadataNode* nextSubresourceMetadata = subresourceMetadata->NextPassNode;
			
			uint32_t barrierId = AddAfterPassBarrier(subresourceMetadata->FirstFrameHandle, subresourceMetadata->PassType, subresourceMetadata->SubresourceInfoIndex, nextSubresourceMetadata->PassType, nextSubresourceMetadata->SubresourceInfoIndex);
			if(barrierId != (uint32_t)(-1))
			{
				currentBarrierCount++;

				if(subresourceMetadata->FrameCount > 1)
				{
					ModernFrameGraph::MultiframeBarrierInfo multiframeBarrierInfo;
					multiframeBarrierInfo.BarrierIndex  = barrierId;
					multiframeBarrierInfo.BaseTexIndex  = subresourceMetadata->FirstFrameHandle;
					multiframeBarrierInfo.TexturePeriod = subresourceMetadata->FrameCount;

					mGraphToBuild->mMultiframeBarrierInfos.push_back(multiframeBarrierInfo);
				}
			}
		}

		mGraphToBuild->mRenderPassBarriers[i] = barrierSpan;
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
			differentImageViewSpans.push_back(Span<uint32_t>{.Begin = (uint32_t)nodeIndex, .End = (uint32_t)nodeIndex});

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