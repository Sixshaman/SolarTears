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
	std::unordered_map<RenderPassName, uint32_t> passMetadataIndices;
	for(uint32_t passMetadataIndex = mRenderPassMetadataSpan.Begin; passMetadataIndex < mRenderPassMetadataSpan.End; passMetadataIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passMetadataIndex];

		uniquePassTypes.insert(passMetadata.Type);
		passMetadataIndices[passMetadata.Name] = passMetadataIndex;
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
	for(const auto& subresourceNaming: subresourceNames)
	{
		uint32_t passIndex = passMetadataIndices[subresourceNaming.PassName];

		uint_fast16_t passSubresourceIndex = passSubresourceIndices.at(subresourceNaming.PassSubresourceId);
		uint32_t      flatMetadataIndex    = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan.Begin + passSubresourceIndex;
		
		uint32_t subresourceFrameCount = 1; //TODO: per-subresource frame count

		auto resourceMetadataIt = resourceMetadataIndices.find(subresourceNaming.PassSubresourceName);
		if(resourceMetadataIt != resourceMetadataIndices.end())
		{
			uint32_t resourceMetadataIndex = resourceMetadataIt->second;

			mSubresourceMetadataNodesFlat[flatMetadataIndex].ResourceMetadataIndex = resourceMetadataIndex;
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

			mSubresourceMetadataNodesFlat[flatMetadataIndex].ResourceMetadataIndex = (uint32_t)(mResourceMetadatas.size() - 1);
		}
	}

	
	ResourceMetadata backbufferResourceMetadata = 
	{
		.Name          = backbufferName,
		.SourceType    = TextureSourceType::Backbuffer,
		.HeadNodeIndex = (uint32_t)(-1)
	};

	mResourceMetadatas.push_back(backbufferResourceMetadata);

	const PassMetadata& presentPassMetadata = mTotalPassMetadatas[mPresentPassMetadataSpan.Begin];
	mSubresourceMetadataNodesFlat[presentPassMetadata.SubresourceMetadataSpan.Begin + (size_t)PresentPassSubresourceId::Backbuffer].ResourceMetadataIndex = (uint32_t)(mResourceMetadatas.size() - 1);
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

	AssignDependencyLevels(unsortedPassAdjacencyList);

	SortRenderPassesByTopology(unsortedPassAdjacencyList);
	SortRenderPassesByDependency();
}

void ModernFrameGraphBuilder::BuildReadWriteSubresourceSpans(std::vector<std::span<std::uint32_t>>& outReadIndexSpans, std::vector<std::span<std::uint32_t>>& outWriteIndexSpans, std::vector<uint32_t>& outIndicesFlat)
{
	for(uint32_t passMetadataIndex = mRenderPassMetadataSpan.Begin; passMetadataIndex < mRenderPassMetadataSpan.End; passMetadataIndex++)
	{
		const PassMetadata& renderPassMetadata = mTotalPassMetadatas[passMetadataIndex];

		std::span<uint32_t> readSpan  = {outIndicesFlat.end(), outIndicesFlat.end()};
		std::span<uint32_t> writeSpan = {outIndicesFlat.end(), outIndicesFlat.end()};

		for(uint32_t subresourceMetadataIndex = renderPassMetadata.SubresourceMetadataSpan.Begin; subresourceMetadataIndex < renderPassMetadata.SubresourceMetadataSpan.End; subresourceMetadataIndex++)
		{
			uint32_t resourceIndex = mSubresourceMetadataNodesFlat[subresourceMetadataIndex].ResourceMetadataIndex;

			if(IsReadSubresource(subresourceMetadataIndex))
			{
				outIndicesFlat.push_back(resourceIndex);

				std::swap(writeSpan.back(), writeSpan.front());

				readSpan  = std::span<uint32_t>(readSpan.begin(),      readSpan.end()  + 1);
				writeSpan = std::span<uint32_t>(writeSpan.begin() + 1, writeSpan.end() + 1);
			}

			if(IsWriteSubresource(subresourceMetadataIndex))
			{
				outIndicesFlat.push_back(resourceIndex);

				writeSpan = std::span<uint32_t>(writeSpan.begin(), writeSpan.end() + 1);
			}		
		}

		std::sort(readSpan.begin(),  readSpan.end());
		std::sort(writeSpan.begin(), writeSpan.end());
	}
}

void ModernFrameGraphBuilder::BuildAdjacencyList(const std::vector<std::span<uint32_t>>& sortedReadNameSpansPerPass, const std::vector<std::span<uint32_t>>& sortedwriteNameSpansPerPass, std::vector<std::span<uint32_t>>& outAdjacencyList, std::vector<uint32_t>& outAdjacentPassIndicesFlat)
{
	outAdjacencyList.clear();
	outAdjacentPassIndicesFlat.clear();

	for(uint32_t writePassIndex = 0; writePassIndex < sortedwriteNameSpansPerPass.size(); writePassIndex++)
	{
		std::span<uint32_t> writtenResourceIndices = sortedwriteNameSpansPerPass[writePassIndex];

		auto spanStart = outAdjacentPassIndicesFlat.end();
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

		auto spanEnd = outAdjacentPassIndicesFlat.end();
		outAdjacencyList[writePassIndex] = {spanStart, spanEnd};
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

void ModernFrameGraphBuilder::AssignDependencyLevels(const std::vector<std::span<uint32_t>>& adjacencyList)
{
	for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
	{
		uint32_t mainPassDependencyLevel = mTotalPassMetadatas[passIndex].DependencyLevel;

		const std::span<uint32_t> adjacentPassIndices = adjacencyList.at(passIndex);
		for(const uint32_t adjacentPassIndex: adjacentPassIndices)
		{
			mTotalPassMetadatas[adjacentPassIndex].DependencyLevel = std::max(mTotalPassMetadatas[adjacentPassIndex].DependencyLevel, mainPassDependencyLevel + 1);
		}
	}
}

void ModernFrameGraphBuilder::SortRenderPassesByTopology(const std::vector<std::span<uint32_t>>& unsortedPassAdjacencyList)
{
	std::vector<PassMetadata> sortedPasses;
	sortedPasses.reserve(mRenderPassMetadataSpan.End - mRenderPassMetadataSpan.Begin);

	std::vector<uint8_t> traversalMarkFlags(mRenderPassMetadataSpan.End - mRenderPassMetadataSpan.Begin, 0);
	for(uint32_t passIndex = 0; passIndex < unsortedPassAdjacencyList.size(); passIndex++)
	{
		TopologicalSortNode(passIndex, unsortedPassAdjacencyList, traversalMarkFlags, sortedPasses);
	}
	
	for(size_t i = 0; i < sortedPasses.size(); i++) //Reverse the order and save the sorted order
	{
		mTotalPassMetadatas[mRenderPassMetadataSpan.Begin + i] = sortedPasses[sortedPasses.size() - i - 1];
	}
}

void ModernFrameGraphBuilder::TopologicalSortNode(uint32_t passIndex, const std::vector<std::span<uint32_t>>& unsortedPassAdjacencyList, std::vector<uint8_t>& inoutTraversalMarkFlags, std::vector<PassMetadata>& inoutSortedPasses)
{
	constexpr uint8_t VisitedFlag = 0x01;
	constexpr uint8_t OnStackFlag = 0x02;

	bool isVisited = inoutTraversalMarkFlags[passIndex] & VisitedFlag;
	bool isOnStack = inoutTraversalMarkFlags[passIndex] & OnStackFlag;

	assert(!isVisited || !isOnStack); //Detect circular dependency
	if(isVisited)
	{
		return;
	}

	inoutTraversalMarkFlags[passIndex] = (VisitedFlag | OnStackFlag);

	std::span<uint32_t> passAdjacencyIndices = unsortedPassAdjacencyList[passIndex];
	for(uint32_t adjacentPassIndex: passAdjacencyIndices)
	{
		TopologicalSortNode(adjacentPassIndex, unsortedPassAdjacencyList, inoutTraversalMarkFlags, inoutSortedPasses);
	}

	inoutTraversalMarkFlags[passIndex] &= (~OnStackFlag);
	inoutSortedPasses.push_back(mTotalPassMetadatas[mRenderPassMetadataSpan.Begin + passIndex]);
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

	AmplifyResourcesAndPasses();
	InitMetadataPayloads();

	ValidateSubresourceLinks();
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


	for(uint32_t resourceIndex = 0; resourceIndex < oldResourceMetadatas.size(); resourceIndex++)
	{
		resourceRemapInfos[resourceIndex].BaseTextureIndex = (uint32_t)oldResourceMetadatas.size();
		for(uint32_t frameIndex = 0; frameIndex < resourceRemapInfos[resourceIndex].FrameCount; frameIndex++)
		{
			mResourceMetadatas.push_back(ResourceMetadata
			{
				.Name          = oldResourceMetadatas[resourceIndex].Name + "#" + std::to_string(frameIndex),
				.HeadNodeIndex = (uint32_t)(-1)
			});
		}
	}

	Span<uint32_t> amplifiedRenderPassMetadataSpan = {.Begin = (uint32_t)mTotalPassMetadatas.size(), .End = (uint32_t)mTotalPassMetadatas.size()};
	for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passIndex];
		AddAmplifiedPassMetadata(passMetadata, resourceRemapInfos);
	}

	amplifiedRenderPassMetadataSpan.End = (uint32_t)mTotalPassMetadatas.size();
	mRenderPassMetadataSpan = amplifiedRenderPassMetadataSpan;

	Span<uint32_t> amplifiedPresentPassMetadataSpan = {.Begin = (uint32_t)mTotalPassMetadatas.size(), .End = (uint32_t)mTotalPassMetadatas.size()};
	for(uint32_t passIndex = mPresentPassMetadataSpan.Begin; passIndex < mPresentPassMetadataSpan.End; passIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passIndex];
		AddAmplifiedPassMetadata(passMetadata, resourceRemapInfos);
	}

	amplifiedPresentPassMetadataSpan.End = (uint32_t)mTotalPassMetadatas.size();
	mPresentPassMetadataSpan = amplifiedPresentPassMetadataSpan;
}

void ModernFrameGraphBuilder::AddAmplifiedPassMetadata(const PassMetadata& passMetadata, const std::vector<TextureRemapInfo>& resourceRemapInfos)
{
	RenderPassFrameSwapType swapType = RenderPassFrameSwapType::Constant;

	uint32_t passFrameCount = 1;
	for(uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < passMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
	{
		uint32_t resourceIndex = mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex;
		passFrameCount = std::lcm(resourceRemapInfos[resourceIndex].FrameCount, passFrameCount);

		RenderPassFrameSwapType resourceType = RenderPassFrameSwapType::Constant;
		if(resourceRemapInfos[resourceIndex].FrameCount > 1)
		{
			if(mResourceMetadatas[resourceIndex].SourceType == TextureSourceType::Backbuffer)
			{
				resourceType = RenderPassFrameSwapType::PerBackbufferImage;
			}
			else
			{
				resourceType = RenderPassFrameSwapType::PerLinearFrame;
			}

			//Do not allow resources with different swap types
			assert(swapType == RenderPassFrameSwapType::Constant || resourceType == swapType);
			swapType = resourceType;
		}
	}

	mGraphToBuild->mFrameSpansPerRenderPass.push_back
	({
		.Begin    = (uint32_t)mTotalPassMetadatas.size(),
		.End      = (uint32_t)mTotalPassMetadatas.size() + passFrameCount,
		.SwapType = swapType
	});

	uint32_t passSubresourceCount = passMetadata.SubresourceMetadataSpan.End - passMetadata.SubresourceMetadataSpan.Begin;
	for(uint32_t passFrameIndex = 0; passFrameIndex < passFrameCount; passFrameIndex++)
	{
		Span<uint32_t> newSubresourceMetadataSpan = Span<uint32_t>
		{
			.Begin = (uint32_t)(mTotalPassMetadatas.size()),
			.End   = (uint32_t)(mTotalPassMetadatas.size() + passSubresourceCount)
		};

		mTotalPassMetadatas.push_back(PassMetadata
		{
			.Name                    = passMetadata.Name + "#" + std::to_string(passFrameIndex),
			.Class                   = passMetadata.Class,
			.Type                    = passMetadata.Type,
			.DependencyLevel         = passMetadata.DependencyLevel,
			.SubresourceMetadataSpan = newSubresourceMetadataSpan
		});

		mSubresourceMetadataNodesFlat.resize(newSubresourceMetadataSpan.End);
		for(uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < passMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
		{
			uint32_t newSubresourceIndex = newSubresourceMetadataSpan.Begin + (subresourceIndex - passMetadata.SubresourceMetadataSpan.Begin);

			uint32_t oldResourceIndex = mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex;
			mSubresourceMetadataNodesFlat[newSubresourceIndex] = SubresourceMetadataNode
			{
				.PrevPassNodeIndex     = (uint32_t)(-1),
				.NextPassNodeIndex     = (uint32_t)(-1),
				.ResourceMetadataIndex = resourceRemapInfos[oldResourceIndex].BaseTextureIndex + passFrameIndex % resourceRemapInfos[oldResourceIndex].FrameCount,
				.ImageViewHandle       = (uint32_t)(-1),
				.PassClass             = passMetadata.Class
			};
		}
	}
}

void ModernFrameGraphBuilder::ValidateSubresourceLinks()
{
	std::vector<uint32_t> lastNodeIndicesPerResource(mResourceMetadatas.size(), (uint32_t)(-1));

	//Connect previous passes
	for(const PassMetadata& passMetadata: mTotalPassMetadatas)
	{
		for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
			uint32_t resourceIndex = metadataNode.ResourceMetadataIndex;

			uint32_t lastIndex = lastNodeIndicesPerResource[resourceIndex];
			if(lastIndex == (uint32_t)(-1))
			{
				SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[lastIndex];

				prevSubresourceNode.NextPassNodeIndex = metadataIndex;
				metadataNode.PrevPassNodeIndex        = lastIndex;
			}

			if(mResourceMetadatas[resourceIndex].HeadNodeIndex == (uint32_t)(-1))
			{
				mResourceMetadatas[resourceIndex].HeadNodeIndex = metadataIndex;
			}

			lastNodeIndicesPerResource[resourceIndex] = metadataIndex;
		}
	}

	//Validate cyclic links (so first pass knows what state to barrier from, and the last pass knows what state to barrier to)
	for(const PassMetadata& passMetadata: mTotalPassMetadatas)
	{
		for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& subresourceNode = mSubresourceMetadataNodesFlat[metadataIndex];
			uint32_t resourceIndex = subresourceNode.ResourceMetadataIndex;

			if(subresourceNode.PrevPassNodeIndex == (uint32_t)(-1))
			{
				uint32_t prevNodeIndex = lastNodeIndicesPerResource[resourceIndex];
				SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[prevNodeIndex];

				subresourceNode.PrevPassNodeIndex     = prevNodeIndex;
				prevSubresourceNode.NextPassNodeIndex = metadataIndex;
			}

			if(subresourceNode.NextPassNodeIndex == (uint32_t)(-1))
			{
				uint32_t nextNodeIndex = mResourceMetadatas[resourceIndex].HeadNodeIndex;
				SubresourceMetadataNode& nextSubresourceMetadata = mSubresourceMetadataNodesFlat[nextNodeIndex];

				subresourceNode.NextPassNodeIndex         = nextNodeIndex;
				nextSubresourceMetadata.PrevPassNodeIndex = metadataIndex;
			}
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