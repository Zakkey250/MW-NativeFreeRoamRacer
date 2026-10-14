#pragma once
#include <cstdint>

namespace native_freeroam::junkman {
constexpr std::uint32_t kParts = 0x7fu;
inline std::uint32_t Mask(std::uint32_t bits) noexcept { return bits & kParts; }
inline unsigned Count(std::uint32_t bits) noexcept {
    bits = Mask(bits);
    unsigned count = 0;
    while (bits) { count += bits & 1u; bits >>= 1; }
    return count;
}
// Preserve the player's exact categories first. If the selected AI model
// cannot take one of them, fill another supported category up to the same
// count. Existing AI parts are counted rather than installed twice.
template<class CanInstall>
std::uint32_t Plan(std::uint32_t player, std::uint32_t installed, CanInstall canInstall) {
    player = Mask(player); installed = Mask(installed);
    const auto desired = Count(player);
    for (unsigned pass = 0; pass < 2 && Count(installed) < desired; ++pass) {
        for (unsigned type = 0; type < 7 && Count(installed) < desired; ++type) {
            const auto bit = 1u << type;
            if ((installed & bit) || bool(player & bit) != (pass == 0)) continue;
            if (canInstall(type)) installed |= bit;
        }
    }
    return installed;
}
}
