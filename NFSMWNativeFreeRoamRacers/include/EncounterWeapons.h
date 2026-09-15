#pragma once
#include <cmath>
namespace native_freeroam::encounter_weapons {
constexpr bool PlayerEmpCaller(unsigned rva) noexcept {return rva==0xC979;}
constexpr bool PlayerShockCaller(unsigned rva) noexcept {return rva==0x8F89;}
constexpr bool Forfeit(bool active,bool playerSource,bool affected,bool otherCar) noexcept {
    return active&&playerSource&&affected&&otherCar;
}
struct EmpSchedule {
    unsigned stock=3;float cooldown=5,remaining=0;bool locked=false;
    void Tick(float dt) noexcept {
        if(!std::isfinite(dt)||dt<=0||dt>.5f)return;
        cooldown=std::fmax(0.f,cooldown-dt);remaining=std::fmax(0.f,remaining-dt);
    }
    bool Start(float delay) noexcept {
        if(locked||!stock||cooldown>0||!std::isfinite(delay)||delay<0||delay>30)return false;
        --stock;remaining=delay;locked=true;return true;
    }
    void Finish() noexcept {locked=false;remaining=0;cooldown=20;}
};
}
