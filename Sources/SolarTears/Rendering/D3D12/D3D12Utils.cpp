#include "D3D12Utils.hpp"
#include "..\..\Core\Util.hpp"
#include "..\..\Logging\LoggerQueue.hpp"
#include <string>
#include <fstream>
#include <cinttypes>

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
