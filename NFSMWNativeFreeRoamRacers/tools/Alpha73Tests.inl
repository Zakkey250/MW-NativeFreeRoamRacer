    {
        using namespace nfsmw_population;
        const auto savedRelease=g_populationRelease;
        const auto savedTransaction=g_populationTransaction;
        const auto savedCounters=g_populationApiCounters;
        g_populationRelease={};g_populationTransaction={};g_populationApiCounters={};
        unsigned calls=0,held=0;
        unsigned reply=Available,limit=32,live=20,caps=requiredCapabilities;
        auto provider=[&](unsigned requested,Status* status) noexcept -> unsigned {
            ++calls;
            if(reply==Available)held=requested;
            *status={sizeof(Status),caps,limit,live,limit-live,held};
            return reply;
        };
        // Normal full-population/no-anchor frames do not keep querying releases.
        for(unsigned i=0;i<1200;++i)ReleasePopulationReservationWith(100+i*50,provider);
        expect(calls==0 && g_populationApiCounters.releaseSkipped==1200,
            "alpha73 full population idle release is local, zero provider calls in 60s");
        Status status;
        expect(TrackedPopulationCapacity(status,0,61000,provider)==PopulationAccess::Ready,
            "alpha73 due idle lookup uses fresh capability");
        ReleasePopulationReservationWith(61001,provider);
        expect(calls==1 && !held,"alpha73 idle/no-anchor zero lookup needs no duplicate release");
        expect(TrackedPopulationCapacity(status,1,62000,provider)==PopulationAccess::Ready && held==1,
            "alpha73 construction reserves exactly one fresh slot");
        g_populationTransaction.Begin(62000);
        ReleasePopulationReservationWith(62001,provider);
        const auto afterRelease=calls;
        for(unsigned i=0;i<1200;++i)ReleasePopulationReservationWith(62002+i*50,provider);
        expect(calls==afterRelease && held==0 && !g_populationTransaction.deadline,
            "alpha73 completion/world exit releases once and cancels transaction");
        for(auto capacity:{20u,24u,28u,32u,40u,64u,127u}) {
            limit=capacity;live=capacity-1;
            expect(TrackedPopulationCapacity(status,0,130000,provider)==PopulationAccess::Ready,
                "alpha73 coherent API capacity accepted without LB release comparison");
            live=capacity;
            expect(TrackedPopulationCapacity(status,1,130001,provider)==PopulationAccess::Full,
                "alpha73 changed capacity rechecked at construction, never cached admission");
            ReleasePopulationReservationWith(130002,provider);
        }
        limit=32;live=20;
        reply=Unavailable;
        expect(TrackedPopulationCapacity(status,1,140000,provider)==PopulationAccess::Legacy,
            "alpha73 absent/disabled/missing-export path stays legacy");
        const auto absentCalls=calls;
        ReleasePopulationReservationWith(140001,provider);
        expect(calls==absentCalls,"alpha73 unavailable provider creates no release obligation");
        reply=Available;
        expect(TrackedPopulationCapacity(status,1,140100,provider)==PopulationAccess::Ready,
            "alpha73 later available provider is discovered, absence is not cached");
        // A partial failing reservation must still receive a bounded cleanup.
        reply=Unsafe;
        expect(TrackedPopulationCapacity(status,1,150000,provider)==PopulationAccess::Unsafe,
            "alpha73 unsafe reservation never admits construction");
        ReleasePopulationReservationWith(150001,provider);
        const auto firstFailure=calls;
        for(unsigned now=150002;now<150251;++now)ReleasePopulationReservationWith(now,provider);
        expect(calls==firstFailure && g_populationRelease.heldUntil==152000,
            "alpha73 failed cleanup is rate limited without extending V1 lease");
        reply=Available;
        ReleasePopulationReservationWith(150251,provider);
        expect(calls==firstFailure+1 && !held && !g_populationRelease.heldUntil,
            "alpha73 transient cleanup failure retried then stops");
        reply=Busy;
        expect(TrackedPopulationCapacity(status,1,160000,provider)==PopulationAccess::Unsafe,
            "alpha73 another owner remains fail closed");
        for(unsigned now=160001;now<=165000;now+=50)ReleasePopulationReservationWith(now,provider);
        expect(!g_populationRelease.heldUntil,"alpha73 failed releases stop at V1 expiry, never infinite polling");
        reply=Available;caps=0;
        expect(TrackedPopulationCapacity(status,1,170000,provider)==PopulationAccess::Unsafe,
            "alpha73 export alone is insufficient, capabilities still required");
        ReleasePopulationReservationWith(170001,provider);
        expect(g_populationRelease.heldUntil!=0,"alpha73 incoherent release response not treated as confirmation");
        caps=requiredCapabilities;
        ReleasePopulationReservationWith(170251,provider);
        expect(!g_populationRelease.heldUntil,"alpha73 coherent release retry clears uncertain lease");
        // Exercise the production Replenish path with an offline palette and no
        // anchors. No game process, LB DLL, or native vehicle call is involved.
        const auto savedSettings=g_settings;
        const auto savedFrontendReady=g_frontendCache.ready;
        const auto savedCatalogFinished=g_catalogFinished;
        const auto savedPalette=g_palette;
        const auto savedPaletteKey=g_palettePlayerKey;
        const auto savedPacing=g_refillPacing;
        const auto savedSpawnTimer=g_spawnTimer,savedBackoff=g_spawnFailureBackoffSeconds;
        const auto savedRefillAt=g_refillDiagnosticAt;
        auto savedRacers=std::move(g_racers);
        g_settings.maximumRacers=6;g_settings.spawnIntervalSeconds=5;
        g_frontendCache.ready=false;g_catalogFinished=true;
        g_palette={0};g_palettePlayerKey=0x1234;
        VehicleSnapshot offlinePlayer{};offlinePlayer.vehicleKey=0x1234;
        const std::vector<VehicleSnapshot> noAnchors;
        g_refillPacing={};g_spawnTimer=0;g_spawnFailureBackoffSeconds=0;
        g_refillDiagnosticAt=~ULONGLONG{0};
        g_populationRelease={};g_populationTransaction={};g_populationApiCounters={};
        for(unsigned frame=0;frame<1200;++frame)Replenish(offlinePlayer,noAnchors,0.05f);
        expect(g_populationApiCounters.idle>=59 && g_populationApiCounters.idle<=61 &&
            g_populationApiCounters.reserve==0 && g_populationApiCounters.release==0,
            "alpha73 real Replenish no-anchor path queries about 60 not 1200 times per simulated minute");
        g_populationApiCounters={};g_racers.resize(6);
        for(unsigned frame=0;frame<1200;++frame)Replenish(offlinePlayer,noAnchors,0.05f);
        expect(!g_populationApiCounters.idle && !g_populationApiCounters.reserve &&
            !g_populationApiCounters.release && g_populationApiCounters.releaseSkipped==1200,
            "alpha73 real Replenish full population makes zero provider queries");
        g_racers.clear();g_spawnFailureBackoffSeconds=60;g_populationApiCounters={};
        for(unsigned frame=0;frame<1000;++frame)Replenish(offlinePlayer,noAnchors,0.05f);
        expect(!g_populationApiCounters.idle && !g_populationApiCounters.reserve && !g_populationApiCounters.release,
            "alpha73 real Replenish backoff does not poll population");
        g_racers=std::move(savedRacers);g_settings=savedSettings;
        g_frontendCache.ready=savedFrontendReady;g_catalogFinished=savedCatalogFinished;
        g_palette=savedPalette;g_palettePlayerKey=savedPaletteKey;
        g_refillPacing=savedPacing;g_spawnTimer=savedSpawnTimer;
        g_spawnFailureBackoffSeconds=savedBackoff;g_refillDiagnosticAt=savedRefillAt;
        g_populationRelease=savedRelease;g_populationTransaction=savedTransaction;
        g_populationApiCounters=savedCounters;
        wchar_t directory[MAX_PATH]{},path[MAX_PATH]{};
        const bool temporary=GetTempPathW(MAX_PATH,directory)&&GetTempFileNameW(directory,L"n73",0,path);
        expect(temporary,"alpha73 temporary INI fixture created");
        if(temporary) {
            expect(!ReadLbAudioExpansionSetting(path),"alpha73 no optional INI setting defaults OFF");
            WritePrivateProfileStringW(L"Audio",L"LBEngineAudioExpansion",L"1",path);
            expect(ReadLbAudioExpansionSetting(path),"alpha73 legacy INI ON still accepted");
            WritePrivateProfileStringW(L"LimitBreaker",L"LBEngineAudioExpansion",L"0",path);
            expect(!ReadLbAudioExpansionSetting(path),"alpha73 new section OFF overrides legacy ON");
            WritePrivateProfileStringW(L"Audio",L"LBEngineAudioExpansion",L"0",path);
            WritePrivateProfileStringW(L"LimitBreaker",L"LBEngineAudioExpansion",L"1",path);
            expect(ReadLbAudioExpansionSetting(path),"alpha73 new section ON overrides legacy OFF");
            WritePrivateProfileStringW(L"LimitBreaker",L"LBEngineAudioExpansion",L"invalid",path);
            expect(!ReadLbAudioExpansionSetting(path),"alpha73 invalid new value fails to OFF");
            WritePrivateProfileStringW(nullptr,nullptr,nullptr,path);
            expect(DeleteFileW(path)!=0,"alpha73 own temporary INI cleaned up");
        }
    }
