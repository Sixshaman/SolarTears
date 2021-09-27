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
	metadataNode.SubresourceName      = mFrameGraphDescription.GetSubresourceName(subresId);
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

void ModernFrameGraphBuilder::Build(FrameGraphDescription&& frameGraphDescription)
{
	RegisterPasses(frameGraphDescription.mRenderPassTypes, frameGraphDescription.mSubresourceNames);
	SortPasses();
	InitAugmentedData();

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

	CalculatePassPeriods();

	ValidateSubresourceLinks();
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

void ModernFrameGraphBuilder::ValidateSubresourceLinks()
{
	std::vector<uint32_t> subresourceFirstNodeIndicesPerResource(mResourceMetadatas.size(), (uint32_t)(-1));
	std::vector<uint32_t> subresourceLastNodeIndicesPerResource(mResourceMetadatas.size(), (uint32_t)(-1));

	//Connect previous passes
	for(const PassMetadata& passMetadata: mPassMetadatas)
	{
		for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
			uint32_t resourceIndex = metadataNode.ResourceMetadataIndex;

			uint32_t lastIndex = subresourceLastNodeIndicesPerResource[resourceIndex];
			if(lastIndex == (uint32_t)(-1))
			{
				SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[lastIndex];

				prevSubresourceNode.NextPassNodeIndex = metadataIndex;
				metadataNode.PrevPassNodeIndex        = lastIndex;
			}

			if(subresourceFirstNodeIndicesPerResource[resourceIndex] != (uint32_t)(-1))
			{
				subresourceFirstNodeIndicesPerResource[resourceIndex] = metadataIndex;
			}

			subresourceLastNodeIndicesPerResource[resourceIndex] = metadataIndex;
		}
	}


	//Validate backbuffer links
	uint32_t backbufferPresentSubresourceIndex = mPresentPassMetadata.SubresourceMetadataSpan.Begin + (uint32_t)PresentPassSubresourceId::Backbuffer;
	uint32_t backbufferResourceIndex           = mSubresourceMetadataNodesFlat[backbufferPresentSubresourceIndex].ResourceMetadataIndex;

	uint32_t backbufferFirstNodeIndex = subresourceFirstNodeIndicesPerResource[backbufferResourceIndex];
	uint32_t backbufferLastNodeIndex  = subresourceLastNodeIndicesPerResource[backbufferResourceIndex];

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
				uint32_t prevNodeIndex = subresourceLastNodeIndicesPerResource[resourceIndex];
				SubresourceMetadataNode& prevSubresourceNode = mSubresourceMetadataNodesFlat[prevNodeIndex];

				subresourceNode.PrevPassNodeIndex     = prevNodeIndex;
				prevSubresourceNode.NextPassNodeIndex = metadataIndex;
			}

			if(subresourceNode.NextPassNodeIndex == (uint32_t)(-1))
			{
				uint32_t nextNodeIndex = subresourceFirstNodeIndicesPerResource[resourceIndex];
				SubresourceMetadataNode& nextSubresourceMetadata = mSubresourceMetadataNodesFlat[nextNodeIndex];

				subresourceNode.NextPassNodeIndex         = nextNodeIndex;
				nextSubresourceMetadata.PrevPassNodeIndex = metadataIndex;
			}
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
}

void ModernFrameGraphBuilder::BuildPassObjects()
{
	std::unordered_set<FrameGraphDescription::RenderPassName> swapchainPassNames;
	FindBackbufferPasses(swapchainPassNames);

	for(const std::string& passName: mSortedRenderPassNames)
	{
		RenderPassType passType = mFrameGraphDescription.GetPassType(passName);

		uint32_t passSwapchainImageCount = 1;
		if(swapchainPassNames.contains(passName))
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

void ModernFrameGraphBuilder::FindBackbufferPasses(std::unordered_set<FrameGraphDescription::RenderPassName>& swapchainPassNames)
{
	swapchainPassNames.clear();

	for(const auto& renderPassName: mSortedRenderPassNames)
	{
		Span<uint32_t> passNodeSpan = mSubresourceMetadataSpansPerPass.at(renderPassName);
		for(uint32_t metadataIndex = passNodeSpan.Begin; metadataIndex < passNodeSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
			if(metadataNode.SubresourceName == mFrameGraphDescription.GetBackbufferName())
			{
				swapchainPassNames.insert(renderPassName);
				continue;
			}
		}
	}
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
	Span<uint32_t> passSubresourceSpan = mSubresourceMetadataSpansPerPass.at(passName);

	std::vector<uint32_t> indicesForBeforeBarriers;
	std::vector<uint32_t> indicesForAfterBarriers;
	for(uint32_t metadataIndex = passSubresourceSpan.Begin; metadataIndex < passSubresourceSpan.End; metadataIndex++)
	{
		const SubresourceMetadataNode& subresourceMetadata = mSubresourceMetadataNodesFlat[metadataIndex];
	
		const SubresourceMetadataNode& prevMetadata = mSubresourceMetadataNodesFlat[subresourceMetadata.PrevPassNodeIndex];
		const SubresourceMetadataNode& nextMetadata = mSubresourceMetadataNodesFlat[subresourceMetadata.NextPassNodeIndex];

		if(!(subresourceMetadata.Flags & TextureFlagAutoBeforeBarrier) && !(prevMetadata.Flags & TextureFlagAutoAfterBarrier))
		{
			indicesForBeforeBarriers.push_back(metadataIndex);
		}

		if(!(subresourceMetadata.Flags & TextureFlagAutoAfterBarrier) && !(nextMetadata.Flags & TextureFlagAutoBeforeBarrier))
		{
			indicesForAfterBarriers.push_back(metadataIndex);
		}
	}


	uint32_t passBarrierCount = 0;

	ModernFrameGraph::BarrierSpan barrierSpan;
	barrierSpan.BeforePassBegin = barrierOffset + passBarrierCount;

	for(uint32_t metadataIndex: indicesForBeforeBarriers)
	{
		uint32_t barrierId = AddBeforePassBarrier(metadataIndex);
		if(barrierId != (uint32_t)(-1))
		{
			const SubresourceMetadataNode& subresourceMetadata = mSubresourceMetadataNodesFlat[metadataIndex];
			passBarrierCount++;

			if(subresourceMetadata.FrameCount > 1)
			{
				ModernFrameGraph::MultiframeBarrierInfo multiframeBarrierInfo;
				multiframeBarrierInfo.BarrierIndex  = barrierId;
				multiframeBarrierInfo.BaseTexIndex  = subresourceMetadata.FirstFrameHandle;
				multiframeBarrierInfo.TexturePeriod = subresourceMetadata.FrameCount;

				mGraphToBuild->mMultiframeBarrierInfos.push_back(multiframeBarrierInfo);
			}
		}
	}

	barrierSpan.BeforePassEnd  = barrierOffset + passBarrierCount;
	barrierSpan.AfterPassBegin = barrierOffset + passBarrierCount;

	for(uint32_t metadataIndex: indicesForAfterBarriers)
	{
		uint32_t barrierId = AddAfterPassBarrier(metadataIndex);
		if(barrierId != (uint32_t)(-1))
		{
			const SubresourceMetadataNode& subresourceMetadata = mSubresourceMetadataNodesFlat[metadataIndex];
			passBarrierCount++;

			if(subresourceMetadata.FrameCount > 1)
			{
				ModernFrameGraph::MultiframeBarrierInfo multiframeBarrierInfo;
				multiframeBarrierInfo.BarrierIndex  = barrierId;
				multiframeBarrierInfo.BaseTexIndex  = subresourceMetadata.FirstFrameHandle;
				multiframeBarrierInfo.TexturePeriod = subresourceMetadata.FrameCount;

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
	//Change this to filling mResourceMetadatas

	std::unordered_set<std::string_view> processedSubresourceNames;

	TextureResourceCreateInfo backbufferCreateInfo;
	backbufferCreateInfo.Name              = mFrameGraphDescription.GetBackbufferName();
	backbufferCreateInfo.MetadataHeadIndex = mMetadataNodeIndicesPerSubresourceIds.at(std::string(FrameGraphDescription::BackbufferPresentPassId));
	
	outBackbufferCreateInfos.push_back(backbufferCreateInfo);
	processedSubresourceNames.insert(mFrameGraphDescription.GetBackbufferName());

	for(const auto& renderPassName: mSortedRenderPassNames)
	{
		Span<uint32_t> passMetadataSpan = mSubresourceMetadataSpansPerPass.at(renderPassName);
		for(uint32_t metadataIndex = passMetadataSpan.Begin; metadataIndex < passMetadataSpan.End; metadataIndex++)
		{
			const SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
			if(!processedSubresourceNames.contains(metadataNode.SubresourceName))
			{
				TextureResourceCreateInfo textureCreateInfo;
				textureCreateInfo.Name              = metadataNode.SubresourceName;
				textureCreateInfo.MetadataHeadIndex = metadataIndex;

				outTextureCreateInfos.push_back(textureCreateInfo);
				processedSubresourceNames.insert(metadataNode.SubresourceName);
			}
		}
	}
}

void ModernFrameGraphBuilder::BuildSubresourceCreateInfos(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, std::vector<TextureSubresourceCreateInfo>& outTextureViewCreateInfos)
{
	for(const TextureResourceCreateInfo& textureCreateInfo: textureCreateInfos)
	{
		std::unordered_set<uint32_t> uniqueImageViews;

		uint32_t headNodeIndex    = textureCreateInfo.MetadataHeadIndex;
		uint32_t currentNodeIndex = headNodeIndex;
		do
		{
			const SubresourceMetadataNode& currentNode = mSubresourceMetadataNodesFlat[currentNodeIndex];

			if(currentNode.FirstFrameViewHandle != (uint32_t)(-1) && !uniqueImageViews.contains(currentNode.FirstFrameViewHandle))
			{
				for(uint32_t frame = 0; frame < currentNode.FrameCount; frame++)
				{
					TextureSubresourceCreateInfo subresourceCreateInfo;
					subresourceCreateInfo.SubresourceInfoIndex = currentNodeIndex;
					subresourceCreateInfo.ImageIndex           = currentNode.FirstFrameHandle + frame;
					subresourceCreateInfo.ImageViewIndex       = currentNode.FirstFrameViewHandle + frame;

					outTextureViewCreateInfos.push_back(subresourceCreateInfo);
				}

				uniqueImageViews.insert(currentNode.FirstFrameViewHandle);
			}

			currentNodeIndex = currentNode.NextPassNodeIndex;

		} while(currentNodeIndex != headNodeIndex);
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
	uint32_t headNodeIndex    = createInfo.MetadataHeadIndex;
	uint32_t currentNodeIndex = headNodeIndex;

	do
	{
		SubresourceMetadataNode& currentNode        = mSubresourceMetadataNodesFlat[currentNodeIndex];
		const SubresourceMetadataNode& prevPassNode = mSubresourceMetadataNodesFlat[currentNode.PrevPassNodeIndex];

		if(ValidateSubresourceViewParameters(currentNodeIndex, currentNode.PrevPassNodeIndex))
		{
			//Reset the head node if propagation succeeded. This means the further propagation all the way may be needed
			headNodeIndex = currentNodeIndex;
		}

		currentNodeIndex = currentNode.NextPassNodeIndex;

	} while(currentNodeIndex != headNodeIndex);
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

void ModernFrameGraphBuilder::ValidateImageAndViewIndicesInResource(TextureResourceCreateInfo* createInfo, uint32_t imageIndex)
{
	std::vector<uint32_t> resourceMetadataNodeIndices;

	uint32_t headNodeIndex    = createInfo->MetadataHeadIndex;
	uint32_t currentNodeIndex = headNodeIndex;

	do
	{
		const SubresourceMetadataNode& currentNode = mSubresourceMetadataNodesFlat[currentNodeIndex];

		resourceMetadataNodeIndices.push_back(currentNodeIndex);
		currentNodeIndex = currentNode.NextPassNodeIndex;

	} while(currentNodeIndex != headNodeIndex);

	std::sort(resourceMetadataNodeIndices.begin(), resourceMetadataNodeIndices.end(), [this](uint32_t left, uint32_t right)
	{
		return mSubresourceMetadataNodesFlat[left].ViewSortKey < mSubresourceMetadataNodesFlat[right].ViewSortKey;
	});

	//Only assign different image view indices if their ViewSortKey differ
	std::vector<Span<uint32_t>> differentImageViewSpans;
	differentImageViewSpans.push_back({.Begin = 0, .End = 0});

	std::vector<uint64_t> differentSortKeys;
	differentSortKeys.push_back(mSubresourceMetadataNodesFlat[resourceMetadataNodeIndices[0]].ViewSortKey);
	for(uint32_t nodeIndex = 1; nodeIndex < (uint32_t)resourceMetadataNodeIndices.size(); nodeIndex++)
	{
		const SubresourceMetadataNode& node = mSubresourceMetadataNodesFlat[resourceMetadataNodeIndices[nodeIndex]];

		uint64_t currSortKey = node.ViewSortKey;
		if(currSortKey != differentSortKeys.back())
		{
			differentImageViewSpans.back().End = nodeIndex;
			differentImageViewSpans.push_back(Span<uint32_t>{.Begin = (uint32_t)nodeIndex, .End = (uint32_t)nodeIndex});

			differentSortKeys.push_back(currSortKey);
		}
	}

	differentImageViewSpans.back().End = (uint32_t)resourceMetadataNodeIndices.size();

	//Remove this
	std::vector<uint32_t> assignedImageViewIds;
	AllocateImageViews(differentSortKeys, mSubresourceMetadataNodesFlat[headNodeIndex].FrameCount, assignedImageViewIds);


	//Remove this too
	for(size_t spanIndex = 0; spanIndex < differentImageViewSpans.size(); spanIndex++)
	{
		Span<uint32_t> nodeSpan = differentImageViewSpans[spanIndex];
		for(uint32_t nodeIndex = nodeSpan.Begin; nodeIndex < nodeSpan.End; nodeIndex++)
		{
			SubresourceMetadataNode& node = mSubresourceMetadataNodesFlat[resourceMetadataNodeIndices[nodeIndex]];

			node.FirstFrameHandle     = imageIndex;
			node.FirstFrameViewHandle = assignedImageViewIds[spanIndex];
		}
	}
}