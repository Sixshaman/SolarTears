#pragma once

#include <vector>
#include <string>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include "..\..\Logging\LoggerQueue.hpp"

namespace vgs
{
	template<typename T> class StructureChainBlob;
}

namespace Vulkan
{
	class DeviceParameters
	{
		struct DeviceExtensionFlags
		{
			bool IsBlendOperationAdvancedFeaturesEXTExtensionPresent;
			bool IsBufferDeviceAddressFeaturesExtensionPresent;
			bool IsComputeShaderDerivativesFeaturesNVExtensionPresent;
			bool IsConditionalRenderingFeaturesEXTExtensionPresent;
			bool IsDescriptorIndexingFeaturesExtensionPresent;
			bool IsDeviceGeneratedCommandsFeaturesNVExtensionPresent;
			bool IsFragmentDensityMap2FeaturesEXTExtensionPresent;
			bool IsFragmentShaderBarycentricFeaturesNVExtensionPresent;
			bool IsFragmentShaderInterlockFeaturesEXTExtensionPresent;
			bool IsHostQueryResetFeaturesExtensionPresent;
			bool IsRobustness2FeaturesEXTExtensionPresent;
			bool IsImagelessFramebufferFeaturesExtensionPresent;
			bool IsInlineUniformBlockFeaturesEXTExtensionPresent;
			bool IsMeshShaderFeaturesNVExtensionPresent;
			bool IsPerformanceQueryFeaturesKHRExtensionPresent;
			bool IsShaderAtomicFloatFeaturesEXTExtensionPresent;
			bool IsShaderAtomicInt64FeaturesExtensionPresent;
			bool IsShaderClockFeaturesKHRExtensionPresent;
			bool IsShaderDemoteToHelperInvocationFeaturesEXTExtensionPresent;
			bool IsShaderFloat16Int8FeaturesExtensionPresent;
			bool IsShaderImageFootprintFeaturesNVExtensionPresent;
			bool IsShaderIntegerFunctions2FeaturesINTELExtensionPresent;
			bool IsShaderSubgroupExtendedTypesFeaturesExtensionPresent;
			bool IsShadingRateImageFeaturesNVExtensionPresent;
			bool IsTexelBufferAlignmentFeaturesEXTExtensionPresent;
			bool IsUniformBufferStandardLayoutFeaturesExtensionPresent;
			bool IsVulkanMemoryModelFeaturesExtensionPresent;

			bool IsDrawIndirectCountExtensionPresent;
			bool IsImageFormatListExtensionPresent;
			bool IsIncrementalPresentExtensionPresent;
			bool IsPushDescriptorExtensionPresent;
			bool IsShaderFloatControlsExtensionPresent;
			bool IsSpirV14ExtensionPresent;
			bool IsConservativeRasterizationExtensionPresent;
			bool IsDiscardRectanglesExtensionPresent;
			bool IsFilterCubicExtensionPresent;
			bool IsHDRMetadataExtensionPresent;
			bool IsMemoryBudgetExtensionPresent;
			bool IsPipelineCreationFeedbackExtensionPresent;
			bool IsShaderStencilExportExtensionPresent;
			bool IsDisplayNativeHDRExtensionPresent;
			bool IsGCNShaderExtensionPresent;
			bool IsRasterizationOrderExtensionPresent;
			bool IsShaderBallotExtensionPresent;
			bool IsShaderTrinaryMinmaxExtensionPresent;
			bool IsImageFilterCubicExtensionPresent;
			bool IsClipspaceWScalingExtensionPresent;
			bool IsFragmentCoverageToColorExtensionPresent;
			bool IsGeometryShaderPassthroughExtensionPresent;
			bool IsRaytracingExtensionPresent;
			bool IsShaderSubgroupPartitionedExtensionPresent;
			bool IsViewportArray2ExtensionPresent;
			bool IsViewportSwizzleExtensionPresent;
			bool IsMultiviewPerViewAttributesExtensionPresent;
			bool IsCreateRenderpass2ExtensionPresent;
			bool IsShaderViewportIndexLayerExtensionPresent;
			bool IsFullscreenExclusiveExtensionPresent;
			bool Is4444FormatsExtensionPresent;
		};

		static const std::unordered_map<std::string, size_t> global_config_device_extension_flags_offsets;

	public:
		DeviceParameters(LoggerQueue* loggingBoard);
		~DeviceParameters();

		void InvalidateDeviceParameters(const VkPhysicalDevice& physicalDevice);

		void InvalidateDeviceExtensions(const VkPhysicalDevice& physicalDevice, std::vector<std::string>& outEnabledExtensions, vgs::StructureChainBlob<VkDeviceCreateInfo>& outStructureChain);

		bool IsBlendOperationAdvancedFeaturesExtensionEnabled()         const;
		bool IsBufferDeviceAddressFeaturesExtensionEnabled()            const;
		bool IsComputeShaderDerivativesFeaturesExtensionEnabled()       const;
		bool IsConditionalRenderingFeaturesExtensionEnabled()           const; 
		bool IsDescriptorIndexingFeaturesExtensionEnabled()             const;
		bool IsDeviceGeneratedCommandsFeaturesExtensionEnabled()        const;
		bool IsFragmentDensityMap2FeaturesExtensionEnabled()            const;
		bool IsFragmentShaderBarycentricFeaturesExtensionEnabled()      const;
		bool IsFragmentShaderInterlockFeaturesExtensionEnabled()        const;
		bool IsHostQueryResetFeaturesExtensionEnabled()                 const;
		bool IsRobustness2FeaturesExtensionEnabled()                    const;
		bool IsImagelessFramebufferFeaturesExtensionEnabled()           const;
		bool IsInlineUniformBlockFeaturesExtensionEnabled()             const;
		bool IsMeshShaderFeaturesExtensionEnabled()                     const;
		bool IsPerformanceQueryFeaturesExtensionEnabled()               const;
		bool IsShaderAtomicFloatFeaturesExtensionEnabled()              const;
		bool IsShaderAtomicInt64FeaturesExtensionEnabled()              const;
		bool IsShaderClockFeaturesExtensionEnabled()                    const;
		bool IsShaderDemoteToHelperInvocationFeaturesExtensionEnabled() const;
		bool IsShaderFloat16Int8FeaturesExtensionEnabled()              const;
		bool IsShaderImageFootprintFeaturesExtensionEnabled()           const;
		bool IsShaderIntegerFunctions2FeaturesExtensionEnabled()        const;
		bool IsShaderSubgroupExtendedTypesFeaturesExtensionEnabled()    const;
		bool IsShadingRateImageFeaturesExtensionEnabled()               const;
		bool IsTexelBufferAlignmentFeaturesExtensionEnabled()           const;
		bool IsUniformBufferStandardLayoutFeaturesExtensionEnabled()    const;
		bool IsVulkanMemoryModelFeaturesExtensionEnabled()              const;

		bool IsDrawIndirectCountExtensionEnabled()          const;
		bool IsImageFormatListExtensionEnabled()            const;
		bool IsIncrementalPresentExtensionEnabled()         const;
		bool IsPushDescriptorExtensionEnabled()             const;
		bool IsShaderFloatControlsExtensionEnabled()        const;
		bool IsSpirV14ExtensionEnabled()                    const;
		bool IsConservativeRasterizationExtensionEnabled()  const;
		bool IsDiscardRectanglesExtensionEnabled()          const;
		bool IsFilterCubicExtensionEnabled()                const;
		bool IsHDRMetadataExtensionEnabled()                const;
		bool IsMemoryBudgetExtensionEnabled()               const;
		bool IsPipelineCreationFeedbackExtensionEnabled()   const;
		bool IsShaderStencilExportExtensionEnabled()        const;
		bool IsDisplayNativeHDRExtensionEnabled()           const;
		bool IsGCNShaderExtensionEnabled()                  const;
		bool IsRasterizationOrderExtensionEnabled()         const;
		bool IsShaderBallotExtensionEnabled()               const;
		bool IsShaderTrinaryMinmaxExtensionEnabled()        const;
		bool IsImageFilterCubicExtensionEnabled()           const;
		bool IsClipspaceWScalingExtensionEnabled()          const;
		bool IsFragmentCoverageToColorExtensionEnabled()    const;
		bool IsGeometryShaderPassthroughExtensionEnabled()  const;
		bool IsRaytracingExtensionEnabled()                 const;
		bool IsShaderSubgroupPartitionedExtensionEnabled()  const;
		bool IsViewportArray2ExtensionEnabled()             const;
		bool IsViewportSwizzleExtensionEnabled()            const;
		bool IsMultiviewPerViewAttributesExtensionEnabled() const;
		bool IsCreateRenderpass2ExtensionEnabled()          const;
		bool IsShaderViewportIndexLayerExtensionEnabled()   const;
		bool IsFullscreenExclusiveExtensionEnabled()        const;

		const VkPhysicalDeviceProperties& GetDeviceProperties() const;

	private:
		void InvalidateDeviceFeatures(VkPhysicalDevice physicalDevice);
		void InvalidateDeviceProperties(VkPhysicalDevice physicalDevice);
		void InvalidateDeviceMemoryProperties(VkPhysicalDevice physicalDevice);

	private:
		LoggerQueue* mLoggingBoard;

		DeviceExtensionFlags mEnabledExtensionFlags;

		VkPhysicalDeviceFeatures2                                 mFeatures;
		VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT         mBlendOperationAdvancedFeatures;
		VkPhysicalDeviceBufferDeviceAddressFeatures               mBufferDeviceAddressFeatures;
		VkPhysicalDeviceComputeShaderDerivativesFeaturesNV        mComputeShaderDerivativesFeatures;
		VkPhysicalDeviceConditionalRenderingFeaturesEXT           mConditionalRenderingFeatures;
 		VkPhysicalDeviceDescriptorIndexingFeatures                mDescriptorIndexingFeatures;
		VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV         mDeviceGeneratedCommandsFeatures;
		VkPhysicalDeviceFragmentDensityMap2FeaturesEXT            mFragmentDensityMapFeatures;
		VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV       mFragmentShaderBarycentricFeatures;
		VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT        mFragmentShaderInterlockFeatures;
		VkPhysicalDeviceHostQueryResetFeatures                    mHostQueryResetFeatures;
		VkPhysicalDeviceRobustness2FeaturesEXT                    mRobustness2Features;
		VkPhysicalDeviceImagelessFramebufferFeatures              mImagelessFramebufferFeatures;
		VkPhysicalDeviceInlineUniformBlockFeaturesEXT             mInlineUniformBlockFeatures;
		VkPhysicalDeviceMeshShaderFeaturesNV                      mMeshShaderFeatures;
		VkPhysicalDevicePerformanceQueryFeaturesKHR               mPerformanceQueryFeatures;
		VkPhysicalDeviceShaderAtomicFloatFeaturesEXT              mShaderAtomicFloatFeatures;
		VkPhysicalDeviceShaderAtomicInt64Features                 mShaderAtomicInt64Features;
		VkPhysicalDeviceShaderClockFeaturesKHR                    mShaderClockFeatures;
		VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT mDemoteToHelperInvocationFeatures;
		VkPhysicalDeviceShaderFloat16Int8Features                 mShaderFloat16Int8Features;
		VkPhysicalDeviceShaderImageFootprintFeaturesNV            mShaderImageFootprintFeatures;
		VkPhysicalDeviceShaderIntegerFunctions2FeaturesINTEL      mShaderIntegerFunctions2Features;
		VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures       mShaderSubgroupExtendedTypeFeatures;
		VkPhysicalDeviceShadingRateImageFeaturesNV                mShadingRateImageFeatures;
		VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT           mTexelBufferAlignmentFeatures;
		VkPhysicalDeviceUniformBufferStandardLayoutFeatures       mUniformBufferStandardLayoutFeatures;
		VkPhysicalDevice4444FormatsFeaturesEXT                    mVulkan4444FormatFeatures;
		VkPhysicalDeviceVulkanMemoryModelFeatures                 mVulkanMemoryModelFeatures;

		VkPhysicalDeviceProperties2                             mProperties;
		VkPhysicalDeviceMultiviewProperties                     mMultiviewProperties;
		VkPhysicalDevicePerformanceQueryPropertiesKHR           mPerformanceQueryProperties;
		VkPhysicalDevicePushDescriptorPropertiesKHR             mPushDescriptorProperties;
		VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT     mAdvancedBlendOperationsProperties;
		VkPhysicalDeviceConservativeRasterizationPropertiesEXT  mConservativeRasterizationProperties;
		VkPhysicalDeviceDiscardRectanglePropertiesEXT           mDiscardRectangleProperties;
		VkPhysicalDeviceFragmentDensityMapPropertiesEXT         mFragmentDESTINYMapProperties;
		VkPhysicalDeviceInlineUniformBlockPropertiesEXT         mInlineUniformBlockProperties;
		VkPhysicalDeviceRobustness2PropertiesEXT                mRobustness2Properties;
		VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT       mTexelBufferAlignmentProperties;
		VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV     mDeviceGeneratedCommandProperties;
		VkPhysicalDeviceMeshShaderPropertiesNV                  mMeshShaderProperties;
		VkPhysicalDeviceRayTracingPropertiesNV                  mRaytracingProperties;
		VkPhysicalDeviceShadingRateImagePropertiesNV            mShadingRateImageProperties;
		VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX mMultiviewPerViewProperties;
		VkPhysicalDeviceSubgroupProperties                      mDeviceSubgroupProperties;
		VkPhysicalDeviceFloatControlsProperties                 mDeviceFloatControlProperties;
		VkPhysicalDeviceDescriptorIndexingProperties            mDeviceDescriptorIndexingProperties;

		VkPhysicalDeviceMemoryProperties2         mMemoryProperties;
		VkPhysicalDeviceMemoryBudgetPropertiesEXT mMemoryBudgetProperties;
	};
}