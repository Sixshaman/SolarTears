#pragma once

#include "Passes/GBufferPass.hpp"
#include "Passes/CopyImagePass.hpp"
#include "RenderPassTraits.h"
#include <cassert>
#include <algorithm>

#define CHOOSE_PASS_FUNCTION_WITH_TYPE(PassType, FuncName, FuncVariable, TypeMangle)                 \
switch(PassType)                                                                                     \
{                                                                                                    \
	case RenderPassType::GBufferGenerate: FuncVariable = FuncName<GBufferPass##TypeMangle>;   break; \
	case RenderPassType::CopyImage:       FuncVariable = FuncName<CopyImagePass##TypeMangle>; break; \
}                                                                                                    \

#define CHOOSE_PASS_FUNCTION(PassType, FuncName, FuncVariable) CHOOSE_PASS_FUNCTION_WITH_TYPE(PassType, FuncName, FuncVariable, )

template<typename Pass>
inline constexpr RenderPassClass GetPassClass()
{
	return Pass::PassClass;
}

inline RenderPassClass GetPassClass(RenderPassType passType)
{
	using GetPassClassFunc = RenderPassClass(*)();

	GetPassClassFunc GetClass = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, GetPassClass, GetClass, Base);

	assert(GetClass != nullptr);
	return GetClass();
}

template<typename Pass>
inline constexpr uint_fast16_t GetPassSubresourceCount()
{
	return (uint_fast16_t)Pass::PassSubresourceId::Count;
}

inline uint_fast16_t GetPassSubresourceCount(RenderPassType passType)
{
	using GetSubresourceCountFunc = uint_fast16_t(*)();

	GetSubresourceCountFunc GetSubresourceCount = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, GetPassSubresourceCount, GetSubresourceCount, Base);

	assert(GetSubresourceCount != nullptr);
	return GetSubresourceCount();
}

template<typename Pass>
inline constexpr std::string_view GetPassSubresourceStringId(uint_fast16_t passSubresourceIndex)
{
	assert(passSubresourceIndex < GetPassSubresourceCount<Pass>());

	using SubresourceIdType = Pass::PassSubresourceId;
	return Pass::GetSubresourceStringId((SubresourceIdType)passSubresourceIndex);
}

inline std::string_view GetPassSubresourceStringId(RenderPassType passType, uint_fast16_t passSubresourceIndex)
{ 
	using GetSubresourceIdFunc = std::string_view(*)(uint_fast16_t);
	assert(passSubresourceIndex < GetPassSubresourceCount(passType));

	GetSubresourceIdFunc GetSubresourceStringId = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, GetPassSubresourceStringId, GetSubresourceStringId, Base);

	assert(GetSubresourceStringId != nullptr);
	return GetSubresourceStringId(passSubresourceIndex);
}

template<typename Pass>
inline constexpr uint32_t GetPassReadSubresourceCount()
{
	if constexpr(HasReadSubresourceIds<Pass>::value)
	{
		return (uint32_t)Pass::ReadSubresourceIds.size();
	}
	else
	{
		return 0;
	}
}

inline uint32_t GetPassReadSubresourceCount(RenderPassType passType)
{
	using GetReadSubresourceCountFunc = uint32_t(*)();
	
	GetReadSubresourceCountFunc GetReadSubresourceCount = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, GetPassReadSubresourceCount, GetReadSubresourceCount, Base);

	assert(GetReadSubresourceCount != nullptr);
	return GetReadSubresourceCount();
}

template<typename Pass>
inline constexpr uint32_t GetPassWriteSubresourceCount()
{
	if constexpr(HasWriteSubresourceIds<Pass>::value)
	{
		return (uint32_t)Pass::WriteSubresourceIds.size();
	}
	else
	{
		return 0;
	}
}

inline uint32_t GetPassWriteSubresourceCount(RenderPassType passType)
{
	using GetWriteSubresourceCountFunc = uint32_t(*)();
	
	GetWriteSubresourceCountFunc GetWriteSubresourceCount = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, GetPassWriteSubresourceCount, GetWriteSubresourceCount, Base);

	assert(GetWriteSubresourceCount != nullptr);
	return GetWriteSubresourceCount();
}

template<typename Pass>
inline constexpr void FillPassReadSubresourceIds(std::span<uint_fast16_t> outReadIdSpan)
{
	if constexpr(GetPassReadSubresourceCount<Pass>() == 0)
	{
		return;
	}
	else
	{
		assert(outReadIdSpan.size() == GetPassReadSubresourceCount<Pass>());

		using SubresourceIdType = Pass::PassSubresourceId;
		std::transform(Pass::ReadSubresourceIds.begin(), Pass::ReadSubresourceIds.end(), outReadIdSpan.begin(), [](SubresourceIdType subresourceId)
		{
			return (uint_fast16_t)subresourceId;
		});
	}
}

inline void FillPassReadSubresourceIds(RenderPassType passType, std::span<uint_fast16_t> outReadIdSpan)
{
	using FillPassReadSubresourceIdsFunc = void(*)(std::span<uint_fast16_t>);
	
	FillPassReadSubresourceIdsFunc FillReadSubresourceIds = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, FillPassReadSubresourceIds, FillReadSubresourceIds, Base);

	assert(FillReadSubresourceIds != nullptr);
	return FillReadSubresourceIds(outReadIdSpan);
}

template<typename Pass>
inline constexpr void FillPassWriteSubresourceIds(std::span<uint_fast16_t> outWriteIdSpan)
{
	if constexpr(GetPassWriteSubresourceCount<Pass>() == 0)
	{
		return;
	}
	else
	{
		assert(outWriteIdSpan.size() == GetPassWriteSubresourceCount<Pass>());

		using SubresourceIdType = Pass::PassSubresourceId;
		std::transform(Pass::WriteSubresourceIds.begin(), Pass::WriteSubresourceIds.end(), outWriteIdSpan.begin(), [](SubresourceIdType subresourceId)
		{
			return (uint_fast16_t)subresourceId;
		});
	}
}

inline void FillPassWriteSubresourceIds(RenderPassType passType, std::span<uint_fast16_t> outWriteIdSpan)
{
	using FillPassWriteSubresourceIdsFunc = void(*)(std::span<uint_fast16_t>);
	
	FillPassWriteSubresourceIdsFunc FillWriteSubresourceIds = nullptr;
	CHOOSE_PASS_FUNCTION_WITH_TYPE(passType, FillPassWriteSubresourceIds, FillWriteSubresourceIds, Base);

	assert(FillWriteSubresourceIds != nullptr);
	return FillWriteSubresourceIds(outWriteIdSpan);
}