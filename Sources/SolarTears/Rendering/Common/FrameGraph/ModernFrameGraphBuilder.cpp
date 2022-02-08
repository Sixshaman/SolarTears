#include "ModernFrameGraphBuilder.hpp"
#include "ModernFrameGraph.hpp"
#include "RenderPassDispatchFuncs.hpp"
#include "../../../Core/Utils/MockSpan.hpp"
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

	InitAugmentedPassData();

	ValidateSubresourceLinkedLists();
	AmplifyResourcesAndPasses();

	InitAugmentedResourceData();

	BuildResources();
	BuildPassObjects();
	BuildBarriers();
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

			.PassClass = mTotalPassMetadatas.back().Class,
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

		.PassClass = RenderPassClass::Present,
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

	//Record the per-pass subresource indices for all subresources passed in the description
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

	std::vector<uint_fast16_t> tempSubresourceIds; //The temporary buffer to write the pass subresource ids to
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

		//We can't store any references to outIndicesFlat right now, because it can reallocate at any moment and invalidate all references. Use mock spans for now
		std::span readSpan  = Utils::CreateMockSpan<uint32_t>(readBeginOffset,  readEndOffset);
		std::span writeSpan = Utils::CreateMockSpan<uint32_t>(writeBeginOffset, writeEndOffset);

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

		outReadIndexSpans.push_back(readSpan);
		outWriteIndexSpans.push_back(writeSpan);
	}

	//Validate and sort spans, outIndicesFlat won't change anymore
	for(uint32_t readSpanIndex = 0; readSpanIndex < outReadIndexSpans.size(); readSpanIndex++)
	{
		outReadIndexSpans[readSpanIndex] = Utils::ValidateMockSpan(outReadIndexSpans[readSpanIndex], outIndicesFlat.begin());
		std::sort(outReadIndexSpans[readSpanIndex].begin(), outReadIndexSpans[readSpanIndex].end());
	}

	for(uint32_t writeSpanIndex = 0; writeSpanIndex < outWriteIndexSpans.size(); writeSpanIndex++)
	{
		outWriteIndexSpans[writeSpanIndex] = Utils::ValidateMockSpan(outWriteIndexSpans[writeSpanIndex], outIndicesFlat.begin());
		std::sort(outWriteIndexSpans[writeSpanIndex].begin(), outWriteIndexSpans[writeSpanIndex].end());
	}
}

void ModernFrameGraphBuilder::BuildAdjacencyList(const std::vector<std::span<uint32_t>>& sortedReadNameSpansPerPass, const std::vector<std::span<uint32_t>>& sortedwriteNameSpansPerPass, std::vector<std::span<uint32_t>>& outAdjacencyList, std::vector<uint32_t>& outAdjacentPassIndicesFlat)
{
	outAdjacencyList.resize(mRenderPassMetadataSpan.End - mRenderPassMetadataSpan.Begin);
	outAdjacentPassIndicesFlat.clear();

	for(uint32_t writePassIndex = 0; writePassIndex < sortedwriteNameSpansPerPass.size(); writePassIndex++)
	{
		std::span<uint32_t> writtenResourceIndices = sortedwriteNameSpansPerPass[writePassIndex];

		size_t oldSize = outAdjacentPassIndicesFlat.size();
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

		size_t newSize = outAdjacentPassIndicesFlat.size();
		outAdjacencyList[writePassIndex] = Utils::CreateMockSpan<uint_fast16_t>(oldSize, newSize); //We have to use mock spans here too, for the same reasons as in BuildReadWriteSubresourceSpans
	}

	//Validate mock spans
	for(uint32_t passIndex = 0; passIndex < outAdjacencyList.size(); passIndex++)
	{
		outAdjacencyList[passIndex] = Utils::ValidateMockSpan(outAdjacencyList[passIndex], outAdjacentPassIndicesFlat.begin());
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
	constexpr uint8_t AlreadyVisitedFlag     = 0x01;
	constexpr uint8_t CurrentlyProcessedFlag = 0x02;

	bool isVisited = inoutTraversalMarkFlags[passIndex] & AlreadyVisitedFlag;

#if defined(DEBUG) || defined(_DEBUG)
	bool isCurrentlyProcessed = inoutTraversalMarkFlags[passIndex] & CurrentlyProcessedFlag;
	assert(!isVisited || !isCurrentlyProcessed); //Detect circular dependency
#endif

	if(isVisited)
	{
		return;
	}

	inoutTraversalMarkFlags[passIndex] = (AlreadyVisitedFlag | CurrentlyProcessedFlag);

	std::span<uint32_t> passAdjacencyIndices = unsortedPassAdjacencyList[passIndex];
	for(uint32_t adjacentPassIndex: passAdjacencyIndices)
	{
		TopologicalSortNode(adjacentPassIndex, unsortedPassAdjacencyList, inoutTraversalMarkFlags, inoutPassIndexRemap);
	}

	inoutTraversalMarkFlags[passIndex] &= (~CurrentlyProcessedFlag);
	inoutPassIndexRemap.push_back(passIndex);
}

void ModernFrameGraphBuilder::SortRenderPassesByDependency()
{
	std::stable_sort(mTotalPassMetadatas.begin() + mRenderPassMetadataSpan.Begin, mTotalPassMetadatas.begin() + mRenderPassMetadataSpan.End, [](const PassMetadata& left, const PassMetadata& right)
	{
		return left.DependencyLevel < right.DependencyLevel;
	});
}

void ModernFrameGraphBuilder::InitAugmentedPassData()
{
	AdjustPassClasses();
	BuildPassSpans();
}

void ModernFrameGraphBuilder::AdjustPassClasses()
{
	//The loop wraps around 0
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

void ModernFrameGraphBuilder::ValidateSubresourceLinkedLists()
{
	std::vector<uint32_t> lastSubresourceNodeIndices(mResourceMetadatas.size(), (uint32_t)(-1));
	for(const PassMetadata& passMetadata: mTotalPassMetadatas)
	{
		for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];

			uint32_t lastSubresourceIndex = lastSubresourceNodeIndices[metadataNode.ResourceMetadataIndex];
			if(lastSubresourceIndex != (uint32_t)(-1))
			{
				SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[lastSubresourceIndex];

				prevSubresourceNode.NextPassNodeIndex = metadataIndex;
				metadataNode.PrevPassNodeIndex        = lastSubresourceIndex;
			}

			lastSubresourceNodeIndices[metadataNode.ResourceMetadataIndex] = metadataIndex;
		}
	}

	//Validate cyclic links (last <-> first nodes)
	for(uint32_t metadataIndex = 0; metadataIndex < mSubresourceMetadataNodesFlat.size(); metadataIndex++)
	{
		SubresourceMetadataNode& subresourceNode = mSubresourceMetadataNodesFlat[metadataIndex];
		if(subresourceNode.PrevPassNodeIndex == (uint32_t)(-1))
		{
			uint32_t lastNodeIndex = lastSubresourceNodeIndices[subresourceNode.ResourceMetadataIndex];
			SubresourceMetadataNode& lastSubresourceNode = mSubresourceMetadataNodesFlat[lastNodeIndex];

			subresourceNode.PrevPassNodeIndex     = lastNodeIndex;
			lastSubresourceNode.NextPassNodeIndex = metadataIndex;
		}
	}
}


void ModernFrameGraphBuilder::AmplifyResourcesAndPasses()
{
	std::vector<Span<uint32_t>> amplifiedResourceSpans(mResourceMetadatas.size(), {.Begin = 0, .End = 1});
	for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passIndex];
		for(uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < passMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
		{
			uint32_t resourceIndex = mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex;

			uint32_t resourceFrameCount = 1;
			uint32_t recordedFrameCount = amplifiedResourceSpans[resourceIndex].End - amplifiedResourceSpans[resourceIndex].Begin;

			assert(recordedFrameCount == 1 || resourceFrameCount == 1); //Do not allow M -> N connections between passes
			
			uint32_t newFrameCount = std::lcm(recordedFrameCount, resourceFrameCount);
			amplifiedResourceSpans[resourceIndex].End = amplifiedResourceSpans[resourceIndex].Begin + newFrameCount;
		}
	}

	static_assert((uint32_t)PresentPassSubresourceId::Count == 1);
	for(uint32_t passIndex = mPresentPassMetadataSpan.Begin; passIndex < mPresentPassMetadataSpan.End; passIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passIndex];
		for(uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < passMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
		{
			uint32_t swapchainResourceIndex = mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex;

			uint32_t swapchainResourceFrameCount = GetSwapchainImageCount();
			uint32_t swapchainRecordedFrameCount = amplifiedResourceSpans[swapchainResourceIndex].End - amplifiedResourceSpans[swapchainResourceIndex].Begin;

			assert(swapchainResourceFrameCount == 1 || swapchainRecordedFrameCount == 1); //Do not allow M -> N connections between passes

			uint32_t newSwapchainFrameCount = std::lcm(swapchainResourceFrameCount, swapchainRecordedFrameCount);
			amplifiedResourceSpans[swapchainResourceIndex].End = amplifiedResourceSpans[swapchainResourceIndex].Begin + newSwapchainFrameCount;
		}
	}

	std::vector<ResourceMetadata> nonAmplifiedResourceMetadatas;
	mResourceMetadatas.swap(nonAmplifiedResourceMetadatas);

	std::vector<PassMetadata> nonAmplifiedPassMetadatas;
	mTotalPassMetadatas.swap(nonAmplifiedPassMetadatas);

	std::vector<SubresourceMetadataNode> nonAmplifiedSubresourceMetadatas;
	mSubresourceMetadataNodesFlat.swap(nonAmplifiedSubresourceMetadatas);


	//Amplify resources
	for(uint32_t resourceIndex = 0; resourceIndex < nonAmplifiedResourceMetadatas.size(); resourceIndex++)
	{
		//Extract the frame count from the span
		uint32_t frameCount = amplifiedResourceSpans[resourceIndex].End - amplifiedResourceSpans[resourceIndex].Begin;

		amplifiedResourceSpans[resourceIndex].Begin = (uint32_t)mResourceMetadatas.size();
		amplifiedResourceSpans[resourceIndex].End   = amplifiedResourceSpans[resourceIndex].Begin + frameCount;

		if(frameCount == 1)
		{
			mResourceMetadatas.push_back(ResourceMetadata
			{
				.Name          = nonAmplifiedResourceMetadatas[resourceIndex].Name,
				.SourceType    = nonAmplifiedResourceMetadatas[resourceIndex].SourceType,
				.HeadNodeIndex = (uint32_t)(-1)
			});
		}
		else
		{
			for(uint32_t frameIndex = 0; frameIndex < frameCount; frameIndex++)
			{
				mResourceMetadatas.push_back(ResourceMetadata
				{
					.Name          = nonAmplifiedResourceMetadatas[resourceIndex].Name + "#" + std::to_string(frameIndex),
					.SourceType    = nonAmplifiedResourceMetadatas[resourceIndex].SourceType,
					.HeadNodeIndex = (uint32_t)(-1)
				});
			}
		}
	}


	//Amplify render passes
	Span<uint32_t> amplifiedRenderPassMetadataSpan = {.Begin = (uint32_t)mTotalPassMetadatas.size(), .End = (uint32_t)mTotalPassMetadatas.size()};
	for(uint32_t nonAmplifiedPassIndex = mRenderPassMetadataSpan.Begin; nonAmplifiedPassIndex < mRenderPassMetadataSpan.End; nonAmplifiedPassIndex++)
	{
		const PassMetadata& nonAmplifiedPassMetadata = nonAmplifiedPassMetadatas[nonAmplifiedPassIndex];
		std::span<const SubresourceMetadataNode> nonAmplifiedPassSubresources = {nonAmplifiedSubresourceMetadatas.begin() + nonAmplifiedPassMetadata.SubresourceMetadataSpan.Begin, nonAmplifiedSubresourceMetadatas.begin() + nonAmplifiedPassMetadata.SubresourceMetadataSpan.End};

		uint32_t                passFrameCount = 0;
		RenderPassFrameSwapType passSwapType   = RenderPassFrameSwapType::Constant;
		FindFrameCountAndSwapType(amplifiedResourceSpans, nonAmplifiedPassSubresources, &passFrameCount, &passSwapType);

		//Add the amplified pass span
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
				.Name                    = nonAmplifiedPassMetadata.Name + "#" + std::to_string(passFrameIndex),
				.Class                   = nonAmplifiedPassMetadata.Class,
				.Type                    = nonAmplifiedPassMetadata.Type,
				.DependencyLevel         = nonAmplifiedPassMetadata.DependencyLevel,
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
	for(uint32_t nonAmplifiedPresentPassIndex = mPresentPassMetadataSpan.Begin; nonAmplifiedPresentPassIndex < mPresentPassMetadataSpan.End; nonAmplifiedPresentPassIndex++)
	{
		const PassMetadata& nonAmplifiedPresentPassMetadata = nonAmplifiedPassMetadatas[nonAmplifiedPresentPassIndex];
		std::span<const SubresourceMetadataNode> nonAmplifiedPresentSubresourceMetadatas = {nonAmplifiedSubresourceMetadatas.begin() + nonAmplifiedPresentPassMetadata.SubresourceMetadataSpan.Begin, nonAmplifiedSubresourceMetadatas.begin() + nonAmplifiedPresentPassMetadata.SubresourceMetadataSpan.End};

		uint32_t                passFrameCount = 0;
		RenderPassFrameSwapType passSwapType   = RenderPassFrameSwapType::Constant;
		FindFrameCountAndSwapType(amplifiedResourceSpans, nonAmplifiedPresentSubresourceMetadatas, &passFrameCount, &passSwapType);

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
				.Name                    = nonAmplifiedPresentPassMetadata.Name + "#" + std::to_string(passFrameIndex),
				.Class                   = nonAmplifiedPresentPassMetadata.Class,
				.Type                    = nonAmplifiedPresentPassMetadata.Type,
				.DependencyLevel         = nonAmplifiedPresentPassMetadata.DependencyLevel,
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
	

	//Amplify subresources: allocate the subresource node spans for amplified passes
	for(uint32_t nonAmplifiedPassIndex = 0; nonAmplifiedPassIndex < nonAmplifiedPassMetadatas.size(); nonAmplifiedPassIndex++)
	{
		const PassMetadata& nonAmplifiedPassMetadata = nonAmplifiedPassMetadatas[nonAmplifiedPassIndex];
		uint32_t passSubresourceCount = nonAmplifiedPassMetadata.SubresourceMetadataSpan.End - nonAmplifiedPassMetadata.SubresourceMetadataSpan.Begin;

		const ModernFrameGraph::PassFrameSpan& amplifiedPassSpan = mGraphToBuild->mFrameSpansPerRenderPass[nonAmplifiedPassIndex];
		for(uint32_t perFramePassIndex = amplifiedPassSpan.Begin; perFramePassIndex < amplifiedPassSpan.End; perFramePassIndex++)
		{
			PassMetadata& perFramePassMetadata = mTotalPassMetadatas[perFramePassIndex];

			perFramePassMetadata.SubresourceMetadataSpan.Begin = (uint32_t)mSubresourceMetadataNodesFlat.size();
			perFramePassMetadata.SubresourceMetadataSpan.End   = (uint32_t)mSubresourceMetadataNodesFlat.size() + passSubresourceCount;

			mSubresourceMetadataNodesFlat.resize(perFramePassMetadata.SubresourceMetadataSpan.End, SubresourceMetadataNode
			{
				.PrevPassNodeIndex     = (uint32_t)(-1),
				.NextPassNodeIndex     = (uint32_t)(-1),
				.ResourceMetadataIndex = (uint32_t)(-1),

				.PassClass = perFramePassMetadata.Class,

#if defined(DEBUG) || defined(_DEBUG)
				.PassName     = perFramePassMetadata.Name,
				.ResourceName = ""
#endif
			});
		}
	}

	//Amplify the subresources: remember the mapping between the non-amplified subresources and non-amplified passes
	std::vector<uint32_t> nonAmplifiedPassIndicesPerSubresource(nonAmplifiedSubresourceMetadatas.size());
	for(uint32_t nonAmplifiedPassIndex = 0; nonAmplifiedPassIndex < nonAmplifiedPassMetadatas.size(); nonAmplifiedPassIndex++)
	{
		Span<uint32_t> nonAmplifiedSubresourceSpan = nonAmplifiedPassMetadatas[nonAmplifiedPassIndex].SubresourceMetadataSpan;
		for(uint32_t nonAmplifiedSubresourceIndex = nonAmplifiedSubresourceSpan.Begin; nonAmplifiedSubresourceIndex < nonAmplifiedSubresourceSpan.End; nonAmplifiedSubresourceIndex++)
		{
			nonAmplifiedPassIndicesPerSubresource[nonAmplifiedSubresourceIndex] = nonAmplifiedPassIndex;
		}
	}

	mPrimarySubresourceNodeSpan.Begin = 0;
	mPrimarySubresourceNodeSpan.End   = (uint32_t)mSubresourceMetadataNodesFlat.size();

	mHelperNodeSpansPerPassSubresource.resize(mPrimarySubresourceNodeSpan.End - mPrimarySubresourceNodeSpan.Begin, Span<uint32_t>{.Begin = mPrimarySubresourceNodeSpan.End, .End = mPrimarySubresourceNodeSpan.End});

	//Amplify subresources: initialize new subresource nodes
	for(uint32_t nonAmplifiedPassIndex = 0; nonAmplifiedPassIndex < mGraphToBuild->mFrameSpansPerRenderPass.size(); nonAmplifiedPassIndex++)
	{
		const PassMetadata&                    nonAmplifiedPassMetadata = nonAmplifiedPassMetadatas[nonAmplifiedPassIndex];
		const ModernFrameGraph::PassFrameSpan& amplifiedPassSpan        = mGraphToBuild->mFrameSpansPerRenderPass[nonAmplifiedPassIndex];

		uint32_t passFrameCount       = amplifiedPassSpan.End - amplifiedPassSpan.Begin;
		uint32_t passSubresourceCount = nonAmplifiedPassMetadata.SubresourceMetadataSpan.End - nonAmplifiedPassMetadata.SubresourceMetadataSpan.Begin;

		for(uint32_t subresourceId = 0; subresourceId < passSubresourceCount; subresourceId++)
		{
			//Find the resource this subresource belongs to
			const SubresourceMetadataNode& nonAmplifiedSubresourceMetadata = nonAmplifiedSubresourceMetadatas[nonAmplifiedPassMetadata.SubresourceMetadataSpan.Begin + subresourceId];
			Span<uint32_t>                 amplifiedResourceSpan           = amplifiedResourceSpans[nonAmplifiedSubresourceMetadata.ResourceMetadataIndex];

			uint32_t resourceFrameCount = amplifiedResourceSpan.End - amplifiedResourceSpan.Begin;
			for(uint32_t passFrameIndex = 0; passFrameIndex < passFrameCount; passFrameIndex++)
			{
				const PassMetadata& perFramePassMetadata = mTotalPassMetadatas[amplifiedPassSpan.Begin + passFrameIndex];

				uint32_t perFrameResourceIndex    = amplifiedResourceSpan.Begin + passFrameIndex % resourceFrameCount;
				uint32_t perFrameSubresourceIndex = perFramePassMetadata.SubresourceMetadataSpan.Begin + subresourceId;

				SubresourceMetadataNode& perFrameSubresourceMetadata = mSubresourceMetadataNodesFlat[perFrameSubresourceIndex];
				perFrameSubresourceMetadata.ResourceMetadataIndex = perFrameResourceIndex;

				//Initialize previous subresource node
				uint32_t perFramePrevSubresourceIndex = perFrameSubresourceMetadata.PrevPassNodeIndex;
				if(perFramePrevSubresourceIndex == (uint32_t)(-1))
				{
					uint32_t nonAmplifiedPrevSubresourceIndex = nonAmplifiedSubresourceMetadata.PrevPassNodeIndex;
					uint32_t nonAmplifiedPrevPassIndex        = nonAmplifiedPassIndicesPerSubresource[nonAmplifiedPrevSubresourceIndex];

					const PassMetadata& nonAmplifiedPrevPassMetadata = nonAmplifiedPassMetadatas[nonAmplifiedPrevPassIndex];
					uint32_t prevPassSubresourceId = nonAmplifiedPrevSubresourceIndex - nonAmplifiedPrevPassMetadata.SubresourceMetadataSpan.Begin;

					ModernFrameGraph::PassFrameSpan& amplifiedPrevPassSpan = mGraphToBuild->mFrameSpansPerRenderPass[nonAmplifiedPrevPassIndex];
					uint32_t prevPassFrameCount                            = amplifiedPrevPassSpan.End - amplifiedPrevPassSpan.Begin;

					uint32_t prevPassFrameIndex = CalculatePrevPassFrameIndex(nonAmplifiedPrevPassIndex, nonAmplifiedPassIndex, passFrameIndex);
					if(prevPassFrameIndex < prevPassFrameCount)
					{
						//The actual pass metadata for the frame exists. Choose a primary subresource node in the pass
						const PassMetadata& perFramePrevPassMetadata = mTotalPassMetadatas[(size_t)amplifiedPrevPassSpan.Begin + prevPassFrameIndex];
						perFramePrevSubresourceIndex = perFramePrevPassMetadata.SubresourceMetadataSpan.Begin + prevPassSubresourceId;
					}
					else
					{
						//The actual pass metadata for the frame does not exist. Create a helper subresource span if necessary and choose a subresource node from there
						uint32_t helperNodesNeeded        = passFrameCount - prevPassFrameCount;
						uint32_t templateSubresourceIndex = mTotalPassMetadatas[(size_t)amplifiedPrevPassSpan.Begin].SubresourceMetadataSpan.Begin + prevPassSubresourceId;

						Span<uint32_t> helperSubresourceSpan = AllocateHelperSubresourceSpan(templateSubresourceIndex, helperNodesNeeded);
						perFramePrevSubresourceIndex = helperSubresourceSpan.Begin + (prevPassFrameIndex - prevPassFrameCount) % helperNodesNeeded;
					}
				
					mSubresourceMetadataNodesFlat[perFrameSubresourceIndex].PrevPassNodeIndex     = perFramePrevSubresourceIndex;
					mSubresourceMetadataNodesFlat[perFramePrevSubresourceIndex].NextPassNodeIndex = perFrameSubresourceIndex;
				}

				//Initialize next subresource node
				uint32_t perFrameNextSubresourceIndex = perFrameSubresourceMetadata.NextPassNodeIndex;
				if(perFrameNextSubresourceIndex == (uint32_t)(-1))
				{
					uint32_t nonAmplifiedNextSubresourceIndex = nonAmplifiedSubresourceMetadata.NextPassNodeIndex;
					uint32_t nonAmplifiedNextPassIndex        = nonAmplifiedPassIndicesPerSubresource[nonAmplifiedNextSubresourceIndex];

					const PassMetadata& nonAmplifiedNextPassMetadata = nonAmplifiedPassMetadatas[nonAmplifiedNextPassIndex];
					uint32_t nextPassSubresourceId = nonAmplifiedNextSubresourceIndex - nonAmplifiedNextPassMetadata.SubresourceMetadataSpan.Begin;

					ModernFrameGraph::PassFrameSpan& amplifiedNextPassSpan = mGraphToBuild->mFrameSpansPerRenderPass[nonAmplifiedNextPassIndex];
					uint32_t nextPassFrameCount                            = amplifiedNextPassSpan.End - amplifiedNextPassSpan.Begin;

					uint32_t nextPassFrameIndex = CalculateNextPassFrameIndex(nonAmplifiedNextPassIndex, nonAmplifiedPassIndex, passFrameIndex);
					if(nextPassFrameIndex < nextPassFrameCount)
					{
						//The actual pass metadata for the frame exists. Choose a primary subresource node in the pass
						const PassMetadata& perFrameNextPassMetadata = mTotalPassMetadatas[(size_t)amplifiedNextPassSpan.Begin + nextPassFrameIndex];
						perFrameNextSubresourceIndex = perFrameNextPassMetadata.SubresourceMetadataSpan.Begin + nextPassSubresourceId;
					}
					else
					{
						//The actual pass metadata for the frame does not exist. Create a helper subresource span if necessary and choose a subresource node from there
						uint32_t helperNodesNeeded        = passFrameCount - nextPassFrameCount;
						uint32_t templateSubresourceIndex = mTotalPassMetadatas[(size_t)amplifiedNextPassSpan.Begin].SubresourceMetadataSpan.Begin + nextPassSubresourceId;

						Span<uint32_t> helperSubresourceSpan = AllocateHelperSubresourceSpan(templateSubresourceIndex, helperNodesNeeded);
						perFrameNextSubresourceIndex = helperSubresourceSpan.Begin + (nextPassFrameIndex - nextPassFrameCount) % helperNodesNeeded;
					}
				
					mSubresourceMetadataNodesFlat[perFrameSubresourceIndex].NextPassNodeIndex     = perFrameNextSubresourceIndex;
					mSubresourceMetadataNodesFlat[perFrameNextSubresourceIndex].PrevPassNodeIndex = perFrameSubresourceIndex;
				}

#if defined(DEBUG) || defined(_DEBUG)
				mSubresourceMetadataNodesFlat[perFrameSubresourceIndex].ResourceName = mResourceMetadatas[perFrameResourceIndex].Name;
#endif
			}
		}
	}
}

void ModernFrameGraphBuilder::FindFrameCountAndSwapType(const std::vector<Span<uint32_t>>& resourceRemapInfos, std::span<const SubresourceMetadataNode> oldPassSubresourceMetadataSpan, uint32_t* outFrameCount, RenderPassFrameSwapType* outSwapType)
{
	assert(outFrameCount != nullptr);
	assert(outSwapType   != nullptr);

	RenderPassFrameSwapType swapType = RenderPassFrameSwapType::Constant;

	uint32_t passFrameCount = 1;
	for(const SubresourceMetadataNode& subresourceMetadataNode: oldPassSubresourceMetadataSpan)
	{
		uint32_t resourceIndex      = subresourceMetadataNode.ResourceMetadataIndex;
		uint32_t resourceFrameCount = resourceRemapInfos[resourceIndex].End - resourceRemapInfos[resourceIndex].Begin;

		passFrameCount = std::lcm(resourceFrameCount, passFrameCount);

		RenderPassFrameSwapType resourceSwapType = RenderPassFrameSwapType::Constant;
		if(resourceFrameCount > 1)
		{
			if(mResourceMetadatas[resourceRemapInfos[resourceIndex].Begin].SourceType == TextureSourceType::Backbuffer)
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

uint32_t ModernFrameGraphBuilder::CalculatePrevPassFrameIndex(uint32_t prevPassNonAmplifiedIndex, uint32_t currPassNonAmplifiedIndex, uint32_t currPassFrameIndex)
{
	//If the previous pass is LATER tham the current one in the frame timeline, it can only exist in the previous frame
	const bool frameBoundsCrossed = (prevPassNonAmplifiedIndex >= currPassNonAmplifiedIndex);
	if(frameBoundsCrossed)
	{
		//Frame bounds crossed: the corresponding per-pass frame index is 1 less than for the current pass, with wrap
		const ModernFrameGraph::PassFrameSpan& currPassFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[currPassNonAmplifiedIndex];
		const ModernFrameGraph::PassFrameSpan& prevPassFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[prevPassNonAmplifiedIndex];

		uint32_t currPassFrameCount = currPassFrameSpan.End - currPassFrameSpan.Begin;
		uint32_t prevPassFrameCount = prevPassFrameSpan.End - prevPassFrameSpan.Begin;

		uint32_t commonFrameCount = std::lcm(currPassFrameCount, prevPassFrameCount);
		return (currPassFrameIndex + commonFrameCount - 1) % commonFrameCount;
	}
	else
	{
		//Frame bounds not crossed: the corresponding per-pass frame index is the same for the previous pass as for the current pass
		return currPassFrameIndex;
	}
}

uint32_t ModernFrameGraphBuilder::CalculateNextPassFrameIndex(uint32_t nextPassNonAmplifiedIndex, uint32_t currPassNonAmplifiedIndex, uint32_t currPassFrameIndex)
{
	//If the next pass is BEFORE the current one in the frame timeline, it can only exist in the previous frame
	const bool frameBoundsCrossed = (nextPassNonAmplifiedIndex <= currPassNonAmplifiedIndex);
	if(frameBoundsCrossed)
	{
		//Frame bounds crossed: the corresponding per-pass frame index for the next pass is 1 greater than for the current pass, with wrap
		const ModernFrameGraph::PassFrameSpan& currPassFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[currPassNonAmplifiedIndex];
		const ModernFrameGraph::PassFrameSpan& nextPassFrameSpan = mGraphToBuild->mFrameSpansPerRenderPass[nextPassNonAmplifiedIndex];

		uint32_t currPassFrameCount = currPassFrameSpan.End - currPassFrameSpan.Begin;
		uint32_t nextPassFrameCount = nextPassFrameSpan.End - nextPassFrameSpan.Begin;

		uint32_t commonFrameCount = std::lcm(currPassFrameCount, nextPassFrameCount);
		return (currPassFrameIndex + 1) % commonFrameCount;
	}
	else
	{
		//Frame bounds not crossed: the corresponding per-pass frame index is the same for the next pass as for the current pass
		return currPassFrameIndex;
	}
}

Span<uint32_t> ModernFrameGraphBuilder::AllocateHelperSubresourceSpan(uint32_t templateSubresourceIndex, uint32_t helperNodesNeeded)
{
	Span<uint32_t>& helperSubresourceSpan = mHelperNodeSpansPerPassSubresource[templateSubresourceIndex];
	if(helperSubresourceSpan.End == helperSubresourceSpan.Begin)
	{
		//The helper subresource span for this subresource is not allocated yet. 
		//Allocate new helper span with (currPassFrameCount - 1) nodes, filling it with copies of the primary subresource
		SubresourceMetadataNode templateSubresource = mSubresourceMetadataNodesFlat[templateSubresourceIndex];
		templateSubresource.PrevPassNodeIndex = (uint32_t)(-1);
		templateSubresource.NextPassNodeIndex = (uint32_t)(-1);

		helperSubresourceSpan.Begin = (uint32_t)mSubresourceMetadataNodesFlat.size();
		helperSubresourceSpan.End   = (uint32_t)mSubresourceMetadataNodesFlat.size() + helperNodesNeeded;

		mSubresourceMetadataNodesFlat.resize(helperSubresourceSpan.End, templateSubresource);
	}
	else
	{
		//The allocated helper node span should have the exact size needed, or we need to make the function much more complicated
		assert(helperSubresourceSpan.End - helperSubresourceSpan.Begin == helperNodesNeeded);
	}

	return helperSubresourceSpan;
}

void ModernFrameGraphBuilder::InitAugmentedResourceData()
{
	InitializeHeadNodes();

	InitMetadataPayloads();

	PropagateSubresourcePayloadData();
}

void ModernFrameGraphBuilder::InitializeHeadNodes()
{
	//Initialize resource metadatas' head node index
	for(uint32_t metadataIndex = 0; metadataIndex < mSubresourceMetadataNodesFlat.size(); metadataIndex++)
	{
		SubresourceMetadataNode& subresourceMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
		if(mResourceMetadatas[subresourceMetadataNode.ResourceMetadataIndex].HeadNodeIndex == (uint32_t)(-1))
		{
			mResourceMetadatas[subresourceMetadataNode.ResourceMetadataIndex].HeadNodeIndex = metadataIndex;
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

		CreateBeforePassBarriers(passMetadata, passIndex);
		CreateAfterPassBarriers(passMetadata,  passIndex);
	}
}
