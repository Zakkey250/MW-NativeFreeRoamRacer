#pragma once
#include <cstdint>
#include <limits>

namespace native_freeroam::encounter_wager {
// Existing BL base rewards are multiples of 100, so each 5% increment is
// integral (300 -> 315 -> 330). No rounding and no configurable multiplier cap.
inline bool Quote(unsigned base,std::uint64_t wins,unsigned* amount) noexcept {
    if(!amount||base<300||base>3000||base%100)return false;
    constexpr auto limit=std::uint64_t(std::numeric_limits<std::int32_t>::max());
    const auto step=base/20;
    if(wins>(limit-base)/step)return false; // Native cash representation only.
    *amount=base+static_cast<unsigned>(wins*step);return true;
}
inline bool CanStart(std::int32_t cash,unsigned amount) noexcept {
    return cash>=0&&amount>=300&&amount<=unsigned(std::numeric_limits<std::int32_t>::max())&&
        unsigned(cash)>=amount&&unsigned(cash)<=unsigned(std::numeric_limits<std::int32_t>::max())-amount;
}
inline bool Transfer(std::int32_t before,unsigned amount,bool won,std::int32_t* after) noexcept {
    if(!after||before<0||amount<300||amount>unsigned(std::numeric_limits<std::int32_t>::max()))return false;
    if(won) {
        if(unsigned(before)>unsigned(std::numeric_limits<std::int32_t>::max())-amount)return false;
        *after=before+static_cast<std::int32_t>(amount);
    } else {
        if(unsigned(before)<amount)return false;
        *after=before-static_cast<std::int32_t>(amount);
    }
    return true;
}
struct Session {
    std::uintptr_t profile=0;
    std::uint64_t wins=0;
    void Reset() noexcept {profile=0;wins=0;}
    void Bind(std::uintptr_t current) noexcept {if(profile!=current){Reset();profile=current;}}
    void Complete(bool won,bool paid) noexcept {
        if(won&&paid&&wins<std::numeric_limits<std::uint64_t>::max())++wins;
        else wins=0;
    }
};
}
