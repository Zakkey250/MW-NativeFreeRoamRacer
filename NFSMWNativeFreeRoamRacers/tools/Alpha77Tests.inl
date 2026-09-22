template<class Expect>
__declspec(noinline) void RunAlpha77Tests(Expect expect) {
    using namespace nfsmw_population;
    expect(ParseNativeHeadroom(nullptr)==0&&ParseNativeHeadroom(L"")==0&&
        ParseNativeHeadroom(L"garbage")==0&&ParseNativeHeadroom(L"6x")==0,
        "alpha77 missing malformed setting defaults OFF");
    expect(ParseNativeHeadroom(L" 6 \t")==6&&ParseNativeHeadroom(L"0")==0&&
        ParseNativeHeadroom(L"-2")==0&&ParseNativeHeadroom(L"65")==64,
        "alpha77 headroom setting has bounded documented range");
    for(unsigned limit:{1u,4u,20u,24u,28u,32u,40u,64u,127u}){
        const auto reserve=EffectiveNativeHeadroom(6,limit);
        expect(reserve<limit,"alpha77 headroom never underflows small provider limit");
        for(unsigned managed=0;managed<=limit+1;++managed){
            for(auto access:{PopulationAccess::Ready,PopulationAccess::Full}){
                const auto blocked=NativeHeadroomReached(access,limit,managed,6);
                expect(blocked==(managed>=limit-reserve),"alpha77 dynamic provider limit defines non-racer share without fixed capacity");
                expect(!NativeHeadroomReached(access,limit,managed,0),"alpha77 OFF preserves original admission path");
            }
            expect(!NativeHeadroomReached(PopulationAccess::Legacy,limit,managed,6),"alpha77 LB absent headroom policy does not change legacy path");
        }
    }
    expect(!NativeHeadroomReached(PopulationAccess::Ready,32,6,6)&&
        !NativeHeadroomReached(PopulationAccess::Full,32,6,6),
        "alpha77 current six racer target remains eligible even at full native population");
    expect(NativeHeadroomReached(PopulationAccess::Ready,20,14,6)&&
        !NativeHeadroomReached(PopulationAccess::Ready,20,13,6),
        "alpha77 fourteen-racer quota waits then resumes after one racer retires");
    expect(!NativeHeadroomReached(PopulationAccess::Ready,32,14,6),
        "alpha77 increased compatible LB capacity resumes without restart or version check");
    for(unsigned moving=0;moving<8;++moving){
        expect(MayReclaimWithNativeHeadroom(6,moving,true)==(moving>2),"alpha77 last two moving traffic cars are not reclaimed");
        expect(MayReclaimWithNativeHeadroom(6,moving,false),"alpha77 parked safe victim does not consume moving traffic floor");
        expect(MayReclaimWithNativeHeadroom(0,moving,true),"alpha77 OFF retains old traffic reclaim policy");
    }
    const auto savedSettings=g_settings;auto savedRacers=std::move(g_racers);
    const auto savedRelease=g_populationRelease;const auto savedTransaction=g_populationTransaction;
    const auto savedCounters=g_populationApiCounters;const auto savedLogAt=g_nativeHeadroomDiagnosticAt;
    g_settings.lbNonRacerSlots=6;g_racers.resize(14);
    g_nativeHeadroomDiagnosticAt=~ULONGLONG{0};
    g_populationRelease={};g_populationTransaction={};g_populationApiCounters={};
    unsigned calls=0,held=0,limit=32,live=19,result=Available;
    auto provider=[&](unsigned missing,Status* s) noexcept {
        ++calls;if(result==Available)held=missing;
        *s={sizeof(Status),requiredCapabilities,limit,live,limit-live,held};return result;
    };
    Status s;
    auto access=TrackedPopulationCapacity(s,0,100,provider);
    expect(!DeferForNativeHeadroom(access,s)&&held==0,"alpha77 early idle admission never reserves headroom slots");
    limit=20;
    access=TrackedPopulationCapacity(s,1,200,provider);
    expect(DeferForNativeHeadroom(access,s)&&held==1,"alpha77 fresh construction query catches reduced capacity quota");
    ReleasePopulationReservationWith(201,provider);
    expect(!held&&!g_populationRelease.heldUntil&&!g_populationTransaction.deadline,
        "alpha77 headroom deferral releases one-slot lease without persistent traffic suppression");
    const auto before=calls;DeferForNativeHeadroom(access,s);
    expect(calls==before&&g_racers.size()==14,"alpha77 quota policy performs no provider calls or racer eviction");
    g_racers.resize(13);access=TrackedPopulationCapacity(s,0,300,provider);
    expect(!DeferForNativeHeadroom(access,s),"alpha77 real bridge resumes next check after managed retirement");
    result=Unavailable;access=TrackedPopulationCapacity(s,0,400,provider);g_racers.resize(15);
    expect(access==PopulationAccess::Legacy&&!DeferForNativeHeadroom(access,s),"alpha77 no provider preserves original legacy branch");
    for(unsigned response:{Unsafe,Busy}){result=response;access=TrackedPopulationCapacity(s,0,500,provider);
        expect(access==PopulationAccess::Unsafe,"alpha77 Busy and Unsafe remain fail closed before admission policy");}
    g_settings=savedSettings;g_racers=std::move(savedRacers);g_populationRelease=savedRelease;
    g_populationTransaction=savedTransaction;g_populationApiCounters=savedCounters;g_nativeHeadroomDiagnosticAt=savedLogAt;
}
