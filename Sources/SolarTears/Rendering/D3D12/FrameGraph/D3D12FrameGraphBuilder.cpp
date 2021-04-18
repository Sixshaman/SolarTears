#include "D3D12FrameGraphBuilder.hpp"
#include "D3D12FrameGraph.hpp"
#include "../D3D12DeviceFeatures.hpp"
#include "../D3D12WorkerCommandLists.hpp"
#include "../D3D12Memory.hpp"
#include "../D3D12Shaders.hpp"
#include "../D3D12SwapChain.hpp"
#include "../D3D12DeviceQueues.hpp"
#include "../../../Core/Util.hpp"
#include <algorithm>

D3D12::FrameGraphBuilder::FrameGraphBuilder(FrameGraph* graphToBuild, const RenderableScene* scene, const DeviceFeatures* deviceFeatures, const ShaderManager* shaderManager): mGraphToBuild(graphToBuild), mScene(scene), mDeviceFeatures(deviceFeatures), mShaderManager(shaderManager)
{
	mSubresourceMetadataCounter = 0;

	//Create present quasi-pass
	std::string presentPassName(PresentPassName);

	mRenderPassTypes[presentPassName] = RenderPassType::PRESENT;

	RegisterWriteSubresource(PresentPassName, BackbufferPresentPassId);
	RegisterReadSubresource(PresentPassName, BackbufferPresentPassId);
}

D3D12::FrameGraphBuilder::~FrameGraphBuilder()
{
}

void D3D12::FrameGraphBuilder::RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc, RenderPassType passType)
{
	RenderPassName renderPassName(passName);
	assert(!mRenderPassCreateFunctions.contains(renderPassName));

	mRenderPassNames.push_back(renderPassName);
	mRenderPassCreateFunctions[renderPassName] = createFunc;

	mRenderPassTypes[renderPassName] = passType;
}

void D3D12::FrameGraphBuilder::RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	RenderPassName renderPassName(passName);

	if(!mRenderPassesReadSubresourceIds.contains(renderPassName))
	{
		mRenderPassesReadSubresourceIds[renderPassName] = std::unordered_set<SubresourceId>();
	}

	SubresourceId subresId(subresourceId);
	assert(renderPassName == PresentPassName || !mRenderPassesReadSubresourceIds[renderPassName].contains(subresId));

	mRenderPassesReadSubresourceIds[renderPassName].insert(subresId);

	if(!mRenderPassesSubresourceMetadatas.contains(renderPassName))
	{
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, TextureSubresourceMetadata>();
	}

	TextureSubresourceMetadata subresourceMetadata;
	subresourceMetadata.MetadataId       = mSubresourceMetadataCounter;
	subresourceMetadata.PrevPassMetadata = nullptr;
	subresourceMetadata.NextPassMetadata = nullptr;
	subresourceMetadata.MetadataFlags    = 0;
	subresourceMetadata.QueueOwnership   = D3D12_COMMAND_LIST_TYPE_DIRECT;
	subresourceMetadata.Format           = DXGI_FORMAT_UNKNOWN;
	subresourceMetadata.ImageIndex       = (uint32_t)(-1);
	subresourceMetadata.ResourceState    = D3D12_RESOURCE_STATE_COMMON;

	mSubresourceMetadataCounter++;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = subresourceMetadata;
}

void D3D12::FrameGraphBuilder::RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	RenderPassName renderPassName(passName);

	if(!mRenderPassesWriteSubresourceIds.contains(renderPassName))
	{
		mRenderPassesWriteSubresourceIds[renderPassName] = std::unordered_set<SubresourceId>();
	}

	SubresourceId subresId(subresourceId);
	assert(renderPassName == PresentPassName || !mRenderPassesWriteSubresourceIds[renderPassName].contains(subresId));

	mRenderPassesWriteSubresourceIds[renderPassName].insert(subresId);

	if(!mRenderPassesSubresourceMetadatas.contains(renderPassName))
	{
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, TextureSubresourceMetadata>();
	}

	TextureSubresourceMetadata subresourceMetadata;
	subresourceMetadata.MetadataId       = mSubresourceMetadataCounter;
	subresourceMetadata.PrevPassMetadata = nullptr;
	subresourceMetadata.NextPassMetadata = nullptr;
	subresourceMetadata.MetadataFlags    = 0;
	subresourceMetadata.QueueOwnership   = D3D12_COMMAND_LIST_TYPE_DIRECT;
	subresourceMetadata.Format           = DXGI_FORMAT_UNKNOWN;
	subresourceMetadata.ImageIndex       = (uint32_t)(-1);
	subresourceMetadata.ResourceState    = D3D12_RESOURCE_STATE_COMMON;

	mSubresourceMetadataCounter++;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = subresourceMetadata;
}

void D3D12::FrameGraphBuilder::AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName)
{
	RenderPassName  passNameStr(passName);
	SubresourceId   subresourceIdStr(subresourceId);
	SubresourceName subresourceNameStr(subresourceName);

	auto readSubresIt  = mRenderPassesReadSubresourceIds.find(passNameStr);
	auto writeSubresIt = mRenderPassesWriteSubresourceIds.find(passNameStr);

	assert(readSubresIt != mRenderPassesReadSubresourceIds.end() || writeSubresIt != mRenderPassesWriteSubresourceIds.end());
	mRenderPassesSubresourceNameIds[passNameStr][subresourceNameStr] = subresourceIdStr;
}

void D3D12::FrameGraphBuilder::AssignBackbufferName(const std::string_view backbufferName)
{
	mBackbufferName = backbufferName;

	AssignSubresourceName(PresentPassName, BackbufferPresentPassId, mBackbufferName);

	TextureSubresourceMetadata presentAcquirePassMetadata;
	presentAcquirePassMetadata.MetadataId       = mSubresourceMetadataCounter;
	presentAcquirePassMetadata.PrevPassMetadata = nullptr;
	presentAcquirePassMetadata.NextPassMetadata = nullptr;
	presentAcquirePassMetadata.MetadataFlags    = 0;
	presentAcquirePassMetadata.QueueOwnership   = D3D12_COMMAND_LIST_TYPE_DIRECT;
	presentAcquirePassMetadata.Format           = DXGI_FORMAT_UNKNOWN;
	presentAcquirePassMetadata.ImageIndex       = (uint32_t)(-1);
	presentAcquirePassMetadata.ResourceState    = D3D12_RESOURCE_STATE_COMMON;

	mSubresourceMetadataCounter++;

	mRenderPassesSubresourceMetadatas[std::string(PresentPassName)][std::string(BackbufferPresentPassId)] = presentAcquirePassMetadata;
}

void D3D12::FrameGraphBuilder::SetPassSubresourceFormat(const std::string_view passName, const std::string_view subresourceId, DXGI_FORMAT format)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].Format = format;
}

void D3D12::FrameGraphBuilder::SetPassSubresourceState(const std::string_view passName, const std::string_view subresourceId, D3D12_RESOURCE_STATES state)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].ResourceState = state;
}

void D3D12::FrameGraphBuilder::EnableSubresourceAutoBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBaarrier)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	if(autoBaarrier)
	{
		mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].MetadataFlags |= TextureFlagAutoBarrier;
	}
	else
	{
		mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].MetadataFlags &= ~TextureFlagAutoBarrier;
	}
}

const D3D12::RenderableScene* D3D12::FrameGraphBuilder::GetScene() const
{
	return mScene;
}

const D3D12::DeviceFeatures* D3D12::FrameGraphBuilder::GetDeviceFeatures() const
{
	return mDeviceFeatures;
}

const D3D12::ShaderManager* D3D12::FrameGraphBuilder::GetShaderManager() const
{
	return mShaderManager;
}

const FrameGraphConfig* D3D12::FrameGraphBuilder::GetConfig() const
{
	return &mGraphToBuild->mFrameGraphConfig;
}

ID3D12Resource2* D3D12::FrameGraphBuilder::GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const TextureSubresourceMetadata& metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.ImageIndex == (uint32_t)(-1))
	{
		return nullptr;
	}
	else
	{
		return mGraphToBuild->mTextures[metadata.ImageIndex];
	}
}

DXGI_FORMAT D3D12::FrameGraphBuilder::GetRegisteredSubresourceFormat(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const TextureSubresourceMetadata& metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.Format;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetRegisteredSubresourceState(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const TextureSubresourceMetadata& metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.ResourceState;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetPreviousPassSubresourceState(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const TextureSubresourceMetadata& metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.PrevPassMetadata != nullptr)
	{
		return metadata.PrevPassMetadata->ResourceState;
	}
	else
	{
		return D3D12_RESOURCE_STATE_COMMON;
	}
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetNextPassSubresourceState(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const TextureSubresourceMetadata& metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.NextPassMetadata != nullptr)
	{
		return metadata.NextPassMetadata->ResourceState;
	}
	else
	{
		return D3D12_RESOURCE_STATE_COMMON;
	}
}

void D3D12::FrameGraphBuilder::Build(ID3D12Device8* device, const MemoryManager* memoryAllocator, const SwapChain* swapChain)
{
	std::vector<wil::com_ptr_nothrow<ID3D12Resource2>> swapchainImageOwners(swapChain->SwapchainImageCount);

	std::vector<ID3D12Resource2*> swapchainImages(swapChain->SwapchainImageCount);
	for(uint32_t i = 0; i < swapChain->SwapchainImageCount; i++)
	{
		ID3D12Resource* swapchainImage = swapChain->GetSwapchainImage(i);
		THROW_IF_FAILED(swapchainImage->QueryInterface(IID_PPV_ARGS(swapchainImageOwners[i].put())));

		swapchainImages[i] = swapchainImageOwners[i].get();
	}

	std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>> adjacencyList;
	BuildAdjacencyList(adjacencyList);

	std::vector<RenderPassName> sortedPassNames;
	SortRenderPassesTopological(adjacencyList, sortedPassNames);
	SortRenderPassesDependency(adjacencyList, sortedPassNames);
	mRenderPassNames = std::move(sortedPassNames);

	BuildDependencyLevels();

	ValidateSubresourceLinks();
	ValidateSubresourceCommandListTypes();
	ValidateCommonPromotion();

	std::unordered_set<RenderPassName> backbufferPassNames; //All passes that use the backbuffer
	BuildSubresources(device, memoryAllocator, swapchainImages, backbufferPassNames);
	BuildPassObjects(device, backbufferPassNames);

	BuildBarriers();
}

void D3D12::FrameGraphBuilder::BuildAdjacencyList(std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList)
{
	adjacencyList.clear();

	for(const RenderPassName& renderPassName: mRenderPassNames)
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

void D3D12::FrameGraphBuilder::SortRenderPassesTopological(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& unsortedPasses)
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

void D3D12::FrameGraphBuilder::SortRenderPassesDependency(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& topologicallySortedPasses)
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

void D3D12::FrameGraphBuilder::BuildDependencyLevels()
{
	mGraphToBuild->mGraphicsPassSpans.clear();

	uint32_t currentDependencyLevel = 0;
	mGraphToBuild->mGraphicsPassSpans.push_back(FrameGraph::DependencyLevelSpan{.DependencyLevelBegin = 0, .DependencyLevelEnd = 0});
	for(size_t passIndex = 0; passIndex < mRenderPassNames.size(); passIndex++)
	{
		uint32_t dependencyLevel = mRenderPassDependencyLevels[mRenderPassNames[passIndex]];
		if(currentDependencyLevel != dependencyLevel)
		{
			mGraphToBuild->mGraphicsPassSpans.push_back(FrameGraph::DependencyLevelSpan{.DependencyLevelBegin = dependencyLevel, .DependencyLevelEnd = dependencyLevel + 1});
			currentDependencyLevel = dependencyLevel;
		}
		else
		{
			mGraphToBuild->mGraphicsPassSpans.back().DependencyLevelEnd++;
		}
	}

	mGraphToBuild->mFrameRecordedGraphicsCommandLists.resize(mGraphToBuild->mGraphicsPassSpans.size());

	//The loop will wrap around 0
	size_t renderPassNameIndex = mRenderPassNames.size() - 1;
	while(renderPassNameIndex < mRenderPassNames.size())
	{
		//Graphics queue only for now
		//Async compute passes should be in the end of the frame. BUT! They should be executed AT THE START of THE PREVIOUS frame. Need to reorder stuff for that
		//Async copy passes should have all resources in COMMON state, which requires additional work
		RenderPassName renderPassName = mRenderPassNames[renderPassNameIndex];

		mRenderPassCommandListTypes[renderPassName] = D3D12_COMMAND_LIST_TYPE_DIRECT;
		renderPassNameIndex--;
	}

	//No present-from-compute
	mRenderPassCommandListTypes[std::string(PresentPassName)] = D3D12_COMMAND_LIST_TYPE_DIRECT;
}

void D3D12::FrameGraphBuilder::TopologicalSortNode(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::unordered_set<RenderPassName>& visited, std::unordered_set<RenderPassName>& onStack, const RenderPassName& renderPassName, std::vector<RenderPassName>& sortedPassNames)
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

void D3D12::FrameGraphBuilder::ValidateSubresourceLinks()
{
	std::unordered_map<SubresourceName, RenderPassName> subresourceFirstRenderPasses; //The fisrt pass for each subresource
	std::unordered_map<SubresourceName, RenderPassName> subresourceLastRenderPasses;  //The last pass for each subresource

	//Connect previous passes
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			SubresourceName             subresourceName     = subresourceNameId.first;
			TextureSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);

			if(subresourceLastRenderPasses.contains(subresourceName))
			{
				RenderPassName prevPassName = subresourceLastRenderPasses.at(subresourceName);
				
				SubresourceId               prevSubresourceId       = mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
				TextureSubresourceMetadata& prevSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(prevPassName).at(prevSubresourceId);

				prevSubresourceMetadata.NextPassMetadata = &subresourceMetadata;
				subresourceMetadata.PrevPassMetadata     = &prevSubresourceMetadata;
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

	TextureSubresourceMetadata& backbufferFirstSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(backbufferFirstPassName).at(backbufferFirstSubresourceId);
	TextureSubresourceMetadata& backbufferLastSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(backbufferLastPassName).at(backbufferLastSubresourceId);

	TextureSubresourceMetadata& backbufferPresentSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(std::string(PresentPassName)).at(std::string(BackbufferPresentPassId));

	backbufferFirstSubresourceMetadata.PrevPassMetadata = &backbufferPresentSubresourceMetadata;
	backbufferLastSubresourceMetadata.NextPassMetadata  = &backbufferPresentSubresourceMetadata;

	backbufferPresentSubresourceMetadata.PrevPassMetadata = &backbufferLastSubresourceMetadata;
	backbufferPresentSubresourceMetadata.NextPassMetadata = &backbufferFirstSubresourceMetadata;


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

			TextureSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);

			if(subresourceMetadata.PrevPassMetadata == nullptr)
			{
				SubresourceName subresourceName = subresourceNameId.first;

				RenderPassName prevPassName = subresourceLastRenderPasses.at(subresourceName);

				SubresourceId               prevSubresourceId       = mRenderPassesSubresourceNameIds.at(prevPassName).at(subresourceName);
				TextureSubresourceMetadata& prevSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(prevPassName).at(prevSubresourceId);

                //No, the barriers should be circular
				subresourceMetadata.PrevPassMetadata     = &prevSubresourceMetadata; //Barrier from the last state at the beginning of frame
				prevSubresourceMetadata.NextPassMetadata = &subresourceMetadata;
			}

			if(subresourceMetadata.NextPassMetadata == nullptr)
			{
				SubresourceName subresourceName = subresourceNameId.first;

				RenderPassName nextPassName = subresourceFirstRenderPasses.at(subresourceName);

				SubresourceId               nextSubresourceId       = mRenderPassesSubresourceNameIds.at(nextPassName).at(subresourceName);
				TextureSubresourceMetadata& nextSubresourceMetadata = mRenderPassesSubresourceMetadatas.at(nextPassName).at(nextSubresourceId);

                //No, the barriers should be circular
				subresourceMetadata.NextPassMetadata     = &nextSubresourceMetadata; //Barrier from the last state at the beginning of frame
				nextSubresourceMetadata.PrevPassMetadata = &subresourceMetadata;
			}
		}
	}
}

void D3D12::FrameGraphBuilder::ValidateSubresourceCommandListTypes()
{
	//Command list types should be known
	assert(!mRenderPassCommandListTypes.empty());

	std::string presentPassName(PresentPassName);

	for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[presentPassName])
	{
		TextureSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(presentPassName).at(subresourceNameId.second);
		subresourceMetadata.QueueOwnership              = mRenderPassCommandListTypes[presentPassName];
	}

	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			TextureSubresourceMetadata& subresourceMetadata = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);
			subresourceMetadata.QueueOwnership              = mRenderPassCommandListTypes[renderPassName];
		}
	}
}

void D3D12::FrameGraphBuilder::ValidateCommonPromotion()
{
	//Subresource links should be validated
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];
		for(auto& subresourceNameId: mRenderPassesSubresourceNameIds[renderPassName])
		{
			TextureSubresourceMetadata& subresourceMetadata     = mRenderPassesSubresourceMetadatas.at(renderPassName).at(subresourceNameId.second);
			TextureSubresourceMetadata& prevSubresourceMetadata = *subresourceMetadata.PrevPassMetadata;

			if(prevSubresourceMetadata.ResourceState == D3D12_RESOURCE_STATE_COMMON && D3D12Utils::IsStatePromoteableTo(subresourceMetadata.ResourceState)) //Promotion from common
			{
				subresourceMetadata.MetadataFlags |= TextureFlagStateAutoPromoted;
			}
			else if(prevSubresourceMetadata.QueueOwnership == subresourceMetadata.QueueOwnership) //Queue hasn't changed => ExecuteCommandLists wasn't called => No decay happened
			{
				if(prevSubresourceMetadata.ResourceState == subresourceMetadata.ResourceState) //Propagation of state promotion
				{
					subresourceMetadata.MetadataFlags |= TextureFlagStateAutoPromoted;
				}
			}
			else
			{
				bool prevWasPromoted = (prevSubresourceMetadata.MetadataFlags & TextureFlagStateAutoPromoted);
				if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevSubresourceMetadata.ResourceState)) //Read-only promoted state decays to COMMON after ECL call
				{
					if(D3D12Utils::IsStatePromoteableTo(subresourceMetadata.ResourceState)) //State promotes again
					{
						subresourceMetadata.MetadataFlags |= TextureFlagStateAutoPromoted;
					}
				}
			}
		}
	}
}

void D3D12::FrameGraphBuilder::PropagateMetadatasFromImageViews(const std::unordered_map<SubresourceName, TextureCreateInfo>& textureCreateInfos, const std::unordered_map<SubresourceName, BackbufferCreateInfo>& backbufferCreateInfos)
{
	for(auto& nameWithCreateInfo: textureCreateInfos)
	{
		for(size_t i = 0; i < nameWithCreateInfo.second.ViewInfos.size(); i++)
		{
			DXGI_FORMAT format = nameWithCreateInfo.second.ViewInfos[i].Format;
			for(size_t j = 0; j < nameWithCreateInfo.second.ViewInfos[i].ViewAddresses.size(); j++)
			{
				const SubresourceAddress& address = nameWithCreateInfo.second.ViewInfos[i].ViewAddresses[j];
				mRenderPassesSubresourceMetadatas.at(address.PassName).at(address.SubresId).Format = format;
			}
		}
	}

	for(auto& nameWithCreateInfo: backbufferCreateInfos)
	{
		for(size_t i = 0; i < nameWithCreateInfo.second.ViewInfos.size(); i++)
		{
			DXGI_FORMAT format = nameWithCreateInfo.second.ViewInfos[i].Format;
			for(size_t j = 0; j < nameWithCreateInfo.second.ViewInfos[i].ViewAddresses.size(); j++)
			{
				const SubresourceAddress& address = nameWithCreateInfo.second.ViewInfos[i].ViewAddresses[j];
				mRenderPassesSubresourceMetadatas.at(address.PassName).at(address.SubresId).Format = format;
			}
		}
	}
}

void D3D12::FrameGraphBuilder::BuildSubresources(ID3D12Device8* device, const MemoryManager* memoryAllocator, const std::vector<ID3D12Resource2*>& swapchainTextures, std::unordered_set<RenderPassName>& swapchainPassNames)
{
	std::unordered_map<SubresourceName, TextureCreateInfo>    textureCreateInfos;
	std::unordered_map<SubresourceName, BackbufferCreateInfo> backbufferCreateInfos;
	BuildResourceCreateInfos(textureCreateInfos, backbufferCreateInfos, swapchainPassNames);

	PropagateMetadatasFromImageViews(textureCreateInfos, backbufferCreateInfos);

	CreateTextures(device, textureCreateInfos, memoryAllocator);
	SetSwapchainTextures(backbufferCreateInfos, swapchainTextures);
}

void D3D12::FrameGraphBuilder::BuildPassObjects(ID3D12Device8* device, const std::unordered_set<std::string>& swapchainPassNames)
{
	mGraphToBuild->mRenderPasses.clear();
	mGraphToBuild->mSwapchainRenderPasses.clear();
	mGraphToBuild->mSwapchainPassesSwapMap.clear();

	for(const std::string& passName: mRenderPassNames)
	{
		if(swapchainPassNames.contains(passName))
		{
			//This pass uses swapchain images - create a copy of render pass for each swapchain image
			mGraphToBuild->mRenderPasses.emplace_back(nullptr);

			//Fill in primary swap link (what to swap)
			mGraphToBuild->mSwapchainPassesSwapMap.push_back((uint32_t)(mGraphToBuild->mRenderPasses.size() - 1));

			//Correctness is guaranteed as long as mLastSwapchainImageIndex is the last index in mSwapchainImageRefs
			const uint32_t swapchainImageCount = (uint32_t)mGraphToBuild->mSwapchainImageRefs.size();			
			for(uint32_t swapchainImageIndex = 0; swapchainImageIndex < mGraphToBuild->mSwapchainImageRefs.size(); swapchainImageIndex++)
			{
				//Create a separate render pass for each of swapchain images
				mGraphToBuild->SwitchSwapchainImages(swapchainImageIndex);
				mGraphToBuild->mSwapchainRenderPasses.push_back(mRenderPassCreateFunctions.at(passName)(device, this, passName));

				//Fill in secondary swap link (what to swap to)
				mGraphToBuild->mSwapchainPassesSwapMap.push_back((uint32_t)(mGraphToBuild->mSwapchainRenderPasses.size() - 1));

				mGraphToBuild->mLastSwapchainImageIndex = swapchainImageIndex;
			}
		}
		else
		{
			//This pass does not use any swapchain images - can just create a single copy
			mGraphToBuild->mRenderPasses.push_back(mRenderPassCreateFunctions.at(passName)(device, this, passName));
		}
	}

	//Prepare for the first use
	uint32_t swapchainImageCount = (uint32_t)mGraphToBuild->mSwapchainImageRefs.size();
	for(uint32_t i = 0; i < (uint32_t)mGraphToBuild->mSwapchainPassesSwapMap.size(); i += (swapchainImageCount + 1u))
	{
		uint32_t passIndexToSwitch = mGraphToBuild->mSwapchainPassesSwapMap[i];
		mGraphToBuild->mRenderPasses[passIndexToSwitch].swap(mGraphToBuild->mSwapchainRenderPasses[mGraphToBuild->mSwapchainPassesSwapMap[i + mGraphToBuild->mLastSwapchainImageIndex + 1]]);
	}
}

void D3D12::FrameGraphBuilder::BuildBarriers() //When present from compute is out, you're gonna have a fun time writing all the new rules lol
{
	mGraphToBuild->mRenderPassBarriers.resize(mRenderPassNames.size());
	mGraphToBuild->mSwapchainBarrierIndices.clear();

	std::unordered_set<uint64_t> processedMetadataIds; //This is here so no barrier gets added twice
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];

		const auto& nameIdMap     = mRenderPassesSubresourceNameIds.at(renderPassName);
		const auto& idMetadataMap = mRenderPassesSubresourceMetadatas.at(renderPassName);

		std::vector<D3D12_RESOURCE_BARRIER> beforeBarriers;
		std::vector<D3D12_RESOURCE_BARRIER> afterBarriers;
		std::vector<size_t>                 beforeSwapchainBarrierIndices;
		std::vector<size_t>                 afterSwapchainBarrierIndices;
		for(auto& subresourceNameId: nameIdMap)
		{
			SubresourceName subresourceName = subresourceNameId.first;
			SubresourceId   subresourceId   = subresourceNameId.second;

			TextureSubresourceMetadata subresourceMetadata = idMetadataMap.at(subresourceId);
			if(!(subresourceMetadata.MetadataFlags & TextureFlagAutoBarrier))
			{
				
				{
					/*
					*    Before barrier:
					*
					*    1.  Same queue, state unchanged:                                  No barrier needed
					*    2.  Same queue, state changed, PRESENT -> automatically promoted: No barrier needed
					*    3.  Same queue, state changed other cases:                        Need a barrier Old state -> New state
					*
					*    4.  Graphics -> Compute,  state automatically promoted:               No barrier needed
					*    5.  Graphics -> Compute,  state non-promoted, was promoted read-only: Need a barrier COMMON -> New state
					*    6.  Graphics -> Compute,  state unchanged:                            No barrier needed
					*    7.  Graphics -> Compute,  state changed other cases:                  No barrier needed, handled by the previous state's barrier
					* 
					*    8.  Compute  -> Graphics, state automatically promoted:                       No barrier needed, state will be promoted again
					* 	 9.  Compute  -> Graphics, state non-promoted, was promoted read-only:         Need a barrier COMMON -> New state   
					*    10. Compute  -> Graphics, state changed Compute/graphics -> Compute/Graphics: No barrier needed, handled by the previous state's barrier
					*    11. Compute  -> Graphics, state changed Compute/Graphics -> Graphics:         Need a barrier Old state -> New state
					*    12. Compute  -> Graphics, state unchanged:                                    No barrier needed
					*
					*    13. Graphics/Compute -> Copy, was promoted read-only: No barrier needed, state decays
					*    14. Graphics/Compute -> Copy, other cases:            No barrier needed, handled by previous state's barrier
					* 
					*    15. Copy -> Graphics/Compute, state automatically promoted: No barrier needed
					*    16. Copy -> Graphics/Compute, state non-promoted:           Need a barrier COMMON -> New state
					*/

					bool needBarrier = false;
					D3D12_RESOURCE_STATES prevPassState = subresourceMetadata.PrevPassMetadata->ResourceState;
					D3D12_RESOURCE_STATES nextPassState = subresourceMetadata.ResourceState;

					const D3D12_COMMAND_LIST_TYPE prevStateQueue = subresourceMetadata.PrevPassMetadata->QueueOwnership;
					const D3D12_COMMAND_LIST_TYPE nextStateQueue = subresourceMetadata.QueueOwnership;

					const bool prevWasPromoted = (subresourceMetadata.PrevPassMetadata->MetadataFlags & TextureFlagStateAutoPromoted);
					const bool nextIsPromoted  = (subresourceMetadata.MetadataFlags & TextureFlagStateAutoPromoted);

					if(prevStateQueue == nextStateQueue) //Rules 1, 2, 3
					{
						if(prevPassState == D3D12_RESOURCE_STATE_PRESENT) //Rule 2 or 3
						{
							needBarrier = !nextIsPromoted;
						}
						else //Rule 1 or 3
						{
							needBarrier = (prevPassState != nextPassState);
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT && nextStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE) //Rules 4, 5, 6, 7
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 5
						{
							prevPassState = D3D12_RESOURCE_STATE_COMMON; //Common state decay from promoted read-only
							needBarrier   = !nextIsPromoted;
						}
						else //Rules 4, 6, 7
						{
							needBarrier = false;
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE && nextStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT) //Rules 8, 9, 10, 11, 12
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 9
						{
							prevPassState = D3D12_RESOURCE_STATE_COMMON; //Common state decay from promoted read-only
							needBarrier   = !nextIsPromoted;
						}
						else //Rules 8, 10, 11, 12
						{
							needBarrier = !nextIsPromoted && (prevPassState != nextPassState) && !D3D12Utils::IsStateComputeFriendly(nextPassState);
						}
					}
					else if(nextStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 13, 14
					{
						needBarrier = false;
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 15, 16
					{
						needBarrier = !nextIsPromoted;
					}

					if(needBarrier)
					{
						//Swapchain image barriers are changed every frame
						ID3D12Resource2* barrieredTexture = nullptr;
						if(subresourceMetadata.ImageIndex != mGraphToBuild->mBackbufferRefIndex)
						{
							barrieredTexture = mGraphToBuild->mTextures[subresourceMetadata.ImageIndex];
						}

						D3D12_RESOURCE_BARRIER textureTransitionBarrier;
						textureTransitionBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
						textureTransitionBarrier.Transition.pResource   = barrieredTexture;
						textureTransitionBarrier.Transition.Subresource = 0;
						textureTransitionBarrier.Transition.StateBefore = prevPassState;
						textureTransitionBarrier.Transition.StateAfter  = nextPassState;
						textureTransitionBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;

						beforeBarriers.push_back(textureTransitionBarrier);

						//Mark the swapchain barriers
						if(barrieredTexture == nullptr)
						{
							beforeSwapchainBarrierIndices.push_back(beforeBarriers.size() - 1);
						}
					}
				}

				
				{
					/*
					*    After barrier:
					*
					*    1.  Same queue, state unchanged:                              No barrier needed
					*    2.  Same queue, state changed, Promoted read-only -> PRESENT: No barrier needed, state decays
					*    3.  Same queue, state changed, other cases:                   Need a barrier Old state -> New state
					*
					*    4.  Graphics -> Compute,  state will be automatically promoted:               No barrier needed
					*    5.  Graphics -> Compute,  state will not be promoted, was promoted read-only: No barrier needed, will be handled by the next state's barrier
					*    6.  Graphics -> Compute,  state unchanged:                                    No barrier needed
					*    7.  Graphics -> Compute,  state changed other cases:                          Need a barrier Old state -> New state
					* 
					*    8.  Compute  -> Graphics, state will be automatically promoted:                 No barrier needed
					* 	 9.  Compute  -> Graphics, state will not be promoted, was promoted read-only:   No barrier needed, will be handled by the next state's barrier     
					*    10. Compute  -> Graphics, state changed Promoted read-only -> PRESENT:          No barrier needed, state will decay
					*    11. Compute  -> Graphics, state changed Compute/graphics   -> Compute/Graphics: Need a barrier Old state -> New state
					*    12. Compute  -> Graphics, state changed Compute/Graphics   -> Graphics:         No barrier needed, will be handled by the next state's barrier
					*    13. Compute  -> Graphics, state unchanged:                                      No barrier needed
					*   
					*    15. Graphics/Compute -> Copy, from Promoted read-only: No barrier needed
					*    16. Graphics/Compute -> Copy, other cases:             Need a barrier Old state -> COMMON
					* 
					*    17. Copy -> Graphics/Compute, state will be automatically promoted: No barrier needed
					*    18. Copy -> Graphics/Compute, state will not be promoted:           No barrier needed, will be handled by the next state's barrier
					*/

					bool needBarrier = false;
					D3D12_RESOURCE_STATES prevPassState = subresourceMetadata.ResourceState;
					D3D12_RESOURCE_STATES nextPassState = subresourceMetadata.NextPassMetadata->ResourceState;

					const D3D12_COMMAND_LIST_TYPE prevStateQueue = subresourceMetadata.QueueOwnership;
					const D3D12_COMMAND_LIST_TYPE nextStateQueue = subresourceMetadata.NextPassMetadata->QueueOwnership;

					const bool prevWasPromoted = (subresourceMetadata.MetadataFlags & TextureFlagStateAutoPromoted);
					const bool nextIsPromoted  = (subresourceMetadata.NextPassMetadata->MetadataFlags & TextureFlagStateAutoPromoted);

					if(prevStateQueue == nextStateQueue) //Rules 1, 2, 3, 4
					{
						if(nextPassState == D3D12_RESOURCE_STATE_PRESENT)
						{
							needBarrier = !prevWasPromoted || D3D12Utils::IsStateWriteable(prevPassState);
						}
						else
						{
							needBarrier = (prevPassState != nextPassState);
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT && nextStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE) //Rules 4, 5, 6, 7
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 5
						{
							needBarrier = false;
						}
						else //Rules 4, 6, 7
						{
							needBarrier = !nextIsPromoted && (prevPassState != nextPassState);
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE && nextStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT) //Rules 8, 9, 10, 11, 12, 13
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 9, 10
						{
							needBarrier = false;
						}
						else //Rules 8, 11, 12, 13
						{
							needBarrier = !nextIsPromoted && (prevPassState != nextPassState) && D3D12Utils::IsStateComputeFriendly(nextPassState);
						}
					}
					else if(nextStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 14, 15
					{
						needBarrier = !prevWasPromoted || D3D12Utils::IsStateWriteable(prevPassState);
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 16, 17
					{
						needBarrier = false;
					}

					if(needBarrier)
					{
						//Swapchain image barriers are changed every frame
						ID3D12Resource2* barrieredTexture = nullptr;
						if(subresourceMetadata.ImageIndex != mGraphToBuild->mBackbufferRefIndex)
						{
							barrieredTexture = mGraphToBuild->mTextures[subresourceMetadata.ImageIndex];
						}

						D3D12_RESOURCE_BARRIER textureTransitionBarrier;
						textureTransitionBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
						textureTransitionBarrier.Transition.pResource   = barrieredTexture;
						textureTransitionBarrier.Transition.Subresource = 0;
						textureTransitionBarrier.Transition.StateBefore = prevPassState;
						textureTransitionBarrier.Transition.StateAfter  = nextPassState;
						textureTransitionBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;

						afterBarriers.push_back(textureTransitionBarrier);

						//Mark the swapchain barriers
						if(barrieredTexture == nullptr)
						{
							afterSwapchainBarrierIndices.push_back(beforeBarriers.size() - 1);
						}
					}
				}
			}

			processedMetadataIds.insert(subresourceMetadata.MetadataId);
		}

		uint32_t beforeBarrierBegin = (uint32_t)(mGraphToBuild->mResourceBarriers.size());
		mGraphToBuild->mResourceBarriers.insert(mGraphToBuild->mResourceBarriers.end(), beforeBarriers.begin(), beforeBarriers.end());

		uint32_t afterBarrierBegin = (uint32_t)(mGraphToBuild->mResourceBarriers.size());
		mGraphToBuild->mResourceBarriers.insert(mGraphToBuild->mResourceBarriers.end(), afterBarriers.begin(), afterBarriers.end());

		FrameGraph::BarrierSpan barrierSpan;
		barrierSpan.BeforePassBegin = beforeBarrierBegin;
		barrierSpan.BeforePassEnd   = beforeBarrierBegin + (uint32_t)(beforeBarriers.size());
		barrierSpan.AfterPassBegin  = afterBarrierBegin;
		barrierSpan.AfterPassEnd    = afterBarrierBegin + (uint32_t)(afterBarriers.size());

		mGraphToBuild->mRenderPassBarriers[i] = barrierSpan;

		for(size_t beforeSwapchainBarrierIndex: beforeSwapchainBarrierIndices)
		{
			mGraphToBuild->mSwapchainBarrierIndices.push_back(beforeBarrierBegin + (uint32_t)beforeSwapchainBarrierIndex);
		}

		for(size_t afterSwapchainBarrierIndex: afterSwapchainBarrierIndices)
		{
			mGraphToBuild->mSwapchainBarrierIndices.push_back(afterBarrierBegin + (uint32_t)afterSwapchainBarrierIndex);
		}
	}
}
 
void D3D12::FrameGraphBuilder::BuildResourceCreateInfos(std::unordered_map<SubresourceName, TextureCreateInfo>& outImageCreateInfos, std::unordered_map<SubresourceName, BackbufferCreateInfo>& outBackbufferCreateInfos, std::unordered_set<RenderPassName>& swapchainPassNames)
{
	for(const auto& renderPassName: mRenderPassNames)
	{
		const auto& nameIdMap = mRenderPassesSubresourceNameIds[renderPassName];
		for(const auto& nameId: nameIdMap)
		{
			const SubresourceName subresourceName = nameId.first;
			const SubresourceId   subresourceId   = nameId.second;

			if(subresourceName == mBackbufferName)
			{
				TextureSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas[renderPassName][subresourceId];

				TextureViewInfo viewInfo;
				viewInfo.Format = metadata.Format;
				viewInfo.ViewAddresses.push_back({.PassName = renderPassName, .SubresId = subresourceId});

				BackbufferCreateInfo backbufferCreateInfo;
				backbufferCreateInfo.ViewInfos.push_back(viewInfo);

				outBackbufferCreateInfos[subresourceName] = backbufferCreateInfo;

				swapchainPassNames.insert(renderPassName);
			}
			else
			{
				TextureSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas[renderPassName][subresourceId];
				if(!outImageCreateInfos.contains(subresourceName))
				{
					//TODO: Multisampling
					D3D12_RESOURCE_DESC1 resourceDesc;
					resourceDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
					resourceDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
					resourceDesc.Width                           = mGraphToBuild->mFrameGraphConfig.GetViewportHeight();
					resourceDesc.Height                          = mGraphToBuild->mFrameGraphConfig.GetViewportHeight();
					resourceDesc.DepthOrArraySize                = 1;
					resourceDesc.MipLevels                       = 1;
					resourceDesc.Format                          = metadata.Format;
					resourceDesc.SampleDesc.Count                = 1;
					resourceDesc.SampleDesc.Quality              = 0;
					resourceDesc.Layout                          = D3D12_TEXTURE_LAYOUT_UNKNOWN;
					resourceDesc.Flags                           = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
					resourceDesc.SamplerFeedbackMipRegion.Width  = 0;
					resourceDesc.SamplerFeedbackMipRegion.Height = 0;
					resourceDesc.SamplerFeedbackMipRegion.Depth  = 0;

					outImageCreateInfos[subresourceName] = TextureCreateInfo();
					outImageCreateInfos[subresourceName].ResourceDesc = resourceDesc;

					//Mark the optimized clear value with UNKNOWN format, this means that the optimized clear value is undefined for the resource
					outImageCreateInfos[subresourceName].OptimizedClearValue.Format = DXGI_FORMAT_UNKNOWN;
				}
				else
				{
					auto& imageResourceCreateInfo = outImageCreateInfos[subresourceName];
					if(metadata.Format != DXGI_FORMAT_UNKNOWN)
					{
						if(imageResourceCreateInfo.ResourceDesc.Format == DXGI_FORMAT_UNKNOWN)
						{
							//Not all passes specify the format
							imageResourceCreateInfo.ResourceDesc.Format = metadata.Format;
						}
						else if(metadata.Format != imageResourceCreateInfo.ResourceDesc.Format)
						{
							imageResourceCreateInfo.ResourceDesc.Format = D3D12Utils::ConvertToTypeless(metadata.Format);
						}
					}
				}

				if(metadata.ResourceState & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
				{
					outImageCreateInfos[subresourceName].ResourceDesc.Flags &= ~(D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
				}
				else if(metadata.ResourceState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				{
					outImageCreateInfos[subresourceName].ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				}
				else if(metadata.ResourceState & D3D12_RESOURCE_STATE_RENDER_TARGET)
				{
					outImageCreateInfos[subresourceName].ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				}
				else if(metadata.ResourceState & (D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE))
				{
					outImageCreateInfos[subresourceName].ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				}

				//It's fine if format is UNDEFINED. It means it can be arbitrary and we'll fix it later based on other image view infos for this resource
				TextureViewInfo viewInfo;
				viewInfo.Format     = metadata.Format;
				viewInfo.ViewAddresses.push_back({.PassName = renderPassName, .SubresId = subresourceId});

				outImageCreateInfos[subresourceName].ViewInfos.push_back(viewInfo);
			}
		}
	}

	MergeImageViewInfos(outImageCreateInfos);
	InitResourceInitialStatesAndClearValues(outImageCreateInfos); //TODO: embed it in this exact function (I don't have the time right now)
}

void D3D12::FrameGraphBuilder::MergeImageViewInfos(std::unordered_map<SubresourceName, TextureCreateInfo>& inoutTextureCreateInfos)
{
	//Fix all "arbitrary" texture view infos
	for(auto& nameWithCreateInfo: inoutTextureCreateInfos)
	{
		for(int i = 0; i < nameWithCreateInfo.second.ViewInfos.size(); i++)
		{
			if(nameWithCreateInfo.second.ViewInfos[i].Format == DXGI_FORMAT_UNKNOWN)
			{
				//Assign the closest non-UNDEFINED format
				for(int j = i + 1; j < (int)nameWithCreateInfo.second.ViewInfos.size(); j++)
				{
					if(nameWithCreateInfo.second.ViewInfos[j].Format != DXGI_FORMAT_UNKNOWN)
					{
						nameWithCreateInfo.second.ViewInfos[i].Format = nameWithCreateInfo.second.ViewInfos[j].Format;
						break;
					}
				}

				//Still UNDEFINED? Try to find the format in elements before
				if(nameWithCreateInfo.second.ViewInfos[i].Format == DXGI_FORMAT_UNKNOWN)
				{
					for(int j = i - 1; j >= 0; j++)
					{
						if(nameWithCreateInfo.second.ViewInfos[j].Format != DXGI_FORMAT_UNKNOWN)
						{
							nameWithCreateInfo.second.ViewInfos[i].Format = nameWithCreateInfo.second.ViewInfos[j].Format;
							break;
						}
					}
				}
			}

			assert(nameWithCreateInfo.second.ViewInfos[i].Format != DXGI_FORMAT_UNKNOWN);
		}

		//Merge all of the same image views into one
		std::sort(nameWithCreateInfo.second.ViewInfos.begin(), nameWithCreateInfo.second.ViewInfos.end(), [](const TextureViewInfo& left, const TextureViewInfo& right)
		{
			return (uint64_t)left.Format < (uint64_t)right.Format;
		});

		std::vector<TextureViewInfo> newViewInfos;

		DXGI_FORMAT currentFormat = DXGI_FORMAT_UNKNOWN;
		for(size_t i = 0; i < nameWithCreateInfo.second.ViewInfos.size(); i++)
		{
			if(currentFormat != nameWithCreateInfo.second.ViewInfos[i].Format)
			{
				TextureViewInfo viewInfo;
				viewInfo.Format = nameWithCreateInfo.second.ViewInfos[i].Format;
				viewInfo.ViewAddresses.push_back({.PassName = nameWithCreateInfo.second.ViewInfos[i].ViewAddresses[0].PassName, .SubresId = nameWithCreateInfo.second.ViewInfos[i].ViewAddresses[0].SubresId});

				newViewInfos.push_back(viewInfo);
			}
			else
			{
				newViewInfos.back().ViewAddresses.push_back({.PassName = nameWithCreateInfo.second.ViewInfos[i].ViewAddresses[0].PassName, .SubresId = nameWithCreateInfo.second.ViewInfos[i].ViewAddresses[0].SubresId});
			}

			currentFormat = nameWithCreateInfo.second.ViewInfos[i].Format;
		}

		nameWithCreateInfo.second.ViewInfos = newViewInfos;
	}
}

void D3D12::FrameGraphBuilder::InitResourceInitialStatesAndClearValues(std::unordered_map<SubresourceName, TextureCreateInfo>& inoutTextureCreateInfos)
{
	for(const auto& renderPassName: mRenderPassNames)
	{
		const auto& nameIdMap = mRenderPassesSubresourceNameIds[renderPassName];
		for(const auto& nameId : nameIdMap)
		{
			const SubresourceName subresourceName = nameId.first;
			const SubresourceId   subresourceId   = nameId.second;

			if(subresourceName != mBackbufferName)
			{
				const TextureSubresourceMetadata& metadata = mRenderPassesSubresourceMetadatas[renderPassName][subresourceId];
				TextureCreateInfo& textureCreateInfo       = inoutTextureCreateInfos[subresourceName];

				textureCreateInfo.InitialState = metadata.ResourceState; //Replace until the final state

				//If the resource already has optimized clear value, can't add a new one
				assert((textureCreateInfo.OptimizedClearValue.Format == DXGI_FORMAT_UNKNOWN) || ((metadata.ResourceState & (D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_DEPTH_WRITE)) == 0));

				if(metadata.ResourceState & D3D12_RESOURCE_STATE_RENDER_TARGET)
				{
					textureCreateInfo.OptimizedClearValue.Format   = metadata.Format;
					textureCreateInfo.OptimizedClearValue.Color[0] = 0.0f;
					textureCreateInfo.OptimizedClearValue.Color[1] = 0.0f;
					textureCreateInfo.OptimizedClearValue.Color[2] = 0.0f;
					textureCreateInfo.OptimizedClearValue.Color[3] = 0.0f;
				}
				else if(metadata.ResourceState & D3D12_RESOURCE_STATE_DEPTH_WRITE)
				{
					textureCreateInfo.OptimizedClearValue.Format               = metadata.Format;
					textureCreateInfo.OptimizedClearValue.DepthStencil.Depth   = 1.0f;
					textureCreateInfo.OptimizedClearValue.DepthStencil.Stencil = 0;
				}
			}
		}
	}
}

void D3D12::FrameGraphBuilder::SetSwapchainTextures(const std::unordered_map<SubresourceName, BackbufferCreateInfo>& backbufferResourceCreateInfos, const std::vector<ID3D12Resource2*>& swapchainTextures)
{
	mGraphToBuild->mSwapchainImageRefs.clear();

	mGraphToBuild->mTextures.push_back(nullptr);
	mGraphToBuild->mBackbufferRefIndex = (uint32_t)(mGraphToBuild->mTextures.size() - 1);

	for(size_t i = 0; i < swapchainTextures.size(); i++)
	{
		mGraphToBuild->mSwapchainImageRefs.push_back(swapchainTextures[i]);
	}
	
	for(const auto& nameWithCreateInfo: backbufferResourceCreateInfos)
	{
		//IMAGE VIEWS
		for(const auto& imageViewInfo: nameWithCreateInfo.second.ViewInfos)
		{
			for(const auto& viewAddress: imageViewInfo.ViewAddresses)
			{
				mRenderPassesSubresourceMetadatas[viewAddress.PassName][viewAddress.SubresId].ImageIndex = mGraphToBuild->mBackbufferRefIndex;
			}
		}
	}
}

void D3D12::FrameGraphBuilder::CreateTextures(ID3D12Device8* device, const std::unordered_map<SubresourceName, TextureCreateInfo>& textureCreateInfos, const MemoryManager* memoryAllocator)
{
	mGraphToBuild->mTextures.clear();
	mGraphToBuild->mTextureHeap.reset();

	std::unordered_map<SubresourceName, size_t> textureIndices;

	std::vector<D3D12_RESOURCE_DESC1> textureDescsVec;
	textureDescsVec.reserve(textureCreateInfos.size());
	for(const auto& it: textureCreateInfos)
	{
		textureDescsVec.push_back(it.second.ResourceDesc);
		textureIndices[it.first] = (textureDescsVec.size() - 1);
	}

	std::vector<UINT64> textureHeapOffsets;
	textureHeapOffsets.reserve(textureDescsVec.size());
	memoryAllocator->AllocateTextureMemory(device, textureDescsVec, textureHeapOffsets, mGraphToBuild->mTextureHeap.put());

	for(const auto& nameWithCreateInfo: textureCreateInfos)
	{
		const SubresourceName&   subresourceName = nameWithCreateInfo.first;
		const TextureCreateInfo& createInfo      = nameWithCreateInfo.second;

		size_t textureIndex = textureIndices[subresourceName];

		const D3D12_CLEAR_VALUE* resourceClearValuePtr = nullptr;
		if(createInfo.OptimizedClearValue.Format != DXGI_FORMAT_UNKNOWN) //Only use initialized values
		{
			resourceClearValuePtr = &createInfo.OptimizedClearValue;
		}

		mGraphToBuild->mOwnedResources.emplace_back();
		THROW_IF_FAILED(device->CreatePlacedResource1(mGraphToBuild->mTextureHeap.get(), textureHeapOffsets[textureIndex], &createInfo.ResourceDesc, createInfo.InitialState, resourceClearValuePtr, IID_PPV_ARGS(mGraphToBuild->mOwnedResources.back().put())));
		
		mGraphToBuild->mTextures.push_back(mGraphToBuild->mOwnedResources.back().get());

		for(const auto& imageViewInfo: nameWithCreateInfo.second.ViewInfos)
		{
			for(const auto& viewAddress: imageViewInfo.ViewAddresses)
			{
				mRenderPassesSubresourceMetadatas[viewAddress.PassName][viewAddress.SubresId].ImageIndex = (uint32_t)(mGraphToBuild->mTextures.size() - 1);
			}
		}
		
		SetDebugObjectName(mGraphToBuild->mOwnedResources.back().get(), nameWithCreateInfo.first); //Only does something in debug
	}
}

bool D3D12::FrameGraphBuilder::PassesIntersect(const RenderPassName& writingPass, const RenderPassName& readingPass)
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

void D3D12::FrameGraphBuilder::SetDebugObjectName(ID3D12Resource2* texture, const SubresourceName& name) const
{
#if defined(DEBUG) || defined(_DEBUG)
	THROW_IF_FAILED(texture->SetName(Utils::ConvertUTF8ToWstring(name).c_str()));
#else
	UNREFERENCED_PARAMETER(image);
	UNREFERENCED_PARAMETER(name);
#endif
}