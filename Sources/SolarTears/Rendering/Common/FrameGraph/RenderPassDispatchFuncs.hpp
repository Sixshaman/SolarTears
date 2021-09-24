#pragma once

#include "Passes/GBufferPass.hpp"
#include "Passes/CopyImagePass.hpp"
#include <cassert>

template<typename Pass>
constexpr uint_fast16_t GetPassSubresourceCount()
{
	return (uint_fast16_t)Pass::PassSubresourceId::Count;
}

constexpr uint_fast16_t GetPassSubresourceCount(RenderPassType passType)
{
	switch(passType)
	{
		case RenderPassType::GBufferGenerate: return GetPassSubresourceCount<GBufferPassBase>();
		case RenderPassType::CopyImage:       return GetPassSubresourceCount<CopyImagePassBase>();
	}

	assert(false);
	return 0;
}

#define DEFINE_GET_PASS_SUBRESOURCE_INFO_TEMPLATE(ReturnType, SubresourceInfo)                    \
template<typename Pass>                                                                           \
constexpr ReturnType GetPassSubresource##SubresourceInfo(uint_fast16_t passSubresourceIndex)      \
{                                                                                                 \
	assert(passSubresourceIndex < GetPassSubresourceCount<Pass>());                               \
	return Pass::GetSubresource##SubresourceInfo((Pass::PassSubresourceId)passSubresourceIndex);  \
}

#define DEFINE_GET_PASS_SUBRESOURCE_INFO(ReturnType, SubresourceInfo, DefaultReturn)                                           \
constexpr ReturnType GetPassSubresource##SubresourceInfo(RenderPassType passType, uint_fast16_t passSubresourceIndex)          \
{                                                                                                                              \
	switch(passType)                                                                                                           \
	{                                                                                                                          \
		case RenderPassType::GBufferGenerate: return GetPassSubresource##SubresourceInfo<GBufferPass>(passSubresourceIndex);   \
		case RenderPassType::CopyImage:       return GetPassSubresource##SubresourceInfo<CopyImagePass>(passSubresourceIndex); \
	}                                                                                                                          \
                                                                                                                               \
	assert(false);                                                                                                             \
	return DefaultReturn;                                                                                                      \
}