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
	mSrvUavCbvDescriptorCount = 0;
	mRtvDescriptorCount       = 0;
	mDsvDescriptorCount       = 0;
	
	mSrvUavCbvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mRtvDescriptorSize       = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize       = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
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

ID3D12Resource2* D3D12::FrameGraphBuilder::GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadataNode.FirstFrameHandle == (uint32_t)(-1))
	{
		return nullptr;
	}
	else
	{
		return mD3d12GraphToBuild->mTextures[metadataNode.FirstFrameHandle + frame];
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceSrvUav(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	const SubresourceInfo& subresourceInfo = mSubresourceInfos[metadataNode.SubresourceInfoIndex];

	const D3D12_RESOURCE_STATES resourceState = (D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	assert((subresourceInfo.State & resourceState) && (metadataNode.FirstFrameViewHandle != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphSrvHeapStart();
	descriptorHandle.ptr                        += (metadataNode.FirstFrameViewHandle + frame) * mSrvUavCbvDescriptorSize;

	return descriptorHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceRtv(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	const SubresourceInfo& subresourceInfo = mSubresourceInfos[metadataNode.SubresourceInfoIndex];

	const D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	assert((subresourceInfo.State & resourceState) && (metadataNode.FirstFrameViewHandle != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphRtvHeapStart();
	descriptorHandle.ptr                        += (metadataNode.FirstFrameViewHandle + frame) * mRtvDescriptorSize;

	return descriptorHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::FrameGraphBuilder::GetRegisteredSubresourceDsv(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	const SubresourceInfo& subresourceInfo = mSubresourceInfos[metadataNode.SubresourceInfoIndex];

	const D3D12_RESOURCE_STATES resourceState = (D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE);
	assert((subresourceInfo.State & resourceState) && (metadataNode.FirstFrameViewHandle != (uint32_t)(-1)));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = GetFrameGraphDsvHeapStart();
	descriptorHandle.ptr                        += (metadataNode.FirstFrameViewHandle + frame) * mDsvDescriptorSize;

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
 
void D3D12::FrameGraphBuilder::CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos, uint32_t totalTextureCount) const
{
	mD3d12GraphToBuild->mTextures.resize(totalTextureCount);
	mD3d12GraphToBuild->mTextureHeap.reset();

	std::vector<D3D12_RESOURCE_DESC1> textureDescs;
	textureDescs.resize(totalTextureCount);

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
		resourceDesc.SampleDesc.Quality              = 1;
		resourceDesc.Layout                          = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags                           = resourceFlags;
		resourceDesc.SamplerFeedbackMipRegion.Width  = 0;
		resourceDesc.SamplerFeedbackMipRegion.Height = 0;
		resourceDesc.SamplerFeedbackMipRegion.Depth  = 0;

		for(uint32_t frame = 0; frame < textureCreateInfo.MetadataHead->FrameCount; frame++)
		{
			textureDescs[textureCreateInfo.MetadataHead->FirstFrameHandle + frame] = resourceDesc;
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
			D3D12Utils::SetDebugObjectName(mD3d12GraphToBuild->mTextures[imageIndex], textureCreateInfo.Name);
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
			D3D12Utils::SetDebugObjectName(mD3d12GraphToBuild->mTextures[imageIndex], mBackbufferName);
#endif
		}
	}
}

uint32_t D3D12::FrameGraphBuilder::AddSubresourceMetadata()
{
	SubresourceInfo subresourceInfo;
	subresourceInfo.Format = DXGI_FORMAT_UNKNOWN;
	subresourceInfo.State  = D3D12_RESOURCE_STATE_COMMON;
	subresourceInfo.Flags  = 0;

	mSubresourceInfos.push_back(subresourceInfo);
	return (uint32_t)(mSubresourceInfos.size() - 1);
}

bool D3D12::FrameGraphBuilder::ValidateSubresourceViewParameters(SubresourceMetadataNode* node)
{
	SubresourceInfo& prevPassSubresourceInfo = mSubresourceInfos[node->PrevPassNode->SubresourceInfoIndex];
	SubresourceInfo& thisPassSubresourceInfo = mSubresourceInfos[node->SubresourceInfoIndex];

	if(thisPassSubresourceInfo.Format == DXGI_FORMAT_UNKNOWN && prevPassSubresourceInfo.Format != DXGI_FORMAT_UNKNOWN)
	{
		thisPassSubresourceInfo.Format = prevPassSubresourceInfo.Format;
	}

	const uint32_t srvStateMask = (uint32_t)(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	const uint32_t uavStateMask = (uint32_t)(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const uint32_t rtvStateMask = (uint32_t)(D3D12_RESOURCE_STATE_RENDER_TARGET);
	const uint32_t dsvStateMask = (uint32_t)(D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE);

	const uint32_t stateKey  = (uint32_t)thisPassSubresourceInfo.State & (srvStateMask | uavStateMask | rtvStateMask | dsvStateMask); //Other states are not needed for separate views (i.e. descriptors)
	const uint32_t formatKey = (uint32_t)thisPassSubresourceInfo.Format * (stateKey != 0); //Creating the view is only needed if the state is descriptorable

	node->ViewSortKey = (stateKey << 32) | formatKey;

	return thisPassSubresourceInfo.Format != 0; //Only the format should be propagated
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

void D3D12::FrameGraphBuilder::InitializeTraverseData() const
{
	mD3d12GraphToBuild->mFrameRecordedGraphicsCommandLists.resize(mD3d12GraphToBuild->mGraphicsPassSpans.size());
	mD3d12GraphToBuild->mCurrentFramePasses.resize(mRenderPassNames.size());
}

uint32_t D3D12::FrameGraphBuilder::GetSwapchainImageCount() const
{
	return mSwapChain->SwapchainImageCount;
}
