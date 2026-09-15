#pragma once
#include "EncounterCustomAI.h"

namespace native_freeroam::encounter_custom {
// Direction guidance, not mandatory checkpoints or a traffic-road path.
class DirectionalTrail {
public:
    static constexpr std::size_t Capacity=2048;
    void Reset() noexcept {first_=count_=0;nextId_=1;targetId_=0;sampled_=false;latest_={};tangent_={};}
    bool Record(Point player,Point heading) noexcept {
        if(!battle::Finite(player)||!battle::Finite(heading)) return false;
        if(sampled_&&battle::Length(battle::Sub(player,latest_))>80) {
            Reset();Record(player,heading);return false;
        }
        const auto movement=battle::Sub(player,latest_);
        if(!sampled_) tangent_=battle::UnitXZ(heading);
        else if(battle::Length(movement)>.1f) tangent_=battle::UnitXZ(movement);
        latest_=player;sampled_=true;
        if(count_&&battle::Length(battle::Sub(player,At(count_-1).position))<2) return true;
        // Old points are expendable; never freeze on unvisited-buffer overflow.
        if(count_==Capacity) {first_=(first_+1)%Capacity;--count_;}
        points_[(first_+count_)%Capacity]={player,tangent_,nextId_++};++count_;return true;
    }
    Destination Select(Point rival,float speed,Point forward) noexcept {
        if(!count_||!battle::Finite(rival)||!battle::Finite(forward)||!std::isfinite(speed)) return {};
        forward=battle::UnitXZ(forward);
        if(battle::Length(forward)<.9f) return {};
        std::size_t best=0;float bestT=0,bestScore=1e30f,arc=0;
        bool matched=false;
        // Bounded local reacquisition, not a global nearest point on a loop.
        for(std::size_t i=0;i+1<count_&&i<256&&arc<=500;++i) {
            const auto a=At(i).position,leg=battle::Sub(At(i+1).position,a);
            const float length=battle::Length(leg),square=battle::DotXZ(leg,leg);
            arc+=length;if(square<.01f) continue;
            if(battle::DotXZ(forward,battle::UnitXZ(leg))<.15f) continue;
            const auto offset=battle::Sub(rival,a);
            const float t=std::clamp(battle::DotXZ(offset,leg)/square,0.f,1.f);
            const Point projected{a.x+t*leg.x,a.y+t*leg.y,a.z+t*leg.z};
            const auto error=battle::Sub(rival,projected);
            const float lateral=std::hypot(error.x,error.z);
            if(lateral>80||std::abs(error.y)>5) continue;
            const float score=lateral+arc*.002f;
            if(score<bestScore) {matched=true;bestScore=score;best=i;bestT=t;}
        }
        first_=(first_+best)%Capacity;count_-=best;
        const float lookAhead=std::clamp(std::abs(speed)*.8f+12.f,20.f,60.f);
        Point goal=At(0).position,routeDirection=At(0).heading;
        std::size_t target=0;
        if(matched&&count_>1) {
            const auto leg=battle::Sub(At(1).position,goal);
            goal={goal.x+leg.x*bestT,goal.y+leg.y*bestT,goal.z+leg.z*bestT};
            routeDirection=battle::UnitXZ(leg);
            float remaining=lookAhead;
            for(std::size_t i=1;i<count_&&i<=64;++i) {
                const auto span=battle::Sub(At(i).position,goal);
                const float length=battle::Length(span);target=i;
                if(length>remaining&&length>.01f) {
                    const float t=remaining/length;
                    goal={goal.x+t*span.x,goal.y+t*span.y,goal.z+t*span.z};break;
                }
                goal=At(i).position;remaining-=length;
                if(remaining<=0) break;
            }
        }
        auto desired=battle::UnitXZ(battle::Sub(goal,rival));
        // A passed/behind point is never a U-turn command. Keep forward travel
        // until recorded, similarly oriented segments can be reacquired.
        if(!matched||battle::DotXZ(forward,desired)<.15f||battle::Length(desired)<.9f)
            desired=battle::DotXZ(forward,routeDirection)>=.15f?routeDirection:forward;
        if(battle::DotXZ(forward,desired)<.5f) {
            const float sign=(forward.z*desired.x-forward.x*desired.z)>=0?1.f:-1.f;
            desired={forward.x*.5f+forward.z*.8660254f*sign,0,
                forward.z*.5f-forward.x*.8660254f*sign};
        }
        targetId_=At(target).id;
        // Synthetic forward aim, not a checkpoint the car must physically touch.
        return {{rival.x+desired.x*lookAhead,rival.y,rival.z+desired.z*lookAhead},desired,true};
    }
    std::size_t size() const noexcept {return count_;}
    std::uint64_t cursor() const noexcept {return count_?At(0).id:0;}
    std::uint64_t target() const noexcept {return targetId_;}
    bool blocked() const noexcept {return false;}
private:
    struct Node {Point position{},heading{};std::uint64_t id=0;};
    const Node& At(std::size_t n) const noexcept {return points_[(first_+n)%Capacity];}
    std::array<Node,Capacity> points_{};
    std::size_t first_=0,count_=0;
    std::uint64_t nextId_=1,targetId_=0;
    bool sampled_=false;
    Point latest_{},tangent_{};
};
}
