#pragma once
#include "LimitBreakerResourceApi.h"
namespace native_freeroam {
inline bool SupportsSixAudioSlots(unsigned result,const nfsmw_limitbreaker::ResourceLimitsV1& s) noexcept {
    return result&&s.structSize>=sizeof(s)&&s.abiVersion==nfsmw_limitbreaker::kResourceLimitsAbiV1&&
        (s.capabilities&nfsmw_limitbreaker::ResourceAudioMixerPoolExpanded)&&s.audioMixerPoolBytes>=0x00800000u;
}
inline unsigned ChooseAudioCeiling(bool enabled,bool optIn,bool capability) noexcept {
    return enabled&&optIn&&capability?6u:4u;
}
}
