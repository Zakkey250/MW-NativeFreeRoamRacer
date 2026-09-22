#pragma once
#include <algorithm>
#include <cstdint>
namespace limitbreaker {
struct PopulationLease {
    std::uint32_t owner=0,slots=0;
    std::uint64_t until=0;
    std::uint32_t Active(std::uint64_t now) const {return now<until?slots:0;}
    bool Refresh(std::uint32_t who,std::uint32_t missing,std::uint64_t now) {
        if(!who||missing>15)return false;
        if(Active(now)&&owner!=who)return false;
        if(!missing){owner=slots=0;until=0;return true;}
        owner=who;slots=missing;until=now+2000;return true;
    }
};
inline bool TrafficAllowed(bool native,std::uint32_t live,std::uint32_t limit,std::uint32_t reserved) {
    if(!native||!reserved)return native;
    return limit>reserved&&live<limit-reserved;
}
}
