#include "VulkanCFrameGraphBuilder.hpp"
#include "VulkanCFrameGraph.hpp"
#include <algorithm>

VulkanCBindings::FrameGraphBuilder::FrameGraphBuilder(FrameGraph* graphToBuild, const RenderableScene* scene, const DeviceParameters* deviceParameters, const ShaderManager* shaderManager): mGraphToBuild(graphToBuild), mScene(scene), mDeviceParameters(deviceParameters), mShaderManager(shaderManager)
{
}

VulkanCBindings::FrameGraphBuilder::~FrameGraphBuilder()
{
}

void VulkanCBindings::FrameGraphBuilder::RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc)
{
	RenderPassName renderPassName(passName);
	assert(!mRenderPassCreateFunctions.contains(renderPassName));

	mRenderPassNames.push_back(renderPassName);
	mRenderPassCreateFunctions[renderPassName] = createFunc;
}

void VulkanCBindings::FrameGraphBuilder::RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	RenderPassName renderPassName(passName);

	if(!mRenderPassesReadSubresourceIds.contains(renderPassName))
	{
		mRenderPassesReadSubresourceIds[renderPassName] = std::unordered_set<SubresourceId>();
	}

	SubresourceId subresId(subresourceId);
	assert(!mRenderPassesReadSubresourceIds[renderPassName].contains(subresId));

	mRenderPassesReadSubresourceIds[renderPassName].insert(subresId);

	if(!mRenderPassesSubresourceMetadatas.contains(renderPassName))
	{
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, ImageSubresourceMetadata>();
	}

	ImageSubresourceMetadata subresourceMetadata;
	subresourceMetadata.ImageIndex         = (uint32_t)(-1);
	subresourceMetadata.ImageViewIndex     = (uint32_t)(-1);
	subresourceMetadata.Layout             = VK_IMAGE_LAYOUT_GENERAL;
	subresourceMetadata.UsageFlags         = 0;
	subresourceMetadata.AspectFlags        = 0;
	subresourceMetadata.PipelineStageFlags = 0;
	subresourceMetadata.AccessFlags        = 0;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = subresourceMetadata;
}

void VulkanCBindings::FrameGraphBuilder::RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId)
{
	RenderPassName renderPassName(passName);

	if(!mRenderPassesWriteSubresourceIds.contains(renderPassName))
	{
		mRenderPassesWriteSubresourceIds[renderPassName] = std::unordered_set<SubresourceId>();
	}

	SubresourceId subresId(subresourceId);
	assert(!mRenderPassesWriteSubresourceIds[renderPassName].contains(subresId));

	mRenderPassesWriteSubresourceIds[renderPassName].insert(subresId);

	if(!mRenderPassesSubresourceMetadatas.contains(renderPassName))
	{
		mRenderPassesSubresourceMetadatas[renderPassName] = std::unordered_map<SubresourceId, ImageSubresourceMetadata>();
	}

	ImageSubresourceMetadata subresourceMetadata;
	subresourceMetadata.ImageIndex         = (uint32_t)(-1);
	subresourceMetadata.ImageViewIndex     = (uint32_t)(-1);
	subresourceMetadata.Layout             = VK_IMAGE_LAYOUT_UNDEFINED;
	subresourceMetadata.UsageFlags         = 0;
	subresourceMetadata.AspectFlags        = 0;
	subresourceMetadata.PipelineStageFlags = 0;
	subresourceMetadata.AccessFlags        = 0;

	mRenderPassesSubresourceMetadatas[renderPassName][subresId] = subresourceMetadata;
}

void VulkanCBindings::FrameGraphBuilder::AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName)
{
	RenderPassName  passNameStr(passName);
	SubresourceId   subresourceIdStr(subresourceId);
	SubresourceName subresourceNameStr(subresourceName);

	auto readSubresIt  = mRenderPassesReadSubresourceIds.find(passNameStr);
	auto writeSubresIt = mRenderPassesWriteSubresourceIds.find(passNameStr);

	assert(readSubresIt  != mRenderPassesReadSubresourceIds.end() || writeSubresIt != mRenderPassesWriteSubresourceIds.end());
	assert(readSubresIt  == mRenderPassesReadSubresourceIds.end() || readSubresIt->second.contains(subresourceIdStr));
	assert(writeSubresIt == mRenderPassesReadSubresourceIds.end() || writeSubresIt->second.contains(subresourceIdStr));

	mRenderPassesSubresourceNameIds[passNameStr][subresourceNameStr] = subresourceIdStr;
}

void VulkanCBindings::FrameGraphBuilder::AssignBackbufferName(const std::string_view backbufferName)
{
	mBackbufferName = backbufferName;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId, VkImageLayout layout)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].Layout = layout;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId, VkImageUsageFlags usage)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].UsageFlags = usage;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId, VkImageAspectFlags aspect)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].AspectFlags = aspect;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId, VkPipelineStageFlags stage)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].PipelineStageFlags = stage;
}

void VulkanCBindings::FrameGraphBuilder::SetPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId, VkAccessFlags accessFlags)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	mRenderPassesSubresourceMetadatas[passNameStr][subresourceIdStr].AccessFlags = accessFlags;
}

const VulkanCBindings::RenderableScene* VulkanCBindings::FrameGraphBuilder::GetScene() const
{
	return mScene;
}

const VulkanCBindings::DeviceParameters* VulkanCBindings::FrameGraphBuilder::GetDeviceParameters() const
{
	return mDeviceParameters;
}

const VulkanCBindings::ShaderManager* VulkanCBindings::FrameGraphBuilder::GetShaderManager() const
{
	return mShaderManager;
}

const VulkanCBindings::FrameGraphConfig* VulkanCBindings::FrameGraphBuilder::GetConfig() const
{
	return &mGraphToBuild->mFrameGraphConfig;
}

VkImage VulkanCBindings::FrameGraphBuilder::GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.ImageIndex == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		return mGraphToBuild->mImages[metadata.ImageIndex];
	}
}

VkImageView VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresource(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadata.ImageViewIndex == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		return mGraphToBuild->mImageViews[metadata.ImageViewIndex];
	}
}

VkImageLayout VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.Layout;
}

VkImageUsageFlags VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.UsageFlags;
}

VkPipelineStageFlags VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.PipelineStageFlags;
}

VkImageAspectFlags VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.AspectFlags;
}

VkAccessFlags VulkanCBindings::FrameGraphBuilder::GetRegisteredSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	return metadata.AccessFlags;
}

void VulkanCBindings::FrameGraphBuilder::Build()
{
	BuildAdjacencyList();

	std::vector<size_t> sortedPassIndices;

	std::vector<uint_fast8_t> visited(mGraphToBuild->mRenderPasses.size(), false);
	std::vector<uint_fast8_t> onStack(mGraphToBuild->mRenderPasses.size(), false);

	for(size_t passIndex = 0; passIndex < mGraphToBuild->mRenderPasses.size(); passIndex++)
	{
		TopologicalSortNode(visited, onStack, passIndex, sortedPassIndices);
	}

	std::vector<RenderPassName> sortedRenderPassNames(mRenderPassNames.size());
	for(size_t i = 0; i < mGraphToBuild->mRenderPasses.size(); i++)
	{
		sortedRenderPassNames[mRenderPassNames.size() - i - 1] = mRenderPassNames[sortedPassIndices[i]];
	}

	mRenderPassNames = std::move(sortedRenderPassNames);

	mRenderPassIndices.clear();
	for(uint32_t i = 0; i < mRenderPassNames.size(); i++)
	{
		mRenderPassIndices[mRenderPassNames[i]] = i;
	}

	BuildAdjacencyList(); //Need to rebuild for other purposes, such as building dependency levels

	BuildPassObjects();
}

void VulkanCBindings::FrameGraphBuilder::BuildAdjacencyList()
{
	mAdjacencyList.clear();

	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		mAdjacencyList.emplace_back();

		for(size_t j = 0; j < mRenderPassNames.size(); j++)
		{
			if(i == j)
			{
				continue;
			}

			if(PassesIntersect(mRenderPassNames[i], mRenderPassNames[j]))
			{
				mAdjacencyList[i].insert(j);
			}
		}
	}
}

void VulkanCBindings::FrameGraphBuilder::TopologicalSortNode(std::vector<uint_fast8_t>& visited, std::vector<uint_fast8_t>& onStack, size_t passIndex, std::vector<size_t>& sortedPassIndices)
{
	if(visited[passIndex])
	{
		return;
	}

	onStack[passIndex] = true;
	visited[passIndex] = true;

	for(size_t adjacentPassIndex: mAdjacencyList[passIndex])
	{
		TopologicalSortNode(visited, onStack, adjacentPassIndex, sortedPassIndices);
	}

	onStack[passIndex] = false;
	sortedPassIndices.push_back(passIndex);
}

void VulkanCBindings::FrameGraphBuilder::BuildPassObjects()
{
	mGraphToBuild->mRenderPasses.clear();

	for(const std::string& passName: mRenderPassNames)
	{
		mGraphToBuild->mRenderPasses.push_back(mRenderPassCreateFunctions[passName](mGraphToBuild->mDeviceRef, this, passName));
	}
}

bool VulkanCBindings::FrameGraphBuilder::PassesIntersect(const RenderPassName& writingPass, const RenderPassName& readingPass)
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