#include "D3D12FrameGraphBuilder.hpp"
#include "D3D12FrameGraph.hpp"
#include "D3D12RenderPassDispatchFuncs.hpp"
#include "../D3D12DeviceFeatures.hpp"
#include "../D3D12WorkerCommandLists.hpp"
#include "../D3D12Memory.hpp"
#include "../D3D12Shaders.hpp"
#include "../D3D12SwapChain.hpp"
#include "../D3D12DeviceQueues.hpp"
#include "../../../Core/Util.hpp"
#include <algorithm>
#include <numeric>

D3D12::FrameGraphBuilder::FrameGraphBuilder(FrameGraph* graphToBuild, const SwapChain* swapChain): ModernFrameGraphBuilder(graphToBuild), mD3d12GraphToBuild(graphToBuild), mSwapChain(swapChain)
{
}

D3D12::FrameGraphBuilder::~FrameGraphBuilder()
{
}

ID3D12Device8* D3D12::FrameGraphBuilder::GetDevice() const
{
	return mDevice;
}

const D3D12::ShaderManager* D3D12::FrameGraphBuilder::GetShaderManager() const
{
	return mShaderManager;
}

ID3D12Resource2* D3D12::FrameGraphBuilder::GetRegisteredResource(uint32_t passIndex, uint_fast16_t subresourceIndex, uint32_t frame) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;

	const SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex];
	const ResourceMetadata& resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];
	if(resourceMetadata.FirstFrameHandle == (uint32_t)(-1))
	{
		return nullptr;
	}
	else
	{
		if(resourceMetadata.FirstFrameHandle == GetBackbufferImageSpan().Begin)
		{
			uint32_t passPeriod = mPassMetadatas[passIndex].OwnPeriod;
			return mD3d12GraphToBuild->mTextures[resourceMetadata.FirstFrameHandle + frame / passPeriod];
		}
		else
		{
			return mD3d12GraphToBuild->mTextures[resourceMetadata.FirstFrameHandle + frame % resourceMetadata.FrameCount];
		}
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceSrvUav(uint32_t passIndex, uint_fast16_t subresourceIndex, uint32_t frame) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;

	const SubresourceMetadataNode&    metadataNode     = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex];
	const SubresourceMetadataPayload& metadataPayload  = mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex];
	const ResourceMetadata&           resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];

	const D3D12_RESOURCE_STATES resourceState = (D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	assert((metadataPayload.State & resourceState) && (metadataNode.FirstFrameViewHandle != (uint32_t)(-1)));

	D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphSrvHeapStart();
	UINT descriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	if(resourceMetadata.FirstFrameHandle == GetBackbufferImageSpan().Begin)
	{
		uint32_t passPeriod = mPassMetadatas[passIndex].OwnPeriod;
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame / passPeriod) * descriptorSize;
	}
	else
	{
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame % resourceMetadata.FrameCount) * descriptorSize;
	}

	return descriptorHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceRtv(uint32_t passIndex, uint_fast16_t subresourceIndex, uint32_t frame) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;

	const SubresourceMetadataNode&    metadataNode     = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex];
	const SubresourceMetadataPayload& metadataPayload  = mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex];
	const ResourceMetadata&           resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];

	const D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	assert((metadataPayload.State & resourceState) && (metadataNode.FirstFrameViewHandle != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphRtvHeapStart();
	UINT descriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	if(resourceMetadata.FirstFrameHandle == GetBackbufferImageSpan().Begin)
	{
		uint32_t passPeriod = mPassMetadatas[passIndex].OwnPeriod;
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame / passPeriod) * descriptorSize;
	}
	else
	{
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame % resourceMetadata.FrameCount) * descriptorSize;
	}

	return descriptorHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceDsv(uint32_t passIndex, uint_fast16_t subresourceIndex, uint32_t frame) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;

	const SubresourceMetadataNode&    metadataNode     = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex];
	const SubresourceMetadataPayload& metadataPayload  = mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex];
	const ResourceMetadata&           resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];

	const D3D12_RESOURCE_STATES resourceState = (D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE);
	assert((metadataPayload.State & resourceState) && (metadataNode.FirstFrameViewHandle != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphDsvHeapStart();
	UINT descriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	if(resourceMetadata.FirstFrameHandle == GetBackbufferImageSpan().Begin)
	{
		uint32_t passPeriod = mPassMetadatas[passIndex].OwnPeriod;
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame / passPeriod) * descriptorSize;
	}
	else
	{
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame % resourceMetadata.FrameCount) * descriptorSize;
	}

	return descriptorHandle;
}

DXGI_FORMAT D3D12::FrameGraphBuilder::GetRegisteredSubresourceFormat(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Format;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetRegisteredSubresourceState(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].State;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetPreviousPassSubresourceState(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].State;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetNextPassSubresourceState(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].State;
}

void D3D12::FrameGraphBuilder::Build(FrameGraphDescription&& frameGraphDescription, const FrameGraphBuildInfo& buildInfo)
{
	mDevice          = buildInfo.Device;
	mShaderManager   = buildInfo.ShaderMgr;
	mMemoryAllocator = buildInfo.MemoryAllocator;

	ModernFrameGraphBuilder::Build(std::move(frameGraphDescription));
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetFrameGraphSrvHeapStart() const
{
	return mD3d12GraphToBuild->mFrameGraphDescriptorStart; 
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetFrameGraphRtvHeapStart() const
{
	return mD3d12GraphToBuild->mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetFrameGraphDsvHeapStart() const
{
	return mD3d12GraphToBuild->mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3D12::FrameGraphBuilder::InitMetadataPayloads()
{
	mSubresourceMetadataPayloads.resize(mSubresourceMetadataNodesFlat.size(), SubresourceMetadataPayload());

	for(const PassMetadata& passMetadata: mPassMetadatas)
	{
		Span<uint32_t> passMetadataSpan = passMetadata.SubresourceMetadataSpan;
		std::span<SubresourceMetadataPayload> payloadSpan = {mSubresourceMetadataPayloads.begin() + passMetadataSpan.Begin, mSubresourceMetadataPayloads.begin() + passMetadataSpan.End};

		RegisterPassSubresources(passMetadata.Type, payloadSpan);
	}


	uint32_t backbufferPayloadIndex = mPresentPassMetadata.SubresourceMetadataSpan.Begin + (uint32_t)PresentPassSubresourceId::Backbuffer;
	mSubresourceMetadataPayloads[backbufferPayloadIndex].Format = mSwapChain->GetBackbufferFormat();
	mSubresourceMetadataPayloads[backbufferPayloadIndex].State  = D3D12_RESOURCE_STATE_PRESENT;
	mSubresourceMetadataPayloads[backbufferPayloadIndex].Flags  = 0;
}

bool D3D12::FrameGraphBuilder::IsReadSubresource(uint32_t subresourceInfoIndex)
{
	D3D12_RESOURCE_STATES readStates = D3D12_RESOURCE_STATE_COMMON | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_UNORDERED_ACCESS 
			                         | D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
			                         | D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_RESOLVE_SOURCE | D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE | D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE
			                         | D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_PREDICATION | D3D12_RESOURCE_STATE_VIDEO_DECODE_READ | D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ | D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ;

	return mSubresourceMetadataPayloads[subresourceInfoIndex].State & readStates;
}

bool D3D12::FrameGraphBuilder::IsWriteSubresource(uint32_t subresourceInfoIndex)
{
	D3D12_RESOURCE_STATES writeStates = D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_DEPTH_WRITE 
	                                  | D3D12_RESOURCE_STATE_STREAM_OUT | D3D12_RESOURCE_STATE_COPY_DEST | D3D12_RESOURCE_STATE_RESOLVE_DEST | D3D12_RESOURCE_STATE_PREDICATION 
			                          | D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE | D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE | D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE;

	return mSubresourceMetadataPayloads[subresourceInfoIndex].State & writeStates;
}

bool D3D12::FrameGraphBuilder::PropagateSubresourcePayloadDataVertically(const ResourceMetadata& resourceMetadata)
{
	bool propagationHappened = false;

	uint32_t headNodeIndex = resourceMetadata.HeadNodeIndex;
	uint32_t currNodeIndex = headNodeIndex;

	do
	{
		const SubresourceMetadataNode& subresourceNode     = mSubresourceMetadataNodesFlat[currNodeIndex];
		const SubresourceMetadataNode& nextSubresourceNode = mSubresourceMetadataNodesFlat[subresourceNode.NextPassNodeIndex];
			
		const SubresourceMetadataPayload& currSubresourcePayload = mSubresourceMetadataPayloads[currNodeIndex];
		SubresourceMetadataPayload&       nextSubresourcePayload = mSubresourceMetadataPayloads[subresourceNode.NextPassNodeIndex];

		//Format gets propagated from the previous pass to the current pass
		if(nextSubresourcePayload.Format == DXGI_FORMAT_UNKNOWN && currSubresourcePayload.Format != DXGI_FORMAT_UNKNOWN)
		{
			nextSubresourcePayload.Format = currSubresourcePayload.Format;
			propagationHappened = true;
		}

		//Common promotion flag gets propagated according to common promotion rules
		if((nextSubresourcePayload.Flags & TextureFlagBarrierCommonPromoted) == 0)
		{
			if(currSubresourcePayload.State == D3D12_RESOURCE_STATE_COMMON && D3D12Utils::IsStatePromoteableTo(nextSubresourcePayload.State)) //Promotion from common
			{
				nextSubresourcePayload.Flags |= TextureFlagBarrierCommonPromoted;
				propagationHappened = true;
			}
			else if(subresourceNode.PassClass == nextSubresourceNode.PassClass) //Queue hasn't changed => ExecuteCommandLists wasn't called => No decay happened
			{
				if(currSubresourcePayload.State == nextSubresourcePayload.State) //Propagation of state promotion
				{
					nextSubresourcePayload.Flags |= TextureFlagBarrierCommonPromoted;
					propagationHappened = true;
				}
			}
			else
			{
				if((nextSubresourcePayload.Flags & TextureFlagBarrierCommonPromoted) != 0 && !D3D12Utils::IsStateWriteable(currSubresourcePayload.State)) //Read-only promoted state decays to COMMON after ECL call
				{
					if(D3D12Utils::IsStatePromoteableTo(nextSubresourcePayload.State)) //State promotes again
					{
						nextSubresourcePayload.Flags |= TextureFlagBarrierCommonPromoted;
						propagationHappened = true;
					}
				}
			}
		}

	} while (headNodeIndex != currNodeIndex);

	return propagationHappened;
}

bool D3D12::FrameGraphBuilder::PropagateSubresourcePayloadDataHorizontally(const PassMetadata& passMetadata)
{
	Span<uint32_t> subresourceSpan = passMetadata.SubresourceMetadataSpan;

	std::span<SubresourceMetadataPayload> subresourcePayloads = {mSubresourceMetadataPayloads.begin() + subresourceSpan.Begin, mSubresourceMetadataPayloads.begin() + subresourceSpan.End};
	return PropagateSubresourceInfos(passMetadata.Type, subresourcePayloads);
}

void D3D12::FrameGraphBuilder::CreateTextures()
{
	mD3d12GraphToBuild->mTextureHeap.reset();
	uint32_t backbufferResourceIndex = mSubresourceMetadataNodesFlat[mPresentPassMetadata.SubresourceMetadataSpan.Begin + (size_t)PresentPassSubresourceId::Backbuffer].ResourceMetadataIndex;

	std::vector<D3D12_RESOURCE_DESC1> textureDescs;

	//For TYPELESS formats, we lose the information for clear values. Fortunately, this is not a very common case
	//It's only needed for RTV formats, since depth-stencil format can be recovered from a typeless one
	std::unordered_map<uint32_t, DXGI_FORMAT> optimizedClearFormatsForTypeless;

	for(uint32_t resourceMetadataIndex = 0; resourceMetadataIndex < mResourceMetadatas.size(); resourceMetadataIndex++)
	{
		if(resourceMetadataIndex == backbufferResourceIndex)
		{
			continue; //Backbuffer is handled separately
		}

		ResourceMetadata& textureMetadata = mResourceMetadatas[resourceMetadataIndex];

		uint8_t formatChanges = 0;

		D3D12_RESOURCE_DESC1 resourceDesc;
		resourceDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resourceDesc.Width                           = GetConfig()->GetViewportWidth();
		resourceDesc.Height                          = GetConfig()->GetViewportHeight();
		resourceDesc.DepthOrArraySize                = 1;
		resourceDesc.MipLevels                       = 1;
		resourceDesc.Format                          = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count                = 1;
		resourceDesc.SampleDesc.Quality              = 0;
		resourceDesc.Layout                          = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags                           = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		resourceDesc.SamplerFeedbackMipRegion.Width  = 0;
		resourceDesc.SamplerFeedbackMipRegion.Height = 0;
		resourceDesc.SamplerFeedbackMipRegion.Depth  = 0;

		DXGI_FORMAT formatForClear = DXGI_FORMAT_UNKNOWN;

		uint32_t headNodeIndex    = textureMetadata.HeadNodeIndex;
		uint32_t currentNodeIndex = headNodeIndex;
		do
		{
			const SubresourceMetadataNode&    subresourceMetadata        = mSubresourceMetadataNodesFlat[currentNodeIndex];
			const SubresourceMetadataPayload& subresourceMetadataPayload = mSubresourceMetadataPayloads[currentNodeIndex];

			assert(subresourceMetadataPayload.Format != DXGI_FORMAT_UNKNOWN);
			if(resourceDesc.Format != subresourceMetadataPayload.Format)
			{
				assert(formatChanges < 255);
				formatChanges++;

				resourceDesc.Format = subresourceMetadataPayload.Format;
			}

			if(subresourceMetadataPayload.State & (D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_DEPTH_READ))
			{
				resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				formatForClear = subresourceMetadataPayload.Format;
			}

			if(subresourceMetadataPayload.State & (D3D12_RESOURCE_STATE_RENDER_TARGET))
			{
				resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				formatForClear = subresourceMetadataPayload.Format;
			}

			if(subresourceMetadataPayload.State & (D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
			{
				resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			if(subresourceMetadataPayload.State & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
			{
				resourceDesc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
			}

			currentNodeIndex = mSubresourceMetadataNodesFlat[currentNodeIndex].NextPassNodeIndex;

		} while(currentNodeIndex != headNodeIndex);


		uint32_t newTextureIndex = (uint32_t)textureDescs.size();
		textureMetadata.FirstFrameHandle = newTextureIndex;

		if(formatChanges > 1)
		{
			resourceDesc.Format = D3D12Utils::ConvertToTypeless(resourceDesc.Format);

			if(!(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) && (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
			{
				optimizedClearFormatsForTypeless[newTextureIndex] = formatForClear;
			}
		}

		//D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE is only allowed with D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		if((resourceDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) && !(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			resourceDesc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}

		for(uint32_t frame = 0; frame < textureMetadata.FrameCount; frame++)
		{
			textureDescs.push_back(resourceDesc);
		}
	}

	std::vector<UINT64> textureHeapOffsets;
	textureHeapOffsets.reserve(textureDescs.size());
	mMemoryAllocator->AllocateTextureMemory(mDevice, textureDescs, textureHeapOffsets, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, mD3d12GraphToBuild->mTextureHeap.put());

	for(const ResourceMetadata& textureMetadata: mResourceMetadatas)
	{
		uint32_t baseTextureIndex = textureMetadata.FirstFrameHandle;
		const D3D12_RESOURCE_DESC1& baseResourceDesc = textureDescs[baseTextureIndex];

		D3D12_CLEAR_VALUE  clearValue;
		D3D12_CLEAR_VALUE* clearValuePtr = nullptr;
		if(baseResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		{
			if(D3D12Utils::IsTypelessFormat(baseResourceDesc.Format))
			{
				clearValue.Format = optimizedClearFormatsForTypeless[baseTextureIndex];
			}
			else
			{
				clearValue.Format = baseResourceDesc.Format;
			}

			clearValue.Color[0] = 0.0f;
			clearValue.Color[1] = 0.0f;
			clearValue.Color[2] = 0.0f;
			clearValue.Color[3] = 1.0f;

			clearValuePtr = &clearValue;
		}
		else if(baseResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		{
			clearValue.Format               = D3D12Utils::ConvertToDepthStencil(baseResourceDesc.Format);
			clearValue.DepthStencil.Depth   = 1.0f;
			clearValue.DepthStencil.Stencil = 0;

			clearValuePtr = &clearValue;
		}

		uint32_t lastMetadataIndex = mSubresourceMetadataNodesFlat[textureMetadata.HeadNodeIndex].PrevPassNodeIndex;
		D3D12_RESOURCE_STATES lastState = mSubresourceMetadataPayloads[lastMetadataIndex].State;

		for(uint32_t frame = 0; frame < textureMetadata.FrameCount; frame++)
		{
			uint32_t imageIndex = baseTextureIndex + frame;
			const D3D12_RESOURCE_DESC1& resourceDesc = textureDescs[imageIndex];

			mD3d12GraphToBuild->mOwnedResources.emplace_back();
			THROW_IF_FAILED(mDevice->CreatePlacedResource1(mD3d12GraphToBuild->mTextureHeap.get(), textureHeapOffsets[imageIndex], &resourceDesc, lastState, clearValuePtr, IID_PPV_ARGS(mD3d12GraphToBuild->mOwnedResources.back().put())));

			mD3d12GraphToBuild->mTextures[imageIndex] = mD3d12GraphToBuild->mOwnedResources.back().get();

#if defined(DEBUG) || defined(_DEBUG)
			if(textureMetadata.FrameCount == 1)
			{
				D3D12Utils::SetDebugObjectName(mD3d12GraphToBuild->mTextures[imageIndex], textureMetadata.Name);
			}
			else
			{
				D3D12Utils::SetDebugObjectName(mD3d12GraphToBuild->mTextures[imageIndex], textureMetadata.Name + std::to_string(frame));
			}
#endif
		}
	}

	//Process the backbuffer
	ResourceMetadata& backbufferMetadata = mResourceMetadatas[backbufferResourceIndex];

	uint32_t backbufferImageIndex = (uint32_t)mD3d12GraphToBuild->mTextures.size();
	backbufferMetadata.FirstFrameHandle = backbufferImageIndex;
	for(uint32_t frame = 0; frame < backbufferMetadata.FrameCount; frame++)
	{
		ID3D12Resource* swapchainImage = mSwapChain->GetSwapchainImage(frame);

		wil::com_ptr_nothrow<ID3D12Resource2> swapchainImage2;
		THROW_IF_FAILED(swapchainImage->QueryInterface(IID_PPV_ARGS(swapchainImage2.put())));

		mD3d12GraphToBuild->mTextures.push_back(swapchainImage2.get());

#if defined(DEBUG) || defined(_DEBUG)
		D3D12Utils::SetDebugObjectName(swapchainImage2.get(), std::string(backbufferMetadata.Name) + std::to_string(frame));
#endif
	}
}

void D3D12::FrameGraphBuilder::CreateTextureViews()
{
	using ViewCreateInfo = std::pair<uint32_t, uint32_t>; //Texture index + subresource info index

	const uint32_t srvStateMask = (uint32_t)(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	const uint32_t uavStateMask = (uint32_t)(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const uint32_t rtvStateMask = (uint32_t)(D3D12_RESOURCE_STATE_RENDER_TARGET);
	const uint32_t dsvStateMask = (uint32_t)(D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE);

	std::vector<ViewCreateInfo> srvUavCreateInfos;
	std::vector<ViewCreateInfo> rtvCreateInfos;
	std::vector<ViewCreateInfo> dsvCreateInfos;
	for(const ResourceMetadata& resourceMetadata: mResourceMetadatas)
	{
		std::unordered_map<DXGI_FORMAT, uint32_t> srvIndicesForFormats; //To only create different image views if the formats differ
		std::unordered_map<DXGI_FORMAT, uint32_t> uavIndicesForFormats; //To only create different image views if the formats differ
		std::unordered_map<DXGI_FORMAT, uint32_t> rtvIndicesForFormats; //To only create different image views if the formats differ
		std::unordered_map<DXGI_FORMAT, uint32_t> dsvIndicesForFormats; //To only create different image views if the formats differ

		uint32_t headNodeIndex = resourceMetadata.HeadNodeIndex;
		uint32_t currNodeIndex = headNodeIndex;
		do
		{
			SubresourceMetadataNode&          subresourceMetadata        = mSubresourceMetadataNodesFlat[currNodeIndex];
			const SubresourceMetadataPayload& subresourceMetadataPayload = mSubresourceMetadataPayloads[currNodeIndex];

			if(subresourceMetadataPayload.Format == DXGI_FORMAT_UNKNOWN)
			{
				//The resource doesn't require a view in the pass
				continue;
			}

			bool isSrvResource = (subresourceMetadataPayload.State & srvStateMask);
			bool isUavResource = (subresourceMetadataPayload.State & uavStateMask);
			bool isRtvResource = (subresourceMetadataPayload.State & rtvStateMask);
			bool isDsvResource = (subresourceMetadataPayload.State & dsvStateMask);

			int stateCount = (int)isSrvResource + (int)isUavResource + (int)isRtvResource + (int)isDsvResource;
			assert(stateCount <= 1); //Only a single state is supported, otherwise FirstImageViewIndex is undefined

			if(stateCount == 0)
			{
				//The resource doesn't require a view in the pass
				continue;
			}

			if(isSrvResource)
			{
				auto srvIndexIt = srvIndicesForFormats.find(subresourceMetadataPayload.Format);
				if(srvIndexIt != srvIndicesForFormats.end())
				{
					subresourceMetadata.FirstFrameViewHandle = srvIndexIt->second;
				}
				else
				{
					uint32_t newSrvUavDescriptorIndex = (uint32_t)srvUavCreateInfos.size();
					subresourceMetadata.FirstFrameViewHandle = newSrvUavDescriptorIndex;

					for(uint32_t frameIndex = 0; frameIndex < resourceMetadata.FrameCount; frameIndex++)
					{
						srvUavCreateInfos.push_back(std::make_pair(resourceMetadata.FirstFrameHandle + frameIndex, currNodeIndex));
					}

					srvIndicesForFormats[subresourceMetadataPayload.Format] = newSrvUavDescriptorIndex;
				}				
			}
			else if(isUavResource)
			{
				auto uavIndexIt = uavIndicesForFormats.find(subresourceMetadataPayload.Format);
				if(uavIndexIt != uavIndicesForFormats.end())
				{
					subresourceMetadata.FirstFrameViewHandle = uavIndexIt->second;
				}
				else
				{
					uint32_t newSrvUavDescriptorIndex = (uint32_t)srvUavCreateInfos.size();
					subresourceMetadata.FirstFrameViewHandle = newSrvUavDescriptorIndex;

					for(uint32_t frameIndex = 0; frameIndex < resourceMetadata.FrameCount; frameIndex++)
					{
						srvUavCreateInfos.push_back(std::make_pair(resourceMetadata.FirstFrameHandle + frameIndex, currNodeIndex));
					}

					uavIndicesForFormats[subresourceMetadataPayload.Format] = newSrvUavDescriptorIndex;
				}
			}
			else if(isRtvResource)
			{
				auto rtvIndexIt = rtvIndicesForFormats.find(subresourceMetadataPayload.Format);
				if(rtvIndexIt != rtvIndicesForFormats.end())
				{
					subresourceMetadata.FirstFrameViewHandle = rtvIndexIt->second;
				}
				else
				{
					uint32_t newRtvDescriptorIndex = (uint32_t)rtvCreateInfos.size();
					subresourceMetadata.FirstFrameViewHandle = newRtvDescriptorIndex;

					for(uint32_t frameIndex = 0; frameIndex < resourceMetadata.FrameCount; frameIndex++)
					{
						rtvCreateInfos.push_back(std::make_pair(resourceMetadata.FirstFrameHandle + frameIndex, currNodeIndex));
					}

					rtvIndicesForFormats[subresourceMetadataPayload.Format] = newRtvDescriptorIndex;
				}
			}
			else if(isDsvResource)
			{
				auto dsvIndexIt = dsvIndicesForFormats.find(subresourceMetadataPayload.Format);
				if(dsvIndexIt != dsvIndicesForFormats.end())
				{
					subresourceMetadata.FirstFrameViewHandle = dsvIndexIt->second;
				}
				else
				{
					uint32_t newDsvDescriptorIndex = (uint32_t)dsvCreateInfos.size();
					subresourceMetadata.FirstFrameViewHandle = newDsvDescriptorIndex;

					for(uint32_t frameIndex = 0; frameIndex < resourceMetadata.FrameCount; frameIndex++)
					{
						dsvCreateInfos.push_back(std::make_pair(resourceMetadata.FirstFrameHandle + frameIndex, currNodeIndex));
					}

					dsvIndicesForFormats[subresourceMetadataPayload.Format] = newDsvDescriptorIndex;
				}
			}

			currNodeIndex = subresourceMetadata.NextPassNodeIndex;

		} while(currNodeIndex != headNodeIndex);
	}


	//The descriptor data will be stored in non-CPU-visible heap for now and copied to the main heap after.
	mD3d12GraphToBuild->mSrvUavDescriptorHeap.reset();
	mD3d12GraphToBuild->mRtvDescriptorHeap.reset();
	mD3d12GraphToBuild->mDsvDescriptorHeap.reset();

	//TODO: mGPU?
	D3D12_DESCRIPTOR_HEAP_DESC srvUavDescriptorHeapDesc;
	srvUavDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvUavDescriptorHeapDesc.NumDescriptors = (uint32_t)srvUavCreateInfos.size();
	srvUavDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	srvUavDescriptorHeapDesc.NodeMask       = 0;

	THROW_IF_FAILED(mDevice->CreateDescriptorHeap(&srvUavDescriptorHeapDesc, IID_PPV_ARGS(mD3d12GraphToBuild->mSrvUavDescriptorHeap.put())));

	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
	rtvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NumDescriptors = (uint32_t)rtvCreateInfos.size();
	rtvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDescriptorHeapDesc.NodeMask       = 0;

	THROW_IF_FAILED(mDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(mD3d12GraphToBuild->mRtvDescriptorHeap.put())));
	
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
	dsvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescriptorHeapDesc.NumDescriptors = (uint32_t)dsvCreateInfos.size();
	dsvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDescriptorHeapDesc.NodeMask       = 0;

	THROW_IF_FAILED(mDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(mD3d12GraphToBuild->mDsvDescriptorHeap.put())));


	UINT srvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	UINT rtvDescriptorSize    = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	UINT dsvDescriptorSize    = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	for(size_t srvUavIndex = 0; srvUavIndex < srvUavCreateInfos.size(); srvUavIndex++)
	{
		ViewCreateInfo createInfo           = srvUavCreateInfos[srvUavIndex];
		uint32_t       resourceIndex        = createInfo.first;
		uint32_t       subresourceInfoIndex = createInfo.second;

		ID3D12Resource* descriptorResource = mD3d12GraphToBuild->mTextures[resourceIndex];
		const SubresourceMetadataPayload& subresourceMetadataPayload = mSubresourceMetadataPayloads[subresourceInfoIndex];
		if(subresourceMetadataPayload.State & srvStateMask)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Format                        = subresourceMetadataPayload.Format;
			srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels           = 1;
			srvDesc.Texture2D.MostDetailedMip     = 0;
			srvDesc.Texture2D.PlaneSlice          = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

			D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = mD3d12GraphToBuild->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			srvHandle.ptr                        += srvUavIndex * srvUavDescriptorSize;
				
			mDevice->CreateShaderResourceView(descriptorResource, &srvDesc, srvHandle);
		}
		else if(subresourceMetadataPayload.State & uavStateMask)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			uavDesc.ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Format               = subresourceMetadataPayload.Format;
			uavDesc.Texture2D.MipSlice   = 0;
			uavDesc.Texture2D.PlaneSlice = 0;

			D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = mD3d12GraphToBuild->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			uavHandle.ptr                        += srvUavIndex * srvUavDescriptorSize;
				
			mDevice->CreateUnorderedAccessView(descriptorResource, nullptr, &uavDesc, uavHandle);
		}
	}

	for(size_t rtvIndex = 0; rtvIndex < rtvCreateInfos.size(); rtvIndex++)
	{
		ViewCreateInfo createInfo           = rtvCreateInfos[rtvIndex];
		uint32_t       resourceIndex        = createInfo.first;
		uint32_t       subresourceInfoIndex = createInfo.second;

		ID3D12Resource* descriptorResource = mD3d12GraphToBuild->mTextures[resourceIndex];
		const SubresourceMetadataPayload& subresourceMetadataPayload = mSubresourceMetadataPayloads[subresourceInfoIndex];
		if(subresourceMetadataPayload.State & rtvStateMask)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Format               = subresourceMetadataPayload.Format;
			rtvDesc.Texture2D.MipSlice   = 0;
			rtvDesc.Texture2D.PlaneSlice = 0;

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mD3d12GraphToBuild->mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			rtvHandle.ptr                        += rtvIndex * rtvDescriptorSize;
				
			mDevice->CreateRenderTargetView(descriptorResource, &rtvDesc, rtvHandle);
		}
	}

	for(size_t dsvIndex = 0; dsvIndex < dsvCreateInfos.size(); dsvIndex++)
	{
		ViewCreateInfo createInfo           = dsvCreateInfos[dsvIndex];
		uint32_t       resourceIndex        = createInfo.first;
		uint32_t       subresourceInfoIndex = createInfo.second;

		ID3D12Resource* descriptorResource = mD3d12GraphToBuild->mTextures[resourceIndex];
		const SubresourceMetadataPayload& subresourceMetadataPayload = mSubresourceMetadataPayloads[subresourceInfoIndex];
		if(subresourceMetadataPayload.State & rtvStateMask)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
			dsvDesc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Format             = subresourceMetadataPayload.Format;
			dsvDesc.Texture2D.MipSlice = 0;

			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mD3d12GraphToBuild->mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			dsvHandle.ptr                        += dsvIndex * dsvDescriptorSize;
				
			mDevice->CreateDepthStencilView(descriptorResource, &dsvDesc, dsvHandle);
		}
	}
}

void D3D12::FrameGraphBuilder::CreateObjectsForPass(uint32_t passMetadataIndex, uint32_t passSwapchainImageCount)
{
	uint32_t oldPassCount = (uint32_t)mD3d12GraphToBuild->mRenderPasses.size();

	const PassMetadata& passMetadata = mPassMetadatas[passMetadataIndex];
	for(uint32_t swapchainImageIndex = 0; swapchainImageIndex < passSwapchainImageCount; swapchainImageIndex++)
	{
		for(uint32_t passFrame = 0; passFrame < passMetadata.OwnPeriod; passFrame++)
		{
			uint32_t frame = passMetadata.OwnPeriod * swapchainImageIndex + passFrame;
			
			mD3d12GraphToBuild->mRenderPasses.emplace_back(MakeUniquePass(passMetadata.Type, this, passMetadataIndex, frame));
		}
	}

	ModernFrameGraph::RenderPassSpanInfo passSpanInfo;
	passSpanInfo.PassSpanBegin = oldPassCount;
	passSpanInfo.OwnFrames     = passMetadata.OwnPeriod;

	mD3d12GraphToBuild->mPassFrameSpans.push_back(passSpanInfo);
}

uint32_t D3D12::FrameGraphBuilder::AddBeforePassBarrier(uint32_t metadataIndex)
{
	/*
	*    1.  Same queue, state unchanged:                                 No barrier needed
	*    2.  Same queue, state changed PRESENT -> automatically promoted: No barrier needed
	*    3.  Same queue, state changed to PRESENT:                        No barrier needed, handled by the previous state's barrier
	*    4.  Same queue, state changed other cases:                       Need a barrier Old state -> New state
	*
	*    5.  Graphics -> Compute, state automatically promoted:               No barrier needed
	*    6.  Graphics -> Compute, state non-promoted, was promoted read-only: Need a barrier COMMON -> New state
	*    7.  Graphics -> Compute, state unchanged:                            No barrier needed
	*    8.  Graphics -> Compute, state changed other cases:                  No barrier needed, handled by the previous state's barrier
	* 
	*    9.  Compute -> Graphics, state automatically promoted:                       No barrier needed, state will be promoted again
	* 	 10. Compute -> Graphics, state non-promoted, was promoted read-only:         Need a barrier COMMON -> New state   
	*    11. Compute -> Graphics, state changed Compute/graphics -> Compute/Graphics: No barrier needed, handled by the previous state's barrier
	*    12. Compute -> Graphics, state changed Compute/Graphics -> Graphics:         Need a barrier Old state -> New state
	*    13. Compute -> Graphics, state unchanged:                                    No barrier needed
	*
	*    14. Graphics/Compute -> Copy, was promoted read-only: No barrier needed, state decays
	*    15. Graphics/Compute -> Copy, other cases:            No barrier needed, handled by previous state's barrier
	* 
	*    16. Copy -> Graphics/Compute, state automatically promoted: No barrier needed
	*    17. Copy -> Graphics/Compute, state non-promoted:           Need a barrier COMMON -> New state
	*/

	const SubresourceMetadataNode& currMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
	const SubresourceMetadataNode& prevMetadataNode = mSubresourceMetadataNodesFlat[currMetadataNode.PrevPassNodeIndex];

	const SubresourceInfo& prevPassInfo = mSubresourceInfos[currMetadataNode.PrevPassNodeIndex];
	const SubresourceInfo& currPassInfo = mSubresourceInfos[metadataIndex];

	bool barrierNeeded = false;
	D3D12_RESOURCE_STATES prevPassState = prevPassInfo.State;
	D3D12_RESOURCE_STATES currPassState = currPassInfo.State;

	if(PassClassToListType(prevMetadataNode.PassClass) == PassClassToListType(currMetadataNode.PassClass)) //Rules 1, 2, 3, 4
	{
		if(prevPassState == D3D12_RESOURCE_STATE_PRESENT) //Rule 2 or 4
		{
			barrierNeeded = !currPassInfo.BarrierPromotedFromCommon;
		}
		else //Rule 1, 3 or 4
		{
			barrierNeeded = (currPassState != D3D12_RESOURCE_STATE_PRESENT) && (prevPassState != currPassState);
		}
	}
	else if(PassClassToListType(prevMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_DIRECT && PassClassToListType(currMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_COMPUTE) //Rules 5, 6, 7, 8
	{
		if(prevPassInfo.BarrierPromotedFromCommon && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 6
		{
			prevPassState = D3D12_RESOURCE_STATE_COMMON; //Common state decay from promoted read-only
			barrierNeeded = !currPassInfo.BarrierPromotedFromCommon;
		}
		else //Rules 6, 7, 8
		{
			barrierNeeded = false;
		}
	}
	else if(PassClassToListType(prevMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_COMPUTE && PassClassToListType(currMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_DIRECT) //Rules 9, 10, 11, 12, 13
	{
		if(prevPassInfo.BarrierPromotedFromCommon && !D3D12Utils::IsStateWriteable(prevPassState)) //Rule 10
		{
			prevPassState = D3D12_RESOURCE_STATE_COMMON; //Common state decay from promoted read-only
			barrierNeeded = !currPassInfo.BarrierPromotedFromCommon;
		}
		else //Rules 9, 11, 12, 13
		{
			barrierNeeded = !currPassInfo.BarrierPromotedFromCommon && (prevPassState != currPassState) && !D3D12Utils::IsStateComputeFriendly(currPassState);
		}
	}
	else if(PassClassToListType(currMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 14, 15
	{
		barrierNeeded = false;
	}
	else if(PassClassToListType(prevMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 16, 17
	{
		barrierNeeded = !currPassInfo.BarrierPromotedFromCommon;
	}

	if(barrierNeeded)
	{
		D3D12_RESOURCE_BARRIER textureTransitionBarrier;
		textureTransitionBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		textureTransitionBarrier.Transition.pResource   = mD3d12GraphToBuild->mTextures[currMetadataNode.FirstFrameHandle];
		textureTransitionBarrier.Transition.Subresource = 0;
		textureTransitionBarrier.Transition.StateBefore = prevPassState;
		textureTransitionBarrier.Transition.StateAfter  = currPassState;
		textureTransitionBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		mD3d12GraphToBuild->mResourceBarriers.push_back(textureTransitionBarrier);

		return (uint32_t)(mD3d12GraphToBuild->mResourceBarriers.size() - 1);
	}

	return (uint32_t)(-1);
}

uint32_t D3D12::FrameGraphBuilder::AddAfterPassBarrier(uint32_t metadataIndex)
{
	/*
	*    1.  Same queue, state unchanged:                                           No barrier needed
	*    2.  Same queue, state changed, Promoted read-only -> PRESENT:              No barrier needed, state decays
	*    3.  Same queue, state changed, Promoted writeable/Non-promoted -> PRESENT: Need a barrier Old State -> PRESENT
	*    4.  Same queue, state changed, other cases:                                No barrier needed, will be handled by the next state's barrier
	*
	*    5.  Graphics -> Compute, state will be automatically promoted:               No barrier needed
	*    6.  Graphics -> Compute, state will not be promoted, was promoted read-only: No barrier needed, will be handled by the next state's barrier
	*    7.  Graphics -> Compute, state unchanged:                                    No barrier needed
	*    8.  Graphics -> Compute, state changed other cases:                          Need a barrier Old state -> New state
	* 
	*    9.  Compute -> Graphics, state will be automatically promoted:                 No barrier needed
	* 	 10. Compute -> Graphics, state will not be promoted, was promoted read-only:   No barrier needed, will be handled by the next state's barrier     
	*    11. Compute -> Graphics, state changed Promoted read-only -> PRESENT:          No barrier needed, state will decay
	*    12. Compute -> Graphics, state changed Compute/graphics   -> Compute/Graphics: Need a barrier Old state -> New state
	*    13. Compute -> Graphics, state changed Compute/Graphics   -> Graphics:         No barrier needed, will be handled by the next state's barrier
	*    14. Compute -> Graphics, state unchanged:                                      No barrier needed
	*   
	*    15. Graphics/Compute -> Copy, from Promoted read-only: No barrier needed
	*    16. Graphics/Compute -> Copy, other cases:             Need a barrier Old state -> COMMON
	* 
	*    17. Copy -> Graphics/Compute, state will be automatically promoted: No barrier needed
	*    18. Copy -> Graphics/Compute, state will not be promoted:           No barrier needed, will be handled by the next state's barrier
	*/

	const SubresourceMetadataNode& currMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
	const SubresourceMetadataNode& nextMetadataNode = mSubresourceMetadataNodesFlat[currMetadataNode.NextPassNodeIndex];

	const SubresourceInfo& currPassInfo = mSubresourceInfos[metadataIndex];
	const SubresourceInfo& nextPassInfo = mSubresourceInfos[currMetadataNode.NextPassNodeIndex];

	bool barrierNeeded = false;
	D3D12_RESOURCE_STATES currPassState = currPassInfo.State;
	D3D12_RESOURCE_STATES nextPassState = nextPassInfo.State;

	if(PassClassToListType(currMetadataNode.PassClass) == PassClassToListType(nextMetadataNode.PassClass)) //Rules 1, 2, 3, 4
	{
		if(nextPassState == D3D12_RESOURCE_STATE_PRESENT) //Rules 2, 3
		{
			barrierNeeded = (!currPassInfo.BarrierPromotedFromCommon || D3D12Utils::IsStateWriteable(currPassState));
		}
		else //Rules 1, 4
		{
			barrierNeeded = false;
		}
	}
	else if(PassClassToListType(currMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_DIRECT && PassClassToListType(nextMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_COMPUTE) //Rules 5, 6, 7, 8
	{
		if(currPassInfo.BarrierPromotedFromCommon && !D3D12Utils::IsStateWriteable(currPassState)) //Rule 6
		{
			barrierNeeded = false;
		}
		else //Rules 5, 7, 8
		{
			barrierNeeded = !nextPassInfo.BarrierPromotedFromCommon && (currPassState != nextPassState);
		}
	}
	else if(PassClassToListType(currMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_COMPUTE && PassClassToListType(nextMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_DIRECT) //Rules 9, 10, 11, 12, 13, 14
	{
		if(currPassInfo.BarrierPromotedFromCommon && !D3D12Utils::IsStateWriteable(currPassState)) //Rule 10, 11
		{
			barrierNeeded = false;
		}
		else //Rules 9, 12, 13, 14
		{
			barrierNeeded = !nextPassInfo.BarrierPromotedFromCommon && (currPassState != nextPassState) && D3D12Utils::IsStateComputeFriendly(nextPassState);
		}
	}
	else if(PassClassToListType(nextMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 15, 16
	{
		barrierNeeded = !currPassInfo.BarrierPromotedFromCommon || D3D12Utils::IsStateWriteable(currPassState);
	}
	else if(PassClassToListType(currMetadataNode.PassClass) == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 17, 18
	{
		barrierNeeded = false;
	}

	if(barrierNeeded)
	{
		D3D12_RESOURCE_BARRIER textureTransitionBarrier;
		textureTransitionBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		textureTransitionBarrier.Transition.pResource   = mD3d12GraphToBuild->mTextures[currMetadataNode.FirstFrameHandle];
		textureTransitionBarrier.Transition.Subresource = 0;
		textureTransitionBarrier.Transition.StateBefore = currPassState;
		textureTransitionBarrier.Transition.StateAfter  = nextPassState;
		textureTransitionBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		mD3d12GraphToBuild->mResourceBarriers.push_back(textureTransitionBarrier);

		return (uint32_t)(mD3d12GraphToBuild->mResourceBarriers.size() - 1);
	}

	return (uint32_t)(-1);
}

void D3D12::FrameGraphBuilder::InitializeTraverseData() const
{
	mD3d12GraphToBuild->mFrameRecordedGraphicsCommandLists.resize(mD3d12GraphToBuild->mGraphicsPassSpans.size());

	//Add a plug present pass
	mD3d12GraphToBuild->mPassFrameSpans.push_back(ModernFrameGraph::RenderPassSpanInfo
	{
		.PassSpanBegin = (uint32_t)mD3d12GraphToBuild->mRenderPasses.size(),
		.OwnFrames     = 0
	});
}

uint32_t D3D12::FrameGraphBuilder::GetSwapchainImageCount() const
{
	return mSwapChain->SwapchainImageCount;
}

D3D12_COMMAND_LIST_TYPE D3D12::FrameGraphBuilder::PassClassToListType(RenderPassClass passClass)
{
	switch(passClass)
	{
	case RenderPassClass::Graphics:
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	case RenderPassClass::Compute:
		return D3D12_COMMAND_LIST_TYPE_COMPUTE;
	case RenderPassClass::Transfer:
		return D3D12_COMMAND_LIST_TYPE_COPY;
	case RenderPassClass::Present:
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	default:
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	}
}