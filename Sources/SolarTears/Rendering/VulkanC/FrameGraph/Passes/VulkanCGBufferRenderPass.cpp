#include "VulkanCGBufferRenderPass.hpp"
#include "../../../../../3rd party/VulkanGenericStructures/Include/VulkanGenericStructures.h"
#include "../../VulkanCFunctions.hpp"
#include "../../VulkanCDeviceParameters.hpp"
#include "../../VulkanCShaders.hpp"
#include "../../Scene/VulkanCSceneBuilder.hpp"
#include "../VulkanCFrameGraph.hpp"
#include "../VulkanCFrameGraphBuilder.hpp"
#include "../VulkanCFrameGraphConfig.hpp"
#include <array>

VulkanCBindings::GBufferPass::GBufferPass(VkDevice device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName): RenderPass(device)
{
	mRenderPass  = VK_NULL_HANDLE;
	mFramebuffer = VK_NULL_HANDLE;

	mPipelineLayout = VK_NULL_HANDLE;
	mPipeline       = VK_NULL_HANDLE;

	//TODO: subpasses (AND SECONDARY COMMAND BUFFERS)
	CreateRenderPass(frameGraphBuilder, frameGraphBuilder->GetDeviceParameters(), passName);
	CreateFramebuffer(frameGraphBuilder, frameGraphBuilder->GetConfig(), passName);
	CreatePipelineLayout(frameGraphBuilder->GetScene());
	CreatePipeline(frameGraphBuilder->GetShaderManager(), frameGraphBuilder->GetConfig());
}

VulkanCBindings::GBufferPass::~GBufferPass()
{
	SafeDestroyObject(vkDestroyPipeline, mDeviceRef, mPipeline);
	SafeDestroyObject(vkDestroyPipelineLayout, mDeviceRef, mPipelineLayout);

	SafeDestroyObject(vkDestroyRenderPass, mDeviceRef, mRenderPass);
	SafeDestroyObject(vkDestroyFramebuffer, mDeviceRef, mFramebuffer);
}

void VulkanCBindings::GBufferPass::Register(FrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	auto PassCreateFunc = [](VkDevice device, const FrameGraphBuilder* builder, const std::string& name) -> std::unique_ptr<RenderPass>
	{
		//Using new because make_unique can't access private constructor
		return std::unique_ptr<GBufferPass>(new GBufferPass(device, builder, name));
	};

	frameGraphBuilder->RegisterRenderPass(passName, PassCreateFunc, RenderPassType::GRAPHICS);

	frameGraphBuilder->RegisterWriteSubresource(passName, ColorBufferImageId);
	frameGraphBuilder->EnableSubresourceAutoBarrier(passName, ColorBufferImageId, true);
	frameGraphBuilder->SetPassSubresourceFormat(passName, ColorBufferImageId, VK_FORMAT_B8G8R8A8_UNORM); //TODO: maybe passing the format????
	frameGraphBuilder->SetPassSubresourceAspectFlags(passName, ColorBufferImageId, VK_IMAGE_ASPECT_COLOR_BIT);
	frameGraphBuilder->SetPassSubresourceStageFlags(passName, ColorBufferImageId, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	frameGraphBuilder->SetPassSubresourceLayout(passName, ColorBufferImageId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	frameGraphBuilder->SetPassSubresourceUsage(passName, ColorBufferImageId, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
}

void VulkanCBindings::GBufferPass::RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig)
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

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

	scene->DrawObjectsOntoGBuffer(commandBuffer, mPipelineLayout);

	vkCmdEndRenderPass(commandBuffer);
}

void VulkanCBindings::GBufferPass::CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, const DeviceParameters* deviceParameters, const std::string& currentPassName)
{
	//TODO: may alias?
	//TODO: multisampling?
	std::array<VkAttachmentDescription, 1> attachments;
	attachments[0].flags          = 0;
	attachments[0].format         = frameGraphBuilder->GetRegisteredSubresourceFormat(currentPassName, ColorBufferImageId);
	attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout  = frameGraphBuilder->GetPreviousPassSubresourceLayout(currentPassName, ColorBufferImageId);
	attachments[0].finalLayout    = frameGraphBuilder->GetNextPassSubresourceLayout(currentPassName,     ColorBufferImageId);

	VkAttachmentReference colorBufferAttachment;
	colorBufferAttachment.attachment = 0;
	colorBufferAttachment.layout     = frameGraphBuilder->GetRegisteredSubresourceLayout(currentPassName, ColorBufferImageId);

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
	dependencies[0].srcStageMask    = frameGraphBuilder->GetPreviousPassSubresourceStageFlags(currentPassName, ColorBufferImageId);
	dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask   = frameGraphBuilder->GetPreviousPassSubresourceAccessFlags(currentPassName, ColorBufferImageId);
	dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	dependencies[1].srcSubpass      = 0;
	dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask    = frameGraphBuilder->GetNextPassSubresourceStageFlags(currentPassName, ColorBufferImageId);
	dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask   = frameGraphBuilder->GetNextPassSubresourceAccessFlags(currentPassName, ColorBufferImageId);
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

void VulkanCBindings::GBufferPass::CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, const FrameGraphConfig* frameGraphConfig, const std::string& currentPassName)
{
	vgs::GenericStructureChain<VkFramebufferCreateInfo> framebufferCreateInfoChain;

	std::array attachmentIds = {ColorBufferImageId};

	std::array<VkImageView, attachmentIds.size()> attachments;
	for(size_t i = 0; i < attachments.size(); i++)
	{
		attachments[i] = frameGraphBuilder->GetRegisteredSubresource(currentPassName, attachmentIds[i]);
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

void VulkanCBindings::GBufferPass::CreatePipelineLayout(const RenderableScene* sceneBaked1stPart)
{
	std::array descriptorSetLayouts = {sceneBaked1stPart->GetGBufferTexturesDescriptorSetLayout(), sceneBaked1stPart->GetGBufferUniformsDescriptorSetLayout()};

	VkPipelineLayoutCreateInfo gbufferPipelineLayoutCreateInfo;
	gbufferPipelineLayoutCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	gbufferPipelineLayoutCreateInfo.pNext                  = nullptr;
	gbufferPipelineLayoutCreateInfo.flags                  = 0;
	gbufferPipelineLayoutCreateInfo.setLayoutCount         = (uint32_t)(descriptorSetLayouts.size());
	gbufferPipelineLayoutCreateInfo.pSetLayouts            = descriptorSetLayouts.data();
	gbufferPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	gbufferPipelineLayoutCreateInfo.pPushConstantRanges    = nullptr;

	ThrowIfFailed(vkCreatePipelineLayout(mDeviceRef, &gbufferPipelineLayoutCreateInfo, nullptr, &mPipelineLayout));
}

void VulkanCBindings::GBufferPass::CreatePipeline(const ShaderManager* shaderManager, const FrameGraphConfig* frameGraphConfig)
{
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo;
	vertexShaderModuleCreateInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext    = nullptr;
	vertexShaderModuleCreateInfo.flags    = 0;
	vertexShaderModuleCreateInfo.codeSize = shaderManager->GetGBufferVertexShaderSize();
	vertexShaderModuleCreateInfo.pCode    = shaderManager->GetGBufferVertexShaderData();

	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo;
	fragmentShaderModuleCreateInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext    = nullptr;
	fragmentShaderModuleCreateInfo.flags    = 0;
	fragmentShaderModuleCreateInfo.codeSize = shaderManager->GetGBufferFragmentShaderSize();
	fragmentShaderModuleCreateInfo.pCode    = shaderManager->GetGBufferFragmentShaderData();

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
	vertexInputBindingDescriptions[0].stride    = (uint32_t)RenderableSceneBuilder::GetVertexSize();
	vertexInputBindingDescriptions[0].binding   = 0;
	vertexInputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	//TODO: Shader reflection to get the location?
	std::array<VkVertexInputAttributeDescription, 3> vertexAttributeDescriptions;
	vertexAttributeDescriptions[0].binding  = 0;
	vertexAttributeDescriptions[0].format   = RenderableSceneBuilder::GetVertexPositionFormat();
	vertexAttributeDescriptions[0].location = 0;
	vertexAttributeDescriptions[0].offset   = RenderableSceneBuilder::GetVertexPositionOffset();
	vertexAttributeDescriptions[1].binding  = 0;
	vertexAttributeDescriptions[1].format   = RenderableSceneBuilder::GetVertexNormalFormat();
	vertexAttributeDescriptions[1].location = 1;
	vertexAttributeDescriptions[1].offset   = RenderableSceneBuilder::GetVertexNormalOffset();
	vertexAttributeDescriptions[2].binding  = 0;
	vertexAttributeDescriptions[2].format   = RenderableSceneBuilder::GetVertexTexcoordFormat();
	vertexAttributeDescriptions[2].location = 2;
	vertexAttributeDescriptions[2].offset   = RenderableSceneBuilder::GetVertexTexcoordOffset();

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
	gbufferPipelineCreateInfo.layout              = mPipelineLayout;
	gbufferPipelineCreateInfo.renderPass          = mRenderPass;
	gbufferPipelineCreateInfo.subpass             = 0;
	gbufferPipelineCreateInfo.basePipelineHandle  = nullptr;
	gbufferPipelineCreateInfo.basePipelineIndex   = 0;

	std::array graphicsPipelineCreateInfos = {gbufferPipelineCreateInfo};
	ThrowIfFailed(vkCreateGraphicsPipelines(mDeviceRef, VK_NULL_HANDLE, (uint32_t)graphicsPipelineCreateInfos.size(), graphicsPipelineCreateInfos.data(), nullptr, &mPipeline));

	for(size_t i = 0; i < shaderModules.size(); i++)
	{
		SafeDestroyObject(vkDestroyShaderModule, mDeviceRef, shaderModules[i]);
	}
}
