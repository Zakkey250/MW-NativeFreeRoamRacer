#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include "EncounterEligibility.h"

// Deterministic domain model, integrated by the alpha.26 experimental adapter.
// Real-game route/leader acceptance remains separate from these offline tests.
// No game addresses, cash writes, AI calls, allocation, or rendering here.
namespace native_freeroam::battle {
struct Point { float x=0, y=0, z=0; };
struct Identity {
    std::uintptr_t vehicle=0, simable=0;
    std::uint32_t key=0;
    bool operator==(const Identity&) const = default;
};
struct Car {
    Identity identity{};
    Point position{}, forward{};
    float speed=0;
    bool present=true;
};
struct Sample { Car player{}, rival{}; bool freeRoam=true, paused=false; };
enum class Phase { Idle, Active, Won, Lost, Cancelled };
enum class Leader { Rival, Player };
enum class Event { None, Started, LeadChanged, Won, Lost, Cancelled };
enum class Reason { None, InvalidSample, IdentityChanged, WorldEnded, Discontinuity };
struct Change {
    Event event=Event::None;
    Reason reason=Reason::None;
    // An intent, not payment. Adapter must consume each finish exactly once.
    unsigned cashIntent=0;
};
inline Point Sub(Point a,Point b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline float Length(Point a) {return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
inline float DotXZ(Point a,Point b) {return a.x*b.x+a.z*b.z;}
inline bool Finite(Point a) {return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline Point UnitXZ(Point a) {
    const float n=std::hypot(a.x,a.z);
    return std::isfinite(n)&&n>0.001f?Point{a.x/n,0,a.z/n}:Point{};
}
inline bool Valid(const Car& c) {
    return c.present && c.identity.vehicle && c.identity.simable && Finite(c.position) &&
        Finite(c.forward) && Length(UnitXZ(c.forward))>0.9f && std::isfinite(c.speed) && c.speed>=0;
}
inline bool ValidPosition(const Car& c) {
    return c.present && c.identity.vehicle && c.identity.simable && Finite(c.position);
}
// MW substitute for UG2's two comparable progress values (005F82D0, +24).
// Both cars project onto ONE rolling leader trail. Body heading, spin state,
// throttle and temporary AI Action availability do not determine the role.
// This is sampled world geometry, not a decoded native UG2 road-progress object.
class SharedTrail {
public:
    struct Projection {float progress=0,lateral=0,height=0;bool valid=false;};
    void Seed(Point rear,Point front,Point axis) {
        count_=2;offset_=0;axis_=UnitXZ(axis);
        const float behind=std::max(30.0f,DotXZ(Sub(front,rear),axis_));
        points_[0]={front.x-axis_.x*behind,rear.y,front.z-axis_.z*behind};
        points_[1]=front;
    }
    void Extend(Point front) {
        if(count_<2) return;
        const auto delta=Sub(front,points_[count_-1]);
        if(std::hypot(delta.x,delta.z)<2 || DotXZ(UnitXZ(delta),axis_)<0) return;
        if(count_==points_.size()) {
            offset_+=Length(Sub(points_[1],points_[0]));
            for(std::size_t i=1;i<count_;++i) points_[i-1]=points_[i];
            --count_;
        }
        points_[count_++]=front;axis_=UnitXZ(delta);
    }
    Projection Project(Point position) const {
        Projection best{};float error=std::numeric_limits<float>::infinity(),base=offset_;
        for(std::size_t i=1;i<count_;++i) {
            const auto a=points_[i-1],d=Sub(points_[i],a),r=Sub(position,a);
            const float horizontal=d.x*d.x+d.z*d.z,length=Length(d);
            if(horizontal<0.01f) continue;
            float t=DotXZ(r,d)/horizontal;
            // Extrapolate only the open ends, so a close pass can go beyond the
            // newest point without projecting around a distant hairpin.
            t=std::clamp(t,i==1?-60.0f/length:0.0f,i+1==count_?1+60.0f/length:1.0f);
            const Point residual{r.x-d.x*t,r.y-d.y*t,r.z-d.z*t};
            const float distance=Length(residual);
            if(distance<=error) {
                error=distance;best={base+length*t,std::hypot(residual.x,residual.z),std::abs(residual.y),true};
            }
            base+=length;
        }
        return best;
    }
    Point axis() const {return axis_;}
private:
    std::array<Point,128> points_{};
    std::size_t count_=0;
    float offset_=0;
    Point axis_{};
};
class Model {
public:
    static constexpr float separationToFinish=300.0f;
    static constexpr unsigned fixedReward=1000;
    Phase phase() const {return phase_;}
    Leader leader() const {return leader_;}
    float gap() const {return gap_;}
    float signedProgress() const {return signedProgress_;}
    bool progressValid() const {return progressValid_;}
    bool roleTrusted() const {return roleTrusted_;}
    void Reset() {*this=Model{};}
    Change Cancel(Reason reason) {
        if(phase_!=Phase::Active) return {};
        phase_=Phase::Cancelled; passSeconds_=0;
        return {Event::Cancelled,reason,0};
    }
    Change Start(const Sample& s) {
        if(phase_==Phase::Active || !s.freeRoam || s.paused || !Valid(s.player) || !Valid(s.rival) ||
            s.player.identity.vehicle==s.rival.identity.vehicle) return {};
        const auto relative=Sub(s.rival.position,s.player.position);
        const auto axis=UnitXZ(s.rival.forward);
        const float separation=Length(relative), behind=DotXZ(relative,axis);
        if(!std::isfinite(separation) || separation>60 || behind<1 ||
            std::abs(relative.y)>eligibility::HeightLimit(relative.x,relative.z) ||
            std::abs(relative.x*axis.z-relative.z*axis.x)>20 ||
            DotXZ(UnitXZ(s.player.forward),axis)<=0) return {};
        Reset(); phase_=Phase::Active; leader_=Leader::Rival; previous_=s;
        gap_=separation; trail_.Seed(s.player.position,s.rival.position,axis);
        return {Event::Started,Reason::None,0};
    }
    Change Step(const Sample& s,float dt) {
        if(phase_!=Phase::Active) return {};
        if(!s.freeRoam) return Cancel(Reason::WorldEnded);
        // Invalid/missing physics observations are not race outcomes. Do not
        // dereference missing objects in the adapter; wait for a valid sample.
        if(!ValidPosition(s.player)||!ValidPosition(s.rival)) {resample_=true;return {};}
        if(s.player.identity!=previous_.player.identity || s.rival.identity!=previous_.rival.identity)
            return Cancel(Reason::IdentityChanged);
        if(s.paused) {previous_=s;resample_=true;return {};}
        if(!std::isfinite(dt)||dt<=0) return {};
        if(dt>0.5f||resample_) {previous_=s;resample_=false;passSeconds_=0;return {};}
        if(Jumped(previous_.player,s.player,dt)||Jumped(previous_.rival,s.rival,dt))
        {
            const auto front=leader_==Leader::Player?s.player.position:s.rival.position;
            const auto rear=leader_==Leader::Player?s.rival.position:s.player.position;
            trail_.Seed(rear,front,trail_.axis());previous_=s;passSeconds_=0;return {};
        }
        const auto& front=leader_==Leader::Player?s.player:s.rival;
        const auto& rear=leader_==Leader::Player?s.rival:s.player;
        trail_.Extend(front.position);
        const auto relative=Sub(rear.position,front.position);
        gap_=Length(relative);
        if(!std::isfinite(gap_)) return {};
        const auto p=trail_.Project(s.player.position),r=trail_.Project(s.rival.position);
        signedProgress_=p.progress-r.progress;
        progressValid_=p.valid&&r.valid&&p.lateral<=12&&r.lateral<=12&&p.height<=4&&r.height<=4;
        // An unobserved close pass must not become a loss from the initial role.
        // Distant projections naturally leave the short trail: preserve the last
        // trusted close comparison there, not an always-invalid initial guess.
        // Trust is evidence of an observed role, not the validity of this frame's
        // projection. A crash/off-trail sample must not erase a confirmed pass
        // and permanently suppress the 300m result after leaving the close zone.
        if(gap_<=60&&progressValid_) roleTrusted_=true;
        const float rearAhead=leader_==Leader::Player?-signedProgress_:signedProgress_;
        Change result{};
        if(progressValid_&&gap_<=60&&rearAhead>=2) {
            passSeconds_+=dt;
            if(passSeconds_>=0.1f) {
                leader_=leader_==Leader::Player?Leader::Rival:Leader::Player;
                passSeconds_=0;result.event=Event::LeadChanged;
            }
        } else {
            passSeconds_=0;
        }
        previous_=s;
        // Never infer the winner by projecting across a distant turn or hairpin.
        // Keep the last locally confirmed role until a real close pass is observed.
        if(gap_>=separationToFinish&&roleTrusted_) {
            phase_=leader_==Leader::Player?Phase::Won:Phase::Lost;
            return {phase_==Phase::Won?Event::Won:Event::Lost,Reason::None,
                    phase_==Phase::Won?fixedReward:0};
        }
        return result;
    }
private:
    static bool Jumped(const Car& before,const Car& now,float dt) {
        const float distance=Length(Sub(now.position,before.position));
        const float a=std::isfinite(before.speed)?std::abs(before.speed):0;
        const float b=std::isfinite(now.speed)?std::abs(now.speed):0;
        const float bound=std::max(25.0f,std::max(a,b)*dt*3+10);
        return !std::isfinite(distance)||distance>bound;
    }
    Phase phase_=Phase::Idle;
    Leader leader_=Leader::Rival;
    Sample previous_{};
    SharedTrail trail_{};
    float gap_=0,passSeconds_=0,signedProgress_=0;
    bool progressValid_=false,resample_=false,roleTrusted_=false;
};
}
