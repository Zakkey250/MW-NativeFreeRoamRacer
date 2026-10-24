#pragma once
#include <cstdint>
namespace car_part_capacity {
constexpr unsigned Stock=1024,Default=8192,Maximum=16384,Stride=24;
constexpr bool Valid(unsigned slots){return slots>=Stock&&slots<=Maximum;}
constexpr std::uint64_t Bytes(unsigned slots){return Valid(slots)?std::uint64_t(slots)*Stride+0x30u:0;}
static_assert(Bytes(Maximum)==393264&&Bytes(Default)==196656);
}
