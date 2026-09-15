#pragma once
#include <algorithm>
#include <cmath>
namespace native_freeroam::encounter_custom {
// Metres/second. This is a desired speed, never a physical velocity write.
inline float ChaseSpeed(float playerSpeed,float gap,float alignment,bool attack=false) noexcept {
    if(!std::isfinite(playerSpeed)||!std::isfinite(gap)||!std::isfinite(alignment)||
        gap<0||gap>300||alignment<.49f||alignment>1.01f) return -1;
    // During overtaking, do not converge to the player's speed as gap shrinks.
    const float closing=std::clamp(gap/8.f,attack?8.f:0.f,18.f);
    const float corner=.6f+.4f*std::clamp((alignment-.5f)/.35f,0.f,1.f);
    return std::clamp((std::abs(playerSpeed)+closing)*corner,0.f,100.f);
}
inline float RaiseChaseSpeed(float native,float desired) noexcept {
    // Keep native stop/reverse commands and all malformed inputs verbatim.
    // Collision recovery actions are additionally excluded by the owner gate.
    if(!std::isfinite(native)||native<=.1f||!std::isfinite(desired)||desired<0||desired>100) return native;
    return std::max(native,desired);
}
inline float AttackEntrySpeed(float desired,float entry,float seconds) noexcept {
    if(!std::isfinite(desired)||desired<0||desired>100)return desired;
    if(!std::isfinite(entry)||entry<0||!std::isfinite(seconds)||seconds<0)return desired;
    // Hold the previous request for 1s, then release its floor over another 1s.
    // This is not an override of native stop/reverse or recovery instructions.
    const float blend=std::clamp(seconds-1.f,0.f,1.f);
    const float floor=std::clamp(entry,0.f,100.f)*(1-blend)+desired*blend;
    return std::max(desired,floor);
}
}
