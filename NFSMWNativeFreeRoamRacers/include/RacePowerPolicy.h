#pragma once
#include <cmath>
namespace native_freeroam::race_power {
inline unsigned Tier(float gap) noexcept {return gap<=100?0:gap<=200?1:2;}
inline float NeoOutput(float native,bool owned) noexcept {
 // Exactly one constant multiplier on positive engine output. Engine drag,
 // braking and vehicles outside NeoAI ownership keep their original values.
 return owned&&std::isfinite(native)&&native>0&&native<1e8f?native*2.f:native;
}
inline float Scale(float native,float scale) noexcept {
 if(!std::isfinite(native)||native<0||native>1||!std::isfinite(scale)||scale<=1||scale>2)return native;
 return (native+2.f)*scale-2.f;
}
}
