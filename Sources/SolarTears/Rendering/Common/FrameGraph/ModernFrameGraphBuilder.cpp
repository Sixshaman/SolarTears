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
	RegisterAndSortPasses(frameGraphDescription.mRenderPassTypes, frameGraphDescription.mSubresourceNames);

	BuildDependencyLevels();
	AdjustPassClasses();
	CalculatePassPeriods();

	ValidateSubresourceLinks();
	PropagateSubresourcePassClasses();

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

void ModernFrameGraphBuilder::RegisterAndSortPasses(const std::unordered_map<RenderPassName, RenderPassType>& renderPassTypes, const std::vector<SubresourceNamingInfo>& subresourceNames, const ResourceName& backbufferName)
{
	InitPassList(renderPassTypes);
	InitSubresourceList(subresourceNames, backbufferName);
	InitMetadataPayloads();

	std::vector<std::string_view>                                                          subresourceNamesFlat;
	std::unordered_map<FrameGraphDescription::RenderPassName, std::span<std::string_view>> readSubresourceNameSpans;
	std::unordered_map<FrameGraphDescription::RenderPassName, std::span<std::string_view>> writeSubresourceNameSpans;
	BuildReadWriteSubresourceSpans(renderPassNameSpan, readSubresourceNameSpans, writeSubresourceNameSpans, subresourceNamesFlat);

	std::vector<FrameGraphDescription::RenderPassName>                                                          adjacencyPassNamesFlat;
	std::unordered_map<FrameGraphDescription::RenderPassName, std::span<FrameGraphDescription::RenderPassName>> adjacencyList;
	BuildAdjacencyList(readSubresourceNameSpans, writeSubresourceNameSpans, adjacencyList, adjacencyPassNamesFlat);

	BuildSortedRenderPassesTopological(renderPassNameSpan, adjacencyList);
	BuildSortedRenderPassesDependency(adjacencyList);
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
			.PassClass            = RenderPassClass::Graphics,
		});
	}

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
			.End   = (uint32_t)(mSubresourceMetadataNodesFlat.size() + 1)
		}
	};

	mSubresourceMetadataNodesFlat.push_back(SubresourceMetadataNode
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
	for(const PassMetadata& passMetadata: mPassMetadatas)
	{
		uniquePassTypes.insert(passMetadata.Type);
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
		uint_fast16_t passSubresourceIndex = passSubresourceIndices.at(subresourceNaming.PassSubresourceId);
		uint32_t      flatMetadataIndex    = mPassMetadatas[passIndex].SubresourceMetadataSpan.Begin + passSubresourceIndex;
		
		auto resourceMetadataIt = resourceMetadataIndices.find(subresourceNaming.PassSubresourceName);
		if(resourceMetadataIt != resourceMetadataIndices.end())
		{
			mSubresourceMetadataNodesFlat[flatMetadataIndex].ResourceMetadataIndex = resourceMetadataIt->second;
		}
		else
		{
			resourceMetadataIndices[subresourceNaming.PassSubresourceName] = (uint32_t)mResourceMetadatas.size();
			mResourceMetadatas.push_back(ResourceMetadata
			{
				.Name = subresourceNaming.PassSubresourceName,

				.HeadNodeIndex    = (uint32_t)(-1),
				.FirstFrameHandle = (uint32_t)(-1),
				.FrameCount       = 1
			});

			mSubresourceMetadataNodesFlat[flatMetadataIndex].ResourceMetadataIndex = (uint32_t)(mResourceMetadatas.size() - 1);
		}
	}

	assert(mPresentPassMetadata.SubresourceMetadataSpan.End - mPresentPassMetadata.SubresourceMetadataSpan.Begin == 1); //Only one present pass backbuffer is supported
	mResourceMetadatas.push_back(ResourceMetadata
	{
		.Name = backbufferName,

		.HeadNodeIndex    = (uint32_t)(-1),
		.FirstFrameHandle = (uint32_t)(-1),
		.FrameCount       = GetSwapchainImageCount()
	});

	mSubresourceMetadataNodesFlat[mPresentPassMetadata.SubresourceMetadataSpan.Begin].ResourceMetadataIndex = (uint32_t)(mResourceMetadatas.size() - 1);
}

void ModernFrameGraphBuilder::BuildReadWriteSubresourceSpans(std::span<const FrameGraphDescription::RenderPassName> passes, std::unordered_map<FrameGraphDescription::RenderPassName, std::span<std::string_view>>& outReadSpans, std::unordered_map<FrameGraphDescription::RenderPassName, std::span<std::string_view>>& outWriteSpans, std::vector<std::string_view>& outNamesFlat)
{
	for(const FrameGraphDescription::RenderPassName& renderPassName: passes)
	{
		std::span<std::string_view> readSpan  = {outNamesFlat.end(), outNamesFlat.end()};
		std::span<std::string_view> writeSpan = {outNamesFlat.end(), outNamesFlat.end()};

		Span<uint32_t> metadataSpan = mSubresourceMetadataSpansPerPass.at(renderPassName);
		for(uint32_t metadataIndex = metadataSpan.Begin; metadataIndex < metadataSpan.End; metadataIndex++)
		{
			std::string_view subresourceName = mSubresourceMetadataNodesFlat[metadataIndex].SubresourceName;

			if(IsReadSubresource(metadataIndex))
			{
				outNamesFlat.push_back(subresourceName);

				std::swap(writeSpan.back(), writeSpan.front());

				readSpan  = std::span<std::string_view>(readSpan.begin(),      readSpan.end()  + 1);
				writeSpan = std::span<std::string_view>(writeSpan.begin() + 1, writeSpan.end() + 1);
			}

			if(IsWriteSubresource(metadataIndex))
			{
				outNamesFlat.push_back(subresourceName);

				writeSpan = std::span<std::string_view>(writeSpan.begin(), writeSpan.end() + 1);
			}		
		}

		std::sort(readSpan.begin(),  readSpan.end());
		std::sort(writeSpan.begin(), writeSpan.end());
	}
}

void ModernFrameGraphBuilder::BuildAdjacencyList(const std::unordered_map<FrameGraphDescription::RenderPassName, std::span<std::string_view>>& sortedReadNameSpans, const std::unordered_map<FrameGraphDescription::RenderPassName, std::span<std::string_view>>& sortedwriteNameSpans, std::unordered_map<FrameGraphDescription::RenderPassName, std::span<FrameGraphDescription::RenderPassName>>& outAdjacencyList, std::vector<FrameGraphDescription::RenderPassName>& outPassNamesFlat)
{
	outAdjacencyList.clear();
	outPassNamesFlat.clear();

	for(const auto& readNameSpan: sortedReadNameSpans)
	{
		const FrameGraphDescription::RenderPassName readPassName = readNameSpan.first;
		std::span<std::string_view> readSubresourceNames = readNameSpan.second;

		auto spanStart = outPassNamesFlat.end();
		for(const auto& writeNameSpan: sortedwriteNameSpans)
		{
			const FrameGraphDescription::RenderPassName writePassName = writeNameSpan.first;
			std::span<std::string_view> writeSubresourceNames = writeNameSpan.second;

			if(readPassName == writePassName)
			{
				continue;
			}

			if(PassesIntersect(readSubresourceNames, writeSubresourceNames))
			{
				outPassNamesFlat.push_back(writePassName);
			}
		}

		auto spanEnd = outPassNamesFlat.end();
		outAdjacencyList[readPassName] = {spanStart, spanEnd};
	}
}

void ModernFrameGraphBuilder::BuildSortedRenderPassesTopological(std::span<const FrameGraphDescription::RenderPassName> passes, const std::unordered_map<FrameGraphDescription::RenderPassName, std::span<FrameGraphDescription::RenderPassName>>& adjacencyList)
{
	mSortedRenderPassNames.clear();
	mSortedRenderPassNames.reserve(passes.size());

	std::unordered_set<FrameGraphDescription::RenderPassName> visited;
	std::unordered_set<FrameGraphDescription::RenderPassName> onStack;

	for(const FrameGraphDescription::RenderPassName& renderPassName: passes)
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
	uint32_t backbufferFirstNodeIndex = subresourceFirstNodeIndices.at(mFrameGraphDescription.GetBackbufferName());
	uint32_t backbufferLastNodeIndex  = subresourceLastNodeIndices.at(mFrameGraphDescription.GetBackbufferName());

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
			if(mSubresourceMetadataNodesFlat[metadataIndex].SubresourceName == mFrameGraphDescription.GetBackbufferName()) //Backbuffer is already processed
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

void ModernFrameGraphBuilder::PropagateSubresourcePassClasses()
{
	//Pass types should be known
	assert(!mRenderPassClasses.empty());

	FrameGraphDescription::RenderPassName presentPassName(FrameGraphDescription::PresentPassName);

	Span<uint32_t> presentPassNodeSpan = mSubresourceMetadataSpansPerPass.at(presentPassName);
	for(uint32_t metadataIndex = presentPassNodeSpan.Begin; metadataIndex < presentPassNodeSpan.End; metadataIndex++)
	{
		SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
		metadataNode.PassClass = mRenderPassClasses[presentPassName];
	}

	for(size_t i = 0; i < mSortedRenderPassNames.size(); i++)
	{
		const FrameGraphDescription::RenderPassName& renderPassName = mSortedRenderPassNames[i];

		Span<uint32_t> passNodeSpan = mSubresourceMetadataSpansPerPass.at(renderPassName);
		for(uint32_t metadataIndex = presentPassNodeSpan.Begin; metadataIndex < presentPassNodeSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
			metadataNode.PassClass = mRenderPassClasses[renderPassName];
		}
	}
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

void ModernFrameGraphBuilder::CalculatePassPeriods()
{
	for(const FrameGraphDescription::RenderPassName& passName: mSortedRenderPassNames)
	{
		uint32_t passPeriod = 1;

		Span<uint32_t> passNodeSpan = mSubresourceMetadataSpansPerPass.at(passName);
		for(uint32_t metadataIndex = passNodeSpan.Begin; metadataIndex < passNodeSpan.End; metadataIndex++)
		{
			SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
			if(metadataNode.SubresourceName != mFrameGraphDescription.GetBackbufferName()) //Swapchain images don't count as render pass own period
			{
				passPeriod = std::lcm(metadataNode.FrameCount, passPeriod);
			}
		}

		mRenderPassOwnPeriods[passName] = passPeriod;
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

		//Frame count for all nodes should be the same. As for calculating the value, we can do it
		//two ways: with max or with lcm. Since it's only related to a single resource, swapping 
		//can be made as fine with 3 ping-pong data as with 2. So max will suffice
		currentNode.FrameCount = std::max(currentNode.FrameCount, prevPassNode.FrameCount);

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

bool ModernFrameGraphBuilder::PassesIntersect(const std::span<std::string_view> readSortedSubresourceNames, const std::span<std::string_view> writeSortedSubresourceNames)
{
	uint32_t readIndex  = 0;
	uint32_t writeIndex = 0;
	while(readIndex < readSortedSubresourceNames.size() && writeIndex < writeSortedSubresourceNames.size())
	{
		std::string_view readName  = readSortedSubresourceNames[readIndex];
		std::string_view writeName = writeSortedSubresourceNames[readIndex];

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