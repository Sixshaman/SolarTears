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
        logger->PostLogMessage(L"Cannot read shader file " + filename + L"!\n");
        return;
    }

    fin.seekg(0, std::ios::beg);
    auto beg = fin.tellg();
    fin.seekg(0, std::ios::end);
    auto end = fin.tellg();
    fin.seekg(0, std::ios::beg);

    size_t fileLength = end - beg;
    std::vector<char> fileContents(fileLength);
    fin.read(fileContents.data(), fileLength);

    fin.close();

    dataBlob.resize((size_t)ceilf((float)fileLength / (float)sizeof(uint32_t)));
    memcpy(dataBlob.data(), fileContents.data(), fileLength);
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
    Utils::SystemDebugMessage("DEBUG ");
    switch(messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        Utils::SystemDebugMessage("VERBOSE (");
        break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        Utils::SystemDebugMessage("INFO (");
        break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        Utils::SystemDebugMessage("WARNING (");
        break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        Utils::SystemDebugMessage("ERROR (");
        break;

    default:
        break;
    }

    switch(messageType)
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        Utils::SystemDebugMessage("General): ");
        break;

    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        Utils::SystemDebugMessage("Validation): ");
        break;

    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        Utils::SystemDebugMessage("Performance): ");
        break;

    default:
        break;
    }

    Utils::SystemDebugMessage(pCallbackData->pMessage);
    Utils::SystemDebugMessage(std::format(" Id: {} ({:#04x})", pCallbackData->messageIdNumber, pCallbackData->messageIdNumber));
    Utils::SystemDebugMessage(std::format(" See https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#{} for more info.", pCallbackData->pMessageIdName));
    Utils::SystemDebugMessage(" Objects: ");

    for(const VkDebugUtilsObjectNameInfoEXT* objectInfo = pCallbackData->pObjects; objectInfo != pCallbackData->pObjects + pCallbackData->objectCount; objectInfo++)
    {
        Utils::SystemDebugMessage("{");

        switch (objectInfo->objectType)
        {
        case VK_OBJECT_TYPE_INSTANCE:
            Utils::SystemDebugMessage("Instance ");
            break;

        case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
            Utils::SystemDebugMessage("Physical device ");
		    break;

        case VK_OBJECT_TYPE_DEVICE:
            Utils::SystemDebugMessage("Device ");
		    break;

        case VK_OBJECT_TYPE_QUEUE:
            Utils::SystemDebugMessage("Queue ");
		    break;

        case VK_OBJECT_TYPE_SEMAPHORE:
            Utils::SystemDebugMessage("Semaphore ");
		    break;

        case VK_OBJECT_TYPE_COMMAND_BUFFER:
            Utils::SystemDebugMessage("Command buffer ");
		    break;

        case VK_OBJECT_TYPE_FENCE:
            Utils::SystemDebugMessage("Fence ");
		    break;

        case VK_OBJECT_TYPE_DEVICE_MEMORY:
            Utils::SystemDebugMessage("Device memory ");
		    break;

        case VK_OBJECT_TYPE_BUFFER:
            Utils::SystemDebugMessage("Buffer ");
		    break;

        case VK_OBJECT_TYPE_IMAGE:
            Utils::SystemDebugMessage("Image ");
		    break;

        case VK_OBJECT_TYPE_EVENT:
            Utils::SystemDebugMessage("Event ");
		    break;

        case VK_OBJECT_TYPE_QUERY_POOL:
            Utils::SystemDebugMessage("Query pool ");
		    break;

        case VK_OBJECT_TYPE_BUFFER_VIEW:
            Utils::SystemDebugMessage("Buffer view ");
		    break;

        case VK_OBJECT_TYPE_IMAGE_VIEW:
            Utils::SystemDebugMessage("Image view ");
		    break;

        case VK_OBJECT_TYPE_SHADER_MODULE:
            Utils::SystemDebugMessage("Shader module ");
		    break;

        case VK_OBJECT_TYPE_PIPELINE_CACHE:
            Utils::SystemDebugMessage("Pipeline cache ");
		    break;

        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            Utils::SystemDebugMessage("Pipeline layout ");
		    break;

        case VK_OBJECT_TYPE_RENDER_PASS:
            Utils::SystemDebugMessage("Render pass ");
		    break;

        case VK_OBJECT_TYPE_PIPELINE:
            Utils::SystemDebugMessage("Pipeline ");
		    break;

        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            Utils::SystemDebugMessage("Descriptor set layout ");
		    break;

        case VK_OBJECT_TYPE_SAMPLER:
            Utils::SystemDebugMessage("Sampler ");
		    break;

        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            Utils::SystemDebugMessage("Descriptor pool ");
		    break;

        case VK_OBJECT_TYPE_DESCRIPTOR_SET:
            Utils::SystemDebugMessage("Descriptor set ");
		    break;

        case VK_OBJECT_TYPE_FRAMEBUFFER:
            Utils::SystemDebugMessage("Framebuffer ");
		    break;

        case VK_OBJECT_TYPE_COMMAND_POOL:
            Utils::SystemDebugMessage("Command pool ");
		    break;

        case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
            Utils::SystemDebugMessage("Sampler ycbcr conversion ");
            break;

        case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
            Utils::SystemDebugMessage("Descriptor update template ");
            break;

        case VK_OBJECT_TYPE_SURFACE_KHR:
            Utils::SystemDebugMessage("Surface ");
		    break;

        case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
            Utils::SystemDebugMessage("Swapchain ");
		    break;

        case VK_OBJECT_TYPE_DISPLAY_KHR:
            Utils::SystemDebugMessage("Display ");
            break;

        case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
            Utils::SystemDebugMessage("Display mode ");
            break;

        case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
            Utils::SystemDebugMessage("Debug report callback ");
		    break;

        case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
            Utils::SystemDebugMessage("Debug utils messenger ");
            break;

        case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
            Utils::SystemDebugMessage("Acceleration structure ");
            break;

        case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
            Utils::SystemDebugMessage("Validation cache ");
		    break;

        case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR:
            Utils::SystemDebugMessage("Deferred operation ");
            break;

        case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
            Utils::SystemDebugMessage("Indirect commands ");
            break;

        case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT:
            Utils::SystemDebugMessage("Private data slot ");
            break;

        default:
            Utils::SystemDebugMessage("Unknown ");
		    break;
        }

        if(objectInfo->pObjectName != nullptr)
        {
            Utils::SystemDebugMessage("\"");
            Utils::SystemDebugMessage(objectInfo->pObjectName);
            Utils::SystemDebugMessage("\"");
        }

        Utils::SystemDebugMessage(std::format(" [{:#08x}]", objectInfo->objectHandle));
        Utils::SystemDebugMessage("}");
    }

    Utils::SystemDebugMessage("\n");
    return VK_FALSE;
}