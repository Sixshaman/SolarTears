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

#include "Passes/D3D12GBufferPass.hpp"
#include "Passes/D3D12CopyImagePass.hpp"

D3D12::FrameGraphBuilder::FrameGraphBuilder(FrameGraph* graphToBuild, FrameGraphDescription&& frameGraphDescription, const SwapChain* swapChain): ModernFrameGraphBuilder(graphToBuild, std::move(frameGraphDescription)), mD3d12GraphToBuild(graphToBuild), mSwapChain(swapChain)
{
	mSrvUavCbvDescriptorCount = 0;
	mRtvDescriptorCount       = 0;
	mDsvDescriptorCount       = 0;

	InitPassTable();
}

D3D12::FrameGraphBuilder::~FrameGraphBuilder()
{
}

void D3D12::FrameGraphBuilder::SetPassSubresourceFormat(const std::string_view passName, const std::string_view subresourceId, DXGI_FORMAT format)
{
	FrameGraphDescription::RenderPassName passNameStr(passName);
	FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].Format = format;
}

void D3D12::FrameGraphBuilder::SetPassSubresourceState(const std::string_view passName, const std::string_view subresourceId, D3D12_RESOURCE_STATES state)
{
	FrameGraphDescription::RenderPassName passNameStr(passName);
	FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].State = state;
}

const D3D12::ShaderManager* D3D12::FrameGraphBuilder::GetShaderManager() const
{
	return mShaderManager;
}

ID3D12Resource2* D3D12::FrameGraphBuilder::GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	const FrameGraphDescription::RenderPassName passNameStr(passName);
	const FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadataNode.FirstFrameHandle == (uint32_t)(-1))
	{
		return nullptr;
	}
	else
	{
		if(metadataNode.FirstFrameHandle == GetBackbufferImageSpan().Begin)
		{
			uint32_t passPeriod = mRenderPassOwnPeriods.at(std::string(passName));
			return mD3d12GraphToBuild->mTextures[metadataNode.FirstFrameHandle + frame / passPeriod];
		}
		else
		{
			
			return mD3d12GraphToBuild->mTextures[metadataNode.FirstFrameHandle + frame % metadataNode.FrameCount];
		}
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceSrvUav(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	const FrameGraphDescription::RenderPassName passNameStr(passName);
	const FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	const SubresourceInfo& subresourceInfo = mSubresourceInfos[metadataNode.SubresourceInfoIndex];

	const D3D12_RESOURCE_STATES resourceState = (D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	assert((subresourceInfo.State & resourceState) && (metadataNode.FirstFrameViewHandle != (uint32_t)(-1)));

	D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphSrvHeapStart();
	if(metadataNode.FirstFrameHandle == GetBackbufferImageSpan().Begin)
	{
		uint32_t passPeriod = mRenderPassOwnPeriods.at(std::string(passName));
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame / passPeriod) * mSrvUavCbvDescriptorSize;
	}
	else
	{
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame % metadataNode.FrameCount) * mSrvUavCbvDescriptorSize;
	}

	return descriptorHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceRtv(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	const FrameGraphDescription::RenderPassName passNameStr(passName);
	const FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	const SubresourceInfo& subresourceInfo = mSubresourceInfos[metadataNode.SubresourceInfoIndex];

	const D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	assert((subresourceInfo.State & resourceState) && (metadataNode.FirstFrameViewHandle != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphRtvHeapStart();
	if(metadataNode.FirstFrameHandle == GetBackbufferImageSpan().Begin)
	{
		uint32_t passPeriod = mRenderPassOwnPeriods.at(std::string(passName));
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame / passPeriod) * mRtvDescriptorSize;
	}
	else
	{
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame % metadataNode.FrameCount) * mRtvDescriptorSize;
	}

	return descriptorHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceDsv(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	const FrameGraphDescription::RenderPassName passNameStr(passName);
	const FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	const SubresourceInfo& subresourceInfo = mSubresourceInfos[metadataNode.SubresourceInfoIndex];

	const D3D12_RESOURCE_STATES resourceState = (D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE);
	assert((subresourceInfo.State & resourceState) && (metadataNode.FirstFrameViewHandle != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphDsvHeapStart();
	if(metadataNode.FirstFrameHandle == GetBackbufferImageSpan().Begin)
	{
		uint32_t passPeriod = mRenderPassOwnPeriods.at(std::string(passName));
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame / passPeriod) * mDsvDescriptorSize;
	}
	else
	{
		descriptorHandle.ptr += (metadataNode.FirstFrameViewHandle + frame % metadataNode.FrameCount) * mDsvDescriptorSize;
	}

	return descriptorHandle;
}

DXGI_FORMAT D3D12::FrameGraphBuilder::GetRegisteredSubresourceFormat(const std::string_view passName, const std::string_view subresourceId) const
{
	const FrameGraphDescription::RenderPassName passNameStr(passName);
	const FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Format;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetRegisteredSubresourceState(const std::string_view passName, const std::string_view subresourceId) const
{
	const FrameGraphDescription::RenderPassName passNameStr(passName);
	const FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].State;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetPreviousPassSubresourceState(const std::string_view passName, const std::string_view subresourceId) const
{
	const FrameGraphDescription::RenderPassName passNameStr(passName);
	const FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).PrevPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].State;
}

D3D12_RESOURCE_STATES D3D12::FrameGraphBuilder::GetNextPassSubresourceState(const std::string_view passName, const std::string_view subresourceId) const
{
	const FrameGraphDescription::RenderPassName passNameStr(passName);
	const FrameGraphDescription::SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).NextPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].State;
}

void D3D12::FrameGraphBuilder::Build(ID3D12Device8* device, const ShaderManager* shaderManager, const MemoryManager* memoryManager)
{
	mDevice          = device;
	mShaderManager   = shaderManager;
	mMemoryAllocator = memoryManager;

	mSrvUavCbvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mRtvDescriptorSize       = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize       = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	ModernFrameGraphBuilder::Build();
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
 
void D3D12::FrameGraphBuilder::CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos, uint32_t totalTextureCount) const
{
	mD3d12GraphToBuild->mTextures.resize(totalTextureCount);
	mD3d12GraphToBuild->mTextureHeap.reset();

	std::vector<D3D12_RESOURCE_DESC1> textureDescs;

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

			currentNode = currentNode->NextPassNode;

		} while(currentNode != headNode);

		//D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE is only allowed with D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		if((resourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) && !(resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			resourceFlags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}

		if(D3D12Utils::IsTypelessFormat(format) && !(resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) && (resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
		{
			optimizedClearFormats[headNode->FirstFrameHandle] = formatForClear;
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
		resourceDesc.SampleDesc.Quality              = 0;
		resourceDesc.Layout                          = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags                           = resourceFlags;
		resourceDesc.SamplerFeedbackMipRegion.Width  = 0;
		resourceDesc.SamplerFeedbackMipRegion.Height = 0;
		resourceDesc.SamplerFeedbackMipRegion.Depth  = 0;

		for(uint32_t frame = 0; frame < textureCreateInfo.MetadataHead->FrameCount; frame++)
		{
			textureDescs.push_back(resourceDesc);
		}
	}

	std::vector<UINT64> textureHeapOffsets;
	textureHeapOffsets.reserve(textureDescs.size());
	mMemoryAllocator->AllocateTextureMemory(mDevice, textureDescs, textureHeapOffsets, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, mD3d12GraphToBuild->mTextureHeap.put());

	for(size_t createInfoIndex = 0; createInfoIndex < textureCreateInfos.size(); createInfoIndex++)
	{
		const TextureResourceCreateInfo& textureCreateInfo = textureCreateInfos[createInfoIndex];
		const D3D12_RESOURCE_DESC1&      baseResourceDesc  = textureDescs[textureCreateInfo.MetadataHead->FirstFrameHandle];

		D3D12_CLEAR_VALUE  clearValue;
		D3D12_CLEAR_VALUE* clearValuePtr = nullptr;
		if(baseResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		{
			if(D3D12Utils::IsTypelessFormat(baseResourceDesc.Format))
			{
				clearValue.Format = optimizedClearFormats[textureCreateInfo.MetadataHead->FirstFrameHandle];
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

		uint32_t lastSubresourceIndex = textureCreateInfo.MetadataHead->PrevPassNode->SubresourceInfoIndex;
		D3D12_RESOURCE_STATES lastState = mSubresourceInfos[lastSubresourceIndex].State;

		for(uint32_t frame = 0; frame < textureCreateInfo.MetadataHead->FrameCount; frame++)
		{
			uint32_t imageIndex = textureCreateInfo.MetadataHead->FirstFrameHandle + frame;
			const D3D12_RESOURCE_DESC1& resourceDesc = textureDescs[imageIndex];

			mD3d12GraphToBuild->mOwnedResources.emplace_back();
			THROW_IF_FAILED(mDevice->CreatePlacedResource1(mD3d12GraphToBuild->mTextureHeap.get(), textureHeapOffsets[imageIndex], &resourceDesc, lastState, clearValuePtr, IID_PPV_ARGS(mD3d12GraphToBuild->mOwnedResources.back().put())));

			mD3d12GraphToBuild->mTextures[imageIndex] = mD3d12GraphToBuild->mOwnedResources.back().get();

#if defined(DEBUG) || defined(_DEBUG)
			if(textureCreateInfo.MetadataHead->FrameCount == 1)
			{
				D3D12Utils::SetDebugObjectName(mD3d12GraphToBuild->mTextures[imageIndex], textureCreateInfo.Name);
			}
			else
			{
				D3D12Utils::SetDebugObjectName(mD3d12GraphToBuild->mTextures[imageIndex], std::string(textureCreateInfo.Name) + std::to_string(frame));
			}
#endif
		}
	}

	for(const TextureResourceCreateInfo& backbufferCreateInfo: backbufferCreateInfos)
	{
		for(uint32_t frame = 0; frame < backbufferCreateInfo.MetadataHead->FrameCount; frame++)
		{
			uint32_t imageIndex = backbufferCreateInfo.MetadataHead->FirstFrameHandle + frame;

			ID3D12Resource* swapchainImage = mSwapChain->GetSwapchainImage(frame);

			wil::com_ptr_nothrow<ID3D12Resource2> swapchainImage2;
			THROW_IF_FAILED(swapchainImage->QueryInterface(IID_PPV_ARGS(swapchainImage2.put())));

			mD3d12GraphToBuild->mTextures[imageIndex] = swapchainImage2.get();

#if defined(DEBUG) || defined(_DEBUG)
			D3D12Utils::SetDebugObjectName(mD3d12GraphToBuild->mTextures[imageIndex], std::string(backbufferCreateInfo.Name) + std::to_string(frame));
#endif
		}
	}
}

uint32_t D3D12::FrameGraphBuilder::AddSubresourceMetadata()
{
	SubresourceInfo subresourceInfo;
	subresourceInfo.Format                    = DXGI_FORMAT_UNKNOWN;
	subresourceInfo.State                     = D3D12_RESOURCE_STATE_COMMON;
	subresourceInfo.BarrierPromotedFromCommon = false;

	mSubresourceInfos.push_back(subresourceInfo);
	return (uint32_t)(mSubresourceInfos.size() - 1);
}

uint32_t D3D12::FrameGraphBuilder::AddPresentSubresourceMetadata()
{
	SubresourceInfo subresourceInfo;
	subresourceInfo.Format                    = mSwapChain->GetBackbufferFormat();
	subresourceInfo.State                     = D3D12_RESOURCE_STATE_PRESENT;
	subresourceInfo.BarrierPromotedFromCommon = false;

	mSubresourceInfos.push_back(subresourceInfo);
	return (uint32_t)(mSubresourceInfos.size() - 1);
}

void D3D12::FrameGraphBuilder::RegisterPassSubresources(RenderPassType passType, const FrameGraphDescription::RenderPassName& passName)
{
	auto passRegisterFunc = mPassAddFuncTable.at(passType);
	passRegisterFunc(this, passName);
}

void D3D12::FrameGraphBuilder::CreatePassObject(const FrameGraphDescription::RenderPassName& passName, RenderPassType passType, uint32_t frame)
{
	auto passCreateFunc = mPassCreateFuncTable.at(passType);
	mD3d12GraphToBuild->mRenderPasses.push_back(passCreateFunc(mDevice, this, passName, frame));
}

uint32_t D3D12::FrameGraphBuilder::NextPassSpanId()
{
	return (uint32_t)mD3d12GraphToBuild->mRenderPasses.size();
}

bool D3D12::FrameGraphBuilder::ValidateSubresourceViewParameters(SubresourceMetadataNode* currNode, SubresourceMetadataNode* prevNode)
{
	SubresourceInfo& prevPassSubresourceInfo = mSubresourceInfos[prevNode->SubresourceInfoIndex];
	SubresourceInfo& thisPassSubresourceInfo = mSubresourceInfos[currNode->SubresourceInfoIndex];

	bool dataPropagated = false;

	//Validate format
	if(thisPassSubresourceInfo.Format == DXGI_FORMAT_UNKNOWN && prevPassSubresourceInfo.Format != DXGI_FORMAT_UNKNOWN)
	{
		thisPassSubresourceInfo.Format = prevPassSubresourceInfo.Format;

		dataPropagated = true;
	}

	//Validate common promotion flag
	if(!thisPassSubresourceInfo.BarrierPromotedFromCommon)
	{
		if (prevPassSubresourceInfo.State == D3D12_RESOURCE_STATE_COMMON && D3D12Utils::IsStatePromoteableTo(thisPassSubresourceInfo.State)) //Promotion from common
		{
			thisPassSubresourceInfo.BarrierPromotedFromCommon = true;

			dataPropagated = true;
		}
		else if(prevNode->PassClass == currNode->PassClass) //Queue hasn't changed => ExecuteCommandLists wasn't called => No decay happened
		{
			if (prevPassSubresourceInfo.State == thisPassSubresourceInfo.State) //Propagation of state promotion
			{
				thisPassSubresourceInfo.BarrierPromotedFromCommon = true;

				dataPropagated = true;
			}
		}
		else
		{
			if (prevPassSubresourceInfo.BarrierPromotedFromCommon && !D3D12Utils::IsStateWriteable(prevPassSubresourceInfo.State)) //Read-only promoted state decays to COMMON after ECL call
			{
				if (D3D12Utils::IsStatePromoteableTo(thisPassSubresourceInfo.State)) //State promotes again
				{
					thisPassSubresourceInfo.BarrierPromotedFromCommon = true;

					dataPropagated = true;
				}
			}
		}
	}

	const uint32_t srvStateMask = (uint32_t)(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	const uint32_t uavStateMask = (uint32_t)(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const uint32_t rtvStateMask = (uint32_t)(D3D12_RESOURCE_STATE_RENDER_TARGET);
	const uint32_t dsvStateMask = (uint32_t)(D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE);

	const uint32_t stateKey  = (uint32_t)thisPassSubresourceInfo.State & (srvStateMask | uavStateMask | rtvStateMask | dsvStateMask); //Other states are not needed for separate views (i.e. descriptors)
	const uint32_t formatKey = (uint32_t)thisPassSubresourceInfo.Format * (stateKey != 0); //Creating the view is only needed if the state is descriptorable

	currNode->ViewSortKey = ((uint64_t)stateKey << 32) | formatKey;

	return dataPropagated; //Only the format should be propagated. But if common promotion flag has changed, the propagation has to start again
}

void D3D12::FrameGraphBuilder::AllocateImageViews(const std::vector<uint64_t>& sortKeys, uint32_t frameCount, std::vector<uint32_t>& outViewIds)
{
	outViewIds.reserve(sortKeys.size());
	for(uint64_t sortKey: sortKeys)
	{
		uint32_t imageViewId = (uint32_t)(-1);
		if(sortKey != 0)
		{
			const uint32_t srvUavStateMask = (uint32_t)(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			const uint32_t rtvStateMask    = (uint32_t)(D3D12_RESOURCE_STATE_RENDER_TARGET);
			const uint32_t dsvStateMask    = (uint32_t)(D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE);

			D3D12_RESOURCE_STATES viewState = (D3D12_RESOURCE_STATES)((sortKey >> 32) & 0x00000000ffffffffull);
			if(viewState & srvUavStateMask)
			{
				imageViewId = mSrvUavCbvDescriptorCount;
				mSrvUavCbvDescriptorCount += frameCount;
			}
			else if(viewState & rtvStateMask)
			{
				imageViewId = mRtvDescriptorCount;
				mRtvDescriptorCount += frameCount;
			}
			else if(viewState & dsvStateMask)
			{
				imageViewId = mDsvDescriptorCount;
				mDsvDescriptorCount += frameCount;
			}
		}

		outViewIds.push_back(imageViewId);
	}
}

void D3D12::FrameGraphBuilder::CreateTextureViews(const std::vector<TextureSubresourceCreateInfo>& textureViewCreateInfos) const
{
	//The descriptor data will be stored in non-CPU-visible heap for now and copied to the main heap after.
	mD3d12GraphToBuild->mSrvUavDescriptorHeap.reset();
	mD3d12GraphToBuild->mRtvDescriptorHeap.reset();
	mD3d12GraphToBuild->mDsvDescriptorHeap.reset();

	//TODO: mGPU?
	D3D12_DESCRIPTOR_HEAP_DESC srvUavDescriptorHeapDesc;
	srvUavDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvUavDescriptorHeapDesc.NumDescriptors = mSrvUavCbvDescriptorCount;
	srvUavDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	srvUavDescriptorHeapDesc.NodeMask       = 0;

	THROW_IF_FAILED(mDevice->CreateDescriptorHeap(&srvUavDescriptorHeapDesc, IID_PPV_ARGS(mD3d12GraphToBuild->mSrvUavDescriptorHeap.put())));

	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
	rtvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NumDescriptors = mRtvDescriptorCount;
	rtvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDescriptorHeapDesc.NodeMask       = 0;

	THROW_IF_FAILED(mDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(mD3d12GraphToBuild->mRtvDescriptorHeap.put())));
	
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
	dsvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescriptorHeapDesc.NumDescriptors = mDsvDescriptorCount;
	dsvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDescriptorHeapDesc.NodeMask       = 0;

	THROW_IF_FAILED(mDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(mD3d12GraphToBuild->mDsvDescriptorHeap.put())));

	for(const TextureSubresourceCreateInfo& textureViewCreateInfo: textureViewCreateInfos)
	{
		const SubresourceInfo& subresourceInfo = mSubresourceInfos[textureViewCreateInfo.SubresourceInfoIndex];
		ID3D12Resource* descriptorResource = mD3d12GraphToBuild->mTextures[textureViewCreateInfo.ImageIndex];

		if(subresourceInfo.State & (D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Format                        = subresourceInfo.Format;
			srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels           = 1;
			srvDesc.Texture2D.MostDetailedMip     = 0;
			srvDesc.Texture2D.PlaneSlice          = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

			D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = mD3d12GraphToBuild->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			srvHandle.ptr                        += textureViewCreateInfo.ImageViewIndex * mSrvUavCbvDescriptorSize;
				
			mDevice->CreateShaderResourceView(descriptorResource, &srvDesc, srvHandle);
		}
		else if(subresourceInfo.State & (D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			uavDesc.ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Format               = subresourceInfo.Format;
			uavDesc.Texture2D.MipSlice   = 0;
			uavDesc.Texture2D.PlaneSlice = 0;

			D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = mD3d12GraphToBuild->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			uavHandle.ptr                        += textureViewCreateInfo.ImageViewIndex * mSrvUavCbvDescriptorSize;
				
			mDevice->CreateUnorderedAccessView(descriptorResource, nullptr, &uavDesc, uavHandle);
		}
		else if(subresourceInfo.State & (D3D12_RESOURCE_STATE_RENDER_TARGET))
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Format               = subresourceInfo.Format;
			rtvDesc.Texture2D.MipSlice   = 0;
			rtvDesc.Texture2D.PlaneSlice = 0;

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mD3d12GraphToBuild->mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			rtvHandle.ptr                        += textureViewCreateInfo.ImageViewIndex * mRtvDescriptorSize;
				
			mDevice->CreateRenderTargetView(descriptorResource, &rtvDesc, rtvHandle);
		}
		else if(subresourceInfo.State & (D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE))
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
			dsvDesc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Format             = subresourceInfo.Format;
			dsvDesc.Texture2D.MipSlice = 0;

			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mD3d12GraphToBuild->mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			dsvHandle.ptr                        += textureViewCreateInfo.ImageViewIndex * mDsvDescriptorSize;
				
			mDevice->CreateDepthStencilView(descriptorResource, &dsvDesc, dsvHandle);
		}
	}
}

uint32_t D3D12::FrameGraphBuilder::AddBeforePassBarrier(uint32_t imageIndex, RenderPassClass prevPassClass, uint32_t prevPassSubresourceInfoIndex, RenderPassClass currPassClass, uint32_t currPassSubresourceInfoIndex)
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

	const SubresourceInfo& prevPassInfo = mSubresourceInfos[prevPassSubresourceInfoIndex];
	const SubresourceInfo& currPassInfo = mSubresourceInfos[currPassSubresourceInfoIndex];

	bool barrierNeeded = false;
	D3D12_RESOURCE_STATES prevPassState = prevPassInfo.State;
	D3D12_RESOURCE_STATES currPassState = currPassInfo.State;

	if(PassClassToListType(prevPassClass) == PassClassToListType(currPassClass)) //Rules 1, 2, 3, 4
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
	else if(PassClassToListType(prevPassClass) == D3D12_COMMAND_LIST_TYPE_DIRECT && PassClassToListType(currPassClass) == D3D12_COMMAND_LIST_TYPE_COMPUTE) //Rules 5, 6, 7, 8
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
	else if(PassClassToListType(prevPassClass) == D3D12_COMMAND_LIST_TYPE_COMPUTE && PassClassToListType(currPassClass) == D3D12_COMMAND_LIST_TYPE_DIRECT) //Rules 9, 10, 11, 12, 13
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
	else if(PassClassToListType(currPassClass) == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 14, 15
	{
		barrierNeeded = false;
	}
	else if(PassClassToListType(prevPassClass) == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 16, 17
	{
		barrierNeeded = !currPassInfo.BarrierPromotedFromCommon;
	}

	if(barrierNeeded)
	{
		D3D12_RESOURCE_BARRIER textureTransitionBarrier;
		textureTransitionBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		textureTransitionBarrier.Transition.pResource   = mD3d12GraphToBuild->mTextures[imageIndex];
		textureTransitionBarrier.Transition.Subresource = 0;
		textureTransitionBarrier.Transition.StateBefore = prevPassState;
		textureTransitionBarrier.Transition.StateAfter  = currPassState;
		textureTransitionBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		mD3d12GraphToBuild->mResourceBarriers.push_back(textureTransitionBarrier);

		return (uint32_t)(mD3d12GraphToBuild->mResourceBarriers.size() - 1);
	}

	return (uint32_t)(-1);
}

uint32_t D3D12::FrameGraphBuilder::AddAfterPassBarrier(uint32_t imageIndex, RenderPassClass currPassClass, uint32_t currPassSubresourceInfoIndex, RenderPassClass nextPassClass, uint32_t nextPassSubresourceInfoIndex)
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

	const SubresourceInfo& currPassInfo = mSubresourceInfos[currPassSubresourceInfoIndex];
	const SubresourceInfo& nextPassInfo = mSubresourceInfos[nextPassSubresourceInfoIndex];

	bool barrierNeeded = false;
	D3D12_RESOURCE_STATES currPassState = currPassInfo.State;
	D3D12_RESOURCE_STATES nextPassState = nextPassInfo.State;

	if(PassClassToListType(currPassClass) == PassClassToListType(nextPassClass)) //Rules 1, 2, 3, 4
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
	else if(PassClassToListType(currPassClass) == D3D12_COMMAND_LIST_TYPE_DIRECT && PassClassToListType(nextPassClass) == D3D12_COMMAND_LIST_TYPE_COMPUTE) //Rules 5, 6, 7, 8
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
	else if(PassClassToListType(currPassClass) == D3D12_COMMAND_LIST_TYPE_COMPUTE && PassClassToListType(nextPassClass) == D3D12_COMMAND_LIST_TYPE_DIRECT) //Rules 9, 10, 11, 12, 13, 14
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
	else if(PassClassToListType(nextPassClass) == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 15, 16
	{
		barrierNeeded = !currPassInfo.BarrierPromotedFromCommon || D3D12Utils::IsStateWriteable(currPassState);
	}
	else if(PassClassToListType(currPassClass) == D3D12_COMMAND_LIST_TYPE_COPY) //Rules 17, 18
	{
		barrierNeeded = false;
	}

	if(barrierNeeded)
	{
		D3D12_RESOURCE_BARRIER textureTransitionBarrier;
		textureTransitionBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		textureTransitionBarrier.Transition.pResource   = mD3d12GraphToBuild->mTextures[imageIndex];
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
}

uint32_t D3D12::FrameGraphBuilder::GetSwapchainImageCount() const
{
	return mSwapChain->SwapchainImageCount;
}

void D3D12::FrameGraphBuilder::InitPassTable()
{
	mPassAddFuncTable.clear();
	mPassCreateFuncTable.clear();

	AddPassToTable<GBufferPass>();
	AddPassToTable<CopyImagePass>();
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