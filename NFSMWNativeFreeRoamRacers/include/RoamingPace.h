#pragma once
#include <algorithm>
#include <cmath>
#include <cwchar>
namespace native_freeroam::roaming_pace {
inline float Parse(const wchar_t* text,float fallback,float minimum,float maximum) noexcept {
    if(!text) return fallback;
    wchar_t* end=nullptr;const float value=std::wcstof(text,&end);
    if(end==text||!std::isfinite(value)) return fallback;
    while(*end==L' '||*end==L'\t'||*end==L'\r'||*end==L'\n') ++end;
    if(*end&&*end!=L';'&&*end!=L'#') return fallback;
    return std::clamp(value,minimum,maximum);
}
inline float CruiseSpeed(float native,float percent) noexcept {
    // Native stop/reverse/recovery sentinels must not be turned into acceleration.
    if(!std::isfinite(native)||native<=.1f||!std::isfinite(percent)||percent<1||percent>100) return native;
    return native*(percent/100.f);
}
inline bool SpeedMatched(float player,float rival,float toleranceKmh) noexcept {
    return std::isfinite(player)&&std::isfinite(rival)&&player>=0&&rival>=0&&
        std::isfinite(toleranceKmh)&&toleranceKmh>=0&&toleranceKmh<=100&&
        std::abs(player-rival)*3.6f<=toleranceKmh+.0001f;
}
}
