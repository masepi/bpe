#pragma once

#include <cassert>
#include <limits>
#include <utility>

namespace bpe {
	
using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

using i64 = int64_t;
using i32 = int32_t;
using i16 = int16_t;
using i8 = int8_t;


/// Compile time conversion: From --> To
template <typename To, typename From>
requires std::integral<To> && std::integral<From>
To to(From from)
{
	assert(std::cmp_less_equal(from, std::numeric_limits<To>::max()));
	assert(std::cmp_greater_equal(from, std::numeric_limits<To>::min()));
	return static_cast<To>(from);
}

} // namespace bpe
