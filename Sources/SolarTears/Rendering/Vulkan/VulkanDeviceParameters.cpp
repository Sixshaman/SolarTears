#include "VulkanCDeviceParameters.hpp"
#include <VulkanGenericStructures.h>
#include "VulkanCFunctions.hpp"
#include "VulkanCUtils.hpp"
#include <unordered_set>
#include <cassert>

namespace VulkanCBindings
{
	static const std::vector<std::string> global_config_device_extensions =
	{
		VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME,           //Advanced blend operations!
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,              //Reading buffer memory in shaders!
		VK_NV_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME,          //Compute shader derivatives! I think I'll only need quad ones
		VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME,              //Conditional rendering!	
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,                //Only gonna use sampled image indexing... If I ever use other descriptor indexing,  just hit me with a stick
		VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME,           //Device generated commands,  how could I forget about them?	
		VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME,               //Fragment density map! It's almost like variable rate shading
		VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,         //Barycentrics look cool
		VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,          //Prooobably will gonna use only pixel interlock :D Idk really,  all of them seem useful in different situations,  though other two seem less frequently occuring 
		VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,                   //Resetting queries from host. What if I change my mind on a query?
		VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,                       //I'm gonna need both image and buffer robust accesses
		VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME,              //Imageless framebuffers! Oh yeah!
		VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME,	              //INLINE UNIFORM BUFFERS (I don't need update-after-bind)
		VK_NV_MESH_SHADER_EXTENSION_NAME,                         //Mesh shaders, yum!
		VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME,                  //Performance query... I don't know why,  I just felt I might need this later
		VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,                //I DON'T KNOW if I ever need float atomics,  but if I do, I only need them for shared memory and buffer memory
		VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,                //As with float atomics,  I need int64 ones for shared memory and buffer memory
		VK_KHR_SHADER_CLOCK_EXTENSION_NAME,                       //Shader clock will enable me some algorithms... Maybe :D
		VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME, //Ehhh... I thought it's useful,  because it can optimize discard
		VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,                //I only care about float16
		VK_NV_SHADER_IMAGE_FOOTPRINT_EXTENSION_NAME,              //Imagine what can I do with shader image footprint!
		VK_INTEL_SHADER_INTEGER_FUNCTIONS_2_EXTENSION_NAME,       //More integer functions! MOOOOOREEEEE!
		VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME,     //Better subgroup operations won't hurt
		VK_NV_SHADING_RATE_IMAGE_EXTENSION_NAME,                  //VRS. It's cool, it's cool, ESPECIALLY with shading rate image
		VK_EXT_TEXEL_BUFFER_ALIGNMENT_EXTENSION_NAME,             //Haha. I can be much better with better understanding of device texel buffer alignment
		VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME,     //Std-430 in uniform buffers, yay!
		VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,                //Vulkan memory model, because everything is better when it's consistent

		//VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,            //Pipeline libraries! Currently provisional
		VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,           //Automatic draw indirect commands!
		VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,             //This is very important for performance!!!
		VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME,           //Dirty rects for presentation!
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,               //Inline push descriptors!
		VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,         //Controlling float behavior in shafers! Not sure when, but I'll need this!
		VK_KHR_SPIRV_1_4_EXTENSION_NAME,                     //SPIR-V 1.4!
		VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,    //Conservative rasterization! It's always supported, so I don't have to add flag check to extensionEnableFlags
		VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME,            //Discard rectangles!
		VK_EXT_FILTER_CUBIC_EXTENSION_NAME,                  //CUBIC image filtering!
		VK_EXT_HDR_METADATA_EXTENSION_NAME,                  //AYCH DEE ER
		VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,                 //Available memory querying
		VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME,    //Pipeline creation feedback
		VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME,         //Shader stencil
		VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME,            //NATIVE HDR YAY
		VK_AMD_GCN_SHADER_EXTENSION_NAME,                    //Cube map lookup info!
		VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME,           //Relaxed rasterization order
		VK_AMD_SHADER_BALLOT_EXTENSION_NAME,                 //Some cool shader operations
		VK_AMD_SHADER_TRINARY_MINMAX_EXTENSION_NAME,         //TRinary operations!
		VK_IMG_FILTER_CUBIC_EXTENSION_NAME,                  //Another cubic filtering?
		VK_NV_CLIP_SPACE_W_SCALING_EXTENSION_NAME,           //VR distortion correction!
		VK_NV_FRAGMENT_COVERAGE_TO_COLOR_EXTENSION_NAME,     //Fragment coverage output!
		VK_NV_GEOMETRY_SHADER_PASSTHROUGH_EXTENSION_NAME,    //Passthrough GS optimization!
		VK_NV_RAY_TRACING_EXTENSION_NAME,                    //Raytracing!
		VK_NV_SHADER_SUBGROUP_PARTITIONED_EXTENSION_NAME,    //Subgroup partition operations!
		VK_NV_VIEWPORT_ARRAY2_EXTENSION_NAME,                //Primitive broadcasting to MULTIPLE VIEWPORTS
		VK_NV_VIEWPORT_SWIZZLE_EXTENSION_NAME,               //Viewport swizzles!
		VK_NVX_MULTIVIEW_PER_VIEW_ATTRIBUTES_EXTENSION_NAME, //Better VR support!
		VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,           //Extended renderpasses!
		VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,   //Viewport and layer indices!
		VK_EXT_4444_FORMATS_EXTENSION_NAME,                  //R4G4B4A4 formats!

		VK_KHR_SWAPCHAIN_EXTENSION_NAME,             //Swapchains are the most important
		VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME, //Fullscreen-exclusive swapchains!
	};


	//I want: mGPU, some of 1.1 features, advanced blend operations, probably extended address features, compute shader derivatives, conditional rendering, descriptor indexing, device-generated commands,
	//fragment shader barycentrics, interlocks, resetting queries from host, robustness, imageless framebuffers, inline uniform blocks, mesh shaders, performance queries,
	//scalar block alignment, atomic floats, atomic int64, shader clock, demoting to helper invocations, 16-bit floats and 8-bit ints in shaders, image footprints in shaders,
	//variable rate shading, advanced texel buffer alignment, std-430 uniform buffers, Vulkan memory model

	//ADD RAYTRACING TOO WHEN VK_KHR_ray_tracing STOPS BEING PROVISIONAL
	static const std::unordered_map<std::string, VkStructureType> global_config_device_extension_structure_types =
	{
		{
			VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT
		},

		{
			VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
		},

		{
			VK_NV_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV
		},

		{
			VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT
		},

		{
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES
		},

		{
			VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV
		},

		{
			VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT
		},

		{
			VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV
		},

		{
			VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT
		},

		{
			VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES
		},

		{
			VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT
		},

		{
			VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES
		},

		{
			VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT
		},

		{
			VK_NV_MESH_SHADER_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV
		},

		{
			VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR
		},

		{
			VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT
		},

		{
			VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES
		},

		{
			VK_KHR_SHADER_CLOCK_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR
		},

		{
			VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT
		},

		{
			VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES
		},

		{
			VK_NV_SHADER_IMAGE_FOOTPRINT_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV
		},

		{
			VK_INTEL_SHADER_INTEGER_FUNCTIONS_2_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL
		},

		{
			VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES
		},

		{
			VK_NV_SHADING_RATE_IMAGE_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV
		},

		{
			VK_EXT_TEXEL_BUFFER_ALIGNMENT_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT
		},

		{
			VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES
		},

		{
			VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES
		},

		{
			VK_EXT_4444_FORMATS_EXTENSION_NAME,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT
		}
	};
}

const std::unordered_map<std::string, size_t> VulkanCBindings::DeviceParameters::global_config_device_extension_flags_offsets =
{
	{
		VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsBlendOperationAdvancedFeaturesEXTExtensionPresent)
	},

	{
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsBufferDeviceAddressFeaturesExtensionPresent)
	},

	{
		VK_NV_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsComputeShaderDerivativesFeaturesNVExtensionPresent)
	},

	{
		VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsConditionalRenderingFeaturesEXTExtensionPresent)
	},

	{
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsDescriptorIndexingFeaturesExtensionPresent)
	},

	{
		VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsDeviceGeneratedCommandsFeaturesNVExtensionPresent)
	},

	{
		VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsFragmentDensityMap2FeaturesEXTExtensionPresent)
	},

	{
		VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsFragmentShaderBarycentricFeaturesNVExtensionPresent)
	},

	{
		VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsFragmentShaderInterlockFeaturesEXTExtensionPresent)
	},

	{
		VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsHostQueryResetFeaturesExtensionPresent)
	},

	{
		VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsRobustness2FeaturesEXTExtensionPresent)
	},

	{
		VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsImagelessFramebufferFeaturesExtensionPresent)
	},

	{
		VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsInlineUniformBlockFeaturesEXTExtensionPresent)
	},

	{
		VK_NV_MESH_SHADER_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsMeshShaderFeaturesNVExtensionPresent)
	},

	{
		VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsPerformanceQueryFeaturesKHRExtensionPresent)
	},

	{
		VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderAtomicFloatFeaturesEXTExtensionPresent)
	},

	{
		VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderAtomicInt64FeaturesExtensionPresent)
	},

	{
		VK_KHR_SHADER_CLOCK_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderClockFeaturesKHRExtensionPresent)
	},

	{
		VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderDemoteToHelperInvocationFeaturesEXTExtensionPresent)
	},

	{
		VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderFloat16Int8FeaturesExtensionPresent)
	},

	{
		VK_NV_SHADER_IMAGE_FOOTPRINT_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderImageFootprintFeaturesNVExtensionPresent)
	},

	{
		VK_INTEL_SHADER_INTEGER_FUNCTIONS_2_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderIntegerFunctions2FeaturesINTELExtensionPresent)
	},

	{
		VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderSubgroupExtendedTypesFeaturesExtensionPresent)
	},

	{
		VK_NV_SHADING_RATE_IMAGE_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShadingRateImageFeaturesNVExtensionPresent)
	},

	{
		VK_EXT_TEXEL_BUFFER_ALIGNMENT_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsTexelBufferAlignmentFeaturesEXTExtensionPresent)
	},

	{
		VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsUniformBufferStandardLayoutFeaturesExtensionPresent)
	},

	{
		VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsVulkanMemoryModelFeaturesExtensionPresent)
	},

	{
		VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsDrawIndirectCountExtensionPresent)
	},

	{
		VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsImageFormatListExtensionPresent)
	},

	{
		VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsIncrementalPresentExtensionPresent)
	},

	{
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsPushDescriptorExtensionPresent)
	},

	{
		VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderFloatControlsExtensionPresent)
	},

	{
		VK_KHR_SPIRV_1_4_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsSpirV14ExtensionPresent)
	},

	{
		VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsConservativeRasterizationExtensionPresent)
	},

	{
		VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsDiscardRectanglesExtensionPresent)
	},

	{
		VK_EXT_FILTER_CUBIC_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsFilterCubicExtensionPresent)
	},

	{
		VK_EXT_HDR_METADATA_EXTENSION_NAME,
offsetof(DeviceExtensionFlags, IsHDRMetadataExtensionPresent)
	},

	{
		VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsMemoryBudgetExtensionPresent)
	},

	{
		VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsPipelineCreationFeedbackExtensionPresent)
	},

	{
		VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderStencilExportExtensionPresent)
	},

	{
		VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsDisplayNativeHDRExtensionPresent)
	},

	{
		VK_AMD_GCN_SHADER_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsGCNShaderExtensionPresent)
	},

	{
		VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsRasterizationOrderExtensionPresent)
	},

	{
		VK_AMD_SHADER_BALLOT_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderBallotExtensionPresent)
	},

	{
		VK_AMD_SHADER_TRINARY_MINMAX_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderTrinaryMinmaxExtensionPresent)
	},

	{
		VK_IMG_FILTER_CUBIC_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsImageFilterCubicExtensionPresent)
	},

	{
		VK_NV_CLIP_SPACE_W_SCALING_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsClipspaceWScalingExtensionPresent)
	},

	{
		VK_NV_FRAGMENT_COVERAGE_TO_COLOR_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsFragmentCoverageToColorExtensionPresent)
	},

	{
		VK_NV_GEOMETRY_SHADER_PASSTHROUGH_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsGeometryShaderPassthroughExtensionPresent)
	},

	{
		VK_NV_RAY_TRACING_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsRaytracingExtensionPresent)
	},

	{
		VK_NV_SHADER_SUBGROUP_PARTITIONED_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderSubgroupPartitionedExtensionPresent)
	},

	{
		VK_NV_VIEWPORT_ARRAY2_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsViewportArray2ExtensionPresent)
	},

	{
		VK_NV_VIEWPORT_SWIZZLE_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsViewportSwizzleExtensionPresent)
	},

	{
		VK_NVX_MULTIVIEW_PER_VIEW_ATTRIBUTES_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsMultiviewPerViewAttributesExtensionPresent)
	},

	{
		VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsCreateRenderpass2ExtensionPresent)
	},

	{
		VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsShaderViewportIndexLayerExtensionPresent)
	},

	{
		VK_EXT_4444_FORMATS_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, Is4444FormatsExtensionPresent)
	},

	{
		VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,
		offsetof(DeviceExtensionFlags, IsFullscreenExclusiveExtensionPresent)
	}
};

VulkanCBindings::DeviceParameters::DeviceParameters(LoggerQueue* loggingBoard) : mLoggingBoard(loggingBoard)
{
	assert(global_config_device_extension_flags_offsets.size() * sizeof(bool) == sizeof(DeviceExtensionFlags));

	memset(&mEnabledExtensionFlags, 0, sizeof(DeviceExtensionFlags));
}

VulkanCBindings::DeviceParameters::~DeviceParameters()
{
}

void VulkanCBindings::DeviceParameters::InvalidateDeviceParameters(const VkPhysicalDevice& physicalDevice)
{
	InvalidateDeviceFeatures(physicalDevice);
	InvalidateDeviceProperties(physicalDevice);
	InvalidateDeviceMemoryProperties(physicalDevice);
}

void VulkanCBindings::DeviceParameters::InvalidateDeviceExtensions(const VkPhysicalDevice& physicalDevice, std::vector<std::string>& outEnabledExtensions, vgs::StructureChainBlob<VkDeviceCreateInfo>& outStructureChain)
{
	outStructureChain.AppendToChain(mFeatures);

	std::vector<vgs::GenericStruct> deviceFeatureStructures;
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mBlendOperationAdvancedFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mBufferDeviceAddressFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mComputeShaderDerivativesFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mConditionalRenderingFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mDescriptorIndexingFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mDeviceGeneratedCommandsFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mFragmentDensityMapFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mFragmentShaderBarycentricFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mFragmentShaderInterlockFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mHostQueryResetFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mRobustness2Features));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mImagelessFramebufferFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mInlineUniformBlockFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mMeshShaderFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mPerformanceQueryFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mShaderAtomicFloatFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mShaderAtomicInt64Features));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mShaderClockFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mDemoteToHelperInvocationFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mShaderFloat16Int8Features));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mShaderImageFootprintFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mShaderIntegerFunctions2Features));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mShaderSubgroupExtendedTypeFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mShadingRateImageFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mTexelBufferAlignmentFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mUniformBufferStandardLayoutFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mVulkan4444FormatFeatures));
	deviceFeatureStructures.push_back(vgs::TransmuteTypeToSType(mVulkanMemoryModelFeatures));

	std::unordered_map<VkStructureType, size_t> featureStructureIndices;
	for (size_t i = 0; i < deviceFeatureStructures.size(); i++)
	{
		featureStructureIndices[deviceFeatureStructures[i].GetSType()] = i;
	}

	std::vector<VkExtensionProperties> availableDeviceExtensions;

	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

	availableDeviceExtensions.resize(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableDeviceExtensions.data());

	std::unordered_set<std::string> availableDeviceExtensionNames;
	for (const VkExtensionProperties& extension : availableDeviceExtensions)
	{
		availableDeviceExtensionNames.insert(extension.extensionName);
	}

	//First extensions in requiredExtensions are enabled by flags
	for (size_t i = 0; i < global_config_device_extensions.size(); i++)
	{
		const std::string& extension = global_config_device_extensions[i];
		if (availableDeviceExtensionNames.contains(extension))
		{
			outEnabledExtensions.push_back(extension);

			auto extensionStructureTypeIt = global_config_device_extension_structure_types.find(extension);
			if (extensionStructureTypeIt != global_config_device_extension_structure_types.end())
			{
				auto featureStructureIndexIt = featureStructureIndices.find(extensionStructureTypeIt->second);
				if (featureStructureIndexIt != featureStructureIndices.end())
				{
					outStructureChain.AppendToChainGeneric(deviceFeatureStructures[featureStructureIndexIt->second]);
				}
			}

			auto offsetIt = global_config_device_extension_flags_offsets.find(extension);
			if (offsetIt != global_config_device_extension_flags_offsets.end())
			{
				assert(offsetIt->second + sizeof(bool) <= sizeof(DeviceExtensionFlags));

				bool* extFlagPtr = (bool*)(((std::byte*) & mEnabledExtensionFlags) + offsetIt->second);
				*extFlagPtr = true;
			}
		}
		else
		{
			mLoggingBoard->PostLogMessage("Vulkan device initialization: extension " + extension + " is not supported");
		}
	}
}

bool VulkanCBindings::DeviceParameters::IsBlendOperationAdvancedFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsBlendOperationAdvancedFeaturesEXTExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsBufferDeviceAddressFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsBufferDeviceAddressFeaturesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsComputeShaderDerivativesFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsComputeShaderDerivativesFeaturesNVExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsConditionalRenderingFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsConditionalRenderingFeaturesEXTExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsDescriptorIndexingFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsDescriptorIndexingFeaturesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsDeviceGeneratedCommandsFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsDeviceGeneratedCommandsFeaturesNVExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsFragmentDensityMap2FeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsFragmentDensityMap2FeaturesEXTExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsFragmentShaderBarycentricFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsFragmentShaderBarycentricFeaturesNVExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsFragmentShaderInterlockFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsFragmentShaderInterlockFeaturesEXTExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsHostQueryResetFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsHostQueryResetFeaturesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsRobustness2FeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsRobustness2FeaturesEXTExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsImagelessFramebufferFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsImagelessFramebufferFeaturesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsInlineUniformBlockFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsInlineUniformBlockFeaturesEXTExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsMeshShaderFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsMeshShaderFeaturesNVExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsPerformanceQueryFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsPerformanceQueryFeaturesKHRExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderAtomicFloatFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderAtomicFloatFeaturesEXTExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderAtomicInt64FeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderAtomicInt64FeaturesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderClockFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderClockFeaturesKHRExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderDemoteToHelperInvocationFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderDemoteToHelperInvocationFeaturesEXTExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderFloat16Int8FeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderFloat16Int8FeaturesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderImageFootprintFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderImageFootprintFeaturesNVExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderIntegerFunctions2FeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderIntegerFunctions2FeaturesINTELExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderSubgroupExtendedTypesFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderSubgroupExtendedTypesFeaturesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShadingRateImageFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShadingRateImageFeaturesNVExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsTexelBufferAlignmentFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsTexelBufferAlignmentFeaturesEXTExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsUniformBufferStandardLayoutFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsUniformBufferStandardLayoutFeaturesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsVulkanMemoryModelFeaturesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsVulkanMemoryModelFeaturesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsDrawIndirectCountExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsDrawIndirectCountExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsImageFormatListExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsImageFormatListExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsIncrementalPresentExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsIncrementalPresentExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsPushDescriptorExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsPushDescriptorExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderFloatControlsExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderFloatControlsExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsSpirV14ExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsSpirV14ExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsConservativeRasterizationExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsConservativeRasterizationExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsDiscardRectanglesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsDiscardRectanglesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsFilterCubicExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsFilterCubicExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsHDRMetadataExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsHDRMetadataExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsMemoryBudgetExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsMemoryBudgetExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsPipelineCreationFeedbackExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsPipelineCreationFeedbackExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderStencilExportExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderStencilExportExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsDisplayNativeHDRExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsDisplayNativeHDRExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsGCNShaderExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsGCNShaderExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsRasterizationOrderExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsRasterizationOrderExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderBallotExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderBallotExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderTrinaryMinmaxExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderTrinaryMinmaxExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsImageFilterCubicExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsImageFilterCubicExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsClipspaceWScalingExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsClipspaceWScalingExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsFragmentCoverageToColorExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsFragmentCoverageToColorExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsGeometryShaderPassthroughExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsGeometryShaderPassthroughExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsRaytracingExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsRaytracingExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderSubgroupPartitionedExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderSubgroupPartitionedExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsViewportArray2ExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsViewportArray2ExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsViewportSwizzleExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsViewportSwizzleExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsMultiviewPerViewAttributesExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsMultiviewPerViewAttributesExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsCreateRenderpass2ExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsCreateRenderpass2ExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsShaderViewportIndexLayerExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsShaderViewportIndexLayerExtensionPresent;
}

bool VulkanCBindings::DeviceParameters::IsFullscreenExclusiveExtensionEnabled() const
{
	return mEnabledExtensionFlags.IsFullscreenExclusiveExtensionPresent;
}

const VkPhysicalDeviceProperties& VulkanCBindings::DeviceParameters::GetDeviceProperties() const
{
	return mProperties.properties;
}

void VulkanCBindings::DeviceParameters::InvalidateDeviceFeatures(VkPhysicalDevice physicalDevice)
{
	vgs::GenericStructureChain<VkPhysicalDeviceFeatures2> physicalDeviceFeaturesChain;
	physicalDeviceFeaturesChain.AppendToChain(mBlendOperationAdvancedFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mBufferDeviceAddressFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mComputeShaderDerivativesFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mConditionalRenderingFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mDescriptorIndexingFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mDeviceGeneratedCommandsFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mFragmentDensityMapFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mFragmentShaderBarycentricFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mFragmentShaderInterlockFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mHostQueryResetFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mRobustness2Features);
	physicalDeviceFeaturesChain.AppendToChain(mImagelessFramebufferFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mInlineUniformBlockFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mMeshShaderFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mPerformanceQueryFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mShaderAtomicFloatFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mShaderAtomicInt64Features);
	physicalDeviceFeaturesChain.AppendToChain(mShaderClockFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mDemoteToHelperInvocationFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mShaderFloat16Int8Features);
	physicalDeviceFeaturesChain.AppendToChain(mShaderImageFootprintFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mShaderIntegerFunctions2Features);
	physicalDeviceFeaturesChain.AppendToChain(mShaderSubgroupExtendedTypeFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mShadingRateImageFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mTexelBufferAlignmentFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mUniformBufferStandardLayoutFeatures);
	physicalDeviceFeaturesChain.AppendToChain(mVulkanMemoryModelFeatures);

	vkGetPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeaturesChain.GetChainHead());

	mFeatures = physicalDeviceFeaturesChain.GetChainHead();
}

void VulkanCBindings::DeviceParameters::InvalidateDeviceProperties(VkPhysicalDevice physicalDevice)
{
	//And here I want: VR, performance query, push descriptors, advanced blending, conservative rasterization, discard rectangles, fragment density map,
	//inline uniform blocks, robustness, texel buffer alignment, device generated commands, mesh shader, raytracing, shading rate image, more VR,
	//subgroup features, float controls, dynamic descriptor indexing

	vgs::GenericStructureChain<VkPhysicalDeviceProperties2> physicalDevicePropertiesChain;
	physicalDevicePropertiesChain.AppendToChain(mMultiviewProperties);
	physicalDevicePropertiesChain.AppendToChain(mPerformanceQueryProperties);
	physicalDevicePropertiesChain.AppendToChain(mPushDescriptorProperties);
	physicalDevicePropertiesChain.AppendToChain(mAdvancedBlendOperationsProperties);
	physicalDevicePropertiesChain.AppendToChain(mConservativeRasterizationProperties);
	physicalDevicePropertiesChain.AppendToChain(mDiscardRectangleProperties);
	physicalDevicePropertiesChain.AppendToChain(mFragmentDESTINYMapProperties);
	physicalDevicePropertiesChain.AppendToChain(mInlineUniformBlockProperties);
	physicalDevicePropertiesChain.AppendToChain(mRobustness2Properties);
	physicalDevicePropertiesChain.AppendToChain(mTexelBufferAlignmentProperties);
	physicalDevicePropertiesChain.AppendToChain(mDeviceGeneratedCommandProperties);
	physicalDevicePropertiesChain.AppendToChain(mMeshShaderProperties);
	physicalDevicePropertiesChain.AppendToChain(mRaytracingProperties);
	physicalDevicePropertiesChain.AppendToChain(mShadingRateImageProperties);
	physicalDevicePropertiesChain.AppendToChain(mMultiviewPerViewProperties);
	physicalDevicePropertiesChain.AppendToChain(mDeviceSubgroupProperties);
	physicalDevicePropertiesChain.AppendToChain(mDeviceFloatControlProperties);
	physicalDevicePropertiesChain.AppendToChain(mDeviceDescriptorIndexingProperties);

	vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDevicePropertiesChain.GetChainHead());

	mProperties = physicalDevicePropertiesChain.GetChainHead();
}

void VulkanCBindings::DeviceParameters::InvalidateDeviceMemoryProperties(VkPhysicalDevice physicalDevice)
{
	//And here there's only memory budget

	vgs::GenericStructureChain<VkPhysicalDeviceMemoryProperties2> physicalDeviceMemoryPropertiesChain;
	physicalDeviceMemoryPropertiesChain.AppendToChain(mMemoryBudgetProperties);

	vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &physicalDeviceMemoryPropertiesChain.GetChainHead());

	mMemoryProperties = physicalDeviceMemoryPropertiesChain.GetChainHead();
}