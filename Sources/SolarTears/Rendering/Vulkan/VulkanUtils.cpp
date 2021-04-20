#include "VulkanUtils.hpp"
#include "..\..\Core\Util.hpp"
#include "..\..\Logging\LoggerQueue.hpp"
#include <string>
#include <fstream>
#include <cinttypes>

Vulkan::VulkanUtils::VkException::VkException(VkResult res, const std::wstring& funcionName, const std::wstring& filename, int lineNumber): ErrorCode(res), FunctionName(funcionName), Filename(filename), LineNumber(lineNumber)
{
}

std::wstring Vulkan::VulkanUtils::VkException::ToString() const
{
    return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error code: " + std::to_wstring(ErrorCode);
}

VkDeviceSize Vulkan::VulkanUtils::AlignMemory(VkDeviceSize value, VkDeviceSize alignment)
{
    return value + (alignment - value % alignment) % alignment;
}

void Vulkan::VulkanUtils::LoadShaderModuleFromFile(const std::wstring& filename, std::vector<uint32_t>& dataBlob, LoggerQueue* logger)
{
    std::ifstream fin(filename, std::ios::binary); //Открываем файл
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

VkBool32 Vulkan::VulkanUtils::DebugReportCCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    UNREFERENCED_PARAMETER(pUserData);

    std::string strResult;

    switch (flags)
    {
    case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
        strResult += "INFO: ";
        break;
    case VK_DEBUG_REPORT_WARNING_BIT_EXT:
        strResult += "WARNING: ";
        break;
    case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
        strResult += "PERFORMANCE WARNING: ";
        break;
    case VK_DEBUG_REPORT_ERROR_BIT_EXT:
        strResult += "ERROR: ";
        break;
    case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
        strResult += "DEBUG: ";
        break;
    default:
        break;
    }

    std::string objectName;
    if(objectType == VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT || object == VK_NULL_HANDLE)
    {
        objectName = "undefined";
    }
    else
    {
        objectName = Utils::StringFormat("0x%.16" PRIx64, object);
    }

    strResult += "The component ";
    strResult += pLayerPrefix;
    strResult += " has triggered the report. [";
    strResult += Utils::StringFormat("%d", (uint32_t)messageCode);
    strResult += "] ";
    strResult += pMessage;
    strResult += " Location: ";
    strResult += Utils::StringFormat("0x%.8X", location);

    Utils::SystemDebugMessage(strResult);
    return VK_FALSE;
}