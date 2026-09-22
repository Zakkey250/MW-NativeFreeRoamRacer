#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace native_freeroam {
struct RefillPacing {
    float sinceSpawn=1000.0f,shortage=0.0f;
    void Tick(float dt,std::size_t count,std::size_t maximum) noexcept {
        if(!std::isfinite(dt)||dt<=0)return;
        sinceSpawn=std::min(1000.0f,sinceSpawn+dt);
        shortage=count<maximum?std::min(1000.0f,shortage+dt):0.0f;
    }
    static float Interval(float configured,std::size_t count,std::size_t maximum) noexcept {
        if(!count)return std::min(configured,1.0f);
        if(count<maximum&&maximum-count>=(maximum+1)/2)return std::min(configured,2.0f);
        return configured;
    }
    bool Ready(float configured,std::size_t count,std::size_t maximum) const noexcept {
        return count<maximum&&sinceSpawn>=Interval(configured,count,maximum);
    }
    void Spawned() noexcept {sinceSpawn=0.0f;shortage=0.0f;}
};
// Lower rank wins; -1 excludes. No reversed heading, distance expansion, or
// stopped-traffic fallback. Only the delayed lateral-heading fallback needs
// explicit offscreen evidence in addition to the unchanged distance guard.
inline int RefillAnchorRank(float distance,float aheadDot,float headingDot,float minimum,float maximum,
                            float requiredHeading,float shortage,bool offscreen) noexcept {
    if(!std::isfinite(distance)||!std::isfinite(aheadDot)||!std::isfinite(headingDot)||
       distance<minimum||distance>maximum)return -1;
    if(headingDot>=requiredHeading){
        if(aheadDot>=0.25f)return distance<=std::min(maximum,minimum+125.0f)?0:1;
        return aheadDot>=-0.25f?2:3;
    }
    if(shortage>=10.0f&&aheadDot>=0.25f&&headingDot>=0.0f&&offscreen)return 2;
    return -1;
}
}
