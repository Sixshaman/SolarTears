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

const Span<uint32_t> ModernFrameGraphBuilder::GetBackbufferImageSpan() const
{
	return mGraphToBuild->mBackbufferImageSpan;
}

void ModernFrameGraphBuilder::RegisterPasses(const std::unordered_map<RenderPassName, RenderPassType>& renderPassTypes, const std::vector<SubresourceNamingInfo>& subresourceNames, const ResourceName& backbufferName)
{
	InitPassList(renderPassTypes);
	InitSubresourceList(subresourceNames, backbufferName);
	InitMetadataPayloads();
}

void ModernFrameGraphBuilder::InitPassList(const std::unordered_map<RenderPassName, RenderPassType>& renderPassTypes)
{
	mPassMetadatas.reserve(renderPassTypes.size());
	for(const auto& passNameWithType: renderPassTypes)
	{
		uint32_t currentMetadataNodeCount = (uint32_t)mSubresourceMetadataNodesFlat.size();
		uint32_t newMetadataNodeCount     = currentMetadataNodeCount + GetPassSubresourceCount(passNameWithType.second);

		mPassMetadatas.push_back(PassMetadata
		{
			.Name = passNameWithType.first,

			.Class = GetPassClass(passNameWithType.second),
			.Type  = passNameWithType.second,

			.DependencyLevel = 0,
			.OwnPeriod       = 1,

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

			.FirstFrameViewHandle = (uint32_t)(-1),
			.PassClass            = mPassMetadatas.back().Class,
		});
	}
	
	static_assert((uint32_t)PresentPassSubresourceId::Count == 1);
	mPresentPassMetadata = PassMetadata
	{
		.Name = RenderPassName(PresentPassName),

		.Class = RenderPassClass::Present,
		.Type  = RenderPassType::None,

		.DependencyLevel = 0,
		.OwnPeriod       = 1,

		.SubresourceMetadataSpan =
		{
			.Begin = (uint32_t)mSubresourceMetadataNodesFlat.size(),
			.End   = (uint32_t)(mSubresourceMetadataNodesFlat.size() + (size_t)PresentPassSubresourceId::Count)
		}
	};

	mSubresourceMetadataNodesFlat.resize(mPresentPassMetadata.SubresourceMetadataSpan.End, SubresourceMetadataNode
	{
		.PrevPassNodeIndex     = (uint32_t)(-1),
		.NextPassNodeIndex     = (uint32_t)(-1),
		.ResourceMetadataIndex = (uint32_t)(-1),

		.FirstFrameViewHandle = (uint32_t)(-1),
		.PassClass            = RenderPassClass::Present,
	});
}

void ModernFrameGraphBuilder::InitSubresourceList(const std::vector<SubresourceNamingInfo>& subresourceNames, const ResourceName& backbufferName)
{
	std::unordered_set<RenderPassType> uniquePassTypes;
	std::unordered_map<RenderPassName, uint32_t> passMetadataIndices;
	for(uint32_t passMetadataIndex = 0; passMetadataIndex < (uint32_t)mPassMetadatas.size(); passMetadataIndex++)
	{
		const PassMetadata& passMetadata = mPassMetadatas[passMetadataIndex];

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
		uint32_t      flatMetadataIndex    = mPassMetadatas[passIndex].SubresourceMetadataSpan.Begin + passSubresourceIndex;
		
		uint32_t subresourceFrameCount = 1; //TODO: per-subresource frame count

		auto resourceMetadataIt = resourceMetadataIndices.find(subresourceNaming.PassSubresourceName);
		if(resourceMetadataIt != resourceMetadataIndices.end())
		{
			uint32_t resourceMetadataIndex = resourceMetadataIt->second;

			mSubresourceMetadataNodesFlat[flatMetadataIndex].ResourceMetadataIndex = resourceMetadataIndex;

			//Frame count for all nodes should be the same. As for calculating the value, we can do it
			//two ways: with max or with lcm. Since it's only related to a single resource, swapping 
			//can be made as fine with 3 ping-pong data as with 2. So max will suffice
			mResourceMetadatas[resourceMetadataIndex].FrameCount = std::max(mResourceMetadatas[resourceMetadataIndex].FrameCount, subresourceFrameCount);
		}
		else
		{
			resourceMetadataIndices[subresourceNaming.PassSubresourceName] = (uint32_t)mResourceMetadatas.size();
			mResourceMetadatas.push_back(ResourceMetadata
			{
				.Name = subresourceNaming.PassSubresourceName,

				.HeadNodeIndex    = (uint32_t)(-1),
				.FirstFrameHandle = (uint32_t)(-1),
				.FrameCount       = subresourceFrameCount
			});

			mSubresourceMetadataNodesFlat[flatMetadataIndex].ResourceMetadataIndex = (uint32_t)(mResourceMetadatas.size() - 1);
		}
	}

	
	ResourceMetadata backbufferResourceMetadata = 
	{
		.Name = backbufferName,

		.HeadNodeIndex    = (uint32_t)(-1),
		.FirstFrameHandle = (uint32_t)(-1),
		.FrameCount       = GetSwapchainImageCount()
	};

	mResourceMetadatas.push_back(backbufferResourceMetadata);
	mSubresourceMetadataNodesFlat[mPresentPassMetadata.SubresourceMetadataSpan.Begin + (size_t)PresentPassSubresourceId::Backbuffer].ResourceMetadataIndex = (uint32_t)(mResourceMetadatas.size() - 1);
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
	for(const PassMetadata& renderPassMetadata: mPassMetadatas)
	{
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

	for(uint32_t writePassIndex = 0; writePassIndex < mPassMetadatas.size(); writePassIndex++)
	{
		std::span<uint32_t> writtenResourceIndices = sortedwriteNameSpansPerPass[writePassIndex];

		auto spanStart = outAdjacentPassIndicesFlat.end();
		for(uint32_t readPassIndex = 0; readPassIndex < mPassMetadatas.size(); readPassIndex++)
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
	for(uint32_t passIndex = 0; passIndex < mPassMetadatas.size(); passIndex++)
	{
		uint32_t mainPassDependencyLevel = mPassMetadatas[passIndex].DependencyLevel;

		const std::span<uint32_t> adjacentPassIndices = adjacencyList.at(passIndex);
		for(const uint32_t adjacentPassIndex: adjacentPassIndices)
		{
			mPassMetadatas[adjacentPassIndex].DependencyLevel = std::max(mPassMetadatas[adjacentPassIndex].DependencyLevel, mainPassDependencyLevel + 1);
		}
	}
}

void ModernFrameGraphBuilder::SortRenderPassesByTopology(const std::vector<std::span<uint32_t>>& unsortedPassAdjacencyList)
{
	std::vector<PassMetadata> sortedPasses;
	sortedPasses.reserve(mPassMetadatas.size());

	std::vector<uint8_t> traversalMarkFlags(mPassMetadatas.size(), 0);
	for(uint32_t passIndex = 0; passIndex < mPassMetadatas.size(); passIndex++)
	{
		TopologicalSortNode(passIndex, unsortedPassAdjacencyList, traversalMarkFlags, sortedPasses);
	}
	
	for(size_t i = 0; i < sortedPasses.size(); i++) //Reverse the order and save the sorted order
	{
		mPassMetadatas[i] = sortedPasses[sortedPasses.size() - i - 1];
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
	inoutSortedPasses.push_back(mPassMetadatas[passIndex]);
}

void ModernFrameGraphBuilder::SortRenderPassesByDependency()
{
	std::stable_sort(mPassMetadatas.begin(), mPassMetadatas.end(), [](const PassMetadata& left, const PassMetadata& right)
	{
		return left.DependencyLevel < right.DependencyLevel;
	});
}

void ModernFrameGraphBuilder::InitAugmentedData()
{
	AdjustPassClasses();
	PropagateSubresourcePassClasses();
	BuildPassSpans();	

	ValidateSubresourceLinks();

	struct TextureRemapInfo
	{
		uint32_t BaseTextureIndex;
		uint32_t FrameCount;
	};

	std::vector<uint32_t> resourceCounts(mResourceMetadatas.size(), 1);
	for(const PassMetadata& passMetadata: mPassMetadatas)
	{
		for(uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < passMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
		{
			uint32_t resourceFrameCount = GetPassSubresourceFrameCount(resourceFrameCount);

			uint32_t resourceIndex = mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex;
			assert(resourceCounts[resourceIndex] == 1 || resourceFrameCount == 1); //Do not allow M -> N connections between passes

			resourceCounts[resourceIndex] = std::lcm(resourceCounts[resourceIndex], resourceFrameCount);
		}
	}

	std::vector<PassMetadata> amplifiedPassMetadatas;
	for(const PassMetadata& passMetadata: mPassMetadatas)
	{
		uint32_t passFrameCount = 1;
		for(uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < passMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
		{
			uint32_t resourceIndex = mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex;
			passFrameCount = std::lcm(resourceCounts[resourceIndex], resourceIndex);
		}

		uint32_t passSubresourceCount = passMetadata.SubresourceMetadataSpan.End - passMetadata.SubresourceMetadataSpan.Begin;
		for(uint32_t passFrameIndex = 0; passFrameIndex < passFrameCount; passFrameIndex++)
		{
			PassMetadata framePassMetadata;
			framePassMetadata.Name                    = passMetadata.Name + "#" + std::to_string(passFrameIndex);
			framePassMetadata.Class                   = passMetadata.Class;
			framePassMetadata.Type                    = passMetadata.Type;
			framePassMetadata.DependencyLevel         = passMetadata.DependencyLevel;
			framePassMetadata.SubresourceMetadataSpan = transformSpan(passMetadata.SubresourceMetadataSpan);

			Span<uint32_t> newSubresourceMetadataSpan = Span<uint32_t>
			{
				.Begin = (uint32_t)(mSubresourceMetadataNodesFlat.size()),
				.End   = (uint32_t)(mSubresourceMetadataNodesFlat.size() + passSubresourceCount)
			};

			mSubresourceMetadataNodesFlat.resize(mSubresourceMetadataNodesFlat.size() + passSubresourceCount);
			std::copy(mSubresourceMetadataNodesFlat.begin() + passMetadata.SubresourceMetadataSpan.Begin, mSubresourceMetadataNodesFlat.begin() + passMetadata.SubresourceMetadataSpan.End, mSubresourceMetadataNodesFlat.begin() + newSubresourceMetadataSpan.Begin);

			for(uint32_t subresourceIndex = newSubresourceMetadataSpan.Begin; subresourceIndex < newSubresourceMetadataSpan.End; subresourceIndex++)
			{
				uint32_t resourceIndex = mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex;
				uint32_t resourceFrameIndex = passFrameIndex % resourceCounts[resourceIndex];

				if(resourceFrameIndex != 0)
				{
					mSubresourceMetadataNodesFlat[subresourceIndex].ResourceMetadataIndex = 
				}
			}
		}
	}

	PropagateSubresourcePayloadData();
}

void ModernFrameGraphBuilder::AdjustPassClasses()
{
	//The loop will wrap around 0
	size_t renderPassIndex = mPassMetadatas.size() - 1;
	while(renderPassIndex < mPassMetadatas.size())
	{
		//Graphics queue only for now
		//Async compute passes should be in the end of the frame. BUT! They should be executed AT THE START of THE PREVIOUS frame. Need to reorder stuff for that
		mPassMetadatas[renderPassIndex].Class = RenderPassClass::Graphics;

		renderPassIndex--;
	}

	//Just make sure this is unchanged
	assert(mPresentPassMetadata.Class == RenderPassClass::Present);
}

void ModernFrameGraphBuilder::PropagateSubresourcePassClasses()
{
	for(const PassMetadata& passMetadata: mPassMetadatas)
	{
		for(uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < passMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
		{
			mSubresourceMetadataNodesFlat[subresourceIndex].PassClass = passMetadata.Class;
		}
	}

	for(uint32_t subresourceIndex = mPresentPassMetadata.SubresourceMetadataSpan.Begin; subresourceIndex < mPresentPassMetadata.SubresourceMetadataSpan.End; subresourceIndex++)
	{
		mSubresourceMetadataNodesFlat[subresourceIndex].PassClass = mPresentPassMetadata.Class;
	}
}

void ModernFrameGraphBuilder::BuildPassSpans()
{
	mGraphToBuild->mGraphicsPassSpans.clear();

	uint32_t currentDependencyLevel = 0;
	mGraphToBuild->mGraphicsPassSpans.push_back(Span<uint32_t>{.Begin = 0, .End = 0});
	for(size_t passIndex = 0; passIndex < mPassMetadatas.size(); passIndex++)
	{
		uint32_t dependencyLevel = mPassMetadatas[passIndex].DependencyLevel;
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

void ModernFrameGraphBuilder::CalculatePassPeriods()
{
	uint32_t backbufferResourceIndex = mSubresourceMetadataNodesFlat[mPresentPassMetadata.SubresourceMetadataSpan.Begin + (size_t)PresentPassSubresourceId::Backbuffer].ResourceMetadataIndex;

	for(PassMetadata& passMetadata: mPassMetadatas)
	{
		uint32_t passPeriod = 1;
		for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
		{
			const SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
			if(metadataNode.ResourceMetadataIndex != backbufferResourceIndex) //Swapchain images don't count as render pass own period
			{
				const ResourceMetadata& resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];
				passPeriod = std::lcm(resourceMetadata.FrameCount, passPeriod);
			}
		}

		passMetadata.OwnPeriod = passPeriod;
	}

	mPresentPassMetadata.OwnPeriod = 0;
}

void ModernFrameGraphBuilder::ValidateSubresourceLinks()
{
	std::vector<uint32_t> lastNodeIndicesPerResource(mResourceMetadatas.size(), (uint32_t)(-1));

	//Connect previous passes
	for(const PassMetadata& passMetadata: mPassMetadatas)
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


	//Validate backbuffer links
	uint32_t backbufferPresentSubresourceIndex = mPresentPassMetadata.SubresourceMetadataSpan.Begin + (uint32_t)PresentPassSubresourceId::Backbuffer;
	uint32_t backbufferResourceIndex           = mSubresourceMetadataNodesFlat[backbufferPresentSubresourceIndex].ResourceMetadataIndex;

	uint32_t backbufferFirstNodeIndex = mResourceMetadatas[backbufferResourceIndex].HeadNodeIndex;
	uint32_t backbufferLastNodeIndex  = lastNodeIndicesPerResource[backbufferResourceIndex];

	SubresourceMetadataNode& backbufferFirstNode   = mSubresourceMetadataNodesFlat[backbufferFirstNodeIndex];
	SubresourceMetadataNode& backbufferLastNode    = mSubresourceMetadataNodesFlat[backbufferLastNodeIndex];
	SubresourceMetadataNode& backbufferPresentNode = mSubresourceMetadataNodesFlat[backbufferPresentSubresourceIndex];

	backbufferFirstNode.PrevPassNodeIndex = backbufferPresentSubresourceIndex;
	backbufferLastNode.NextPassNodeIndex  = backbufferPresentSubresourceIndex;

	backbufferPresentNode.PrevPassNodeIndex = backbufferLastNodeIndex;
	backbufferPresentNode.NextPassNodeIndex = backbufferFirstNodeIndex;


	//Validate cyclic links (so first pass knows what state to barrier from, and the last pass knows what state to barrier to)
	for(const PassMetadata& passMetadata: mPassMetadatas)
	{
		for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& subresourceNode = mSubresourceMetadataNodesFlat[metadataIndex];
			uint32_t resourceIndex = subresourceNode.ResourceMetadataIndex;

			if(resourceIndex == backbufferResourceIndex) //Backbuffer is already processed
			{
				continue;
			}

			if(subresourceNode.PrevPassNodeIndex == (uint32_t)(-1))
			{
				uint32_t prevNodeIndex = lastNodeIndicesPerResource[resourceIndex];
				SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[prevNodeIndex];

				subresourceNode.PrevPassNodeIndex     = prevNodeIndex;
				prevSubresourceNode.NextPassNodeIndex = metadataIndex;
			}

			if(subresourceNode.NextPassNodeIndex == (uint32_t)(-1))
			{
				uint32_t nextNodeIndex = mResourceMetadatas[backbufferResourceIndex].HeadNodeIndex;
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

		for(const PassMetadata& passMetadata: mPassMetadatas)
		{
			propagationHappened |= PropagateSubresourcePayloadDataHorizontally(passMetadata);
		}
	}
}

void ModernFrameGraphBuilder::BuildResources()
{
	CreateTextures();
	CreateTextureViews();
}

void ModernFrameGraphBuilder::BuildPassObjects()
{
	uint32_t backbufferResourceIndex = mSubresourceMetadataNodesFlat[mPresentPassMetadata.SubresourceMetadataSpan.Begin + (size_t)PresentPassSubresourceId::Backbuffer].ResourceMetadataIndex;

	for(uint32_t passMetadataIndex = 0; passMetadataIndex < mPassMetadatas.size(); passMetadataIndex++)
	{
		Span<uint32_t> passNodeSpan = mPassMetadatas[passMetadataIndex].SubresourceMetadataSpan;

		uint32_t passSwapchainImageCount = 1;
		for(uint32_t metadataIndex = passNodeSpan.Begin; metadataIndex < passNodeSpan.End; metadataIndex++)
		{
			if(mSubresourceMetadataNodesFlat[metadataIndex].ResourceMetadataIndex == backbufferResourceIndex)
			{
				passSwapchainImageCount = GetSwapchainImageCount();
				break;
			}
		}

		CreateObjectsForPass(passMetadataIndex, passSwapchainImageCount);
	}
}

void ModernFrameGraphBuilder::BuildBarriers()
{
	mGraphToBuild->mMultiframeBarrierInfos.clear();
	mGraphToBuild->mRenderPassBarriers.resize(mPassMetadatas.size() + 1, ModernFrameGraph::BarrierSpan
	{
		.BeforePassBegin = 0,
		.BeforePassEnd   = 0,
		.AfterPassBegin  = 0,
		.AfterPassEnd    = 0
	});

	for(uint32_t passIndex = 0; passIndex < mPassMetadatas.size() - 1; passIndex++)
	{
		const PassMetadata& passMetadata = mPassMetadatas[passIndex];

		AddBeforePassBarriers(passMetadata, passIndex);
		AddAfterPassBarriers(passMetadata,  passIndex);
	}

	AddBeforePassBarriers(mPresentPassMetadata, (uint32_t)(mPassMetadatas.size() - 1));
	AddAfterPassBarriers(mPresentPassMetadata,  (uint32_t)(mPassMetadatas.size() - 1));
}