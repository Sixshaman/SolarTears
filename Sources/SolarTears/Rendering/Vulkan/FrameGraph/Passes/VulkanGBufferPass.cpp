#include "VulkanGBufferPass.hpp"
#include "../../VulkanFunctions.hpp"
#include "../../Scene/VulkanScene.hpp"
#include "../../VulkanShaders.hpp"
#include "../VulkanFrameGraphBuilder.hpp"
#include <array>
#include <VulkanGenericStructures.h>

Vulkan::GBufferPass::GBufferPass(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassId): RenderPass(frameGraphBuilder->GetDevice())
{
	mRenderPass  = VK_NULL_HANDLE;
	mFramebuffer = VK_NULL_HANDLE;

	mStaticPipelineLayout = VK_NULL_HANDLE;
	mRigidPipelineLayout  = VK_NULL_HANDLE;

	mStaticPipeline          = VK_NULL_HANDLE;
	mStaticInstancedPipeline = VK_NULL_HANDLE;
	mRigidPipeline           = VK_NULL_HANDLE;

	//TODO: subpasses (AND SECONDARY COMMAND BUFFERS)
	CreateRenderPass(frameGraphBuilder,  frameGraphPassId, frameGraphBuilder->GetDeviceParameters());
	CreateFramebuffer(frameGraphBuilder, frameGraphPassId, frameGraphBuilder->GetConfig());

	ShaderDatabase* shaderDatabase = frameGraphBuilder->GetShaderDatabase();
	ObtainDescriptorData(shaderDatabase, frameGraphPassId);

	CreatePipelineLayouts(shaderDatabase);
	CreatePipelines(shaderDatabase, frameGraphBuilder->GetConfig());
}

Vulkan::GBufferPass::~GBufferPass()
{
	SafeDestroyObject(vkDestroyPipeline, mDeviceRef, mStaticPipeline);
	SafeDestroyObject(vkDestroyPipeline, mDeviceRef, mStaticInstancedPipeline);
	SafeDestroyObject(vkDestroyPipeline, mDeviceRef, mRigidPipeline);

	SafeDestroyObject(vkDestroyPipelineLayout, mDeviceRef, mStaticPipelineLayout);
	SafeDestroyObject(vkDestroyPipelineLayout, mDeviceRef, mRigidPipelineLayout);

	SafeDestroyObject(vkDestroyRenderPass,  mDeviceRef, mRenderPass);
	SafeDestroyObject(vkDestroyFramebuffer, mDeviceRef, mFramebuffer);
}

void Vulkan::GBufferPass::RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig, uint32_t frameResourceIndex) const
{
	constexpr uint32_t WriteAttachmentCount = 1;

	std::array<VkClearValue, WriteAttachmentCount> clearValues;
	for(uint32_t i = 0; i < WriteAttachmentCount; i++)
	{
		clearValues[i].color.float32[0] = 0.0f;
		clearValues[i].color.float32[1] = 0.0f;
		clearValues[i].color.float32[2] = 0.0f;
		clearValues[i].color.float32[3] = 0.0f;
	}

	//TODO: imageless framebuffer attachments (VkRenderPassAttachmentBeginInfo). Doing it in runtime with StructureBlob may hurt performance...
	//TODO: mGPU?
	VkRenderPassBeginInfo renderPassBeginInfo;
	renderPassBeginInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext                    = nullptr;
	renderPassBeginInfo.renderPass               = mRenderPass;
	renderPassBeginInfo.framebuffer              = mFramebuffer;
	renderPassBeginInfo.renderArea.offset.x      = 0;
	renderPassBeginInfo.renderArea.offset.y      = 0;
	renderPassBeginInfo.renderArea.extent.width  = frameGraphConfig.GetViewportWidth();
	renderPassBeginInfo.renderArea.extent.height = frameGraphConfig.GetViewportHeight();
	renderPassBeginInfo.clearValueCount          = (uint32_t)(clearValues.size());
	renderPassBeginInfo.pClearValues             = clearValues.data();

	//TODO: Begin renderpass #2
	//TODO: Secondary command buffers
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkPipelineLayout currentPipelineLayout = mStaticPipelineLayout;
	auto submeshCallback = [this, currentPipelineLayout](VkCommandBuffer commandBuffer, uint32_t materialIndex)
	{
		std::array pushConstants = {materialIndex};
		vkCmdPushConstants(commandBuffer, currentPipelineLayout, mMaterialIndexPushConstantStages, mMaterialIndexPushConstantOffset, (uint32_t)(pushConstants.size() * sizeof(decltype(pushConstants)::value_type)), pushConstants.data());
	};

	auto meshCallback = [this, currentPipelineLayout](VkCommandBuffer commandBuffer, uint32_t objectDataIndex)
	{
		std::array pushConstants = {objectDataIndex};
		vkCmdPushConstants(commandBuffer, currentPipelineLayout, mObjectIndexPushConstantStages, mObjectIndexPushConstantOffset, (uint32_t)(pushConstants.size() * sizeof(decltype(pushConstants)::value_type)), pushConstants.data());
	};

	//Prepare to first drawing
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipelineLayout, mStaticDrawSetBindOffset, mStaticDrawChangedSetSpan.End - mStaticDrawChangedSetSpan.Begin, mDescriptorSets[frameResourceIndex].data() + mStaticDrawChangedSetSpan.Begin, 0, nullptr);
	scene->PrepareDrawBuffers(commandBuffer);
	
	//Draw static meshes
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mStaticPipeline);
	scene->DrawStaticObjects(commandBuffer, submeshCallback);

	//Draw static instanced meshes
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mStaticInstancedPipeline);
	scene->DrawStaticInstancedObjects(commandBuffer, meshCallback, submeshCallback);

	//Change pipeline layout and descriptor bindings
	currentPipelineLayout = mRigidPipelineLayout;

	//Draw rigid meshes
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipelineLayout, mRigidDrawSetBindOffset, mRigidDrawChangedSetSpan.End - mRigidDrawChangedSetSpan.Begin, mDescriptorSets[frameResourceIndex].data() + mRigidDrawChangedSetSpan.Begin, 0, nullptr);
	scene->DrawRigidObjects(commandBuffer, meshCallback, submeshCallback);

	vkCmdEndRenderPass(commandBuffer);
}

void Vulkan::GBufferPass::ValidateDescriptorSetSpans(DescriptorDatabase* descriptorDatabase, const VkDescriptorSet* originalSpanStartPoint)
{
	for(uint32_t frameResourceIndex = 0; frameResourceIndex < Utils::InFlightFrameCount; frameResourceIndex++)
	{
		mDescriptorSets[frameResourceIndex] = descriptorDatabase->ValidateSetSpan(mDescriptorSets[frameResourceIndex], originalSpanStartPoint);
	}
}

void Vulkan::GBufferPass::ObtainDescriptorData(ShaderDatabase* shaderDatabase, uint32_t passIndex)
{
	//The pipelines will be bound in this order
	constexpr uint32_t staticGroupIndex          = 0;
	constexpr uint32_t staticInstancedGroupIndex = 1;
	constexpr uint32_t rigidGroupIndex           = 2;

	std::array<std::string_view, 3> shaderGroupPipelineOrder;
	shaderGroupPipelineOrder[staticGroupIndex]          = StaticShaderGroup;
	shaderGroupPipelineOrder[staticInstancedGroupIndex] = StaticInstancedShaderGroup;
	shaderGroupPipelineOrder[rigidGroupIndex]           = RigidShaderGroup;
	
	std::array<DescriptorSetBindRange, shaderGroupPipelineOrder.size()> setBindRangesPerShaderGroup;
	for(uint32_t frameResourceIndex = 0; frameResourceIndex < Utils::InFlightFrameCount; frameResourceIndex++)
	{
		mDescriptorSets[frameResourceIndex] = shaderDatabase->AssignPassSets(passIndex, frameResourceIndex, shaderGroupPipelineOrder, setBindRangesPerShaderGroup);
	}

	//Static and static instanced sets should match
	assert(setBindRangesPerShaderGroup[staticInstancedGroupIndex].BindPoint == setBindRangesPerShaderGroup[staticInstancedGroupIndex].Begin);

	mStaticDrawChangedSetSpan = Span<uint32_t>{.Begin = setBindRangesPerShaderGroup[staticGroupIndex].Begin, .End = setBindRangesPerShaderGroup[staticInstancedGroupIndex].End};
	mRigidDrawChangedSetSpan  = Span<uint32_t>{.Begin = setBindRangesPerShaderGroup[rigidGroupIndex].Begin,  .End = setBindRangesPerShaderGroup[rigidGroupIndex].End};

	mStaticDrawSetBindOffset = setBindRangesPerShaderGroup[staticGroupIndex].BindPoint;
	mRigidDrawSetBindOffset  = setBindRangesPerShaderGroup[rigidGroupIndex].BindPoint;
}

void Vulkan::GBufferPass::CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassId, const DeviceParameters* deviceParameters)
{
	//TODO: may alias?
	//TODO: multisampling?
	std::array<VkAttachmentDescription, 1> attachments;
	attachments[0].flags          = 0;
	attachments[0].format         = frameGraphBuilder->GetRegisteredSubresourceFormat(frameGraphPassId, (uint_fast16_t)PassSubresourceId::ColorBufferImage);
	attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout  = frameGraphBuilder->GetPreviousPassSubresourceLayout(frameGraphPassId, (uint_fast16_t)PassSubresourceId::ColorBufferImage);
	attachments[0].finalLayout    = frameGraphBuilder->GetNextPassSubresourceLayout(frameGraphPassId, (uint_fast16_t)PassSubresourceId::ColorBufferImage);

	VkAttachmentReference colorBufferAttachment;
	colorBufferAttachment.attachment = 0;
	colorBufferAttachment.layout     = frameGraphBuilder->GetRegisteredSubresourceLayout(frameGraphPassId, (uint_fast16_t)PassSubresourceId::ColorBufferImage);

	std::array colorAttachments = {colorBufferAttachment};
		
	std::array<VkSubpassDescription, 1> subpasses;
	subpasses[0].flags                   = 0;
	subpasses[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].inputAttachmentCount    = 0;
	subpasses[0].pInputAttachments       = nullptr;
	subpasses[0].colorAttachmentCount    = (uint32_t)(colorAttachments.size());
	subpasses[0].pColorAttachments       = colorAttachments.data();
	subpasses[0].pResolveAttachments     = nullptr;
	subpasses[0].pDepthStencilAttachment = nullptr;
	subpasses[0].preserveAttachmentCount = 0;
	subpasses[0].pPreserveAttachments    = 0;

	std::array<VkSubpassDependency, 2> dependencies;
	dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass      = 0;
	dependencies[0].srcStageMask    = frameGraphBuilder->GetPreviousPassSubresourceStageFlags(frameGraphPassId, (uint_fast16_t)PassSubresourceId::ColorBufferImage);
	dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask   = frameGraphBuilder->GetPreviousPassSubresourceAccessFlags(frameGraphPassId, (uint_fast16_t)PassSubresourceId::ColorBufferImage);
	dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	dependencies[1].srcSubpass      = 0;
	dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask    = frameGraphBuilder->GetNextPassSubresourceStageFlags(frameGraphPassId, (uint_fast16_t)PassSubresourceId::ColorBufferImage);
	dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask   = frameGraphBuilder->GetNextPassSubresourceAccessFlags(frameGraphPassId, (uint_fast16_t)PassSubresourceId::ColorBufferImage);
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	//TODO: multiview (VR)
	//TODO: RenderPass 2
	//TODO: Input attachment aspects (when input attachments are present)
	vgs::StructureChainBlob<VkRenderPassCreateInfo> renderPassCreateInfoChain;
	if(deviceParameters->IsFragmentDensityMap2FeaturesExtensionEnabled())
	{
		//TODO: Fragment density map
	}

	VkRenderPassCreateInfo& renderPassCreateInfo = renderPassCreateInfoChain.GetChainHead();
	renderPassCreateInfo.flags                   = 0;
	renderPassCreateInfo.attachmentCount         = (uint32_t)(attachments.size());
	renderPassCreateInfo.pAttachments            = attachments.data();
	renderPassCreateInfo.subpassCount            = (uint32_t)(subpasses.size());
	renderPassCreateInfo.pSubpasses              = subpasses.data();
	renderPassCreateInfo.dependencyCount         = (uint32_t)(dependencies.size());
	renderPassCreateInfo.pDependencies           = dependencies.data();

	ThrowIfFailed(vkCreateRenderPass(mDeviceRef, &renderPassCreateInfo, nullptr, &mRenderPass));
}

void Vulkan::GBufferPass::CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassId, const FrameGraphConfig* frameGraphConfig)
{
	vgs::GenericStructureChain<VkFramebufferCreateInfo> framebufferCreateInfoChain;

	std::array attachmentIds = {PassSubresourceId::ColorBufferImage};

	std::array<VkImageView, attachmentIds.size()> attachments;
	for(size_t i = 0; i < attachments.size(); i++)
	{
		attachments[i] = frameGraphBuilder->GetRegisteredSubresource(frameGraphPassId, (uint_fast16_t)attachmentIds[i]);
	}

	//TODO: imageless framebuffer (only if needed, set via config)
	VkFramebufferCreateInfo& framebufferCreateInfo = framebufferCreateInfoChain.GetChainHead();
	framebufferCreateInfo.flags                    = 0;
	framebufferCreateInfo.renderPass               = mRenderPass;
	framebufferCreateInfo.attachmentCount          = (uint32_t)(attachments.size());
	framebufferCreateInfo.pAttachments             = attachments.data();
	framebufferCreateInfo.width                    = frameGraphConfig->GetViewportWidth();
	framebufferCreateInfo.height                   = frameGraphConfig->GetViewportHeight();
	framebufferCreateInfo.layers                   = 1;

	ThrowIfFailed(vkCreateFramebuffer(mDeviceRef, &framebufferCreateInfo, nullptr, &mFramebuffer));
}

void Vulkan::GBufferPass::CreatePipelineLayouts(const ShaderDatabase* shaderDatabase)
{
	std::array staticGroupNames = {StaticShaderGroup, StaticInstancedShaderGroup};
	std::array rigidGroupNames  = {RigidShaderGroup};
	std::array allGroupNames    = {StaticShaderGroup, StaticInstancedShaderGroup, RigidShaderGroup};

	shaderDatabase->CreateMatchingPipelineLayout(staticGroupNames, allGroupNames, &mStaticPipelineLayout);
	shaderDatabase->CreateMatchingPipelineLayout(rigidGroupNames,  allGroupNames, &mRigidPipelineLayout);

	shaderDatabase->GetPushConstantInfo(RigidShaderGroup, "MaterialIndex", &mMaterialIndexPushConstantOffset, &mMaterialIndexPushConstantStages);
	shaderDatabase->GetPushConstantInfo(RigidShaderGroup, "ObjectIndex",   &mObjectIndexPushConstantOffset,   &mObjectIndexPushConstantStages);
}

void Vulkan::GBufferPass::CreatePipelines(const ShaderDatabase* shaderDatabase, const FrameGraphConfig* frameGraphConfig)
{
	const std::wstring shaderFolder = Utils::GetMainDirectory() + L"Shaders/Vulkan/GBuffer/";

	const uint32_t* staticVertexShaderCode = nullptr;
	uint32_t        staticVertexShaderSize = 0;
	shaderDatabase->GetRegisteredShaderInfo(shaderFolder + L"GBufferDrawStatic.vert.spv", &staticVertexShaderCode, &staticVertexShaderSize);

	const uint32_t* staticInstancedVertexShaderCode = nullptr;
	uint32_t        staticInstancedVertexShaderSize = 0;
	shaderDatabase->GetRegisteredShaderInfo(shaderFolder + L"GBufferDrawStaticInstanced.vert.spv", &staticInstancedVertexShaderCode, &staticInstancedVertexShaderSize);

	const uint32_t* rigidVertexShaderCode = nullptr;
	uint32_t        rigidVertexShaderSize = 0;
	shaderDatabase->GetRegisteredShaderInfo(shaderFolder + L"GBufferDrawRigid.vert.spv", &rigidVertexShaderCode, &rigidVertexShaderSize);

	const uint32_t* fragmentShaderCode = nullptr;
	uint32_t        fragmentShaderSize = 0;
	shaderDatabase->GetRegisteredShaderInfo(shaderFolder + L"GBufferDraw.frag.spv", &fragmentShaderCode, &fragmentShaderSize);

	//Create pipelines
	CreateGBufferPipeline(staticVertexShaderCode,          staticVertexShaderSize,          fragmentShaderCode, fragmentShaderSize, mStaticPipelineLayout, frameGraphConfig, &mStaticPipeline);
	CreateGBufferPipeline(staticInstancedVertexShaderCode, staticInstancedVertexShaderSize, fragmentShaderCode, fragmentShaderSize, mStaticPipelineLayout, frameGraphConfig, &mStaticInstancedPipeline);
	CreateGBufferPipeline(rigidVertexShaderCode,           rigidVertexShaderSize,           fragmentShaderCode, fragmentShaderSize, mRigidPipelineLayout,  frameGraphConfig, &mRigidPipeline);
}

void Vulkan::GBufferPass::CreateGBufferPipeline(const uint32_t* vertexShaderModule, uint32_t vertexShaderModuleSize, const uint32_t* fragmentShaderModule, uint32_t fragmetShaderModuleSize, VkPipelineLayout pipelineLayout, const FrameGraphConfig* frameGraphConfig, VkPipeline* outPipeline)
{
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo;
	vertexShaderModuleCreateInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext    = nullptr;
	vertexShaderModuleCreateInfo.flags    = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderModuleSize;
	vertexShaderModuleCreateInfo.pCode    = vertexShaderModule;

	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo;
	fragmentShaderModuleCreateInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext    = nullptr;
	fragmentShaderModuleCreateInfo.flags    = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmetShaderModuleSize;
	fragmentShaderModuleCreateInfo.pCode    = fragmentShaderModule;

	//TODO: varying subrgroup size
	std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos;

	std::array<VkShaderModule, pipelineShaderStageCreateInfos.size()> shaderModules;
	ThrowIfFailed(vkCreateShaderModule(mDeviceRef, &vertexShaderModuleCreateInfo,   nullptr, &shaderModules[0]));
	ThrowIfFailed(vkCreateShaderModule(mDeviceRef, &fragmentShaderModuleCreateInfo, nullptr, &shaderModules[1]));

	const char* enrtyPointName = "main";

	std::array pipelineShaderStages = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};
	for(size_t i = 0; i < pipelineShaderStageCreateInfos.size(); i++)
	{
		pipelineShaderStageCreateInfos[i].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineShaderStageCreateInfos[i].pNext               = nullptr;
		pipelineShaderStageCreateInfos[i].flags               = 0;
		pipelineShaderStageCreateInfos[i].stage               = pipelineShaderStages[i];
		pipelineShaderStageCreateInfos[i].module              = shaderModules[i];
		pipelineShaderStageCreateInfos[i].pName               = enrtyPointName;
		pipelineShaderStageCreateInfos[i].pSpecializationInfo = nullptr;
	}


	//Vertex input state
	std::array<VkVertexInputBindingDescription, 1> vertexInputBindingDescriptions;
	vertexInputBindingDescriptions[0].stride    = (uint32_t)sizeof(RenderableSceneVertex);
	vertexInputBindingDescriptions[0].binding   = 0;
	vertexInputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	//TODO: Shader reflection to get the location?
	std::array<VkVertexInputAttributeDescription, 3> vertexAttributeDescriptions;
	vertexAttributeDescriptions[0].binding  = 0;
	vertexAttributeDescriptions[0].format   = VulkanUtils::FormatForVectorType<decltype(RenderableSceneVertex::Position)>;
	vertexAttributeDescriptions[0].location = 0;
	vertexAttributeDescriptions[0].offset   = offsetof(RenderableSceneVertex, Position);
	vertexAttributeDescriptions[1].binding  = 0;
	vertexAttributeDescriptions[1].format   = VulkanUtils::FormatForVectorType<decltype(RenderableSceneVertex::Normal)>;
	vertexAttributeDescriptions[1].location = 1;
	vertexAttributeDescriptions[1].offset   = offsetof(RenderableSceneVertex, Normal);
	vertexAttributeDescriptions[2].binding  = 0;
	vertexAttributeDescriptions[2].format   = VulkanUtils::FormatForVectorType<decltype(RenderableSceneVertex::Texcoord)>;
	vertexAttributeDescriptions[2].location = 2;
	vertexAttributeDescriptions[2].offset   = offsetof(RenderableSceneVertex, Texcoord);

	VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
	pipelineVertexInputStateCreateInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipelineVertexInputStateCreateInfo.pNext                           = nullptr;
	pipelineVertexInputStateCreateInfo.flags                           = 0;
	pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount   = (uint32_t)(vertexInputBindingDescriptions.size());
	pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions      = vertexInputBindingDescriptions.data();
	pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = (uint32_t)(vertexAttributeDescriptions.size());
	pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions    = vertexAttributeDescriptions.data();


	//Input assembly state
	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo;
	pipelineInputAssemblyStateCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineInputAssemblyStateCreateInfo.pNext                  = nullptr;
	pipelineInputAssemblyStateCreateInfo.flags                  = 0;
	pipelineInputAssemblyStateCreateInfo.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = false;


	//Viewport state
	//TODO: variable rate shading
	//TODO: viewport swizzles
	//TODO: clip space W scaling (needed for VR)
	std::array<VkViewport, 1> viewports;
	std::array<VkRect2D, 1>   scissors;

	viewports[0].x        = 0.0f;
	viewports[0].y        = 0.0f;
	viewports[0].width    = (float)frameGraphConfig->GetViewportWidth();
	viewports[0].height   = (float)frameGraphConfig->GetViewportHeight();
	viewports[0].minDepth = 0.0f;
	viewports[0].maxDepth = 1.0f;

	scissors[0].offset.x      = 0;
	scissors[0].offset.y      = 0;
	scissors[0].extent.width  = frameGraphConfig->GetViewportWidth();
	scissors[0].extent.height = frameGraphConfig->GetViewportHeight();

	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo;
	pipelineViewportStateCreateInfo.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.pNext         = nullptr;
	pipelineViewportStateCreateInfo.flags         = 0;
	pipelineViewportStateCreateInfo.viewportCount = (uint32_t)(viewports.size());
	pipelineViewportStateCreateInfo.pViewports    = viewports.data();
	pipelineViewportStateCreateInfo.scissorCount  = (uint32_t)(scissors.size());
	pipelineViewportStateCreateInfo.pScissors     = scissors.data();


	//Rasterization state
	//TODO: Conservative rasterization
	//TODO: Rasterization order
	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo;
	pipelineRasterizationStateCreateInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineRasterizationStateCreateInfo.pNext                   = nullptr; 
	pipelineRasterizationStateCreateInfo.flags                   = 0;
	pipelineRasterizationStateCreateInfo.depthClampEnable        = false;
	pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = false;
	pipelineRasterizationStateCreateInfo.polygonMode             = VK_POLYGON_MODE_FILL;
	pipelineRasterizationStateCreateInfo.cullMode                = VK_CULL_MODE_BACK_BIT;
	pipelineRasterizationStateCreateInfo.frontFace               = VK_FRONT_FACE_CLOCKWISE;
	pipelineRasterizationStateCreateInfo.depthBiasEnable         = false;
	pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasClamp          = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor    = 0.0f;
	pipelineRasterizationStateCreateInfo.lineWidth               = 1.0f;


	//Multisample state
	//TODO: Coverage-to-color
	std::array sampleMasks = {0xffffffff};

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;
	pipelineMultisampleStateCreateInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.pNext                 = nullptr;
	pipelineMultisampleStateCreateInfo.flags                 = 0;
	pipelineMultisampleStateCreateInfo.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
	pipelineMultisampleStateCreateInfo.sampleShadingEnable   = VK_FALSE;
	pipelineMultisampleStateCreateInfo.minSampleShading      = 1.0f;
	pipelineMultisampleStateCreateInfo.pSampleMask           = sampleMasks.data();
	pipelineMultisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	pipelineMultisampleStateCreateInfo.alphaToOneEnable      = VK_FALSE;


	//Depth-stencil state
	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo;
	pipelineDepthStencilStateCreateInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencilStateCreateInfo.pNext                 = nullptr;
	pipelineDepthStencilStateCreateInfo.flags                 = 0;
	pipelineDepthStencilStateCreateInfo.depthTestEnable       = VK_TRUE;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable      = VK_TRUE;
	pipelineDepthStencilStateCreateInfo.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	pipelineDepthStencilStateCreateInfo.stencilTestEnable     = VK_FALSE;
	pipelineDepthStencilStateCreateInfo.front.failOp          = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.front.passOp          = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.front.depthFailOp     = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.front.compareOp       = VK_COMPARE_OP_ALWAYS;
	pipelineDepthStencilStateCreateInfo.front.compareMask     = 0xffffffff;
	pipelineDepthStencilStateCreateInfo.front.writeMask       = 0xffffffff;
	pipelineDepthStencilStateCreateInfo.front.reference       = 0;
	pipelineDepthStencilStateCreateInfo.back.failOp           = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.back.passOp           = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.back.depthFailOp      = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.back.compareOp        = VK_COMPARE_OP_ALWAYS;
	pipelineDepthStencilStateCreateInfo.back.compareMask      = 0xffffffff;
	pipelineDepthStencilStateCreateInfo.back.writeMask        = 0xffffffff;
	pipelineDepthStencilStateCreateInfo.back.reference        = 0;
	pipelineDepthStencilStateCreateInfo.minDepthBounds        = 0.0f;
	pipelineDepthStencilStateCreateInfo.maxDepthBounds        = 1.0f;


	//Blend state
	//TODO: Advanced blend operations
	std::array<VkPipelineColorBlendAttachmentState, 1> blendAttachments;
	blendAttachments[0].blendEnable         = VK_FALSE;
	blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachments[0].colorBlendOp        = VK_BLEND_OP_ADD;
	blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachments[0].alphaBlendOp        = VK_BLEND_OP_ADD;
	blendAttachments[0].colorWriteMask      = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

	VkPipelineColorBlendStateCreateInfo pipelineBlendStateCreateInfo;
	pipelineBlendStateCreateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineBlendStateCreateInfo.pNext             = nullptr;
	pipelineBlendStateCreateInfo.flags             = 0;
	pipelineBlendStateCreateInfo.logicOpEnable     = VK_FALSE;
	pipelineBlendStateCreateInfo.logicOp           = VK_LOGIC_OP_NO_OP;
	pipelineBlendStateCreateInfo.attachmentCount   = (uint32_t)(blendAttachments.size());
	pipelineBlendStateCreateInfo.pAttachments      = blendAttachments.data();
	pipelineBlendStateCreateInfo.blendConstants[0] = 1.0f;
	pipelineBlendStateCreateInfo.blendConstants[1] = 1.0f;
	pipelineBlendStateCreateInfo.blendConstants[2] = 1.0f;
	pipelineBlendStateCreateInfo.blendConstants[3] = 1.0f;


	//Dynamic state
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo;
	pipelineDynamicStateCreateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.pNext             = nullptr;
	pipelineDynamicStateCreateInfo.flags             = 0;
	pipelineDynamicStateCreateInfo.dynamicStateCount = 0;
	pipelineDynamicStateCreateInfo.pDynamicStates    = nullptr;


	//TODO: Pipeline creation feedback
	//TODO: Possibly discard rectangles?
	//TODO: Fragment shading rate?
	//TODO: Shader groups (device generated commands)
	//TODO: mGPU
	VkGraphicsPipelineCreateInfo gbufferPipelineCreateInfo;
	gbufferPipelineCreateInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gbufferPipelineCreateInfo.pNext               = nullptr;
	gbufferPipelineCreateInfo.flags               = 0;
	gbufferPipelineCreateInfo.stageCount          = (uint32_t)(pipelineShaderStageCreateInfos.size());
	gbufferPipelineCreateInfo.pStages             = pipelineShaderStageCreateInfos.data();
	gbufferPipelineCreateInfo.pVertexInputState   = &pipelineVertexInputStateCreateInfo;
	gbufferPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
	gbufferPipelineCreateInfo.pTessellationState  = nullptr;
	gbufferPipelineCreateInfo.pViewportState      = &pipelineViewportStateCreateInfo;
	gbufferPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
	gbufferPipelineCreateInfo.pMultisampleState   =	&pipelineMultisampleStateCreateInfo;
	gbufferPipelineCreateInfo.pDepthStencilState  = &pipelineDepthStencilStateCreateInfo;
	gbufferPipelineCreateInfo.pColorBlendState    = &pipelineBlendStateCreateInfo;
	gbufferPipelineCreateInfo.pDynamicState       = &pipelineDynamicStateCreateInfo;
	gbufferPipelineCreateInfo.layout              = pipelineLayout;
	gbufferPipelineCreateInfo.renderPass          = mRenderPass;
	gbufferPipelineCreateInfo.subpass             = 0;
	gbufferPipelineCreateInfo.basePipelineHandle  = nullptr;
	gbufferPipelineCreateInfo.basePipelineIndex   = 0;

	std::array graphicsPipelineCreateInfos = {gbufferPipelineCreateInfo};
	ThrowIfFailed(vkCreateGraphicsPipelines(mDeviceRef, VK_NULL_HANDLE, (uint32_t)graphicsPipelineCreateInfos.size(), graphicsPipelineCreateInfos.data(), nullptr, outPipeline));

	for(size_t i = 0; i < shaderModules.size(); i++)
	{
		SafeDestroyObject(vkDestroyShaderModule, mDeviceRef, shaderModules[i]);
	}
}
