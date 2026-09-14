#pragma once
#include "EncounterRouteTrail.h"

namespace native_freeroam::encounter_route {
// Position is a RoadNav destination, NEVER a steering vector. No breadcrumb
// cursor: after a missed junction the native graph can find another road.
class Pursuit {
public:
    void Reset() noexcept { passing_=false; closeSeconds_=passSeconds_=cooldown_=0; }
    bool passing() const noexcept { return passing_; }
    Trail::Destination Update(Point player,Point heading,Point rival,Point rivalHeading,float dt=.05f) noexcept {
        if(!battle::Finite(player)||!battle::Finite(rival)||
            !battle::Finite(heading)||!battle::Finite(rivalHeading)) {Reset();return {};}
        const auto forward=battle::UnitXZ(heading), other=battle::UnitXZ(rivalHeading);
        const float gap=battle::Length(battle::Sub(player,rival));
        const float aligned=forward.x*other.x+forward.z*other.z;
        dt=std::isfinite(dt)?std::clamp(dt,0.f,.25f):0.f;
        cooldown_=std::max(0.f,cooldown_-dt);
        const auto offset=battle::Sub(player,rival);
        const float lateral=std::abs(offset.x*forward.z-offset.z*forward.x);
        // Close cars on opposite roads / stacked roads are not a passing chance.
        const bool sameDirection=aligned>=.85f&&std::abs(player.y-rival.y)<=2;
        if(passing_) {
            passSeconds_+=dt;
            if(gap>=18||!sameDirection||lateral>5||passSeconds_>=1.5f) {
                passing_=false;closeSeconds_=0;cooldown_=1;
            }
        } else {
            // Geometric corridor, not proof of road connectivity. Require a
            // sustained opportunity, and bound Direction mode at junctions.
            if(gap<=5&&sameDirection&&lateral<=3&&cooldown_<=0) closeSeconds_+=dt;
            else closeSeconds_=0;
            if(closeSeconds_>=.25f){passing_=true;passSeconds_=0;}
        }
        // Hysteresis leaves room to actually pass without 5m boundary chatter.
        // Native Direction/Racer chooses the road and collision avoidance during
        // this phase; do not invent an ahead/lateral point through a wall.
        if(passing_) return {};
        return {player,forward,true};
    }
private:
    bool passing_=false;
    float closeSeconds_=0,passSeconds_=0,cooldown_=0;
};
}
