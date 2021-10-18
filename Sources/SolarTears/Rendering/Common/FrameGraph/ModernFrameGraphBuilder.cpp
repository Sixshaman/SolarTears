#include "ModernFrameGraphBuilder.hpp"
#include "ModernFrameGraph.hpp"
#include "RenderPassDispatchFuncs.hpp"
#include <algorithm>
#include <cassert>
#include <array>
#include <numeric>

ModernFrameGraphBuilder::ModernFrameGraphBuilder(ModernFrameGraph* graphToBuild): mGraphToBuild(graphToBuild)
{
}

ModernFrameGraphBuilder::~ModernFrameGraphBuilder()
{
}

void ModernFrameGraphBuilder::Build(FrameGraphDescription&& frameGraphDescription)
{
	RegisterPasses(frameGraphDescription.mRenderPassTypes, frameGraphDescription.mSubresourceNames, frameGraphDescription.mBackbufferName);
	SortPasses();
	InitAugmentedData();

	BuildResources();
	BuildPassObjects();
	BuildBarriers();

	InitializeTraverseData();
}

const FrameGraphConfig* ModernFrameGraphBuilder::GetConfig() const
{
	return &mGraphToBuild->mFrameGraphConfig;
}

void ModernFrameGraphBuilder::RegisterPasses(const std::unordered_map<RenderPassName, RenderPassType>& renderPassTypes, const std::vector<SubresourceNamingInfo>& subresourceNames, const ResourceName& backbufferName)
{
	InitPassList(renderPassTypes);
	InitSubresourceList(subresourceNames, backbufferName);
}

void ModernFrameGraphBuilder::InitPassList(const std::unordered_map<RenderPassName, RenderPassType>& renderPassTypes)
{
	mTotalPassMetadatas.reserve(renderPassTypes.size() + 1);
	for(const auto& passNameWithType: renderPassTypes)
	{
		uint32_t currentMetadataNodeCount = (uint32_t)mSubresourceMetadataNodesFlat.size();
		uint32_t newMetadataNodeCount     = currentMetadataNodeCount + GetPassSubresourceCount(passNameWithType.second);

		mTotalPassMetadatas.push_back(PassMetadata
		{
			.Name = passNameWithType.first,

			.Class = GetPassClass(passNameWithType.second),
			.Type  = passNameWithType.second,

			.DependencyLevel         = 0,
			.SubresourceMetadataSpan =
			{
				.Begin = currentMetadataNodeCount,
				.End   = newMetadataNodeCount
			}
		});

		mSubresourceMetadataNodesFlat.resize(newMetadataNodeCount, SubresourceMetadataNode
		{
			.PrevPassNodeIndex     = (uint32_t)(-1),
			.NextPassNodeIndex     = (uint32_t)(-1),
			.ResourceMetadataIndex = (uint32_t)(-1),

			.ImageViewHandle = (uint32_t)(-1),
			.PassClass       = mTotalPassMetadatas.back().Class,
		});
	}

	mRenderPassMetadataSpan  = {0,                                    (uint32_t)mTotalPassMetadatas.size()};
	mPresentPassMetadataSpan = {(uint32_t)mTotalPassMetadatas.size(), (uint32_t)(mTotalPassMetadatas.size() + 1)};
	
	static_assert((uint32_t)PresentPassSubresourceId::Count == 1);
	Span<uint32_t> presentSubresourceSpan =
	{
		.Begin = (uint32_t)mSubresourceMetadataNodesFlat.size(),
		.End   = (uint32_t)(mSubresourceMetadataNodesFlat.size() + (size_t)PresentPassSubresourceId::Count)
	};

	mTotalPassMetadatas.push_back(PassMetadata
	{
		.Name = RenderPassName(PresentPassName),

		.Class = RenderPassClass::Present,
		.Type  = RenderPassType::None,

		.DependencyLevel         = 0,
		.SubresourceMetadataSpan = presentSubresourceSpan
	});

	mSubresourceMetadataNodesFlat.resize(presentSubresourceSpan.End, SubresourceMetadataNode
	{
		.PrevPassNodeIndex     = (uint32_t)(-1),
		.NextPassNodeIndex     = (uint32_t)(-1),
		.ResourceMetadataIndex = (uint32_t)(-1),

		.ImageViewHandle = (uint32_t)(-1),
		.PassClass       = RenderPassClass::Present,
	});
}

void ModernFrameGraphBuilder::InitSubresourceList(const std::vector<SubresourceNamingInfo>& subresourceNames, const ResourceName& backbufferName)
{
	std::unordered_set<RenderPassType> uniquePassTypes;
	std::unordered_map<RenderPassName, Span<uint32_t>> passSubresourceSpansPerName;
	for(uint32_t passMetadataIndex = mRenderPassMetadataSpan.Begin; passMetadataIndex < mRenderPassMetadataSpan.End; passMetadataIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passMetadataIndex];

		uniquePassTypes.insert(passMetadata.Type);
		passSubresourceSpansPerName[passMetadata.Name] = passMetadata.SubresourceMetadataSpan;
	}

	std::unordered_map<std::string_view, uint_fast16_t> passSubresourceIndices;
	for(RenderPassType passType: uniquePassTypes)
	{
		uint_fast16_t passSubresourceCount = GetPassSubresourceCount(passType);
		for(uint_fast16_t passSubresourceIndex = 0; passSubresourceIndex < passSubresourceCount; passSubresourceIndex++)
		{
			passSubresourceIndices[GetPassSubresourceStringId(passType, passSubresourceIndex)] = passSubresourceIndex;
		}
	}

	std::unordered_map<std::string_view, uint32_t> resourceMetadataIndices;
	ResourceMetadata backbufferResourceMetadata = 
	{
		.Name          = backbufferName,
		.SourceType    = TextureSourceType::Backbuffer,
		.HeadNodeIndex = (uint32_t)(-1)
	};

	mResourceMetadatas.push_back(backbufferResourceMetadata);
	resourceMetadataIndices[backbufferName] = (uint32_t)(mResourceMetadatas.size() - 1);

	const PassMetadata& presentPassMetadata = mTotalPassMetadatas[mPresentPassMetadataSpan.Begin];
	mSubresourceMetadataNodesFlat[presentPassMetadata.SubresourceMetadataSpan.Begin + (size_t)PresentPassSubresourceId::Backbuffer].ResourceMetadataIndex = (uint32_t)(mResourceMetadatas.size() - 1);

#if defined(DEBUG) || defined(_DEBUG)
	mSubresourceMetadataNodesFlat[presentPassMetadata.SubresourceMetadataSpan.Begin + (size_t)PresentPassSubresourceId::Backbuffer].ResourceName = mResourceMetadatas.back().Name;
	mSubresourceMetadataNodesFlat[presentPassMetadata.SubresourceMetadataSpan.Begin + (size_t)PresentPassSubresourceId::Backbuffer].PassName     = PresentPassName;
#endif

	for(const auto& subresourceNaming: subresourceNames)
	{
		uint_fast16_t passSubresourceIndex = passSubresourceIndices.at(subresourceNaming.PassSubresourceId);
		uint32_t      flatMetadataIndex    = passSubresourceSpansPerName.at(subresourceNaming.PassName).Begin + passSubresourceIndex;
		
		SubresourceMetadataNode& subresourceMetadataNode = mSubresourceMetadataNodesFlat[flatMetadataIndex];

		auto resourceMetadataIt = resourceMetadataIndices.find(subresourceNaming.PassSubresourceName);
		if(resourceMetadataIt != resourceMetadataIndices.end())
		{
			uint32_t resourceMetadataIndex = resourceMetadataIt->second;

			subresourceMetadataNode.ResourceMetadataIndex = resourceMetadataIndex;
		}
		else
		{
			resourceMetadataIndices[subresourceNaming.PassSubresourceName] = (uint32_t)mResourceMetadatas.size();
			mResourceMetadatas.push_back(ResourceMetadata
			{
				.Name          = subresourceNaming.PassSubresourceName,
				.SourceType    = TextureSourceType::PassTexture,
				.HeadNodeIndex = (uint32_t)(-1)
			});

			subresourceMetadataNode.ResourceMetadataIndex = (uint32_t)(mResourceMetadatas.size() - 1);
		}

#if defined(DEBUG) || defined(_DEBUG)
		subresourceMetadataNode.ResourceName = mResourceMetadatas[subresourceMetadataNode.ResourceMetadataIndex].Name;
		subresourceMetadataNode.PassName     = subresourceNaming.PassName;
#endif
	}
}

void ModernFrameGraphBuilder::SortPasses()
{
	std::vector<uint32_t>            subresourceIndicesFlat;
	std::vector<std::span<uint32_t>> readSubresourceIndicesPerPass;
	std::vector<std::span<uint32_t>> writeSubresourceIndicesPerPass;
	BuildReadWriteSubresourceSpans(readSubresourceIndicesPerPass, writeSubresourceIndicesPerPass, subresourceIndicesFlat);

	std::vector<uint32_t>            adjacencyPassIndicesFlat;
	std::vector<std::span<uint32_t>> unsortedPassAdjacencyList;
	BuildAdjacencyList(readSubresourceIndicesPerPass, writeSubresourceIndicesPerPass, unsortedPassAdjacencyList, adjacencyPassIndicesFlat);

	std::vector<uint32_t> passIndexRemap;
	SortRenderPassesByTopology(unsortedPassAdjacencyList, passIndexRemap);

	AssignDependencyLevels(passIndexRemap, unsortedPassAdjacencyList);
	SortRenderPassesByDependency();
}

void ModernFrameGraphBuilder::BuildReadWriteSubresourceSpans(std::vector<std::span<std::uint32_t>>& outReadIndexSpans, std::vector<std::span<std::uint32_t>>& outWriteIndexSpans, std::vector<uint32_t>& outIndicesFlat)
{
	outReadIndexSpans.clear();
	outWriteIndexSpans.clear();

	//We can't make up real spans until outIndicesFlat stops changing. Create mock spans, storing offsets from NULL instead of outIndicesFlat.data()
	uint32_t* dataMarkerBegin = nullptr;
	std::vector<uint_fast16_t> tempSubresourceIds;
	for(uint32_t passMetadataIndex = mRenderPassMetadataSpan.Begin; passMetadataIndex < mRenderPassMetadataSpan.End; passMetadataIndex++)
	{
		const PassMetadata& renderPassMetadata = mTotalPassMetadatas[passMetadataIndex];

		uint32_t readSubresourceCount  = GetPassReadSubresourceCount(renderPassMetadata.Type);
		uint32_t writeSubresourceCount = GetPassWriteSubresourceCount(renderPassMetadata.Type);

		tempSubresourceIds.resize(readSubresourceCount + writeSubresourceCount);

		uint32_t readBeginOffset  = 0;
		uint32_t readEndOffset    = readSubresourceCount;
		uint32_t writeBeginOffset = readSubresourceCount;
		uint32_t writeEndOffset   = readSubresourceCount + writeSubresourceCount;

		uint32_t* currDataMarker = dataMarkerBegin + outIndicesFlat.size();
		std::span<uint32_t> readMockSpan  = {currDataMarker + readBeginOffset,  currDataMarker + readEndOffset};
		std::span<uint32_t> writeMockSpan = {currDataMarker + writeBeginOffset, currDataMarker + writeEndOffset};

		std::span<uint_fast16_t> readSubresources = {tempSubresourceIds.begin() + readBeginOffset, tempSubresourceIds.begin() + readEndOffset};
		FillPassReadSubresourceIds(renderPassMetadata.Type, readSubresources);
		for(uint_fast16_t readSubresourceTypeIndex: readSubresources)
		{
			uint32_t subresourceMetadataIndex = renderPassMetadata.SubresourceMetadataSpan.Begin + readSubresourceTypeIndex;
			outIndicesFlat.push_back(mSubresourceMetadataNodesFlat[subresourceMetadataIndex].ResourceMetadataIndex);
		}

		std::span<uint_fast16_t> writeSubresources = {tempSubresourceIds.begin() + writeBeginOffset, tempSubresourceIds.begin() + writeEndOffset };
		FillPassWriteSubresourceIds(renderPassMetadata.Type, writeSubresources);
		for(uint_fast16_t writeSubresourceTypeIndex: writeSubresources)
		{
			uint32_t subresourceMetadataIndex = renderPassMetadata.SubresourceMetadataSpan.Begin + writeSubresourceTypeIndex;
			outIndicesFlat.push_back(mSubresourceMetadataNodesFlat[subresourceMetadataIndex].ResourceMetadataIndex);
		}

		outReadIndexSpans.push_back(readMockSpan);
		outWriteIndexSpans.push_back(writeMockSpan);
	}

	//Invalidate and sort spans, outIndicesFlat won't change anymore
	for(uint32_t readSpanIndex = 0; readSpanIndex < outReadIndexSpans.size(); readSpanIndex++)
	{
		const std::span<uint32_t> mockReadSpan = outReadIndexSpans[readSpanIndex];

		ptrdiff_t spanDataOffset = mockReadSpan.data() - dataMarkerBegin;
		size_t    spanSize       = mockReadSpan.size();
		std::span<uint32_t> realReadSpan = {outIndicesFlat.begin() + spanDataOffset, outIndicesFlat.begin() + spanDataOffset + spanSize};

		std::sort(realReadSpan.begin(), realReadSpan.end());
		outReadIndexSpans[readSpanIndex] = realReadSpan;
	}

	for(uint32_t writeSpanIndex = 0; writeSpanIndex < outWriteIndexSpans.size(); writeSpanIndex++)
	{
		const std::span<uint32_t> mockWriteSpan = outWriteIndexSpans[writeSpanIndex];

		ptrdiff_t spanDataOffset = mockWriteSpan.data() - dataMarkerBegin;
		size_t    spanSize       = mockWriteSpan.size();
		std::span<uint32_t> realWriteSpan = {outIndicesFlat.begin() + spanDataOffset, outIndicesFlat.begin() + spanDataOffset + spanSize};

		std::sort(realWriteSpan.begin(), realWriteSpan.end());
		outWriteIndexSpans[writeSpanIndex] = realWriteSpan;
	}
}

void ModernFrameGraphBuilder::BuildAdjacencyList(const std::vector<std::span<uint32_t>>& sortedReadNameSpansPerPass, const std::vector<std::span<uint32_t>>& sortedwriteNameSpansPerPass, std::vector<std::span<uint32_t>>& outAdjacencyList, std::vector<uint32_t>& outAdjacentPassIndicesFlat)
{
	outAdjacencyList.resize(mRenderPassMetadataSpan.End - mRenderPassMetadataSpan.Begin);
	outAdjacentPassIndicesFlat.clear();

	uint32_t* dataMarkerBegin = nullptr;
	for(uint32_t writePassIndex = 0; writePassIndex < sortedwriteNameSpansPerPass.size(); writePassIndex++)
	{
		std::span<uint32_t> writtenResourceIndices = sortedwriteNameSpansPerPass[writePassIndex];

		auto mockSpanStart = dataMarkerBegin + outAdjacentPassIndicesFlat.size();
		for(uint32_t readPassIndex = 0; readPassIndex < sortedReadNameSpansPerPass.size(); readPassIndex++)
		{
			if(readPassIndex == writePassIndex)
			{
				continue;
			}

			std::span<uint32_t> readResourceIndices = sortedReadNameSpansPerPass[readPassIndex];
			if(SpansIntersect(readResourceIndices, writtenResourceIndices))
			{
				outAdjacentPassIndicesFlat.push_back(readPassIndex);
			}
		}

		auto mockSpanEnd = dataMarkerBegin + outAdjacentPassIndicesFlat.size();
		outAdjacencyList[writePassIndex] = {mockSpanStart, mockSpanEnd};
	}

	for(uint32_t passIndex = 0; passIndex < outAdjacencyList.size(); passIndex++)
	{
		const std::span mockSpan = outAdjacencyList[passIndex];

		ptrdiff_t spanDataOffset = mockSpan.data() - dataMarkerBegin;
		size_t    spanSize       = mockSpan.size();
		outAdjacencyList[passIndex] = {outAdjacentPassIndicesFlat.begin() + spanDataOffset, outAdjacentPassIndicesFlat.begin() + spanDataOffset + spanSize};
	}
}

bool ModernFrameGraphBuilder::SpansIntersect(const std::span<uint32_t> leftSortedSpan, const std::span<uint32_t> rightSortedSpan)
{
	uint32_t leftIndex  = 0;
	uint32_t rightIndex = 0;
	while(leftIndex < leftSortedSpan.size() && rightIndex < rightSortedSpan.size())
	{
		uint32_t leftVal  = leftSortedSpan[leftIndex];
		uint32_t rightVal = rightSortedSpan[rightIndex];

		if(leftVal == rightVal)
		{
			return true;
		}
		else if(leftVal < rightVal)
		{
			leftIndex++;
		}
		else if(leftVal > rightVal)
		{
			rightIndex++;
		}
	}

	return false;
}

void ModernFrameGraphBuilder::AssignDependencyLevels(const std::vector<uint32_t>& passIndexRemap, const std::vector<std::span<uint32_t>>& oldAdjacencyList)
{
	std::vector<uint32_t> oldPassIndexRemap(passIndexRemap.size());
	for(uint32_t oldPassIndex = 0; oldPassIndex < passIndexRemap.size(); oldPassIndex++)
	{
		uint32_t newPassIndex = passIndexRemap[oldPassIndex];
		oldPassIndexRemap[newPassIndex] = oldPassIndex;
	}

	for(uint32_t newPassIndex = mRenderPassMetadataSpan.Begin; newPassIndex < mRenderPassMetadataSpan.End; newPassIndex++)
	{
		uint32_t oldPassIndex = oldPassIndexRemap[newPassIndex];
		uint32_t mainPassDependencyLevel = mTotalPassMetadatas[mRenderPassMetadataSpan.Begin + newPassIndex].DependencyLevel;

		const std::span<uint32_t> adjacentPassIndices = oldAdjacencyList[oldPassIndex];
		for(const uint32_t oldAdjacentPassIndex: adjacentPassIndices)
		{
			uint32_t newAdjacentPassIndex = passIndexRemap[oldAdjacentPassIndex];
			mTotalPassMetadatas[mRenderPassMetadataSpan.Begin + newAdjacentPassIndex].DependencyLevel = std::max(mTotalPassMetadatas[mRenderPassMetadataSpan.Begin + newAdjacentPassIndex].DependencyLevel, mainPassDependencyLevel + 1);
		}
	}
}

void ModernFrameGraphBuilder::SortRenderPassesByTopology(const std::vector<std::span<uint32_t>>& unsortedPassAdjacencyList, std::vector<uint32_t>& outPassIndexRemap)
{
	std::vector<uint8_t> traversalMarkFlags(mRenderPassMetadataSpan.End - mRenderPassMetadataSpan.Begin, 0);
	for(uint32_t passIndex = 0; passIndex < unsortedPassAdjacencyList.size(); passIndex++)
	{
		TopologicalSortNode(passIndex, unsortedPassAdjacencyList, traversalMarkFlags, outPassIndexRemap);
	}

	std::vector<PassMetadata> sortedPasses(mRenderPassMetadataSpan.End - mRenderPassMetadataSpan.Begin);
	for(uint32_t oldPassIndex = 0; oldPassIndex < outPassIndexRemap.size(); oldPassIndex++)
	{
		uint32_t& remappedPassIndex = outPassIndexRemap[oldPassIndex];

		remappedPassIndex = (uint32_t)(outPassIndexRemap.size() - remappedPassIndex - 1);
		sortedPasses[remappedPassIndex] = mTotalPassMetadatas[mRenderPassMetadataSpan.Begin + oldPassIndex];
	}
	
	for(size_t i = 0; i < sortedPasses.size(); i++) //Reverse the order and save the sorted order
	{
		mTotalPassMetadatas[mRenderPassMetadataSpan.Begin + i] = sortedPasses[i];
	}
}

void ModernFrameGraphBuilder::TopologicalSortNode(uint32_t passIndex, const std::vector<std::span<uint32_t>>& unsortedPassAdjacencyList, std::vector<uint8_t>& inoutTraversalMarkFlags, std::vector<uint32_t>& inoutPassIndexRemap)
{
	constexpr uint8_t VisitedFlag = 0x01;
	constexpr uint8_t OnStackFlag = 0x02;

	bool isVisited = inoutTraversalMarkFlags[passIndex] & VisitedFlag;

#if defined(DEBUG) || defined(_DEBUG)
	bool isOnStack = inoutTraversalMarkFlags[passIndex] & OnStackFlag;
	assert(!isVisited || !isOnStack); //Detect circular dependency
#endif

	if(isVisited)
	{
		return;
	}

	inoutTraversalMarkFlags[passIndex] = (VisitedFlag | OnStackFlag);

	std::span<uint32_t> passAdjacencyIndices = unsortedPassAdjacencyList[passIndex];
	for(uint32_t adjacentPassIndex: passAdjacencyIndices)
	{
		TopologicalSortNode(adjacentPassIndex, unsortedPassAdjacencyList, inoutTraversalMarkFlags, inoutPassIndexRemap);
	}

	inoutTraversalMarkFlags[passIndex] &= (~OnStackFlag);
	inoutPassIndexRemap.push_back(passIndex);
}

void ModernFrameGraphBuilder::SortRenderPassesByDependency()
{
	std::stable_sort(mTotalPassMetadatas.begin() + mRenderPassMetadataSpan.Begin, mTotalPassMetadatas.begin() + mRenderPassMetadataSpan.End, [](const PassMetadata& left, const PassMetadata& right)
	{
		return left.DependencyLevel < right.DependencyLevel;
	});
}

void ModernFrameGraphBuilder::InitAugmentedData()
{
	AdjustPassClasses();
	BuildPassSpans();

	ValidateSubresourceLinks();

	AmplifyResourcesAndPasses();
	InitMetadataPayloads();

	PropagateSubresourcePayloadData();
}

void ModernFrameGraphBuilder::AdjustPassClasses()
{
	//The loop will wrap around 0
	uint32_t renderPassIndex = mRenderPassMetadataSpan.End - 1;
	while(renderPassIndex >= mRenderPassMetadataSpan.Begin && renderPassIndex < mRenderPassMetadataSpan.End)
	{
		//Graphics queue only for now
		//Async compute passes should be in the end of the frame. BUT! They should be executed AT THE START of THE PREVIOUS frame. Need to reorder stuff for that
		mTotalPassMetadatas[renderPassIndex].Class = RenderPassClass::Graphics;

		renderPassIndex--;
	}
}

void ModernFrameGraphBuilder::BuildPassSpans()
{
	mGraphToBuild->mGraphicsPassSpansPerDependencyLevel.clear();

	uint32_t currentDependencyLevel = 0;
	mGraphToBuild->mGraphicsPassSpansPerDependencyLevel.push_back(Span<uint32_t>{.Begin = 0, .End = 0});
	for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
	{
		uint32_t dependencyLevel = mTotalPassMetadatas[passIndex].DependencyLevel;
		if(currentDependencyLevel != dependencyLevel)
		{
			mGraphToBuild->mGraphicsPassSpansPerDependencyLevel.push_back(Span<uint32_t>{.Begin = dependencyLevel, .End = dependencyLevel + 1});
			currentDependencyLevel = dependencyLevel;
		}
		else
		{
			mGraphToBuild->mGraphicsPassSpansPerDependencyLevel.back().End++;
		}
	}
}

void ModernFrameGraphBuilder::AmplifyResourcesAndPasses()
{
	std::vector<TextureRemapInfo> resourceRemapInfos(mResourceMetadatas.size(), {.BaseTextureIndex = (uint32_t)(-1), .FrameCount = 1});
	for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passIndex];
		for(uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < passMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
		{
			uint32_t resourceFrameCount = 1;

			uint32_t resourceIndex = mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex;
			assert(resourceRemapInfos[resourceIndex].FrameCount == 1 || resourceFrameCount == 1); //Do not allow M -> N connections between passes

			resourceRemapInfos[resourceIndex].FrameCount = std::lcm(resourceRemapInfos[resourceIndex].FrameCount, resourceFrameCount);
		}
	}

	static_assert((uint32_t)PresentPassSubresourceId::Count == 1);
	for(uint32_t passIndex = mPresentPassMetadataSpan.Begin; passIndex < mPresentPassMetadataSpan.End; passIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passIndex];
		for(uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < passMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
		{
			uint32_t resourceFrameCount = GetSwapchainImageCount();

			uint32_t swapchainResourceIndex = mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex;
			assert(resourceRemapInfos[swapchainResourceIndex].FrameCount == 1 || resourceFrameCount == 1); //Do not allow M -> N connections between passes

			resourceRemapInfos[swapchainResourceIndex].FrameCount = std::lcm(resourceRemapInfos[swapchainResourceIndex].FrameCount, resourceFrameCount);
		}
	}

	std::vector<ResourceMetadata> oldResourceMetadatas;
	mResourceMetadatas.swap(oldResourceMetadatas);

	std::vector<PassMetadata> oldPassMetadatas;
	mTotalPassMetadatas.swap(oldPassMetadatas);

	std::vector<SubresourceMetadataNode> oldSubresourceMetadatas;
	mSubresourceMetadataNodesFlat.swap(oldSubresourceMetadatas);


	//Amplify resources
	for(uint32_t resourceIndex = 0; resourceIndex < oldResourceMetadatas.size(); resourceIndex++)
	{
		resourceRemapInfos[resourceIndex].BaseTextureIndex = (uint32_t)mResourceMetadatas.size();
		if(resourceRemapInfos[resourceIndex].FrameCount == 1)
		{
			mResourceMetadatas.push_back(ResourceMetadata
			{
				.Name          = oldResourceMetadatas[resourceIndex].Name,
				.SourceType    = oldResourceMetadatas[resourceIndex].SourceType,
				.HeadNodeIndex = (uint32_t)(-1)
			});
		}
		else
		{
			for(uint32_t frameIndex = 0; frameIndex < resourceRemapInfos[resourceIndex].FrameCount; frameIndex++)
			{
				mResourceMetadatas.push_back(ResourceMetadata
				{
					.Name          = oldResourceMetadatas[resourceIndex].Name + "#" + std::to_string(frameIndex),
					.SourceType    = oldResourceMetadatas[resourceIndex].SourceType,
					.HeadNodeIndex = (uint32_t)(-1)
				});
			}
		}
	}


	Span<uint32_t> amplifiedRenderPassMetadataSpan = {.Begin = (uint32_t)mTotalPassMetadatas.size(), .End = (uint32_t)mTotalPassMetadatas.size()};
	for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
	{
		const PassMetadata& passMetadata = oldPassMetadatas[passIndex];
		std::span<const SubresourceMetadataNode> oldSubresourceMetadataSpan = {oldSubresourceMetadatas.begin() + passMetadata.SubresourceMetadataSpan.Begin, oldSubresourceMetadatas.begin() + passMetadata.SubresourceMetadataSpan.End};

		uint32_t                passFrameCount = 0;
		RenderPassFrameSwapType passSwapType   = RenderPassFrameSwapType::Constant;
		FindFrameCountAndSwapType(resourceRemapInfos, oldSubresourceMetadataSpan, &passFrameCount, &passSwapType);

		mGraphToBuild->mFrameSpansPerRenderPass.push_back
		({
			.Begin    = (uint32_t)mTotalPassMetadatas.size(),
			.End      = (uint32_t)mTotalPassMetadatas.size() + passFrameCount,
			.SwapType = passSwapType
		});

		for(uint32_t passFrameIndex = 0; passFrameIndex < passFrameCount; passFrameIndex++)
		{
			mTotalPassMetadatas.push_back(PassMetadata
			{
				.Name                    = passMetadata.Name + "#" + std::to_string(passFrameIndex),
				.Class                   = passMetadata.Class,
				.Type                    = passMetadata.Type,
				.DependencyLevel         = passMetadata.DependencyLevel,
				.SubresourceMetadataSpan = Span<uint32_t>
				{
					.Begin = 0,
					.End   = 0
				}
			});
		}
	}

	amplifiedRenderPassMetadataSpan.End = (uint32_t)mTotalPassMetadatas.size();
	mRenderPassMetadataSpan = amplifiedRenderPassMetadataSpan;

	Span<uint32_t> amplifiedPresentPassMetadataSpan = {.Begin = (uint32_t)mTotalPassMetadatas.size(), .End = (uint32_t)mTotalPassMetadatas.size()};
	for(uint32_t passIndex = mPresentPassMetadataSpan.Begin; passIndex < mPresentPassMetadataSpan.End; passIndex++)
	{
		const PassMetadata& passMetadata = oldPassMetadatas[passIndex];
		std::span<const SubresourceMetadataNode> oldSubresourceMetadataSpan = {oldSubresourceMetadatas.begin() + passMetadata.SubresourceMetadataSpan.Begin, oldSubresourceMetadatas.begin() + passMetadata.SubresourceMetadataSpan.End};

		uint32_t                passFrameCount = 0;
		RenderPassFrameSwapType passSwapType   = RenderPassFrameSwapType::Constant;
		FindFrameCountAndSwapType(resourceRemapInfos, oldSubresourceMetadataSpan, &passFrameCount, &passSwapType);

		mGraphToBuild->mFrameSpansPerRenderPass.push_back
		({
			.Begin    = (uint32_t)mTotalPassMetadatas.size(),
			.End      = (uint32_t)mTotalPassMetadatas.size() + passFrameCount,
			.SwapType = passSwapType
		});

		for(uint32_t passFrameIndex = 0; passFrameIndex < passFrameCount; passFrameIndex++)
		{
			mTotalPassMetadatas.push_back(PassMetadata
			{
				.Name                    = passMetadata.Name + "#" + std::to_string(passFrameIndex),
				.Class                   = passMetadata.Class,
				.Type                    = passMetadata.Type,
				.DependencyLevel         = passMetadata.DependencyLevel,
				.SubresourceMetadataSpan = Span<uint32_t>
				{
					.Begin = 0,
					.End   = 0
				}
			});
		}
	}

	amplifiedPresentPassMetadataSpan.End = (uint32_t)mTotalPassMetadatas.size();
	mPresentPassMetadataSpan = amplifiedPresentPassMetadataSpan;
	

	//Amplify subresources
	for(uint32_t oldPassIndex = 0; oldPassIndex < oldPassMetadatas.size(); oldPassIndex++)
	{
		const PassMetadata& oldPassMetadata = oldPassMetadatas[oldPassIndex];
		uint32_t passSubresourceCount = oldPassMetadata.SubresourceMetadataSpan.End - oldPassMetadata.SubresourceMetadataSpan.Begin;

		const ModernFrameGraph::PassFrameSpan& passFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[oldPassIndex];
		for(uint32_t passMetadataIndex = passFrameSpan.Begin; passMetadataIndex < passFrameSpan.End; passMetadataIndex++)
		{
			PassMetadata& newPassMetadata = mTotalPassMetadatas[passMetadataIndex];

			newPassMetadata.SubresourceMetadataSpan.Begin = (uint32_t)mSubresourceMetadataNodesFlat.size();
			newPassMetadata.SubresourceMetadataSpan.End   = (uint32_t)mSubresourceMetadataNodesFlat.size() + passSubresourceCount;

			mSubresourceMetadataNodesFlat.resize(newPassMetadata.SubresourceMetadataSpan.End, SubresourceMetadataNode
			{
				.PrevPassNodeIndex     = (uint32_t)(-1),
				.NextPassNodeIndex     = (uint32_t)(-1),
				.ResourceMetadataIndex = (uint32_t)(-1),
			    .ImageViewHandle       = (uint32_t)(-1),
				.PassClass             = newPassMetadata.Class,

#if defined(DEBUG) || defined(_DEBUG)
				.PassName     = newPassMetadata.Name,
				.ResourceName = ""
#endif
			});
		}
	}

	std::vector<uint32_t> oldPassIndicesPerSubresource(oldSubresourceMetadatas.size());
	for(uint32_t oldPassIndex = 0; oldPassIndex < oldPassMetadatas.size(); oldPassIndex++)
	{
		const PassMetadata& oldPassMetadata = oldPassMetadatas[oldPassIndex];
		for(uint32_t oldSubresourceIndex = oldPassMetadata.SubresourceMetadataSpan.Begin; oldSubresourceIndex < oldPassMetadata.SubresourceMetadataSpan.End; oldSubresourceIndex++)
		{
			oldPassIndicesPerSubresource[oldSubresourceIndex] = oldPassIndex;
		}
	}

	mPrimarySubresourceNodeSpan.Begin = 0;
	mPrimarySubresourceNodeSpan.End   = (uint32_t)mSubresourceMetadataNodesFlat.size();

	mHelperNodeSpansPerPassSubresource.resize(mPrimarySubresourceNodeSpan.End - mPrimarySubresourceNodeSpan.Begin, Span<uint32_t>{.Begin = mPrimarySubresourceNodeSpan.End, .End = mPrimarySubresourceNodeSpan.End});
	for(uint32_t nonAmplifiedPassIndex = 0; nonAmplifiedPassIndex < mGraphToBuild->mFrameSpansPerRenderPass.size(); nonAmplifiedPassIndex++)
	{
		const PassMetadata& oldPassMetadata                  = oldPassMetadatas[nonAmplifiedPassIndex];
		const ModernFrameGraph::PassFrameSpan& passFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[nonAmplifiedPassIndex];

		uint32_t passSubresourceCount = oldPassMetadata.SubresourceMetadataSpan.End - oldPassMetadata.SubresourceMetadataSpan.Begin;
		for(uint32_t passIndex = passFrameSpan.Begin; passIndex < passFrameSpan.End; passIndex++)
		{
			PassMetadata& newPassMetadata = mTotalPassMetadatas[passIndex];
			uint32_t passFrameIndex = passIndex - passFrameSpan.Begin;

			for(uint32_t subresourceId = 0; subresourceId < passSubresourceCount; subresourceId++)
			{
				uint32_t oldSubresourceIndex = oldPassMetadata.SubresourceMetadataSpan.Begin + subresourceId;

				uint32_t oldResourceIndex = oldSubresourceMetadatas[oldSubresourceIndex].ResourceMetadataIndex;
				uint32_t newResourceIndex = resourceRemapInfos[oldResourceIndex].BaseTextureIndex + passFrameIndex % resourceRemapInfos[oldResourceIndex].FrameCount;

				uint32_t newSubresourceIndex = newPassMetadata.SubresourceMetadataSpan.Begin + subresourceId;
				mSubresourceMetadataNodesFlat[newSubresourceIndex].ResourceMetadataIndex = newResourceIndex;

				if(mSubresourceMetadataNodesFlat[newSubresourceIndex].PrevPassNodeIndex == (uint32_t)(-1))
				{
					const SubresourceMetadataNode& oldSubresourceMetadata = oldSubresourceMetadatas[oldSubresourceIndex];
					uint32_t oldPrevPassIndex = oldPassIndicesPerSubresource[oldSubresourceMetadata.PrevPassNodeIndex];

					uint32_t prevPassSubresourceId   = oldSubresourceMetadata.PrevPassNodeIndex - oldPassMetadatas[oldPrevPassIndex].SubresourceMetadataSpan.Begin;
					uint32_t newPrevSubresourceIndex = CalcAmplifiedPrevSubresourceIndex(nonAmplifiedPassIndex, oldPrevPassIndex, passFrameIndex, prevPassSubresourceId);

					mSubresourceMetadataNodesFlat[newSubresourceIndex].PrevPassNodeIndex = newPrevSubresourceIndex;
					mSubresourceMetadataNodesFlat[newPrevSubresourceIndex].NextPassNodeIndex = newSubresourceIndex;
				}

				if(mSubresourceMetadataNodesFlat[newSubresourceIndex].NextPassNodeIndex == (uint32_t)(-1))
				{
					const SubresourceMetadataNode& oldSubresourceMetadata = oldSubresourceMetadatas[oldSubresourceIndex];
					uint32_t oldNextPassIndex = oldPassIndicesPerSubresource[oldSubresourceMetadata.NextPassNodeIndex];

					uint32_t nextPassSubresourceId = oldSubresourceMetadata.NextPassNodeIndex - oldPassMetadatas[oldNextPassIndex].SubresourceMetadataSpan.Begin;
					uint32_t newNextSubresourceIndex = CalcAmplifiedNextSubresourceIndex(nonAmplifiedPassIndex, oldNextPassIndex, passFrameIndex, nextPassSubresourceId);

					mSubresourceMetadataNodesFlat[newSubresourceIndex].NextPassNodeIndex = newNextSubresourceIndex;
					mSubresourceMetadataNodesFlat[newNextSubresourceIndex].PrevPassNodeIndex = newSubresourceIndex;
				}

#if defined(DEBUG) || defined(_DEBUG)
				mSubresourceMetadataNodesFlat[newSubresourceIndex].ResourceName = mResourceMetadatas[newResourceIndex].Name;
#endif
			}
		}
	}


	//Fix resource metadatas' head node index
	for(uint32_t metadataIndex = 0; metadataIndex < mSubresourceMetadataNodesFlat.size(); metadataIndex++)
	{
		SubresourceMetadataNode& subresourceMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
		if(mResourceMetadatas[subresourceMetadataNode.ResourceMetadataIndex].HeadNodeIndex == (uint32_t)(-1))
		{
			mResourceMetadatas[subresourceMetadataNode.ResourceMetadataIndex].HeadNodeIndex = metadataIndex;
		}
	}
}

void ModernFrameGraphBuilder::FindFrameCountAndSwapType(const std::vector<TextureRemapInfo>& resourceRemapInfos, std::span<const SubresourceMetadataNode> oldPassSubresourceMetadataSpan, uint32_t* outFrameCount, RenderPassFrameSwapType* outSwapType)
{
	assert(outFrameCount != nullptr);
	assert(outSwapType   != nullptr);

	RenderPassFrameSwapType swapType = RenderPassFrameSwapType::Constant;

	uint32_t passFrameCount = 1;
	for(const SubresourceMetadataNode& subresourceMetadataNode: oldPassSubresourceMetadataSpan)
	{
		uint32_t resourceIndex = subresourceMetadataNode.ResourceMetadataIndex;
		passFrameCount = std::lcm(resourceRemapInfos[resourceIndex].FrameCount, passFrameCount);

		RenderPassFrameSwapType resourceSwapType = RenderPassFrameSwapType::Constant;
		if(resourceRemapInfos[resourceIndex].FrameCount > 1)
		{
			if(mResourceMetadatas[resourceRemapInfos[resourceIndex].BaseTextureIndex].SourceType == TextureSourceType::Backbuffer)
			{
				resourceSwapType = RenderPassFrameSwapType::PerBackbufferImage;
			}
			else
			{
				resourceSwapType = RenderPassFrameSwapType::PerLinearFrame;
			}

			//Do not allow resources with different swap types
			assert(swapType == RenderPassFrameSwapType::Constant || resourceSwapType == swapType);
			swapType = resourceSwapType;
		}
	}

	*outSwapType   = swapType;
	*outFrameCount = passFrameCount;
}

uint32_t ModernFrameGraphBuilder::CalcAmplifiedPrevSubresourceIndex(uint32_t currPassFrameSpanIndex, uint32_t prevPassFrameSpanIndex, uint32_t currPassFrameIndex, uint32_t prevPassSubresourceId)
{
	/*
	*    1. Curr pass frames == prev pass frames:  Prev frame index is the same as curr frame index
	*
    *    2. Prev pass was in the same frame, curr pass frames == 1, prev pass frames == N:                         Prev frame index is the first one in the prev frame span
    *    3. Prev pass was in the same frame, curr pass frames == N, prev pass frames == 1, curr frame index == 0:  Prev frame index is the only one in the prev frame span
    *    4. Prev pass was in the same frame, curr pass frames == N, prev pass frames == 1, curr frame index != 0:  Choose a helper prev node with the same index as the curr frame index
	* 
    *    5. Prev pass was in the prev frame, curr pass frames == 1, prev frames == N:                         Prev frame index is the last one in the prev frame span
    *    6. Prev pass was in the prev frame, curr pass frames == N, prev frames == 1, curr frame index == 1:  Prev frame index is the only one in the prev frame span
    *    7. Prev pass was in the prev frame, curr pass frames == N, prev frames == 1, curr frame index != 1:  Choose a helper prev node with the same index as (curr frame index + N - 1) % N
	*/

	const ModernFrameGraph::PassFrameSpan& currPassFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[currPassFrameSpanIndex];
	const ModernFrameGraph::PassFrameSpan& prevPassFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[prevPassFrameSpanIndex];

	uint32_t currPassFrameCount = currPassFrameSpan.End - currPassFrameSpan.Begin;
	uint32_t prevPassFrameCount = prevPassFrameSpan.End - prevPassFrameSpan.Begin;

	if(currPassFrameCount == prevPassFrameCount) //Rule 1
	{
		const PassMetadata& prevPassMetadata = mTotalPassMetadatas[(size_t)prevPassFrameSpan.Begin + currPassFrameIndex];
		return prevPassMetadata.SubresourceMetadataSpan.Begin + prevPassSubresourceId;
	}
	else if(prevPassFrameSpanIndex <= currPassFrameSpanIndex) //Rules 2, 3, 4
	{
		const PassMetadata& prevPassMetadata = mTotalPassMetadatas[(size_t)prevPassFrameSpan.Begin];
		if(currPassFrameIndex == 0) //Rules 2, 3
		{
			return prevPassMetadata.SubresourceMetadataSpan.Begin + prevPassSubresourceId;
		}
		else //Rule 4
		{
			uint32_t prevSubresourceIndex = prevPassMetadata.SubresourceMetadataSpan.Begin + prevPassSubresourceId;

			Span<uint32_t>& helperSubresourceSpan = mHelperNodeSpansPerPassSubresource[prevSubresourceIndex];
			if(helperSubresourceSpan.End == helperSubresourceSpan.Begin)
			{
				//Create new helper node span with (PassFrameCount - 1) helper nodes
				SubresourceMetadataNode templateSubresource = mSubresourceMetadataNodesFlat[prevSubresourceIndex];
				templateSubresource.PrevPassNodeIndex = (uint32_t)(-1);
				templateSubresource.NextPassNodeIndex = (uint32_t)(-1);

				helperSubresourceSpan.Begin = (uint32_t)mSubresourceMetadataNodesFlat.size();
				helperSubresourceSpan.End   = (uint32_t)mSubresourceMetadataNodesFlat.size() + currPassFrameCount - 1;

				mSubresourceMetadataNodesFlat.resize(helperSubresourceSpan.End, templateSubresource);
			}

			assert(helperSubresourceSpan.End - helperSubresourceSpan.Begin == currPassFrameCount - 1);
			return helperSubresourceSpan.Begin + currPassFrameIndex - 1;
		}
	}
	else //Rules 5, 6, 7
	{
		if(currPassFrameCount == 1) //Rule 5
		{
			const PassMetadata& prevPassMetadata = mTotalPassMetadatas[(size_t)prevPassFrameSpan.End - 1];
			return prevPassMetadata.SubresourceMetadataSpan.Begin + prevPassSubresourceId;
		}
		else //Rules 6, 7
		{
			const PassMetadata& prevPassMetadata = mTotalPassMetadatas[(size_t)prevPassFrameSpan.Begin];
			if(currPassFrameIndex == 1) //Rule 6
			{
				return prevPassMetadata.SubresourceMetadataSpan.Begin + prevPassSubresourceId;
			}
			else //Rule 7
			{
				uint32_t prevSubresourceIndex = prevPassMetadata.SubresourceMetadataSpan.Begin + prevPassSubresourceId;

				Span<uint32_t>& helperSubresourceSpan = mHelperNodeSpansPerPassSubresource[prevSubresourceIndex];
				if(helperSubresourceSpan.End == helperSubresourceSpan.Begin)
				{
					//Create new helper node span with (PassFrameCount - 1) helper nodes
					SubresourceMetadataNode templateSubresource = mSubresourceMetadataNodesFlat[prevSubresourceIndex];
					templateSubresource.PrevPassNodeIndex = (uint32_t)(-1);
					templateSubresource.NextPassNodeIndex = (uint32_t)(-1);

					helperSubresourceSpan.Begin = (uint32_t)mSubresourceMetadataNodesFlat.size();
					helperSubresourceSpan.End   = (uint32_t)mSubresourceMetadataNodesFlat.size() + currPassFrameCount - 1;

					mSubresourceMetadataNodesFlat.resize(helperSubresourceSpan.End, templateSubresource);
				}

				assert(helperSubresourceSpan.End - helperSubresourceSpan.Begin == currPassFrameCount - 1);

				uint32_t newPrevSubresourceIndex = helperSubresourceSpan.Begin + (currPassFrameIndex + currPassFrameCount - 1) % currPassFrameCount - 1;
				return newPrevSubresourceIndex;
			}
		}
	}
}

uint32_t ModernFrameGraphBuilder::CalcAmplifiedNextSubresourceIndex(uint32_t currPassFrameSpanIndex, uint32_t nextPassFrameSpanIndex, uint32_t currPassFrameIndex, uint32_t nextPassSubresourceId)
{
	/*
	*    1. Curr pass frames == next pass frames:  Next pass index is the same as curr pass index
	*
	*    2. Next pass is in the same frame, curr pass frames == 1, next pass frames == N:                         Prev frame index is the first one in the next frame span
    *    3. Next pass is in the same frame, curr pass frames == N, next pass frames == 1, curr frame index == 0:  Next frame index is the only one in the next frame span
    *    4. Next pass is in the same frame, curr pass frames == N, next pass frames == 1, curr frame index != 0:  Choose a helper next node with the same index as the curr frame index
	*
    *    5. Next pass is in the next frame, curr pass frames == 1, next pass frames == N:                             Prev frame index is the second one in the next frame span
    *    6. Next pass is in the next frame, curr pass frames == N, next pass frames == 1, curr frame index == N - 1:  Next frame index is the only one in the next frame span
    *    7. Next pass is in the next frame, curr pass frames == N, next pass frames == 1, curr frame index != N - 1:  Choose a helper next node with the same index as (curr frame index + 1) % N
	*/

	const ModernFrameGraph::PassFrameSpan& currPassFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[currPassFrameSpanIndex];
	const ModernFrameGraph::PassFrameSpan& nextPassFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[nextPassFrameSpanIndex];

	uint32_t currPassFrameCount = currPassFrameSpan.End - currPassFrameSpan.Begin;
	uint32_t nextPassFrameCount = nextPassFrameSpan.End - nextPassFrameSpan.Begin;

	if(currPassFrameCount == nextPassFrameCount) //Rule 1
	{
		const PassMetadata& nextPassMetadata = mTotalPassMetadatas[(size_t)nextPassFrameSpan.Begin + currPassFrameIndex];
		return nextPassMetadata.SubresourceMetadataSpan.Begin + nextPassSubresourceId;
	}
	else if(nextPassFrameSpanIndex >= currPassFrameSpanIndex) //Rules 2, 3, 4
	{
		const PassMetadata& nextPassMetadata = mTotalPassMetadatas[(size_t)nextPassFrameSpan.Begin];
		if(currPassFrameIndex == 0) //Rules 2, 3
		{
			return nextPassMetadata.SubresourceMetadataSpan.Begin + nextPassSubresourceId;
		}
		else //Rule 4
		{
			uint32_t nextSubresourceIndex = nextPassMetadata.SubresourceMetadataSpan.Begin + nextPassSubresourceId;

			Span<uint32_t>& helperSubresourceSpan = mHelperNodeSpansPerPassSubresource[nextSubresourceIndex];
			if(helperSubresourceSpan.End == helperSubresourceSpan.Begin)
			{
				//Create new helper node span with (PassFrameCount - 1) helper nodes
				SubresourceMetadataNode templateSubresource = mSubresourceMetadataNodesFlat[nextSubresourceIndex];
				templateSubresource.PrevPassNodeIndex = (uint32_t)(-1);
				templateSubresource.NextPassNodeIndex = (uint32_t)(-1);

				helperSubresourceSpan.Begin = (uint32_t)mSubresourceMetadataNodesFlat.size();
				helperSubresourceSpan.End   = (uint32_t)mSubresourceMetadataNodesFlat.size() + currPassFrameCount - 1;

				mSubresourceMetadataNodesFlat.resize(helperSubresourceSpan.End, templateSubresource);
			}

			assert(helperSubresourceSpan.End - helperSubresourceSpan.Begin == currPassFrameCount - 1);

			uint32_t newNextSubresourceIndex = helperSubresourceSpan.Begin + currPassFrameIndex - 1;
			return newNextSubresourceIndex;
		}
	}
	else //Rules 5, 6, 7
	{
		if(currPassFrameCount == 1) //Rule 5
		{
			const PassMetadata& nextPassMetadata = mTotalPassMetadatas[(size_t)nextPassFrameSpan.Begin + 1];
			return nextPassMetadata.SubresourceMetadataSpan.Begin + nextPassSubresourceId;
		}
		else //Rules 6, 7
		{
			const PassMetadata& nextPassMetadata = mTotalPassMetadatas[(size_t)nextPassFrameSpan.Begin];
			if(currPassFrameIndex == currPassFrameCount - 1) //Rule 6
			{
				return nextPassMetadata.SubresourceMetadataSpan.Begin + nextPassSubresourceId;
			}
			else //Rule 7
			{
				uint32_t nextSubresourceIndex = nextPassMetadata.SubresourceMetadataSpan.Begin + nextPassSubresourceId;

				Span<uint32_t>& helperSubresourceSpan = mHelperNodeSpansPerPassSubresource[nextSubresourceIndex];
				if(helperSubresourceSpan.End == helperSubresourceSpan.Begin)
				{
					//Create new helper node span with (PassFrameCount - 1) helper nodes
					SubresourceMetadataNode templateSubresource = mSubresourceMetadataNodesFlat[nextSubresourceIndex];
					templateSubresource.PrevPassNodeIndex = (uint32_t)(-1);
					templateSubresource.NextPassNodeIndex = (uint32_t)(-1);

					helperSubresourceSpan.Begin = (uint32_t)mSubresourceMetadataNodesFlat.size();
					helperSubresourceSpan.End   = (uint32_t)mSubresourceMetadataNodesFlat.size() + currPassFrameCount - 1;

					mSubresourceMetadataNodesFlat.resize(helperSubresourceSpan.End, templateSubresource);
				}

				assert(helperSubresourceSpan.End - helperSubresourceSpan.Begin == currPassFrameCount - 1);

				uint32_t newNextSubresourceIndex = helperSubresourceSpan.Begin + (currPassFrameIndex + 1) % currPassFrameCount - 1;
				return newNextSubresourceIndex;
			}
		}
	}
}

void ModernFrameGraphBuilder::ValidateSubresourceLinks()
{
	//Connect previous passes, storing the last subresource index for the resource in HeadNodeIndex (it will be rewritten later in AmplifyResourcesAndPasses())
	for(const PassMetadata& passMetadata: mTotalPassMetadatas)
	{
		for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
			ResourceMetadata& resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];

			if(resourceMetadata.HeadNodeIndex != (uint32_t)(-1))
			{
				SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[resourceMetadata.HeadNodeIndex];

				prevSubresourceNode.NextPassNodeIndex = metadataIndex;
				metadataNode.PrevPassNodeIndex        = resourceMetadata.HeadNodeIndex;
			}
			else
			{
				resourceMetadata.HeadNodeIndex = metadataIndex;
			}

			resourceMetadata.HeadNodeIndex = metadataIndex;
		}
	}

	//Validate cyclic links (so first pass knows what state to barrier from, and the last pass knows what state to barrier to)
	for(uint32_t metadataIndex = 0; metadataIndex < mSubresourceMetadataNodesFlat.size(); metadataIndex++)
	{
		SubresourceMetadataNode& subresourceNode = mSubresourceMetadataNodesFlat[metadataIndex];
		ResourceMetadata& resourceMetadata = mResourceMetadatas[subresourceNode.ResourceMetadataIndex];

		if(subresourceNode.NextPassNodeIndex == (uint32_t)(-1))
		{
			uint32_t nextNodeIndex = resourceMetadata.HeadNodeIndex;
			SubresourceMetadataNode& nextSubresourceMetadata = mSubresourceMetadataNodesFlat[nextNodeIndex];

			subresourceNode.NextPassNodeIndex         = nextNodeIndex;
			nextSubresourceMetadata.PrevPassNodeIndex = metadataIndex;
		}

		if(subresourceNode.PrevPassNodeIndex == (uint32_t)(-1))
		{
			uint32_t prevNodeIndex = resourceMetadata.HeadNodeIndex;
			SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[prevNodeIndex];

			subresourceNode.PrevPassNodeIndex     = prevNodeIndex;
			prevSubresourceNode.NextPassNodeIndex = metadataIndex;
		}
	}
}

void ModernFrameGraphBuilder::PropagateSubresourcePayloadData()
{
	//Repeat until propagation stops happening
	bool propagationHappened = true;
	while(propagationHappened)
	{
		propagationHappened = false;

		for(const ResourceMetadata& resourceMetadata: mResourceMetadatas)
		{
			propagationHappened |= PropagateSubresourcePayloadDataVertically(resourceMetadata);
		}

		for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
		{
			const PassMetadata& passMetadata = mTotalPassMetadatas[passIndex];
			propagationHappened |= PropagateSubresourcePayloadDataHorizontally(passMetadata);
		}
	}
}

void ModernFrameGraphBuilder::BuildResources()
{
	CreateTextures();
	CreateTextureViews();
}

void ModernFrameGraphBuilder::BuildBarriers()
{
	mGraphToBuild->mRenderPassBarriers.resize(mTotalPassMetadatas.size(), ModernFrameGraph::BarrierPassSpan
	{
		.BeforePassBegin = 0,
		.BeforePassEnd   = 0,
		.AfterPassBegin  = 0,
		.AfterPassEnd    = 0
	});

	for(uint32_t passIndex = 0; passIndex < mTotalPassMetadatas.size(); passIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passIndex];

		AddBeforePassBarriers(passMetadata, passIndex);
		AddAfterPassBarriers(passMetadata,  passIndex);
	}
}