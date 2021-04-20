#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <DirectXMath.h>

class LoggerQueue;

namespace VulkanCBindings
{
	namespace VulkanUtils
	{
		//3 frames in flight is good enough
		constexpr uint32_t InFlightFrameCount = 3;

		template<typename T>
		static constexpr VkFormat FormatForVectorType = VK_FORMAT_UNDEFINED;

		template<>
		static constexpr VkFormat FormatForVectorType<DirectX::XMFLOAT4> = VK_FORMAT_R32G32B32A32_SFLOAT;

		template<>
		static constexpr VkFormat FormatForVectorType<DirectX::XMFLOAT3> = VK_FORMAT_R32G32B32_SFLOAT;

		template<>
		static constexpr VkFormat FormatForVectorType<DirectX::XMFLOAT2> = VK_FORMAT_R32G32_SFLOAT;

		template<typename T>
		static constexpr VkIndexType FormatForIndexType = VK_INDEX_TYPE_MAX_ENUM;

		template<>
		static constexpr VkIndexType FormatForIndexType<uint32_t> = VK_INDEX_TYPE_UINT32;

		template<>
		static constexpr VkIndexType FormatForIndexType<uint16_t> = VK_INDEX_TYPE_UINT16;

		template<>
		static constexpr VkIndexType FormatForIndexType<uint8_t> = VK_INDEX_TYPE_UINT8_EXT;

		class VkException
		{
		public:
			VkException() = default;

			VkException(VkResult res, const std::wstring& funcionName, const std::wstring& filename, int lineNumber);

			std::wstring ToString() const;

			VkResult ErrorCode = VK_SUCCESS;
			std::wstring FunctionName;
			std::wstring Filename;
			int LineNumber = -1;
		};

		VkDeviceSize AlignMemory(VkDeviceSize value, VkDeviceSize alignment);

		void LoadShaderModuleFromFile(const std::wstring& filename, std::vector<uint32_t>& dataBlob, LoggerQueue* logger);

		VkBool32 DebugReportCCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData);
	}
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                                              \
{                                                                                     \
	VkResult vkres__ = (x);                                                           \
	if(vkres__ != VK_SUCCESS)                                                         \
	{                                                                                 \
		std::wstring wfn = __FILEW__;                                                 \
		throw VulkanCBindings::VulkanUtils::VkException(vkres__, L#x, wfn, __LINE__); \
	}                                                                                 \
}
#endif

#ifndef SafeDestroyObject
#define SafeDestroyObject(vkDestroyObject, Owner, Object) \
{                                                         \
	if(Object != VK_NULL_HANDLE)                          \
	{                                                     \
		vkDestroyObject(Owner, Object, nullptr);          \
		Object = VK_NULL_HANDLE;                          \
	}                                                     \
} 
#endif

#ifndef SafeDestroyDevice
#define SafeDestroyDevice(Device)         \
{                                         \
	if(Device != VK_NULL_HANDLE)          \
	{                                     \
		vkDestroyDevice(Device, nullptr); \
		Device = VK_NULL_HANDLE;          \
	}                                     \
} 
#endif

#ifndef SafeDestroyInstance
#define SafeDestroyInstance(Instance)         \
{                                             \
	if(Instance != VK_NULL_HANDLE)            \
	{                                         \
		vkDestroyInstance(Instance, nullptr); \
		Instance = VK_NULL_HANDLE;            \
	}                                         \
} 
#endif