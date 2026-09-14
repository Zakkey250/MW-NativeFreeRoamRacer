#pragma once
#include <cmath>
namespace native_freeroam::encounter_route {
enum class CommitDecision : unsigned { Invalid, Forced, Stalled, GoalConsumed,
    TimeLimit, RoadChanged, HoldPath, HoldDirection };
inline bool Holding(CommitDecision d) noexcept {
    return d==CommitDecision::HoldPath||d==CommitDecision::HoldDirection;
}
// RoadNav segments are curve pieces, not whole named roads. Use a minimum dwell
// and physical progress before accepting a segment change as a replan boundary.
// Direction is valid native racer navigation too; it does not prove a Path.
inline CommitDecision CommitRoute(bool force,bool stalled,bool owned,bool valid,
    unsigned mode,bool crossed,int edges,int originSegment,int segment,
    float seconds,float progress,float endpointDistance) noexcept {
    if(force||!owned) return CommitDecision::Forced;
    if(stalled) return CommitDecision::Stalled;
    if(!valid||(mode!=2&&mode!=3)||originSegment<0||segment<0||
       edges<0||edges>510||(mode==3&&edges==0)||
       !std::isfinite(seconds)||seconds<0||!std::isfinite(progress)||progress<0||
       !std::isfinite(endpointDistance)||endpointDistance<0) return CommitDecision::Invalid;
    if(crossed||(mode==3&&endpointDistance<=12)) return CommitDecision::GoalConsumed;
    if(seconds>=4) return CommitDecision::TimeLimit;
    if(seconds>=2&&progress>=40&&originSegment!=segment) return CommitDecision::RoadChanged;
    return mode==3?CommitDecision::HoldPath:CommitDecision::HoldDirection;
}
}
