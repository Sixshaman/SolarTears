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
