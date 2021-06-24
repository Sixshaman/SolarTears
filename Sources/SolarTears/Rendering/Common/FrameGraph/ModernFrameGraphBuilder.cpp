#include "ModernFrameGraphBuilder.hpp"
#include "ModernFrameGraph.hpp"
#include <algorithm>
#include <cassert>
#include <array>
#include <numeric>

ModernFrameGraphBuilder::ModernFrameGraphBuilder(ModernFrameGraph* graphToBuild, FrameGraphDescription&& frameGraphDescription): mGraphToBuild(graphToBuild), mFrameGraphDescription(frameGraphDescription)
{
}

ModernFrameGraphBuilder::~ModernFrameGraphBuilder()
{
}

void ModernFrameGraphBuilder::RegisterRenderPass(RenderPassType passType, const std::string_view passName)
{
	FrameGraphDescription::RenderPassName renderPassName(passName);
	mRenderPassClasses[renderPassName] = mRenderPassClassTable[passType];

	RegisterPassSubresources(passType, renderPassName);
}

void ModernFrameGraphBuilder::RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	FrameGraphDescription::RenderPassName renderPassName(passName);

	if(!mRenderPassesReadSubresourceIds.contains(renderPassName))
	{
		mRenderPassesReadSubresourceIds[renderPassName] = std::unordered_set<FrameGraphDescription::SubresourceId>();
	}

	FrameGraphDescription::SubresourceId subresId(subresourceId);
	assert(!mRenderPassesReadSubresourceIds[renderPassName].contains(subresId));

	mRenderPassesReadSubresourceIds[renderPassName].insert(subresId);

	if(!mRenderPassesSubresourceMetadatas.contains(renderPassName))
	{
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<FrameGraphDescription::SubresourceId, SubresourceMetadataNode>();
	}

	SubresourceMetadataNode metadataNode;
	metadataNode.PrevPassNode         = nullptr;
	metadataNode.NextPassNode         = nullptr;
	metadataNode.SubresourceInfoIndex = AddSubresourceMetadata();
	metadataNode.FirstFrameHandle     = (uint32_t)(-1);
	metadataNode.FirstFrameViewHandle = (uint32_t)(-1);
	metadataNode.FrameCount           = 1;
	metadataNode.Flags                = 0;
	metadataNode.PassClass            = RenderPassClass::Graphics;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = metadataNode;
}

void ModernFrameGraphBuilder::RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	FrameGraphDescription::RenderPassName renderPassName(passName);

	if(!mRenderPassesWriteSubresourceIds.contains(renderPassName))
	{
		mRenderPassesWriteSubresourceIds[renderPassName] = std::unordered_set<FrameGraphDescription::SubresourceId>();
	}

	FrameGraphDescription::SubresourceId subresId(subresourceId);
	assert(!mRenderPassesWriteSubresourceIds[renderPassName].contains(subresId));

	mRenderPassesWriteSubresourceIds[renderPassName].insert(subresId);

	if(!mRenderPassesSubresourceMetadatas.contains(renderPassName))
	{
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<FrameGraphDescription::SubresourceId, SubresourceMetadataNode>();
	}

	SubresourceMetadataNode metadataNode;
	metadataNode.PrevPassNode         = nullptr;
	metadataNode.NextPassNode         = nullptr;
	metadataNode.SubresourceInfoIndex = AddSubresourceMetadata();
	metadataNode.FirstFrameHandle     = (uint32_t)(-1);
	metadataNode.FirstFrameViewHandle = (uint32_t)(-1);
	metadataNode.FrameCount           = 1;
	metadataNode.Flags                = 0;
	metadataNode.PassClass            = RenderPassClass::Graphics;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = metadataNode;
}

void ModernFrameGraphBuilder::EnableSubresourceAutoBeforeBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBarrier)
{
	FrameGraphDescription::RenderPassName passNameStr(passName);
	FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

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
	FrameGraphDescription::RenderPassName passNameStr(passName);
	FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

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
	CreatePresentPass();
	for(const auto& passNameType: mFrameGraphDescription.mRenderPassTypes)
	{
		RegisterRenderPass(passNameType.second, passNameType.first);
	}

	std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::RenderPassName>> adjacencyList;
	BuildAdjacencyList(adjacencyList);

	BuildSortedRenderPassesTopological(adjacencyList);
	BuildSortedRenderPassesDependency(adjacencyList);

	BuildDependencyLevels();
	AdjustPassClasses();
	CalculatePassPeriods();

	ValidateSubresourceLinks();
	ValidateSubresourcePassClasses();

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

void ModernFrameGraphBuilder::CreatePresentPass()
{
	//Create present quasi-pass
	FrameGraphDescription::RenderPassName presentPassName(FrameGraphDescription::PresentPassName);
	FrameGraphDescription::SubresourceId  backbufferId(FrameGraphDescription::BackbufferPresentPassId);

	mRenderPassClasses[presentPassName] = RenderPassClass::Present;

	mRenderPassesReadSubresourceIds[presentPassName] = std::unordered_set<FrameGraphDescription::SubresourceId>();
	mRenderPassesReadSubresourceIds[presentPassName].insert(backbufferId);

	mRenderPassesWriteSubresourceIds[presentPassName] = std::unordered_set<FrameGraphDescription::SubresourceId>();
	mRenderPassesWriteSubresourceIds[presentPassName].insert(backbufferId);


	SubresourceMetadataNode presentAcquireMetadataNode;
	presentAcquireMetadataNode.PrevPassNode         = nullptr;
	presentAcquireMetadataNode.NextPassNode         = nullptr;
	presentAcquireMetadataNode.SubresourceInfoIndex = AddPresentSubresourceMetadata();
	presentAcquireMetadataNode.FirstFrameHandle     = (uint32_t)(-1);
	presentAcquireMetadataNode.FirstFrameViewHandle = (uint32_t)(-1);
	presentAcquireMetadataNode.FrameCount           = GetSwapchainImageCount();
	presentAcquireMetadataNode.Flags                = 0;
	presentAcquireMetadataNode.PassClass            = RenderPassClass::Present;

	std::unordered_map<FrameGraphDescription::SubresourceId, SubresourceMetadataNode> presentPassMetadataMap;
	presentPassMetadataMap[backbufferId] = presentAcquireMetadataNode;

	mRenderPassesSubresourceMetadatas[presentPassName] = presentPassMetadataMap;
}

void ModernFrameGraphBuilder::BuildAdjacencyList(std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::RenderPassName>>& adjacencyList)
{
	adjacencyList.clear();

	for (const FrameGraphDescription::RenderPassName& renderPassName: mFrameGraphDescription.mRenderPassNames)
	{
		std::unordered_set<FrameGraphDescription::RenderPassName> renderPassAdjacentPasses;
		for(const FrameGraphDescription::RenderPassName& otherPassName: mFrameGraphDescription.mRenderPassNames)
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

void ModernFrameGraphBuilder::BuildSortedRenderPassesTopological(const std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::RenderPassName>>& adjacencyList)
{
	mSortedRenderPassNames.clear();
	mSortedRenderPassNames.reserve(mFrameGraphDescription.mRenderPassNames.size());

	std::unordered_set<FrameGraphDescription::RenderPassName> visited;
	std::unordered_set<FrameGraphDescription::RenderPassName> onStack;

	for(const FrameGraphDescription::RenderPassName& renderPassName: mFrameGraphDescription.mRenderPassNames)
	{
		TopologicalSortNode(adjacencyList, visited, onStack, renderPassName);
	}

	for(size_t i = 0; i < mSortedRenderPassNames.size() / 2; i++) //Reverse the order
	{
		std::swap(mSortedRenderPassNames[i], mSortedRenderPassNames[mSortedRenderPassNames.size() - i - 1]);
	}
}

void ModernFrameGraphBuilder::BuildSortedRenderPassesDependency(const std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::RenderPassName>>& adjacencyList)
{
	mRenderPassDependencyLevels.clear();

	for(const FrameGraphDescription::RenderPassName& renderPassName: mSortedRenderPassNames)
	{
		mRenderPassDependencyLevels[renderPassName] = 0;
	}

	for(const FrameGraphDescription::RenderPassName& renderPassName: mSortedRenderPassNames)
	{
		uint32_t& mainPassDependencyLevel = mRenderPassDependencyLevels[renderPassName];

		const auto& adjacentPassNames = adjacencyList.at(renderPassName);
		for(const FrameGraphDescription::RenderPassName& adjacentPassName: adjacentPassNames)
		{
			uint32_t& adjacentPassDependencyLevel = mRenderPassDependencyLevels[adjacentPassName];

			if(adjacentPassDependencyLevel < mainPassDependencyLevel + 1)
			{
				adjacentPassDependencyLevel = mainPassDependencyLevel + 1;
			}
		}
	}

	std::stable_sort(mSortedRenderPassNames.begin(), mSortedRenderPassNames.end(), [this](const FrameGraphDescription::RenderPassName& left, const FrameGraphDescription::RenderPassName& right)
	{
		return mRenderPassDependencyLevels[left] < mRenderPassDependencyLevels[right];
	});
}

void ModernFrameGraphBuilder::BuildDependencyLevels()
{
	mGraphToBuild->mGraphicsPassSpans.clear();

	uint32_t currentDependencyLevel = 0;
	mGraphToBuild->mGraphicsPassSpans.push_back(Span<uint32_t>{.Begin = 0, .End = 0});
	for(size_t passIndex = 0; passIndex < mSortedRenderPassNames.size(); passIndex++)
	{
		uint32_t dependencyLevel = mRenderPassDependencyLevels[mSortedRenderPassNames[passIndex]];
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
}

void ModernFrameGraphBuilder::AdjustPassClasses()
{
	//The loop will wrap around 0
	size_t renderPassNameIndex = mSortedRenderPassNames.size() - 1;
	while(renderPassNameIndex < mSortedRenderPassNames.size())
	{
		//Graphics queue only for now
		//Async compute passes should be in the end of the frame. BUT! They should be executed AT THE START of THE PREVIOUS frame. Need to reorder stuff for that
		FrameGraphDescription::RenderPassName renderPassName = mSortedRenderPassNames[renderPassNameIndex];

		mRenderPassClasses[renderPassName] = RenderPassClass::Graphics;
		renderPassNameIndex--;
	}

	mRenderPassClasses[std::string(FrameGraphDescription::PresentPassName)] = RenderPassClass::Present;
}

void ModernFrameGraphBuilder::ValidateSubresourceLinks()
{
	std::unordered_map<FrameGraphDescription::SubresourceName, FrameGraphDescription::RenderPassName> subresourceFirstRenderPasses; //The fisrt pass for each subresource
	std::unordered_map<FrameGraphDescription::SubresourceName, FrameGraphDescription::RenderPassName> subresourceLastRenderPasses;  //The last pass for each subresource

	//Connect previous passes
	for(size_t i = 0; i < mSortedRenderPassNames.size(); i++)
	{
		FrameGraphDescription::RenderPassName renderPassName = mSortedRenderPassNames[i];
		for(auto& subresourceNameId: mFrameGraphDescription.mRenderPassesSubresourceNameIds[renderPassName])
		{
			FrameGraphDescription::SubresourceName subresourceName = subresourceNameId.first;
			SubresourceMetadataNode& subresourceNode = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);

			if(subresourceLastRenderPasses.contains(subresourceName))
			{
				FrameGraphDescription::RenderPassName prevPassName = subresourceLastRenderPasses.at(subresourceName);
				
				FrameGraphDescription::SubresourceId prevSubresourceId   = mFrameGraphDescription.mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
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
	FrameGraphDescription::RenderPassName backbufferLastPassName  = subresourceLastRenderPasses.at(mFrameGraphDescription.mBackbufferName);
	FrameGraphDescription::RenderPassName backbufferFirstPassName = subresourceFirstRenderPasses.at(mFrameGraphDescription.mBackbufferName);

	FrameGraphDescription::SubresourceId backbufferLastSubresourceId  = mFrameGraphDescription.mRenderPassesSubresourceNameIds.at(backbufferLastPassName).at(mFrameGraphDescription.mBackbufferName);
	FrameGraphDescription::SubresourceId backbufferFirstSubresourceId = mFrameGraphDescription.mRenderPassesSubresourceNameIds.at(backbufferFirstPassName).at(mFrameGraphDescription.mBackbufferName);

	SubresourceMetadataNode& backbufferFirstNode = mRenderPassesSubresourceMetadatas.at(backbufferFirstPassName).at(backbufferFirstSubresourceId);
	SubresourceMetadataNode& backbufferLastNode  = mRenderPassesSubresourceMetadatas.at(backbufferLastPassName).at(backbufferLastSubresourceId);

	SubresourceMetadataNode& backbufferPresentNode = mRenderPassesSubresourceMetadatas.at(std::string(FrameGraphDescription::PresentPassName)).at(std::string(FrameGraphDescription::BackbufferPresentPassId));

	backbufferFirstNode.PrevPassNode = &backbufferPresentNode;
	backbufferLastNode.NextPassNode  = &backbufferPresentNode;

	backbufferPresentNode.PrevPassNode = &backbufferLastNode;
	backbufferPresentNode.NextPassNode = &backbufferFirstNode;


	//Validate cyclic links (so first pass knows what state to barrier from, and the last pass knows what state to barrier to)
	for(size_t i = 0; i < mSortedRenderPassNames.size(); i++)
	{
		FrameGraphDescription::RenderPassName renderPassName = mSortedRenderPassNames[i];
		for(auto& subresourceNameId: mFrameGraphDescription.mRenderPassesSubresourceNameIds[renderPassName])
		{
			if(subresourceNameId.first == mFrameGraphDescription.mBackbufferName) //Backbuffer is already processed
			{
				continue;
			}

			SubresourceMetadataNode& subresourceNode = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);

			if(subresourceNode.PrevPassNode == nullptr)
			{
				FrameGraphDescription::SubresourceName subresourceName = subresourceNameId.first;
				FrameGraphDescription::RenderPassName  prevPassName    = subresourceLastRenderPasses.at(subresourceName);

				FrameGraphDescription::SubresourceId prevSubresourceId = mFrameGraphDescription.mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
				SubresourceMetadataNode& prevSubresourceNode = mRenderPassesSubresourceMetadatas.at(prevPassName).at(prevSubresourceId);

				subresourceNode.PrevPassNode     = &prevSubresourceNode;
				prevSubresourceNode.NextPassNode = &subresourceNode;
			}

			if(subresourceNode.NextPassNode == nullptr)
			{
				FrameGraphDescription::SubresourceName subresourceName = subresourceNameId.first;
				FrameGraphDescription::RenderPassName  nextPassName    = subresourceFirstRenderPasses.at(subresourceName);

				FrameGraphDescription::SubresourceId nextSubresourceId = mFrameGraphDescription.mRenderPassesSubresourceNameIds.at(nextPassName).at(subresourceName);
				SubresourceMetadataNode& nextSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(nextPassName).at(nextSubresourceId);

				subresourceNode.NextPassNode         = &nextSubresourceMetadata;
				nextSubresourceMetadata.PrevPassNode = &subresourceNode;
			}
		}
	}
}

void ModernFrameGraphBuilder::ValidateSubresourcePassClasses()
{
	//Pass types should be known
	assert(!mRenderPassClasses.empty());

	std::string presentPassName(FrameGraphDescription::PresentPassName);

	for (auto& subresourceNameId: mFrameGraphDescription.mRenderPassesSubresourceNameIds[presentPassName])
	{
		SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(presentPassName).at(subresourceNameId.second);
		metadataNode.PassClass = mRenderPassClasses[presentPassName];
	}

	for (size_t i = 0; i < mSortedRenderPassNames.size(); i++)
	{
		FrameGraphDescription::RenderPassName renderPassName = mSortedRenderPassNames[i];

		for (auto& subresourceNameId: mFrameGraphDescription.mRenderPassesSubresourceNameIds[renderPassName])
		{
			SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);
			metadataNode.PassClass = mRenderPassClasses[renderPassName];
		}
	}
}

void ModernFrameGraphBuilder::FindBackbufferPasses(std::unordered_set<FrameGraphDescription::RenderPassName>& swapchainPassNames)
{
	swapchainPassNames.clear();

	for(const auto& renderPassName: mSortedRenderPassNames)
	{
		const auto& nameIdMap = mFrameGraphDescription.mRenderPassesSubresourceNameIds[renderPassName];
		for(const auto& nameId: nameIdMap)
		{
			if(nameId.first == mFrameGraphDescription.mBackbufferName)
			{
				swapchainPassNames.insert(renderPassName);
				continue;
			}
		}
	}
}

void ModernFrameGraphBuilder::CalculatePassPeriods()
{
	for(const FrameGraphDescription::RenderPassName& passName: mSortedRenderPassNames)
	{
		uint32_t passPeriod = 1;

		const auto& nameIdMap   = mFrameGraphDescription.mRenderPassesSubresourceNameIds.at(passName);
		const auto& metadataMap = mRenderPassesSubresourceMetadatas.at(passName);
		for(const auto& subresourceNameId: nameIdMap)
		{
			if(subresourceNameId.first != mFrameGraphDescription.mBackbufferName) //Swapchain images don't count as render pass own period
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
	for(const std::string& passName: mSortedRenderPassNames)
	{
		RenderPassType passType = mFrameGraphDescription.mRenderPassTypes[passName];

		uint32_t passSwapchainImageCount = 1;
		if(mFrameGraphDescription.mRenderPassesSubresourceNameIds[passName].contains(mFrameGraphDescription.mBackbufferName))
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
				CreatePassObject(passName, passType, frame);
			}
		}
	}

	ModernFrameGraph::RenderPassSpanInfo presentPassSpanInfo;
	presentPassSpanInfo.PassSpanBegin = NextPassSpanId();
	presentPassSpanInfo.OwnFrames     = 0;
	mGraphToBuild->mPassFrameSpans.push_back(presentPassSpanInfo);
}

void ModernFrameGraphBuilder::BuildBarriers()
{
	mGraphToBuild->mRenderPassBarriers.resize(mSortedRenderPassNames.size() + 1);
	mGraphToBuild->mMultiframeBarrierInfos.clear();

	uint32_t totalBarrierCount = 0;
	for(size_t i = 0; i < mSortedRenderPassNames.size(); i++)
	{
		FrameGraphDescription::RenderPassName renderPassName = mSortedRenderPassNames[i];
		totalBarrierCount += BuildPassBarriers(renderPassName, totalBarrierCount, &mGraphToBuild->mRenderPassBarriers[i]);
	}

	BuildPassBarriers(FrameGraphDescription::RenderPassName(FrameGraphDescription::PresentPassName), totalBarrierCount, &mGraphToBuild->mRenderPassBarriers.back());
}

uint32_t ModernFrameGraphBuilder::BuildPassBarriers(const FrameGraphDescription::RenderPassName& passName, uint32_t barrierOffset, ModernFrameGraph::BarrierSpan* outBarrierSpan)
{
	const auto& nameIdMap   = mFrameGraphDescription.mRenderPassesSubresourceNameIds.at(passName);
	const auto& metadataMap = mRenderPassesSubresourceMetadatas.at(passName);

	std::vector<const SubresourceMetadataNode*> passMetadatasForBeforeBarriers;
	std::vector<const SubresourceMetadataNode*> passMetadatasForAfterBarriers;
	for(auto& subresourceNameId: nameIdMap)
	{
		const SubresourceMetadataNode& subresourceMetadata = metadataMap.at(subresourceNameId.second);
	
		if(!(subresourceMetadata.Flags & TextureFlagAutoBeforeBarrier) && !(subresourceMetadata.PrevPassNode->Flags & TextureFlagAutoAfterBarrier))
		{
			passMetadatasForBeforeBarriers.push_back(&subresourceMetadata);
		}

		if(!(subresourceMetadata.Flags & TextureFlagAutoAfterBarrier) && !(subresourceMetadata.NextPassNode->Flags & TextureFlagAutoBeforeBarrier))
		{
			passMetadatasForAfterBarriers.push_back(&subresourceMetadata);
		}
	}


	uint32_t passBarrierCount = 0;

	ModernFrameGraph::BarrierSpan barrierSpan;
	barrierSpan.BeforePassBegin = barrierOffset + passBarrierCount;

	for(const SubresourceMetadataNode* subresourceMetadata: passMetadatasForBeforeBarriers)
	{
		SubresourceMetadataNode* prevSubresourceMetadata = subresourceMetadata->PrevPassNode;
			
		uint32_t barrierId = AddBeforePassBarrier(subresourceMetadata->FirstFrameHandle, prevSubresourceMetadata->PassClass, prevSubresourceMetadata->SubresourceInfoIndex, subresourceMetadata->PassClass, subresourceMetadata->SubresourceInfoIndex);
		if(barrierId != (uint32_t)(-1))
		{
			passBarrierCount++;

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

	barrierSpan.BeforePassEnd  = barrierOffset + passBarrierCount;
	barrierSpan.AfterPassBegin = barrierOffset + passBarrierCount;

	for(const SubresourceMetadataNode* subresourceMetadata: passMetadatasForBeforeBarriers)
	{
		SubresourceMetadataNode* nextSubresourceMetadata = subresourceMetadata->NextPassNode;
			
		uint32_t barrierId = AddAfterPassBarrier(subresourceMetadata->FirstFrameHandle, subresourceMetadata->PassClass, subresourceMetadata->SubresourceInfoIndex, nextSubresourceMetadata->PassClass, nextSubresourceMetadata->SubresourceInfoIndex);
		if(barrierId != (uint32_t)(-1))
		{
			passBarrierCount++;

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

	barrierSpan.AfterPassEnd = barrierOffset + passBarrierCount;

	if(outBarrierSpan)
	{
		*outBarrierSpan = barrierSpan;
	}

	return passBarrierCount;
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
	std::unordered_set<FrameGraphDescription::SubresourceName> processedSubresourceNames;

	TextureResourceCreateInfo backbufferCreateInfo;
	backbufferCreateInfo.Name         = mFrameGraphDescription.mBackbufferName;
	backbufferCreateInfo.MetadataHead = &mRenderPassesSubresourceMetadatas.at(std::string(FrameGraphDescription::PresentPassName)).at(std::string(FrameGraphDescription::BackbufferPresentPassId));
	
	outBackbufferCreateInfos.push_back(backbufferCreateInfo);
	processedSubresourceNames.insert(mFrameGraphDescription.mBackbufferName);

	for(const auto& renderPassName: mSortedRenderPassNames)
	{
		const auto& nameIdMap = mFrameGraphDescription.mRenderPassesSubresourceNameIds[renderPassName];
		auto& metadataMap     = mRenderPassesSubresourceMetadatas[renderPassName];

		for(const auto& nameId: nameIdMap)
		{
			const FrameGraphDescription::SubresourceName& subresourceName = nameId.first;
			const FrameGraphDescription::SubresourceId&   subresourceId   = nameId.second;

			if(!processedSubresourceNames.contains(subresourceName))
			{
				TextureResourceCreateInfo textureCreateInfo;
				textureCreateInfo.Name         = subresourceName;
				textureCreateInfo.MetadataHead = &metadataMap.at(subresourceId);

				outTextureCreateInfos.push_back(textureCreateInfo);
				processedSubresourceNames.insert(subresourceName);
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
		SubresourceMetadataNode* prevPassNode = currentNode->PrevPassNode;

		//Frame count for all nodes should be the same. As for calculating the value, we can do it
		//two ways: with max or with lcm. Since it's only related to a single resource, swapping 
		//can be made as fine with 3 ping-pong data as with 2. So max will suffice
		currentNode->FrameCount = std::max(currentNode->FrameCount, prevPassNode->FrameCount);

		if(ValidateSubresourceViewParameters(currentNode, prevPassNode))
		{
			//Reset the head node if propagation succeeded. This means the further propagation all the way may be needed
			headNode = currentNode;
		}

		currentNode = currentNode->NextPassNode;

	} while(currentNode != headNode);
}

uint32_t ModernFrameGraphBuilder::PrepareResourceLocations(std::vector<TextureResourceCreateInfo>& textureCreateInfos, std::vector<TextureResourceCreateInfo>& backbufferCreateInfos)
{
	uint32_t imageCount = 0;

	imageCount += ValidateImageAndViewIndices(textureCreateInfos, imageCount);
	mGraphToBuild->mBackbufferImageSpan.Begin = imageCount;

	imageCount += ValidateImageAndViewIndices(backbufferCreateInfos, imageCount);
	mGraphToBuild->mBackbufferImageSpan.End = imageCount;

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
	for(uint32_t nodeIndex = 1; nodeIndex < (uint32_t)resourceMetadataNodes.size(); nodeIndex++)
	{
		uint64_t currSortKey = resourceMetadataNodes[nodeIndex]->ViewSortKey;
		if(currSortKey != differentSortKeys.back())
		{
			differentImageViewSpans.back().End = nodeIndex;
			differentImageViewSpans.push_back(Span<uint32_t>{.Begin = (uint32_t)nodeIndex, .End = (uint32_t)nodeIndex});

			differentSortKeys.push_back(currSortKey);
		}
	}

	differentImageViewSpans.back().End = (uint32_t)resourceMetadataNodes.size();

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

void ModernFrameGraphBuilder::TopologicalSortNode(const std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::RenderPassName>>& adjacencyList, std::unordered_set<FrameGraphDescription::RenderPassName>& visited, std::unordered_set<FrameGraphDescription::RenderPassName>& onStack, const FrameGraphDescription::RenderPassName& renderPassName)
{
	if(visited.contains(renderPassName))
	{
		return;
	}

	onStack.insert(renderPassName);
	visited.insert(renderPassName);

	const auto& adjacentPassNames = adjacencyList.at(renderPassName);
	for(const FrameGraphDescription::RenderPassName& adjacentPassName: adjacentPassNames)
	{
		TopologicalSortNode(adjacencyList, visited, onStack, adjacentPassName);
	}

	onStack.erase(renderPassName);
	mSortedRenderPassNames.push_back(renderPassName);
}

bool ModernFrameGraphBuilder::PassesIntersect(const FrameGraphDescription::RenderPassName& writingPass, const FrameGraphDescription::RenderPassName& readingPass)
{
	const auto& writingNameIdMap = mFrameGraphDescription.mRenderPassesSubresourceNameIds[writingPass];
	const auto& readingNameIdMap = mFrameGraphDescription.mRenderPassesSubresourceNameIds[readingPass];

	if(writingNameIdMap.size() <= readingNameIdMap.size())
	{
		for(const auto& subresourceNameId: writingNameIdMap)
		{
			if(!mRenderPassesWriteSubresourceIds[writingPass].contains(subresourceNameId.second))
			{
				//Is not a written subresource
				continue;
			}

			auto readIt = readingNameIdMap.find(subresourceNameId.first);
			if(readIt != readingNameIdMap.end() && mRenderPassesReadSubresourceIds[readingPass].contains(readIt->second))
			{
				//Is a read subresource
				return true;
			}
		}
	}
	else
	{
		for(const auto& subresourceNameId: readingNameIdMap)
		{
			if(!mRenderPassesReadSubresourceIds[readingPass].contains(subresourceNameId.second))
			{
				//Is not a read subresource
				continue;
			}

			auto writeIt = writingNameIdMap.find(subresourceNameId.first);
			if(writeIt != writingNameIdMap.end() && mRenderPassesReadSubresourceIds[writingPass].contains(writeIt->second))
			{
				//Is a written subresource
				return true;
			}
		}
	}

	return false;
}