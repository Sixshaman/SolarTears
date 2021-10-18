#include "VulkanUtils.hpp"
#include "..\..\Core\Util.hpp"
#include "..\..\Logging\LoggerQueue.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanDeviceParameters.hpp"
#include <string>
#include <fstream>
#include <cinttypes>
#include <format>
#include <numeric>

static inline void SetDebugObjectNameHandle([[maybe_unused]] const VkDevice device, [[maybe_unused]] VkObjectType objectType, [[maybe_unused]] uint64_t handle, [[maybe_unused]] const std::string_view name)
{
#ifdef VK_EXT_debug_utils
    VkDebugUtilsObjectNameInfoEXT imageNameInfo;
	imageNameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	imageNameInfo.pNext        = nullptr;
	imageNameInfo.objectType   = objectType;
	imageNameInfo.objectHandle = handle;
	imageNameInfo.pObjectName  = name.data();

	ThrowIfFailed(vkSetDebugUtilsObjectNameEXT(device, &imageNameInfo));
#endif
}

Vulkan::VulkanUtils::VkException::VkException(VkResult res, const std::wstring& funcionName, const std::wstring& filename, int lineNumber): ErrorCode(res), FunctionName(funcionName), Filename(filename), LineNumber(lineNumber)
{
}

std::wstring Vulkan::VulkanUtils::VkException::ToString() const
{
    return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error code: " + std::to_wstring(ErrorCode);
}

void Vulkan::VulkanUtils::LoadShaderModuleFromFile(const std::wstring_view filename, std::vector<uint32_t>& dataBlob, LoggerQueue* logger)
{
    std::ifstream fin(filename.data(), std::ios::binary); //Открываем файл
    if(!fin)
    {
        logger->PostLogMessage(L"Cannot read shader file " + std::wstring(filename) + L"!\n");
        return;
    }

    fin.seekg(0, std::ios::beg);
    auto beg = fin.tellg();
    fin.seekg(0, std::ios::end);
    auto end = fin.tellg();
    fin.seekg(0, std::ios::beg);

    size_t fileLength = end - beg;
    dataBlob.resize((size_t)ceilf((float)fileLength / (float)sizeof(uint32_t)));
    fin.read(reinterpret_cast<char*>(dataBlob.data()), fileLength);

    fin.close();
}

void Vulkan::VulkanUtils::SetDebugObjectName(const VkDevice device, VkImage image, const std::string_view name)
{
    SetDebugObjectNameHandle(device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image), name);
}

void Vulkan::VulkanUtils::SetDebugObjectName(const VkDevice device, VkCommandBuffer commandBuffer, const std::string_view name)
{
    SetDebugObjectNameHandle(device, VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<uint64_t>(commandBuffer), name);
}

void Vulkan::VulkanUtils::SetDebugObjectName(const VkDevice device, VkCommandPool commandPool, const std::string_view name)
{
    SetDebugObjectNameHandle(device, VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64_t>(commandPool), name);
}

uint64_t Vulkan::VulkanUtils::CalcUniformAlignment(const DeviceParameters& deviceParameters)
{
    return std::lcm(deviceParameters.GetDeviceProperties().limits.minUniformBufferOffsetAlignment, deviceParameters.GetDeviceProperties().limits.nonCoherentAtomSize);
}


VkBool32 Vulkan::VulkanUtils::DebugReportCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, [[maybe_unused]] void* pUserData)
{
    std::string outMessage = "DEBUG ";
    switch(messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        outMessage += "VERBOSE (";
        break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        outMessage += "INFO (";
        break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        outMessage += "WARNING (";
        break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        outMessage += "ERROR (";
        break;

    default:
        break;
    }

    switch(messageType)
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        outMessage += "General): ";
        break;

    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        outMessage += "Validation): ";
        break;

    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        outMessage += "Performance): ";
        break;

    default:
        break;
    }

    outMessage += pCallbackData->pMessage;
    outMessage += std::format(" Id: {} ({:#04x})", pCallbackData->messageIdNumber, pCallbackData->messageIdNumber);
    
    if(pCallbackData->pMessageIdName != nullptr)
    {
        outMessage += std::format(" See https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#{} for more info.", pCallbackData->pMessageIdName);
    }

    outMessage += " Objects: ";
    for(const VkDebugUtilsObjectNameInfoEXT* objectInfo = pCallbackData->pObjects; objectInfo != pCallbackData->pObjects + pCallbackData->objectCount; objectInfo++)
    {
        outMessage += "{";

        switch(objectInfo->objectType)
        {
        case VK_OBJECT_TYPE_INSTANCE:
            outMessage += "Instance ";
            break;

        case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
            outMessage += "Physical device ";
		    break;

        case VK_OBJECT_TYPE_DEVICE:
            outMessage += "Device ";
		    break;

        case VK_OBJECT_TYPE_QUEUE:
            outMessage += "Queue ";
		    break;

        case VK_OBJECT_TYPE_SEMAPHORE:
            outMessage += "Semaphore ";
		    break;

        case VK_OBJECT_TYPE_COMMAND_BUFFER:
            outMessage += "Command buffer ";
		    break;

        case VK_OBJECT_TYPE_FENCE:
            outMessage += "Fence ";
		    break;

        case VK_OBJECT_TYPE_DEVICE_MEMORY:
            outMessage += "Device memory ";
		    break;

        case VK_OBJECT_TYPE_BUFFER:
            outMessage += "Buffer ";
		    break;

        case VK_OBJECT_TYPE_IMAGE:
            outMessage += "Image ";
		    break;

        case VK_OBJECT_TYPE_EVENT:
            outMessage += "Event ";
		    break;

        case VK_OBJECT_TYPE_QUERY_POOL:
            outMessage += "Query pool ";
		    break;

        case VK_OBJECT_TYPE_BUFFER_VIEW:
            outMessage += "Buffer view ";
		    break;

        case VK_OBJECT_TYPE_IMAGE_VIEW:
            outMessage += "Image view ";
		    break;

        case VK_OBJECT_TYPE_SHADER_MODULE:
            outMessage += "Shader module ";
		    break;

        case VK_OBJECT_TYPE_PIPELINE_CACHE:
            outMessage += "Pipeline cache ";
		    break;

        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            outMessage += "Pipeline layout ";
		    break;

        case VK_OBJECT_TYPE_RENDER_PASS:
            outMessage += "Render pass ";
		    break;

        case VK_OBJECT_TYPE_PIPELINE:
            outMessage += "Pipeline ";
		    break;

        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            outMessage += "Descriptor set layout ";
		    break;

        case VK_OBJECT_TYPE_SAMPLER:
            outMessage += "Sampler ";
		    break;

        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            outMessage += "Descriptor pool ";
		    break;

        case VK_OBJECT_TYPE_DESCRIPTOR_SET:
            outMessage += "Descriptor set ";
		    break;

        case VK_OBJECT_TYPE_FRAMEBUFFER:
            outMessage += "Framebuffer ";
		    break;

        case VK_OBJECT_TYPE_COMMAND_POOL:
            outMessage += "Command pool ";
		    break;

        case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
            outMessage += "Sampler ycbcr conversion ";
            break;

        case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
            outMessage += "Descriptor update template ";
            break;

        case VK_OBJECT_TYPE_SURFACE_KHR:
            outMessage += "Surface ";
		    break;

        case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
            outMessage += "Swapchain ";
		    break;

        case VK_OBJECT_TYPE_DISPLAY_KHR:
            outMessage += "Display ";
            break;

        case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
            outMessage += "Display mode ";
            break;

        case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
            outMessage += "Debug report callback ";
		    break;

        case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
            outMessage += "Debug utils messenger ";
            break;

        case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
            outMessage += "Acceleration structure ";
            break;

        case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
            outMessage += "Validation cache ";
		    break;

        case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR:
            outMessage += "Deferred operation ";
            break;

        case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
            outMessage += "Indirect commands ";
            break;

        case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT:
            outMessage += "Private data slot ";
            break;

        default:
            outMessage += "Unknown ";
		    break;
        }

        if(objectInfo->pObjectName != nullptr)
        {
            outMessage += "\"";
            outMessage += objectInfo->pObjectName;
            outMessage += "\"";
        }

        outMessage += std::format(" [{:#08x}]", objectInfo->objectHandle);
        outMessage += "}";
    }

    outMessage += "\n";
    Utils::SystemDebugMessage(outMessage);
    return VK_FALSE;
}