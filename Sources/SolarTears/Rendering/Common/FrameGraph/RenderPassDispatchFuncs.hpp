#pragma once

#include "Passes/GBufferPass.hpp"
#include "Passes/CopyImagePass.hpp"
#include <cassert>

#define CHOOSE_PASS_FUNCTION(PassType, FuncName, FuncVariable, TypeMangle)                           \
switch(PassType)                                                                                     \
{                                                                                                    \
	case RenderPassType::GBufferGenerate: FuncVariable = FuncName<GBufferPass##TypeMangle>;   break; \
	case RenderPassType::CopyImage:       FuncVariable = FuncName<CopyImagePass##TypeMangle>; break; \
}                                                                                                    \

template<typename Pass>
constexpr uint_fast16_t GetPassSubresourceCount()
{
	return (uint_fast16_t)Pass::PassSubresourceId::Count;
}

constexpr uint_fast16_t GetPassSubresourceCount(RenderPassType passType)
{
	using GetSubresourceCountFunc = uint_fast16_t(*)();

	GetSubresourceCountFunc GetSubresourceCount = nullptr;
	CHOOSE_PASS_FUNCTION(passType, GetPassSubresourceCount, GetSubresourceCount, Base);

	assert(GetSubresourceCount != nullptr);
	return GetSubresourceCount();
}

template<typename Pass>
constexpr std::string_view GetPassSubresourceStringId(uint_fast16_t passSubresourceIndex)
{
	assert(passSubresourceIndex < GetPassSubresourceCount<Pass>());
	return Pass::GetSubresourceStringId((Pass::PassSubresourceId)passSubresourceIndex);
}

constexpr std::string_view GetPassSubresourceStringId(RenderPassType passType, uint_fast16_t passSubresourceIndex)
{ 
	using GetSubresourceIdFunc = std::string_view(*)(uint_fast16_t);
	assert(passSubresourceIndex < GetPassSubresourceCount(passType));

	GetSubresourceIdFunc GetSubresourceStringId = nullptr;
	CHOOSE_PASS_FUNCTION(passType, GetPassSubresourceStringId, GetSubresourceStringId, Base);

	assert(GetSubresourceStringId != nullptr);
	return GetSubresourceStringId(passSubresourceIndex);
}