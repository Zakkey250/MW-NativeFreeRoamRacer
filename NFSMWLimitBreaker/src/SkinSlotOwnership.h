#pragma once
#include <cstdint>
namespace skin_slots {
// The native initial free bitmap supplies slots 1..12, even when the external
// allocator is configured to search 20. Never publish occupied or unverified
// slots, and leave every low/native bit unchanged.
constexpr std::uint32_t SupplementalFreeBits(std::uint32_t occupied,unsigned verifiedCount){
 return verifiedCount==20 ? (0x000FF000u & ~occupied) : 0u;
}
}
