#pragma once

#include <cstdint>

namespace nfsmw_limitbreaker {

constexpr std::uint32_t kResourceLimitsAbiV1 = 1u;

enum ResourceCapabilityV1 : std::uint32_t {
    ResourceAudioMixerPoolExpanded = 1u << 0,
    ResourceDynamicPropBodiesExpanded = 1u << 1,
    ResourceCarLoadedRideInfoExpanded = 1u << 2,
};

struct ResourceLimitsV1 {
    std::uint32_t structSize;
    std::uint32_t abiVersion;
    std::uint32_t capabilities;
    std::uint32_t audioMixerPoolBytes;
    std::uint32_t dynamicPropBodyLimit;
    std::uint32_t carLoadedRideInfoSlots;
    std::uint32_t logicalVehicleLimit;
};

using QueryResourceLimitsV1 = std::uint32_t(__cdecl*)(
    ResourceLimitsV1* output, std::uint32_t outputBytes);

} // namespace nfsmw_limitbreaker

extern "C" std::uint32_t __cdecl NFSMWLB_QueryResourceLimitsV1(
    nfsmw_limitbreaker::ResourceLimitsV1* output,
    std::uint32_t outputBytes) noexcept;
