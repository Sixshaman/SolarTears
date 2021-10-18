#pragma once

#include <type_traits>

template<typename PassType, typename = int>
struct HasWriteSubresourceIds: std::false_type {};

template<typename PassType>
struct HasWriteSubresourceIds<PassType, decltype((void)PassType::WriteSubresourceIds, 0)>: std::true_type {};

template<typename PassType, typename = int>
struct HasReadSubresourceIds: std::false_type {};

template<typename PassType>
struct HasReadSubresourceIds<PassType, decltype((void)PassType::ReadSubresourceIds, 0)>: std::true_type {};