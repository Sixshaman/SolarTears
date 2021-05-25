#include "RenderingUtils.hpp"

uint64_t Utils::AlignMemory(uint64_t value, uint64_t alignment)
{
    return value + (alignment - value % alignment) % alignment;
}