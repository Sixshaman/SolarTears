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
#include <numeric>

D3D12::FrameGraphBuilder::FrameGraphBuilder(ID3D12Device8* device, FrameGraph* graphToBuild, const RenderableScene* scene, const DeviceFeatures* deviceFeatures, 
	                                        const SwapChain* swapChain, const ShaderManager* shaderManager, const MemoryManager* memoryManager): ModernFrameGraphBuilder(graphToBuild), mD3d12GraphToBuild(graphToBuild), mDevice(device),
	                                                                                                                                             mScene(scene), mSwapChain(swapChain), mDeviceFeatures(deviceFeatures), 
	                                                                                                                                             mShaderManager(shaderManager), mMemoryAllocator(memoryManager)
{
	mSrvUavCbvDescriptorSize = 0;
	mRtvDescriptorSize       = 0;
	mDsvDescriptorSize       = 0;
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

void D3D12::FrameGraphBuilder::SetPassSubresourceFormat(const std::string_view passName, const std::string_view subresourceId, DXGI_FORMAT format)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].Format = format;
}

void D3D12::FrameGraphBuilder::SetPassSubresourceState(const std::string_view passName, const std::string_view subresourceId, D3D12_RESOURCE_STATES state)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].State = state;
}

void D3D12::FrameGraphBuilder::EnableSubresourceAutoBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBaarrier)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	if(autoBaarrier)
	{
		mSubresourceInfos[subresourceInfoIndex].Flags |= TextureFlagAutoBarrier;
	}
	else
	{
		mSubresourceInfos[subresourceInfoIndex].Flags &= ~TextureFlagAutoBarrier;
	}
}

ID3D12Device8* D3D12::FrameGraphBuilder::GetDevice() const
{
	return mDevice;
}

const D3D12::RenderableScene* D3D12::FrameGraphBuilder::GetScene() const
{
	return mScene;
}

const D3D12::SwapChain* D3D12::FrameGraphBuilder::GetSwapChain() const
{
	return mSwapChain;
}

const D3D12::DeviceFeatures* D3D12::FrameGraphBuilder::GetDeviceFeatures() const
{
	return mDeviceFeatures;
}

const D3D12::ShaderManager* D3D12::FrameGraphBuilder::GetShaderManager() const
{
	return mShaderManager;
}

const D3D12::MemoryManager* D3D12::FrameGraphBuilder::GetMemoryManager() const
{
	return mMemoryAllocator;
}

ID3D12Resource2* D3D12::FrameGraphBuilder::GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadataNode.ImageIndex == (uint32_t)(-1))
	{
		return nullptr;
	}
	else
	{
		return mD3d12GraphToBuild->mTextures[metadataNode.ImageIndex];
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceSrvUav(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	const SubresourceInfo& subresourceInfo = mSubresourceInfos[subresourceInfoIndex];

	const D3D12_RESOURCE_STATES resourceState = (D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	assert((subresourceInfo.State & resourceState) && (subresourceInfo.DescriptorHeapIndex != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphSrvHeapStart();
	descriptorHandle.ptr                        += subresourceInfo.DescriptorHeapIndex * mSrvUavCbvDescriptorSize;

	return descriptorHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceRtv(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	const SubresourceInfo& subresourceInfo = mSubresourceInfos[subresourceInfoIndex];

	const D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	assert((subresourceInfo.State & resourceState) && (subresourceInfo.DescriptorHeapIndex != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphRtvHeapStart();
	descriptorHandle.ptr                        += subresourceInfo.DescriptorHeapIndex * mRtvDescriptorSize;

	return descriptorHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceDsv(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	const SubresourceInfo& subresourceInfo = mSubresourceInfos[subresourceInfoIndex];

	const D3D12_RESOURCE_STATES resourceState = (D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE);
	assert((subresourceInfo.State & resourceState) && (subresourceInfo.DescriptorHeapIndex != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphDsvHeapStart();
	descriptorHandle.ptr                        += subresourceInfo.DescriptorHeapIndex * mDsvDescriptorSize;

	return descriptorHandle;
}

DXGI_FORMAT D3D12::FrameGraphBuilder::GetRegisteredSubresourceFormat(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Format;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetRegisteredSubresourceState(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].State;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetPreviousPassSubresourceState(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).PrevPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].State;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetNextPassSubresourceState(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).NextPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].State;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetFrameGraphSrvHeapStart() const
{
	return mD3d12GraphToBuild->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetFrameGraphRtvHeapStart() const
{
	return mD3d12GraphToBuild->mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetFrameGraphDsvHeapStart() const
{
	return mD3d12GraphToBuild->mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
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
					*    1.  Same queue, state unchanged:                                           No barrier needed
					*    2.  Same queue, state changed, Promoted read-only -> PRESENT:              No barrier needed, state decays
					*    3.  Same queue, state changed, Promoted writeable/Non-promoted -> PRESENT: Need a barrier Old State -> PRESENT
					*    4.  Same queue, state changed, other cases:                                No barrier needed, will be handled by the next state's barrier
					*
					*    5.  Graphics -> Compute,  state will be automatically promoted:               No barrier needed
					*    6.  Graphics -> Compute,  state will not be promoted, was promoted read-only: No barrier needed, will be handled by the next state's barrier
					*    7.  Graphics -> Compute,  state unchanged:                                    No barrier needed
					*    8.  Graphics -> Compute,  state changed other cases:                          Need a barrier Old state -> New state
					* 
					*    9.  Compute  -> Graphics, state will be automatically promoted:                 No barrier needed
					* 	 10. Compute  -> Graphics, state will not be promoted, was promoted read-only:   No barrier needed, will be handled by the next state's barrier     
					*    11. Compute  -> Graphics, state changed Promoted read-only -> PRESENT:          No barrier needed, state will decay
					*    12. Compute  -> Graphics, state changed Compute/graphics   -> Compute/Graphics: Need a barrier Old state -> New state
					*    13. Compute  -> Graphics, state changed Compute/Graphics   -> Graphics:         No barrier needed, will be handled by the next state's barrier
					*    14. Compute  -> Graphics, state unchanged:                                      No barrier needed
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
						if(nextPassState == D3D12_RESOURCE_STATE_PRESENT) //Rules 2, 3
						{
							needBarrier = (!prevWasPromoted || D3D12Utils::IsStateWriteable(prevPassState));
						}
						else //Rules 1, 4
						{
							needBarrier = false;
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT && nextStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE) //Rules 5, 6, 7, 8
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 6
						{
							needBarrier = false;
						}
						else //Rules 5, 7, 8
						{
							needBarrier = !nextIsPromoted && (prevPassState != nextPassState);
						}
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COMPUTE && nextStateQueue == D3D12_COMMAND_LIST_TYPE_DIRECT) //Rules 9, 10, 11, 12, 13, 14
					{
						if(prevWasPromoted && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 10, 11
						{
							needBarrier = false;
						}
						else //Rules 9, 12, 13, 14
						{
							needBarrier = !nextIsPromoted && (prevPassState != nextPassState) && D3D12Utils::IsStateComputeFriendly(nextPassState);
						}
					}
					else if(nextStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 15, 16
					{
						needBarrier = !prevWasPromoted || D3D12Utils::IsStateWriteable(prevPassState);
					}
					else if(prevStateQueue == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 17, 18
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
							afterSwapchainBarrierIndices.push_back(afterBarriers.size() - 1);
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
				viewInfo.ViewType = ViewTypeFromResourceState(metadata.ResourceState);
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

				//It's fine if format is UNDEFINED. It means it can be arbitrary and we'll fix it later based on other image view infos for this resource
				TextureViewInfo viewInfo;
				viewInfo.Format   = metadata.Format;
				viewInfo.ViewType = viewType;
				viewInfo.ViewAddresses.push_back({.PassName = renderPassName, .SubresId = subresourceId});

				outImageCreateInfos[subresourceName].ViewInfos.push_back(viewInfo);
			}
		}
	}

	MergeImageViewInfos(outImageCreateInfos);
	InitResourceInitialStatesAndClearValues(outImageCreateInfos); //TODO: embed it in this exact function (I don't have the time right now)
	CleanDenyShaderResourceFlags(outImageCreateInfos);
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

void D3D12::FrameGraphBuilder::CleanDenyShaderResourceFlags(std::unordered_map<SubresourceName, TextureCreateInfo>& inoutTextureCreateInfos)
{
	for(auto& nameWithCreateInfo: inoutTextureCreateInfos)
	{
		D3D12_RESOURCE_DESC1& resourceDesc = nameWithCreateInfo.second.ResourceDesc;
		if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == 0)
		{
			resourceDesc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
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
		SetDebugObjectName(mGraphToBuild->mSwapchainImageRefs.back(), mBackbufferName);
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

void D3D12::FrameGraphBuilder::CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos)
{
	mD3d12GraphToBuild->mTextures.resize(textureCreateInfos.size());
	mD3d12GraphToBuild->mTextureHeap.reset();

	std::vector<D3D12_RESOURCE_DESC1> textureDescsVec;
	textureDescsVec.resize(textureCreateInfos.size());

	//For TYPELESS formats, we lose the information for clear values. Fortunately, this is not a very common case
	//It's only needed for RTV formats, since depth-stencil format can actually be recovered from a typeless one
	std::unordered_map<uint32_t, DXGI_FORMAT> optimizedClearFormats;

	for(const TextureResourceCreateInfo& textureCreateInfo: textureCreateInfos)
	{
		SubresourceMetadataNode* headNode    = textureCreateInfo.MetadataHead;
		SubresourceMetadataNode* currentNode = headNode;

		DXGI_FORMAT headFormat     = mSubresourceInfos[headNode->SubresourceInfoIndex].Format;
		DXGI_FORMAT format         = headFormat;
		DXGI_FORMAT formatForClear = DXGI_FORMAT_UNKNOWN;

		D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		do
		{
			const SubresourceInfo& subresourceInfo = mSubresourceInfos[currentNode->SubresourceInfoIndex];
			if(subresourceInfo.Format != headFormat)
			{
				format = D3D12Utils::ConvertToTypeless(headFormat);
			}

			if(subresourceInfo.State & (D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_DEPTH_READ))
			{
				resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				formatForClear = subresourceInfo.Format;
			}

			if(subresourceInfo.State & (D3D12_RESOURCE_STATE_RENDER_TARGET))
			{
				resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				formatForClear = subresourceInfo.Format;
			}

			if(subresourceInfo.State & (D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
			{
				resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			if(subresourceInfo.State & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
			{
				resourceFlags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
			}

		} while(currentNode != headNode);

		//D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE is only allowed with D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		if((resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) && !(resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			resourceFlags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}

		if(D3D12Utils::IsTypelessFormat(format) && !(resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) && (resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
		{
			optimizedClearFormats[headNode->ImageIndex] = formatForClear;
		}

		D3D12_RESOURCE_DESC1 resourceDesc;
		resourceDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resourceDesc.Width                           = GetConfig()->GetViewportWidth();
		resourceDesc.Height                          = GetConfig()->GetViewportHeight();
		resourceDesc.DepthOrArraySize                = 1;
		resourceDesc.MipLevels                       = 1;
		resourceDesc.Format                          = format;
		resourceDesc.SampleDesc.Count                = 1;
		resourceDesc.SampleDesc.Quality              = 1;
		resourceDesc.Layout                          = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags                           = resourceFlags;
		resourceDesc.SamplerFeedbackMipRegion.Width  = 0;
		resourceDesc.SamplerFeedbackMipRegion.Height = 0;
		resourceDesc.SamplerFeedbackMipRegion.Depth  = 0;

		textureDescsVec[headNode->ImageIndex] = resourceDesc;
	}

	std::vector<UINT64> textureHeapOffsets;
	textureHeapOffsets.reserve(textureDescsVec.size());
	mMemoryAllocator->AllocateTextureMemory(mDevice, textureDescsVec, textureHeapOffsets, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, mD3d12GraphToBuild->mTextureHeap.put());

	for(const TextureResourceCreateInfo& textureCreateInfo: textureCreateInfos)
	{
		uint32_t imageIndex = textureCreateInfo.MetadataHead->ImageIndex;
		const D3D12_RESOURCE_DESC1& resourceDesc = textureDescsVec[imageIndex];

		D3D12_CLEAR_VALUE  clearValue;
		D3D12_CLEAR_VALUE* clearValuePtr = nullptr;
		if(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		{
			if(D3D12Utils::IsTypelessFormat(resourceDesc.Format))
			{
				clearValue.Format = optimizedClearFormats[imageIndex];
			}
			else
			{
				clearValue.Format = resourceDesc.Format;
			}

			clearValue.Color[0] = 0.0f;
			clearValue.Color[1] = 0.0f;
			clearValue.Color[2] = 0.0f;
			clearValue.Color[3] = 1.0f;

			clearValuePtr = &clearValue;
		}
		else if(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		{
			clearValue.Format               = D3D12Utils::ConvertToDepthStencil(resourceDesc.Format);
			clearValue.DepthStencil.Depth   = 1.0f;
			clearValue.DepthStencil.Stencil = 0;

			clearValuePtr = &clearValue;
		}

		uint32_t lastSubresourceIndex = textureCreateInfo.MetadataHead->PrevPassNode->SubresourceInfoIndex;
		D3D12_RESOURCE_STATES lastState = mSubresourceInfos[lastSubresourceIndex].State;

		mD3d12GraphToBuild->mOwnedResources.emplace_back();
		THROW_IF_FAILED(mDevice->CreatePlacedResource1(mD3d12GraphToBuild->mTextureHeap.get(), textureHeapOffsets[imageIndex], &resourceDesc, lastState, clearValuePtr, IID_PPV_ARGS(mD3d12GraphToBuild->mOwnedResources.back().put())));
		
		mD3d12GraphToBuild->mTextures[imageIndex] = mD3d12GraphToBuild->mOwnedResources.back().get();
		
#if defined(DEBUG) || defined(_DEBUG)
		D3D12Utils::SetDebugObjectName(mD3d12GraphToBuild->mTextures[imageIndex], textureCreateInfo.Name);
#endif
	}
}

void D3D12::FrameGraphBuilder::CreateDescriptors(ID3D12Device8* device, const std::unordered_map<SubresourceName, TextureCreateInfo>& textureCreateInfos)
{
	mGraphToBuild->mSrvUavDescriptorHeap.reset();
	mGraphToBuild->mRtvDescriptorHeap.reset();
	mGraphToBuild->mDsvDescriptorHeap.reset();

	UINT srvUavDescriptorCount = 0;
	UINT rtvDescriptorCount    = 0;
	UINT dsvDescriptorCount    = 0;
	for(auto nameWithCreateInfo: textureCreateInfos)
	{
		const auto& viewInfos = nameWithCreateInfo.second.ViewInfos;
		for(size_t i = 0; i < viewInfos.size(); i++)
		{
			switch (viewInfos[i].ViewType)
			{
			case ShaderResource:
			case UnorderedAccess:
				srvUavDescriptorCount++;
				break;
			case RenderTarget:
				rtvDescriptorCount++;
				break;
			case DepthStencil:
				dsvDescriptorCount++;
				break;
			default:
				break;
			}
		}
	}


	//TODO: mGPU?
	D3D12_DESCRIPTOR_HEAP_DESC srvUavDescriptorHeapDesc;
	srvUavDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvUavDescriptorHeapDesc.NumDescriptors = srvUavDescriptorCount;
	srvUavDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	srvUavDescriptorHeapDesc.NodeMask       = 0;

	THROW_IF_FAILED(device->CreateDescriptorHeap(&srvUavDescriptorHeapDesc, IID_PPV_ARGS(mGraphToBuild->mSrvUavDescriptorHeap.put())));

	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
	rtvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NumDescriptors = rtvDescriptorCount;
	rtvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDescriptorHeapDesc.NodeMask       = 0;

	THROW_IF_FAILED(device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(mGraphToBuild->mRtvDescriptorHeap.put())));
	
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
	dsvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescriptorHeapDesc.NumDescriptors = dsvDescriptorCount;
	dsvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDescriptorHeapDesc.NodeMask       = 0;

	THROW_IF_FAILED(device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(mGraphToBuild->mDsvDescriptorHeap.put())));

	UINT srvUavCounter = 0;
	UINT rtvCounter    = 0;
	UINT dsvCounter    = 0;
	for(auto nameWithCreateInfo: textureCreateInfos)
	{
		const auto& viewInfos = nameWithCreateInfo.second.ViewInfos;
		for(size_t i = 0; i < viewInfos.size(); i++)
		{
			const SubresourceAddress& firstAddress    = viewInfos[i].ViewAddresses[0];
			TextureSubresourceMetadata& firstMetadata = mRenderPassesSubresourceMetadatas.at(firstAddress.PassName).at(firstAddress.SubresId);

			ID3D12Resource* descriptorResource = mGraphToBuild->mTextures[firstMetadata.ImageIndex];
			switch (viewInfos[i].ViewType)
			{
			case ShaderResource:
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
				srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Format                        = viewInfos[i].Format;
				srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Texture2D.MipLevels           = 1;
				srvDesc.Texture2D.MostDetailedMip     = 0;
				srvDesc.Texture2D.PlaneSlice          = 0;
				srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

				D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = mGraphToBuild->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				srvHandle.ptr                        += srvUavCounter * mSrvUavCbvDescriptorSize;
				
				device->CreateShaderResourceView(descriptorResource, &srvDesc, srvHandle);
				for(size_t subresourceAddressIndex = 0; subresourceAddressIndex < viewInfos[i].ViewAddresses.size(); subresourceAddressIndex++)
				{
					mRenderPassesSubresourceMetadatas.at(firstAddress.PassName).at(firstAddress.SubresId).SrvUavIndex = srvUavCounter;
				}

				srvUavCounter++;
				break;
			}
			case UnorderedAccess:
			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				uavDesc.ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Format               = viewInfos[i].Format;
				uavDesc.Texture2D.MipSlice   = 0;
				uavDesc.Texture2D.PlaneSlice = 0;

				D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = mGraphToBuild->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				uavHandle.ptr                        += srvUavCounter * mSrvUavCbvDescriptorSize;
				
				device->CreateUnorderedAccessView(descriptorResource, nullptr, &uavDesc, uavHandle);
				for(size_t subresourceAddressIndex = 0; subresourceAddressIndex < viewInfos[i].ViewAddresses.size(); subresourceAddressIndex++)
				{
					mRenderPassesSubresourceMetadatas.at(firstAddress.PassName).at(firstAddress.SubresId).SrvUavIndex = srvUavCounter;
				}

				srvUavCounter++;
				break;
			}
			case RenderTarget:
			{
				D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
				rtvDesc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
				rtvDesc.Format               = viewInfos[i].Format;
				rtvDesc.Texture2D.MipSlice   = 0;
				rtvDesc.Texture2D.PlaneSlice = 0;

				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mGraphToBuild->mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				rtvHandle.ptr                        += rtvCounter * mRtvDescriptorSize;
				
				device->CreateRenderTargetView(descriptorResource, &rtvDesc, rtvHandle);
				for(size_t subresourceAddressIndex = 0; subresourceAddressIndex < viewInfos[i].ViewAddresses.size(); subresourceAddressIndex++)
				{
					mRenderPassesSubresourceMetadatas.at(firstAddress.PassName).at(firstAddress.SubresId).RtvIndex = rtvCounter;
				}

				rtvCounter++;
				break;
			}
			case DepthStencil:
			{
				D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
				dsvDesc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsvDesc.Format             = viewInfos[i].Format;
				dsvDesc.Texture2D.MipSlice = 0;

				D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mGraphToBuild->mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				dsvHandle.ptr                        += dsvCounter * mDsvDescriptorSize;
				
				device->CreateDepthStencilView(descriptorResource, &dsvDesc, dsvHandle);
				for(size_t subresourceAddressIndex = 0; subresourceAddressIndex < viewInfos[i].ViewAddresses.size(); subresourceAddressIndex++)
				{
					mRenderPassesSubresourceMetadatas.at(firstAddress.PassName).at(firstAddress.SubresId).DsvIndex = dsvCounter;
				}

				dsvCounter++;
				break;
			}
			default:
				break;
			}
		}
	}
}

void D3D12::FrameGraphBuilder::AllocateTextureViews(size_t textureViewCount)
{
	mD3d12GraphToBuild->mImageViews.resize(textureViewCount);
}

uint32_t D3D12::FrameGraphBuilder::AddSubresourceMetadata()
{
	SubresourceInfo subresourceInfo;
	subresourceInfo.Format              = DXGI_FORMAT_UNKNOWN;
	subresourceInfo.State               = D3D12_RESOURCE_STATE_COMMON;
	subresourceInfo.DescriptorHeapIndex = (uint32_t)(-1);
	subresourceInfo.Flags               = 0;

	mSubresourceInfos.push_back(subresourceInfo);
	return (uint32_t)(mSubresourceInfos.size() - 1);
}

bool D3D12::FrameGraphBuilder::PropagateSubresourceParameters(uint32_t indexFrom, uint32_t indexTo)
{
	SubresourceInfo& currentPassSubresourceInfo = mSubresourceInfos[indexFrom];
	SubresourceInfo& nextPassSubresourceInfo    = mSubresourceInfos[indexTo];

	if(nextPassSubresourceInfo.Format == DXGI_FORMAT_UNKNOWN && currentPassSubresourceInfo.Format != DXGI_FORMAT_UNKNOWN)
	{
		nextPassSubresourceInfo.Format = currentPassSubresourceInfo.Format;
	}

	//Other parameters are always fully known

	if(currentPassSubresourceInfo.Format == DXGI_FORMAT_UNKNOWN)
	{
		return false;
	}

	return true;
}

void D3D12::FrameGraphBuilder::BuildUniqueSubresourceList(const std::vector<uint32_t>& subresourceInfoIndices, std::vector<uint32_t>& outUniqueSubresourceInfoIndices, std::vector<uint32_t>& outNewIndexMap)
{
	if(subresourceInfoIndices.size() == 0)
	{
		return;
	}

	std::vector<uint32_t> reorderMap(subresourceInfoIndices.size());
	std::iota(reorderMap.begin(), reorderMap.end(), 0);

	std::sort(reorderMap.begin(), reorderMap.end(), [this, &subresourceInfoIndices](uint32_t left, uint32_t right)
	{
		const uint32_t leftIndex  = subresourceInfoIndices[left];
		const uint32_t rightIndex = subresourceInfoIndices[right];

		const SubresourceInfo& leftSubresource  = mSubresourceInfos[leftIndex];
		const SubresourceInfo& rightSubresource = mSubresourceInfos[rightIndex];

		return (uint32_t)leftSubresource.Format < (uint32_t)rightSubresource.Format;
	});

	outUniqueSubresourceInfoIndices.clear();
	outUniqueSubresourceInfoIndices.push_back(subresourceInfoIndices[reorderMap[0]]);

	outNewIndexMap.resize(subresourceInfoIndices.size());
	outNewIndexMap[reorderMap[0]] = 0;

	for(size_t i = 1; i < reorderMap.size(); i++)
	{
		uint32_t leftIndicesIndex  = reorderMap[i - 1];
		uint32_t rightIndicesIndex = reorderMap[i];

		uint32_t leftIndex  = subresourceInfoIndices[leftIndicesIndex];
		uint32_t rightIndex = subresourceInfoIndices[rightIndicesIndex];

		const SubresourceInfo& leftSubresource  = mSubresourceInfos[leftIndex];
		const SubresourceInfo& rightSubresource = mSubresourceInfos[rightIndex];

		if(leftSubresource.Format != rightSubresource.Format)
		{
			outUniqueSubresourceInfoIndices.push_back(subresourceInfoIndices[rightIndicesIndex]);
		}

		outNewIndexMap[reorderMap[i]] = outUniqueSubresourceInfoIndices.size() - 1;
	}
}

void D3D12::FrameGraphBuilder::CreateTextureViews(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<uint32_t>& subresourceInfoIndices) const
{
	for(const TextureResourceCreateInfo& textureCreateInfo: textureCreateInfos)
	{
		TextureSubresourceInfoSpan subresourceInfoSpan = textureCreateInfo.SubresourceSpan;

		uint32_t imageIndex = textureCreateInfo.MetadataHead->ImageIndex;
		VkImage  image      = mVulkanGraphToBuild->mImages[imageIndex];
		for(uint32_t subresourceInfoIndexIndex = subresourceInfoSpan.FirstSubresourceInfoIndex; subresourceInfoIndexIndex < subresourceInfoSpan.LastSubresourceInfoIndex; subresourceInfoIndexIndex++)
		{
			uint32_t subresourceInfoIndex = subresourceInfoIndices[subresourceInfoIndexIndex];

			//TODO: fragment density map
			VkImageViewCreateInfo imageViewCreateInfo;
			imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.pNext                           = nullptr;
			imageViewCreateInfo.flags                           = 0;
			imageViewCreateInfo.image                           = image;
			imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format                          = mSubresourceInfos[subresourceInfoIndex].Format;
			imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.subresourceRange.aspectMask     = mSubresourceInfos[subresourceInfoIndex].Aspect;

			imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
			imageViewCreateInfo.subresourceRange.levelCount     = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount     = 1;

			VkImageView imageView = VK_NULL_HANDLE;
			ThrowIfFailed(vkCreateImageView(mVulkanGraphToBuild->mDeviceRef, &imageViewCreateInfo, nullptr, &imageView));

			mVulkanGraphToBuild->mImageViews[subresourceInfoIndex] = imageView;
		}
	}
}

uint32_t D3D12::FrameGraphBuilder::AllocateBackbufferResources() const
{
	mVulkanGraphToBuild->mImages.push_back(VK_NULL_HANDLE);

	mVulkanGraphToBuild->mSwapchainImageRefs.clear();
	for(size_t i = 0; i < SwapChain::SwapchainImageCount; i++)
	{
		mVulkanGraphToBuild->mSwapchainImageRefs.push_back(mSwapChain->GetSwapchainImage(i));
	}

	mVulkanGraphToBuild->mSwapchainImageViews.clear();
	mVulkanGraphToBuild->mSwapchainViewsSwapMap.clear();

	return (uint32_t)(mVulkanGraphToBuild->mImages.size() - 1);
}