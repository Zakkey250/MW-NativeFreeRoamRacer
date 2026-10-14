#pragma once
#include <cmath>
#include <cstdint>
namespace native_freeroam::encounter_weapons {
constexpr bool PlayerEmpCaller(unsigned rva) noexcept {return rva==0xC979;}
constexpr bool PlayerShockCaller(unsigned rva) noexcept {return rva==0x8F89;}
constexpr bool RivalEmpTarget(unsigned type,std::uintptr_t target,std::uintptr_t rivalVehicle) noexcept {
    return type==3&&rivalVehicle>0xAC&&target==rivalVehicle-0xAC;
}
constexpr bool RivalShockTarget(std::uintptr_t victim,std::uintptr_t source,
                               std::uintptr_t rivalSim,std::uintptr_t playerSim) noexcept {
    return victim!=source&&rivalSim>0x2C&&playerSim>0x2C&&victim==rivalSim-0x2C&&source==playerSim-0x2C;
}
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
