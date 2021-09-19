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

	RegisterPassInGraph(passType, renderPassName);
}

void RegisterSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	FrameGraphDescription::RenderPassName renderPassName(passName);
	FrameGraphDescription::SubresourceId  subresId(subresourceId);
}

void ModernFrameGraphBuilder::RegisterSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	FrameGraphDescription::RenderPassName renderPassName(passName);
	FrameGraphDescription::SubresourceId  subresId(subresourceId);
	assert(!mMetadataNodeIndicesPerSubresourceIds.contains(subresId));

	uint32_t newSubresourceIndex = (uint32_t)mSubresourceMetadataNodesFlat.size();
	uint16_t passSubresourceId  = (uint16_t)(-1);

	auto subresourceSpanIt = mSubresourceMetadataSpansPerPass.find(renderPassName);
	if(subresourceSpanIt != mSubresourceMetadataSpansPerPass.end())
	{
		Span<uint32_t>& subresourceMetadataSpan = subresourceSpanIt->second;
		assert(subresourceMetadataSpan.End == (uint32_t)newSubresourceIndex); //Interleaved subresource adding is not supported

		subresourceMetadataSpan.End++;

		passSubresourceId = (uint16_t)(subresourceMetadataSpan.End - subresourceMetadataSpan.Begin - 1);
	}
	else
	{
		mSubresourceMetadataSpansPerPass[renderPassName] = Span<uint32_t>
		{
			.Begin = newSubresourceIndex,
			.End   = newSubresourceIndex + 1
		};

		passSubresourceId = 0;
	}

	uint32_t subresourceInfoIndex = AddSubresourceMetadata();
	assert(subresourceInfoIndex == newSubresourceIndex);

	SubresourceMetadataNode metadataNode;
	metadataNode.SubresourceName      = mFrameGraphDescription.mSubresourceIdNames.at(subresId);
	metadataNode.PrevPassNodeIndex    = (uint32_t)(-1);
	metadataNode.NextPassNodeIndex    = (uint32_t)(-1);
	metadataNode.FirstFrameHandle     = (uint32_t)(-1);
	metadataNode.FirstFrameViewHandle = (uint32_t)(-1);
	metadataNode.PerPassSubresourceId = passSubresourceId;
	metadataNode.FrameCount           = (uint16_t)1;
	metadataNode.Flags                = 0;
	metadataNode.PassClass            = RenderPassClass::Graphics;
	mSubresourceMetadataNodesFlat.push_back(metadataNode);

	mMetadataNodeIndicesPerSubresourceIds[subresId] = newSubresourceIndex;
}

void ModernFrameGraphBuilder::EnableSubresourceAutoBeforeBarrier(const std::string_view subresourceId, bool autoBarrier)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);

	SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr)];
	if(autoBarrier)
	{
		metadataNode.Flags |= TextureFlagAutoBeforeBarrier;
	}
	else
	{
		metadataNode.Flags &= ~TextureFlagAutoBeforeBarrier;
	}
}

void ModernFrameGraphBuilder::EnableSubresourceAutoAfterBarrier(const std::string_view subresourceId, bool autoBarrier)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);

	SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr)];
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

	SortPasses();

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
	mRenderPassClasses[presentPassName] = RenderPassClass::Present;

	assert(!mSubresourceMetadataSpansPerPass.contains(presentPassName));
	uint32_t newSubresourceIndex = (uint32_t)mSubresourceMetadataNodesFlat.size();

	FrameGraphDescription::SubresourceId backbufferId(FrameGraphDescription::BackbufferPresentPassId);
	assert(!mMetadataNodeIndicesPerSubresourceIds.contains(backbufferId));
	mMetadataNodeIndicesPerSubresourceIds[backbufferId] = newSubresourceIndex;

	uint32_t subresourceInfoIndex = AddPresentSubresourceMetadata();
	assert(subresourceInfoIndex == newSubresourceIndex);

	SubresourceMetadataNode presentAcquireMetadataNode;
	presentAcquireMetadataNode.SubresourceName      = mFrameGraphDescription.mBackbufferName;
	presentAcquireMetadataNode.PrevPassNodeIndex    = (uint32_t)(-1);
	presentAcquireMetadataNode.NextPassNodeIndex    = (uint32_t)(-1);
	presentAcquireMetadataNode.FirstFrameHandle     = (uint32_t)(-1);
	presentAcquireMetadataNode.FirstFrameViewHandle = (uint32_t)(-1);
	presentAcquireMetadataNode.PerPassSubresourceId = 0;
	presentAcquireMetadataNode.FrameCount           = (uint16_t)GetSwapchainImageCount();
	presentAcquireMetadataNode.Flags                = 0;
	presentAcquireMetadataNode.PassClass            = RenderPassClass::Present;

	mSubresourceMetadataNodesFlat.push_back(presentAcquireMetadataNode);
	mSubresourceMetadataSpansPerPass[presentPassName] = Span<uint32_t>
	{
		.Begin = newSubresourceIndex,
		.End   = newSubresourceIndex + 1
	};
}

void ModernFrameGraphBuilder::SortPasses()
{
	std::vector<uint32_t>                                                          subresourceIdsFlat;
	std::unordered_map<FrameGraphDescription::RenderPassName, std::span<uint32_t>> readSubresourceIdSpans;
	std::unordered_map<FrameGraphDescription::RenderPassName, std::span<uint32_t>> writeSubresourceIdSpans;
	BuildReadWriteSubresourceSpans(subresourceIdsFlat, readSubresourceIdSpans, writeSubresourceIdSpans);;

	std::vector<FrameGraphDescription::RenderPassName>                                                          adjacencyPassNamesFlat;
	std::unordered_map<FrameGraphDescription::RenderPassName, std::span<FrameGraphDescription::RenderPassName>> adjacencyList;
	BuildAdjacencyList(readSubresourceIdSpans, writeSubresourceIdSpans, adjacencyList, adjacencyPassNamesFlat);

	BuildSortedRenderPassesTopological(adjacencyList);
	BuildSortedRenderPassesDependency(adjacencyList);
}

void ModernFrameGraphBuilder::BuildAdjacencyList(std::unordered_map<FrameGraphDescription::RenderPassName, std::span<FrameGraphDescription::RenderPassName>>& outAdjacencyList, std::vector<FrameGraphDescription::RenderPassName>& outPassNamesFlat)
{
	outAdjacencyList.clear();
	outPassNamesFlat.clear();

	for(const FrameGraphDescription::RenderPassName& renderPassName: mFrameGraphDescription.mRenderPassNames)
	{
		Span<uint32_t> writeSpan = mRenderPassSubresourceWriteNameSpans[renderPassName];
		std::span<FrameGraphDescription::SubresourceName> writePassNames = {mRenderPassSubresourceNames.begin() + writeSpan.Begin, mRenderPassSubresourceNames.begin() + writeSpan.End};
		std::sort(writePassNames.begin(), writePassNames.end(), [](const FrameGraphDescription::SubresourceName& left, const FrameGraphDescription::SubresourceName& right)
		{
			return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end());
		});

		auto spanStart = outPassNamesFlat.end();
		for(const FrameGraphDescription::RenderPassName& otherPassName: mFrameGraphDescription.mRenderPassNames)
		{
			Span<uint32_t> readSpan = mRenderPassSubresourceReadNameSpans[renderPassName];
			std::span<FrameGraphDescription::SubresourceName> readPassNames = {mRenderPassSubresourceNames.begin() + readSpan.Begin, mRenderPassSubresourceNames.begin() + readSpan.End};
			std::sort(readPassNames.begin(), readPassNames.end(), [](const FrameGraphDescription::SubresourceName& left, const FrameGraphDescription::SubresourceName& right)
			{
				return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end());
			});

			if(renderPassName == otherPassName)
			{
				continue;
			}

			if(PassesIntersect(readPassNames, writePassNames))
			{
				outPassNamesFlat.push_back(otherPassName);
			}
		}

		auto spanEnd = outPassNamesFlat.end();
		outAdjacencyList[renderPassName] = {spanStart, spanEnd};
	}
}

void ModernFrameGraphBuilder::BuildSortedRenderPassesTopological(const std::unordered_map<FrameGraphDescription::RenderPassName, std::span<FrameGraphDescription::RenderPassName>>& adjacencyList)
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

void ModernFrameGraphBuilder::BuildSortedRenderPassesDependency(const std::unordered_map<FrameGraphDescription::RenderPassName, std::span<FrameGraphDescription::RenderPassName>>& adjacencyList)
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
	std::unordered_map<std::string_view, uint32_t> subresourceFirstNodeIndices; //The fisrt node index for each subresource
	std::unordered_map<std::string_view, uint32_t> subresourceLastNodeIndices;  //The last node index for each subresource

	//Connect previous passes
	for(size_t i = 0; i < mSortedRenderPassNames.size(); i++)
	{
		FrameGraphDescription::RenderPassName renderPassName = mSortedRenderPassNames[i];

		Span<uint32_t> metadataSpan = mSubresourceMetadataSpansPerPass.at(renderPassName);
		for(uint32_t metadataIndex = metadataSpan.Begin; metadataIndex < metadataSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];

			auto lastIndexIt = subresourceLastNodeIndices.find(metadataNode.SubresourceName);
			if(lastIndexIt != subresourceLastNodeIndices.end());
			{
				SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[lastIndexIt->second];

				prevSubresourceNode.NextPassNodeIndex = metadataIndex;
				metadataNode.PrevPassNodeIndex        = lastIndexIt->second;
			}

			if(!subresourceFirstNodeIndices.contains(metadataNode.SubresourceName))
			{
				subresourceFirstNodeIndices[metadataNode.SubresourceName] = metadataIndex;
			}

			subresourceLastNodeIndices[metadataNode.SubresourceName] = metadataIndex;
		}
	}


	//Validate backbuffer links
	uint32_t backbufferFirstNodeIndex = subresourceFirstNodeIndices.at(mFrameGraphDescription.mBackbufferName);
	uint32_t backbufferLastNodeIndex  = subresourceLastNodeIndices.at(mFrameGraphDescription.mBackbufferName);

	SubresourceMetadataNode& backbufferFirstNode = mSubresourceMetadataNodesFlat[backbufferFirstNodeIndex];
	SubresourceMetadataNode& backbufferLastNode  = mSubresourceMetadataNodesFlat[backbufferLastNodeIndex];

	uint32_t backbufferPresentNodeIndex = mMetadataNodeIndicesPerSubresourceIds.at(FrameGraphDescription::SubresourceName(FrameGraphDescription::BackbufferPresentPassId));
	SubresourceMetadataNode& backbufferPresentNode = mSubresourceMetadataNodesFlat[backbufferPresentNodeIndex];

	backbufferFirstNode.PrevPassNodeIndex = backbufferPresentNodeIndex;
	backbufferLastNode.NextPassNodeIndex  = backbufferPresentNodeIndex;

	backbufferPresentNode.PrevPassNodeIndex = backbufferLastNodeIndex;
	backbufferPresentNode.NextPassNodeIndex = backbufferFirstNodeIndex;


	//Validate cyclic links (so first pass knows what state to barrier from, and the last pass knows what state to barrier to)
	for(size_t i = 0; i < mSortedRenderPassNames.size(); i++)
	{
		FrameGraphDescription::RenderPassName renderPassName = mSortedRenderPassNames[i];

		Span<uint32_t> metadataSpan = mSubresourceMetadataSpansPerPass.at(renderPassName);
		for(uint32_t metadataIndex = metadataSpan.Begin; metadataIndex < metadataSpan.End; metadataIndex++)
		{
			if(mSubresourceMetadataNodesFlat[metadataIndex].SubresourceName == mFrameGraphDescription.mBackbufferName) //Backbuffer is already processed
			{
				continue;
			}

			SubresourceMetadataNode& subresourceNode = mSubresourceMetadataNodesFlat[metadataIndex];

			if(subresourceNode.PrevPassNodeIndex == (uint32_t)(-1))
			{
				uint32_t prevNodeIndex = subresourceLastNodeIndices.at(subresourceNode.SubresourceName);
				SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[prevNodeIndex];

				subresourceNode.PrevPassNodeIndex     = prevNodeIndex;
				prevSubresourceNode.NextPassNodeIndex = metadataIndex;
			}

			if(subresourceNode.NextPassNodeIndex == (uint32_t)(-1))
			{
				uint32_t nextNodeIndex = subresourceFirstNodeIndices.at(subresourceNode.SubresourceName);
				SubresourceMetadataNode& nextSubresourceMetadata = mSubresourceMetadataNodesFlat[nextNodeIndex];

				subresourceNode.NextPassNodeIndex         = nextNodeIndex;
				nextSubresourceMetadata.PrevPassNodeIndex = metadataIndex;
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

void ModernFrameGraphBuilder::TopologicalSortNode(const std::unordered_map<FrameGraphDescription::RenderPassName, std::span<FrameGraphDescription::RenderPassName>>& adjacencyList, std::unordered_set<FrameGraphDescription::RenderPassName>& visited, std::unordered_set<FrameGraphDescription::RenderPassName>& onStack, const FrameGraphDescription::RenderPassName& renderPassName)
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

bool ModernFrameGraphBuilder::PassesIntersect(const std::span<FrameGraphDescription::SubresourceName> readSortedSubresourceNames, const std::span<FrameGraphDescription::SubresourceName> writeSortedSubresourceNames)
{
	uint32_t readIndex  = 0;
	uint32_t writeIndex = 0;
	while(readIndex < readSortedSubresourceNames.size() && writeIndex < writeSortedSubresourceNames.size())
	{
		const FrameGraphDescription::SubresourceName& readName  = readSortedSubresourceNames[readIndex];
		const FrameGraphDescription::SubresourceName& writeName = writeSortedSubresourceNames[readIndex];

		std::strong_ordering order = std::lexicographical_compare_three_way(readName.begin(), readName.end(), writeName.begin(), writeName.end());
		if(order == std::strong_ordering::less)
		{
			readIndex++;
		}
		else if(order == std::strong_ordering::greater)
		{
			writeIndex++;
		}
		else
		{
			return true;
		}
	}

	return false;
}