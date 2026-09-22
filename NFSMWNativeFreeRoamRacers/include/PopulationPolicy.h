#pragma once
#include "LimitBreakerPopulationApi.h"
#include <algorithm>
namespace native_freeroam {
// Reservations are for a prepared construction transaction, never the target
// population deficit. Otherwise inactive traffic cache entries can consume
// the entire admission budget and starve the moving anchors we depend on.
struct PopulationTransaction {
    std::uint64_t deadline=0;
    bool Awaiting(std::uint64_t now) const noexcept {return deadline&&now<deadline;}
    bool Expired(std::uint64_t now) const noexcept {return deadline&&now>=deadline;}
    unsigned Requested(std::uint64_t now) const noexcept {return Awaiting(now)?1u:0u;}
    void Begin(std::uint64_t now) noexcept {deadline=now+1500;}
    void Reset() noexcept {deadline=0;}
};
enum class PopulationAccess { Legacy, Ready, Full, Unsafe };
// A share of the negotiated total limit for NON-NFRR vehicles, NOT a provider
// lease and NOT a requirement to keep empty slots while native traffic refills.
inline unsigned EffectiveNativeHeadroom(unsigned requested,unsigned limit) noexcept {
    return limit?std::min(requested,limit-1):0u;
}
inline bool NativeHeadroomReached(PopulationAccess access,unsigned limit,
                                  std::uint64_t managed,unsigned requested) noexcept {
    if(!requested||(access!=PopulationAccess::Ready&&access!=PopulationAccess::Full))return false;
    return managed>=static_cast<std::uint64_t>(limit-EffectiveNativeHeadroom(requested,limit));
}
inline bool MayReclaimWithNativeHeadroom(unsigned requested,unsigned movingTraffic,bool movingVictim) noexcept {
    return !requested||!movingVictim||movingTraffic>2;
}
inline PopulationAccess PopulationDecision(unsigned result,const nfsmw_population::Status& s,unsigned missing) {
    if(result==nfsmw_population::Unavailable)return PopulationAccess::Legacy;
    if(result!=nfsmw_population::Available||s.size!=sizeof(s)||
       (s.capabilities&nfsmw_population::requiredCapabilities)!=nfsmw_population::requiredCapabilities||
       !s.limit||s.live>s.limit||s.freeSlots>s.limit-s.live||s.reserved!=missing||s.reserved>s.limit)
        return PopulationAccess::Unsafe;
    return s.freeSlots?PopulationAccess::Ready:PopulationAccess::Full;
}
// V1 leases expire after 2000 ms. Track possible ownership separately from the
// 1500 ms native-retirement transaction, which is reset during preparation.
// An unsafe/partial nonzero call may have acquired a lease before failing.
struct PopulationReleaseState {
    std::uint64_t heldUntil=0,nextRetry=0;
    void Observe(unsigned requested,unsigned result,PopulationAccess access,std::uint64_t now) noexcept {
        if(result==nfsmw_population::Unavailable) {heldUntil=0;nextRetry=0;return;}
        if(requested) {heldUntil=now+2000;nextRetry=0;return;}
        if(access==PopulationAccess::Ready || access==PopulationAccess::Full) {
            heldUntil=0;nextRetry=0;
        }
    }
    bool ShouldRelease(std::uint64_t now) noexcept {
        if(heldUntil && now>=heldUntil) {heldUntil=0;nextRetry=0;}
        return heldUntil && now>=nextRetry;
    }
    void AttemptedRelease(std::uint64_t now) noexcept {nextRetry=now+250;}
};
}
