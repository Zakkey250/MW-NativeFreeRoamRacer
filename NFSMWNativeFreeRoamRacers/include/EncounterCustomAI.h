#pragma once
#include "EncounterRouteTrail.h"
#include <string_view>

namespace native_freeroam::encounter_custom {
using battle::Point;
using Destination=encounter_route::Trail::Destination;
enum class Mode { Stable, Custom };
inline Mode ParseMode(std::wstring_view value) noexcept {
    value=value.substr(0,value.find_first_of(L";#"));
    const auto first=value.find_first_not_of(L" \t\r\n");
    if(first==value.npos) return Mode::Stable;
    value.remove_prefix(first);
    value=value.substr(0,value.find_last_not_of(L" \t\r\n")+1);
    constexpr std::wstring_view custom=L"custom ai";
    if(value.size()!=custom.size()) return Mode::Stable;
    for(std::size_t i=0;i<value.size();++i) {
        const wchar_t c=value[i]>=L'A'&&value[i]<=L'Z'?value[i]+(L'a'-L'A'):value[i];
        if(c!=custom[i]) return Mode::Stable;
    }
    return Mode::Custom;
}
inline const char* Name(Mode mode) noexcept {return mode==Mode::Custom?"Custom AI":"Stable";}

// Ordered world-space breadcrumbs. No nearest-future search, player
// extrapolation, teleport, or allocation during a management update.
class Trail {
public:
    static constexpr std::size_t Capacity=2048;
    void Reset() noexcept {
        first_=count_=0;nextId_=1;issuedId_=0;sampled_=false;blocked_=false;
        latest_={};tangent_={};issued_={};
    }
    bool Record(Point player,Point heading) noexcept {
        if(!battle::Finite(player)||!battle::Finite(heading)) return false;
        if(sampled_&&battle::Length(battle::Sub(player,latest_))>80) {
            // Missing trajectory: never connect a teleport by an invented road.
            Reset();Append(player);latest_=player;tangent_=battle::UnitXZ(heading);sampled_=true;
            return false;
        }
        if(sampled_&&battle::Length(battle::Sub(player,latest_))>.1f)
            tangent_=battle::UnitXZ(battle::Sub(player,latest_));
        else if(!sampled_) tangent_=battle::UnitXZ(heading);
        latest_=player;sampled_=true;
        if(!count_||battle::Length(battle::Sub(player,At(count_-1).position))>=2) return Append(player);
        return !blocked_;
    }
    Destination Select(Point rival,float speed,Point forward) noexcept {
        if(!count_||!battle::Finite(rival)||!battle::Finite(forward)||!std::isfinite(speed)) return {};
        forward=battle::UnitXZ(forward);
        for(unsigned i=0;i<64&&count_>1&&Reached(rival,At(0).position,At(1).position,forward);++i) {
            first_=(first_+1)%Capacity;--count_;
        }
        // Overflow never silently drops unvisited corners or appends a gap.
        if(blocked_) return {};
        // Keep feeding a recorded straight span before the old endpoint is
        // reached. Waiting for arrival every 10-30m encourages braking. Sharp
        // turns still stop lookahead; never invent a point beyond the player.
        const float horizon=std::clamp(std::abs(speed)*1.5f+20.f,30.f,100.f);
        std::size_t index=0;float length=0;
        Point initial{};
        if(count_>1) initial=battle::UnitXZ(battle::Sub(At(1).position,At(0).position));
        for(std::size_t i=1;i<count_;++i) {
            const auto leg=battle::Sub(At(i).position,At(i-1).position);
            const auto offset=battle::Sub(At(i).position,At(0).position);
            const float lateral=std::abs(offset.x*initial.z-offset.z*initial.x);
            // Look ahead only on a nearly straight recorded span. Stop at the
            // actual corner rather than across an unvisited bend.
            if(length+battle::Length(leg)>horizon||lateral>1||
                battle::DotXZ(initial,battle::UnitXZ(leg))<.98f) break;
            length+=battle::Length(leg);index=i;
        }
        // Slowing down must not retract a previously issued destination. A
        // longer straight recorded span may extend it, but never cut a corner.
        if(issued_.valid&&issuedId_>=At(index).id) return issued_;
        Point heading=index?battle::UnitXZ(battle::Sub(At(index).position,At(index-1).position)):
            (count_>1?initial:tangent_);
        if(battle::Length(heading)<.9f) return {};
        issuedId_=At(index).id;issued_={At(index).position,heading,true};return issued_;
    }
    std::size_t size() const noexcept {return count_;}
    std::uint64_t cursor() const noexcept {return count_?At(0).id:0;}
    std::uint64_t target() const noexcept {return issuedId_;}
    bool blocked() const noexcept {return blocked_;}
private:
    struct Node {Point position{};std::uint64_t id=0;};
    const Node& At(std::size_t n) const noexcept {return points_[(first_+n)%Capacity];}
    bool Append(Point point) noexcept {
        if(blocked_||count_==Capacity) {blocked_=true;return false;}
        points_[(first_+count_)%Capacity]={point,nextId_++};++count_;return true;
    }
    static bool Reached(Point rival,Point point,Point next,Point forward) noexcept {
        if(std::abs(rival.y-point.y)<=2.5f&&battle::Length(battle::Sub(rival,point))<=3.5f) return true;
        const auto leg=battle::Sub(next,point),offset=battle::Sub(rival,point);
        const float square=battle::DotXZ(leg,leg);
        if(square<.01f||battle::DotXZ(forward,battle::UnitXZ(leg))<.5f) return false;
        const float t=battle::DotXZ(offset,leg)/square;
        if(t<0||t*std::sqrt(square)>30) return false;
        // MW rivals can pass on a parallel lane 5-7m off the player's line.
        // Widen only the passed-point corridor, not the near-point radius.
        // Retain ordered, forward-facing, <=30m and same-height progression.
        return std::hypot(offset.x-t*leg.x,offset.z-t*leg.z)<=12.f&&
            std::abs(offset.y-t*leg.y)<=2.5f;
    }
    std::array<Node,Capacity> points_{};
    std::size_t first_=0,count_=0;
    std::uint64_t nextId_=1,issuedId_=0;
    bool sampled_=false,blocked_=false;
    Point latest_{},tangent_{};
    Destination issued_{};
};
inline bool KeepPath(bool force,bool changed,bool stalled,bool valid,unsigned mode,
    bool crossed,int edges,float age) noexcept {
    return !force&&!changed&&!stalled&&valid&&mode==3&&!crossed&&edges>0&&edges<=510&&
        std::isfinite(age)&&age>=0&&age<4;
}
}
