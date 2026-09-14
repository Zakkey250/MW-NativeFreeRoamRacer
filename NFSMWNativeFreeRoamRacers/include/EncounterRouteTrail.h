#pragma once
#include "EncounterBattleModel.h"

namespace native_freeroam::encounter_route {
using battle::Point;
inline bool ReuseActivePath(bool force,bool stalled,unsigned mode,bool crossed,bool destinationSet,float change) noexcept {
    return !force&&!stalled&&mode==3&&!crossed&&destinationSet&&std::isfinite(change)&&change<10;
}
// Ordered, allocation-free route hints, NOT a second steering controller.
// Never select the globally nearest point: hairpins / bridges can overlap.
class Trail {
public:
    struct Destination { Point position{},heading{}; bool valid=false; };
    void Reset() noexcept { first_=count_=0; lastSample_={}; lastTangent_={}; sampled_=false; issued_={}; }
    bool Record(Point p,Point initialHeading={0,0,1}) noexcept {
        if(!battle::Finite(p)) return false;
        if(sampled_ && battle::Length(battle::Sub(p,lastSample_))>80) {
            Reset(); // teleport / missing-stream discontinuity: no invented connecting road
            lastSample_=p;sampled_=true;return false;
        }
        if(!sampled_) lastTangent_=battle::UnitXZ(initialHeading);
        else if(std::hypot(p.x-lastSample_.x,p.z-lastSample_.z)>.25f)
            lastTangent_=battle::UnitXZ(battle::Sub(p,lastSample_));
        lastSample_=p;sampled_=true;
        if(count_ && battle::Length(battle::Sub(p,At(count_-1)))<12) return true;
        if(count_==points_.size()) { Reset();lastSample_=p;sampled_=true;return false; }
        points_[(first_+count_)%points_.size()]=p;++count_;return true;
    }
    Destination Select(Point rival,float speed=0,Point forward={}) noexcept {
        if(!battle::Finite(rival)||!count_) return {};
        forward=battle::Finite(forward)?battle::UnitXZ(forward):Point{};
        while(count_>1 && Reached(rival,At(0),At(1),forward)) {first_=(first_+1)%points_.size();--count_;}
        // A slowdown must not move an already-issued destination backwards.
        // Keep the exact position AND tangent until reached/passed. Native
        // collision recovery may reverse the car; that is not a new route.
        if(issued_.valid) {
            const Point beyond{issued_.position.x+issued_.heading.x*12,issued_.position.y,
                               issued_.position.z+issued_.heading.z*12};
            if(!Reached(rival,issued_.position,beyond,forward)) return issued_;
            issued_={};
        }
        // A short look-ahead avoids stopping at each 12 m sample. It is bounded
        // by recorded route length, never by straight-line proximity to the player.
        const float horizon=std::isfinite(speed)?std::clamp(std::abs(speed)*1.25f+20.f,30.f,100.f):30.f;
        std::size_t index=0;float lookAhead=0;
        while(index+1<count_) {
            // Do not let a high-speed horizon look across an unvisited sharp turn.
            if(index && battle::DotXZ(battle::UnitXZ(battle::Sub(At(index),At(index-1))),
                battle::UnitXZ(battle::Sub(At(index+1),At(index))))<.5f) break;
            const float segment=battle::Length(battle::Sub(At(index+1),At(index)));
            if(lookAhead+segment>horizon) break;
            lookAhead+=segment;++index;
        }
        Point direction{};
        if(index+1<count_) direction=battle::Sub(At(index+1),At(index));
        else direction=lastTangent_; // terminal position is the latest sample too
        direction=battle::UnitXZ(direction);
        if(battle::Length(direction)<.9f) return {};
        issued_={index+1==count_?lastSample_:At(index),direction,true};
        return issued_;
    }
    std::size_t size() const noexcept {return count_;}
private:
    Point At(std::size_t n) const noexcept {return points_[(first_+n)%points_.size()];}
    static bool Reached(Point rival,Point p,Point next,Point forward) noexcept {
        if(battle::Length(battle::Sub(rival,p))<=14 && std::abs(rival.y-p.y)<=5) return true;
        const auto segment=battle::Sub(next,p), offset=battle::Sub(rival,p);
        const float square=battle::DotXZ(segment,segment);
        if(square<1) return false;
        const float t=battle::DotXZ(offset,segment)/square;
        if(t<0) return false;
        // Consume passed collinear samples in order, never jump to a nearest
        // future hairpin / bridge. A distant passed sample also needs matching heading.
        if(t>1.5f && battle::DotXZ(forward,battle::UnitXZ(segment))<.7f) return false;
        const Point projection{p.x+t*segment.x,p.y+t*segment.y,p.z+t*segment.z};
        return std::hypot(rival.x-projection.x,rival.z-projection.z)<=10 &&
            std::abs(rival.y-projection.y)<=5;
    }
    std::array<Point,128> points_{};
    std::size_t first_=0,count_=0;
    Point lastSample_{},lastTangent_{};bool sampled_=false;
    Destination issued_{};
};
}
