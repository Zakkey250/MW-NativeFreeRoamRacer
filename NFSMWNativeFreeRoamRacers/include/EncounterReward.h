#pragma once
namespace native_freeroam::encounter_reward {
// Linear BL15=300 -> BL1=3000, floor to hundreds without floating error.
constexpr unsigned Amount(bool custom,unsigned bin) noexcept {
    if(!custom) return 1000;
    if(bin<1||bin>15) return 300; // Unknown/prologue: never grant a top-tier reward.
    return ((4200u+(15u-bin)*2700u)/1400u)*100u;
}
}
