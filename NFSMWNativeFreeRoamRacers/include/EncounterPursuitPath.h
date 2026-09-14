#pragma once
#include <cmath>
#include <algorithm>
namespace native_freeroam::encounter_route {
// A submitted request is not proof of a completed route. Only keep a native
// Path with actual edges, while its old endpoint is still ahead of consumption.
inline bool KeepPursuitPath(bool force,bool stalled,unsigned mode,bool crossed,
    int edges,bool owned,float endpointDistance,float seconds) noexcept {
    return !force&&!stalled&&owned&&mode==3&&!crossed&&edges>0&&edges<=510&&
        std::isfinite(endpointDistance)&&endpointDistance>40&&
        std::isfinite(seconds)&&seconds>=0&&seconds<3;
}
// Preserve a useful completed path, but refresh a moving endpoint before the
// rival reaches it. These are graph-query policy limits, not steering/boost.
inline bool KeepMovingPursuitPath(bool force,bool stalled,unsigned mode,bool crossed,
    int edges,bool owned,float endpointDistance,float seconds,float moved,
    float headingDot,float rivalSpeed,float playerGap) noexcept {
    if(!KeepPursuitPath(force,stalled,mode,crossed,edges,owned,endpointDistance,seconds)) return false;
    if(!std::isfinite(moved)||!std::isfinite(headingDot)||!std::isfinite(rivalSpeed)||
       !std::isfinite(playerGap)||moved<0||playerGap<0) return false;
    const float horizon=std::clamp(std::abs(rivalSpeed)*1.2f,40.f,120.f);
    const float driftLimit=std::clamp(playerGap*.3f,25.f,80.f);
    return endpointDistance>horizon&&moved<driftLimit&&!(moved>=12&&headingDot<.75f);
}
}
