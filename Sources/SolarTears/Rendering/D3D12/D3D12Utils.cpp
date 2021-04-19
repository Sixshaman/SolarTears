#include "D3D12Utils.hpp"
#include "..\..\Core\Util.hpp"
#include "..\..\Logging\LoggerQueue.hpp"
#include <string>
#include <fstream>
#include <cinttypes>
#include <malloc.h>

UINT64 D3D12::D3D12Utils::AlignMemory(UINT64 value, UINT64 alignment)
{
    return value + (alignment - value % alignment) % alignment;
}

bool D3D12::D3D12Utils::IsStateWriteable(D3D12_RESOURCE_STATES state)
{
    UINT writeableFlags = D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_STREAM_OUT | D3D12_RESOURCE_STATE_COPY_DEST | D3D12_RESOURCE_STATE_RESOLVE_DEST;
    return (state & writeableFlags) != 0;
}

bool D3D12::D3D12Utils::IsStateComputeFriendly(D3D12_RESOURCE_STATES state)
{
    UINT computePossibleStates = D3D12_RESOURCE_STATE_COMMON | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
                               | D3D12_RESOURCE_STATE_COPY_DEST | D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE | D3D12_RESOURCE_STATE_PREDICATION;

    return (state & computePossibleStates) != 0;
}

bool D3D12::D3D12Utils::IsStatePromoteableTo(D3D12_RESOURCE_STATES state)
{
    //This function is only used for non-simultaneous access textures
    UINT implicitCommonPromotionFlags = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_COPY_DEST | D3D12_RESOURCE_STATE_COPY_SOURCE;
    return (state & implicitCommonPromotionFlags) != 0;;
}

DXGI_FORMAT D3D12::D3D12Utils::ConvertToTypeless(DXGI_FORMAT format)
{
    switch(format)
    {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return DXGI_FORMAT_R32G32B32A32_TYPELESS;

        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return DXGI_FORMAT_R32G32B32_TYPELESS;

        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
            return DXGI_FORMAT_R16G16B16A16_TYPELESS;

        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return DXGI_FORMAT_R32G32_TYPELESS;

        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
            return DXGI_FORMAT_R10G10B10A2_TYPELESS;

        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
            return DXGI_FORMAT_R8G8B8A8_TYPELESS;

        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
            return DXGI_FORMAT_R16G16_TYPELESS;

        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
            return DXGI_FORMAT_R32_TYPELESS;

        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
            return DXGI_FORMAT_R24G8_TYPELESS;

        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
            return DXGI_FORMAT_R8G8_TYPELESS;

        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
            return DXGI_FORMAT_R16_TYPELESS;

        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return DXGI_FORMAT_R8_TYPELESS;

        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
            return DXGI_FORMAT_BC1_TYPELESS;

        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
            return DXGI_FORMAT_BC2_TYPELESS;

        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
            return DXGI_FORMAT_BC3_TYPELESS;

        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return DXGI_FORMAT_BC4_TYPELESS;

        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
            return DXGI_FORMAT_BC5_TYPELESS;

        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            return DXGI_FORMAT_B8G8R8A8_TYPELESS;

        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
            return DXGI_FORMAT_B8G8R8X8_TYPELESS;

        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
            return DXGI_FORMAT_BC6H_TYPELESS;

        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return DXGI_FORMAT_BC7_TYPELESS;

        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

D3D12::D3D12Utils::StateSubobjectHelper::StateSubobjectHelper(): mSubobjectStreamBlob(nullptr), mStreamBlobSize(0), mStreamBlobCapacity(0)
{
}

D3D12::D3D12Utils::StateSubobjectHelper::~StateSubobjectHelper()
{
    if(mSubobjectStreamBlob != nullptr)
    {
        _aligned_free(mSubobjectStreamBlob);
    }
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddVertexShader(const void* shaderBlob, size_t shaderDataSize)
{
    AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, shaderBlob, shaderDataSize);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddHullShader(const void* shaderBlob, size_t shaderDataSize)
{
    AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS, shaderBlob, shaderDataSize);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddDomainShader(const void* shaderBlob, size_t shaderDataSize)
{
    AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS, shaderBlob, shaderDataSize);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddGeometryShader(const void* shaderBlob, size_t shaderDataSize)
{
    AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS, shaderBlob, shaderDataSize);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddPixelShader(const void* shaderBlob, size_t shaderDataSize)
{
    AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, shaderBlob, shaderDataSize);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddComputeShader(const void* shaderBlob, size_t shaderDataSize)
{
    AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS, shaderBlob, shaderDataSize);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddAmplificationShader(const void* shaderBlob, size_t shaderDataSize)
{
    AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, shaderBlob, shaderDataSize);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddMeshShader(const void* shaderBlob, size_t shaderDataSize)
{
    AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, shaderBlob, shaderDataSize);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddSampleMask(UINT sampleMask)
{
    AllocateStreamData(sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE) + sizeof(UINT));

    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE subobjectType = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK;

    AddPadding();
    AddStreamData(&subobjectType);
    AddStreamData(&sampleMask);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddNodeMask(UINT nodeMask)
{
    AllocateStreamData(sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE) + sizeof(D3D12_NODE_MASK));

    D3D12_NODE_MASK data = {.NodeMask = nodeMask};

    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE subobjectType = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK;

    AddPadding();
    AddStreamData(&subobjectType);
    AddStreamData(&data);
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddDepthStencilFormat(DXGI_FORMAT format)
{
    AllocateStreamData(sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE) + sizeof(DXGI_FORMAT));

    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE subobjectType = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;

    AddPadding();
    AddStreamData(&subobjectType);
    AddStreamData(&format);
}

void* D3D12::D3D12Utils::StateSubobjectHelper::GetStreamPointer() const
{
    return mSubobjectStreamBlob;
}

size_t D3D12::D3D12Utils::StateSubobjectHelper::GetStreamSize() const
{
    return mStreamBlobSize;
}

void D3D12::D3D12Utils::StateSubobjectHelper::AllocateStreamData(size_t dataSize)
{
    size_t addedSpace  = AlignMemory(dataSize, alignof(void*));
    size_t currentSize = AlignMemory(mStreamBlobSize, alignof(void*));
    if(currentSize + addedSpace > mStreamBlobCapacity)
    {
        ReallocateData(addedSpace);
    }
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddPadding()
{
    size_t alignedSize = AlignMemory(mStreamBlobSize, alignof(void*));
    assert(alignedSize < mStreamBlobCapacity);

    mStreamBlobSize = alignedSize;
}

void D3D12::D3D12Utils::StateSubobjectHelper::AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE subobjectType, const void* shaderData, size_t shaderSize)
{
    AllocateStreamData(sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE) + sizeof(D3D12_SHADER_BYTECODE));

    D3D12_SHADER_BYTECODE shaderBytecode;
    shaderBytecode.pShaderBytecode = shaderData;
    shaderBytecode.BytecodeLength  = shaderSize;

    AddPadding();
    AddStreamData(&subobjectType);
    AddStreamData(&shaderBytecode);
}

void D3D12::D3D12Utils::StateSubobjectHelper::ReallocateData(size_t requiredSize)
{
    size_t newCapacity = mStreamBlobCapacity * 2 + requiredSize;

    std::byte* newData = reinterpret_cast<std::byte*>(_aligned_malloc(newCapacity, alignof(void*))); //Even new is useless here - aligned new won't work there. And MSVC doesn't support aligned_alloc
    assert(reinterpret_cast<uintptr_t>(newData) % alignof(void*) == 0);

    assert(newData != nullptr);
    memcpy(newData, mSubobjectStreamBlob, mStreamBlobSize);

    _aligned_free(mSubobjectStreamBlob);
    mSubobjectStreamBlob = newData;
    mStreamBlobCapacity  = newCapacity;
}