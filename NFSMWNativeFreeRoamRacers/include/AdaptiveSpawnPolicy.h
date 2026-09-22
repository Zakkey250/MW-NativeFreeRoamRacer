#pragma once
#include <cstdint>
namespace native_freeroam {
// A timing hint only. An allowed opportunity must still pass every population,
// anchor, career, lifecycle and construction guard. No sleep and no reservation.
struct AdaptiveSpawnPolicy {
    std::uint64_t lastObservation=0,pressureUntil=0,deadline=0,lastFrameUs=0,triggerFrameUs=0;
    std::uint64_t deferrals=0,forcedOpportunities=0;
    bool sampled=false,waiting=false;
    void CancelDeferral() noexcept {waiting=false;deadline=0;}
    void Observe(std::uint64_t frameUs,std::uint64_t now) noexcept {
        if(!frameUs||frameUs>2000000||(sampled&&now<lastObservation)){
            sampled=false;pressureUntil=0;CancelDeferral();return;
        }
        sampled=true;lastObservation=now;lastFrameUs=frameUs;
        if(frameUs>=50000){pressureUntil=now+750;triggerFrameUs=frameUs;}
    }
    bool Defer(bool enabled,bool inProgress,std::uint64_t now) noexcept {
        if(!enabled||inProgress||!sampled||now<lastObservation||
            now-lastObservation>1000||now>=pressureUntil){CancelDeferral();return false;}
        if(!waiting){waiting=true;deadline=now+2000;}
        if(now>=deadline){CancelDeferral();++forcedOpportunities;return false;}
        ++deferrals;return true;
    }
};
}
