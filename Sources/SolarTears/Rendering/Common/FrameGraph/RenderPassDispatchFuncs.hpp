#pragma once

#include "Passes/GBufferPass.hpp"
#include "Passes/CopyImagePass.hpp"
#include <cassert>

#define CHOOSE_PASS_FUNCTION_WITH_TYPE(PassType, FuncName, FuncVariable, TypeMangle)                 \
switch(PassType)                                                                                     \
{                                                                                                    \
	case RenderPassType::GBufferGenerate: FuncVariable = FuncName<GBufferPass##TypeMangle>;   break; \
	case RenderPassType::CopyImage:       FuncVariable = FuncName<CopyImagePass##TypeMangle>; break; \
}                                                                                                    \

#define CHOOSE_PASS_FUNCTION(PassType, FuncName, FuncVariable) CHOOSE_PASS_FUNCTION_WITH_TYPE(PassType, FuncName, FuncVariable, )

template<typename Pass>
constexpr RenderPassClass GetPassClass()
{
	return Pass::PassClass;
}

constexpr RenderPassClass GetPassClass(RenderPassType passType)
{
	using GetPassClassFunc = RenderPassClass(*)();

	GetPassClassFunc GetClass = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, GetPassClass, GetClass, Base);

	assert(GetClass != nullptr);
	return GetClass();
}

template<typename Pass>
constexpr uint_fast16_t GetPassSubresourceCount()
{
	return (uint_fast16_t)Pass::PassSubresourceId::Count;
}

constexpr uint_fast16_t GetPassSubresourceCount(RenderPassType passType)
{
	using GetSubresourceCountFunc = uint_fast16_t(*)();

	GetSubresourceCountFunc GetSubresourceCount = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, GetPassSubresourceCount, GetSubresourceCount, Base);

	assert(GetSubresourceCount != nullptr);
	return GetSubresourceCount();
}

template<typename Pass>
constexpr std::string_view GetPassSubresourceStringId(uint_fast16_t passSubresourceIndex)
{
	assert(passSubresourceIndex < GetPassSubresourceCount<Pass>());

	using SubresourceIdType = Pass::PassSubresourceId;
	return Pass::GetSubresourceStringId((SubresourceIdType)passSubresourceIndex);
}

constexpr std::string_view GetPassSubresourceStringId(RenderPassType passType, uint_fast16_t passSubresourceIndex)
{ 
	using GetSubresourceIdFunc = std::string_view(*)(uint_fast16_t);
	assert(passSubresourceIndex < GetPassSubresourceCount(passType));

	GetSubresourceIdFunc GetSubresourceStringId = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, GetPassSubresourceStringId, GetSubresourceStringId, Base);

	assert(GetSubresourceStringId != nullptr);
	return GetSubresourceStringId(passSubresourceIndex);
}