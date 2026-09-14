    {
        PerformanceLevels maximum{3,4,3,4,4,3,0};CareerPerformancePlan plan{};
        expect(PrepareCareerPerformance(maximum,&plan,[](unsigned,int){return false;})==PerformanceEligibility::Ready&&
            plan.chosen==PerformanceLevels{},"alpha50 locked career does not grant unowned upgrades or require locked nitrous");
        plan.chosen.fill(77);const auto untouched=plan.chosen;
        expect(PrepareCareerPerformance(maximum,&plan,[](unsigned,int){return true;})==PerformanceEligibility::NoNitrous&&plan.chosen==untouched,
            "alpha50 nitrous-incapable vehicle excluded without writing a performance plan");
        expect(PrepareCareerPerformance(maximum,&plan,[](unsigned,int){return false;})==PerformanceEligibility::Ready,
            "alpha50 switching back to locked career restores eligibility without persistent exclusion");
        maximum[6]=3;
        expect(PrepareCareerPerformance(maximum,&plan,[](unsigned,int shop){return shop<=1;})==PerformanceEligibility::Ready&&plan.chosen[6]==1,
            "alpha50 eligible plan installs nitrous within current career limit");
        expect(PrepareCareerPerformance(maximum,&plan,[](unsigned,int){return true;})==PerformanceEligibility::Ready&&plan.chosen==maximum,
            "alpha50 unlocked career respects each vehicle maximum");
        maximum[0]=7;
        expect(PrepareCareerPerformance(maximum,&plan,[](unsigned,int){return true;})==PerformanceEligibility::Deferred,
            "alpha50 malformed vehicle cap remains fail closed");
        expect(PrepareCareerPerformance(maximum,nullptr,[](unsigned,int){return true;})==PerformanceEligibility::Deferred,
            "alpha50 null performance destination rejected");

        ResetDynamicCatalog();ResetSpawnEligibility();
        InstalledVehicle cars[]={{"ADDON_A",101,100},{"ADDON_B",102,110},{"PLAYER",103,120},{"ADDON_C",104,130},{"ADDON_D",105,140}};
        for(unsigned i=0;i<5;++i)g_catalog.push_back({&cars[i],int(i),{1,1,1}});
        g_palette={0,1,2,3,4};g_vehicleBag={4,3,2,1,0};
        unsigned calls=0;bool nosUnlocked=true,unready=false;
        const auto readPlan=[&](unsigned key,CareerPerformancePlan* result) {
            ++calls;
            if(unready)return PerformanceEligibility::Deferred;
            PerformanceLevels caps{3,4,3,4,4,3,key==101?0u:3u};
            return PrepareCareerPerformance(caps,result,[&](unsigned,int){return nosUnlocked;});
        };
        const auto initialPalette=g_palette;
        const auto* selected=SelectPreparedSpawnVehicle(&plan,readPlan);
        expect(selected&&selected->installed->key==102&&calls==5&&plan.chosen[6]==3,
            "alpha50 stale bag's unsupported head skipped in same spawn opportunity with at most five queries");
        expect(g_palette==initialPalette&&g_vehicleBag.size()==3&&std::find(g_vehicleBag.begin(),g_vehicleBag.end(),0)==g_vehicleBag.end(),
            "alpha50 filtering leaves price neighborhood and player-inclusive model budget unchanged");
        std::array<unsigned,5> seen{};bool valid=true;
        for(unsigned i=0;i<39;++i) {
            selected=SelectPreparedSpawnVehicle(&plan,readPlan);
            valid=valid&&selected&&selected->installed->key!=101;
            if(selected)++seen[selected->installed->key-101];
        }
        ++seen[1]; // First selection above.
        expect(valid&&seen==std::array<unsigned,5>{0,10,10,10,10},
            "alpha50 repeated shuffle cycles never redraw unsupported model and remain balanced");
        nosUnlocked=false;
        bool restored=false;
        for(unsigned i=0;i<8;++i) {
            selected=SelectPreparedSpawnVehicle(&plan,readPlan);
            restored=restored||(selected&&selected->installed->key==101);
        }
        expect(restored&&plan.chosen==PerformanceLevels{},"alpha50 live career policy change restores candidate without game restart");
        unready=true;
        expect(!SelectPreparedSpawnVehicle(&plan,readPlan)&&g_vehicleBag.empty(),
            "alpha50 unavailable career data consumes no spawn candidate");
        unready=false;nosUnlocked=true;
        expect(SelectPreparedSpawnVehicle(&plan,readPlan)!=nullptr,"alpha50 profile recovery resumes selection");
        g_palette={0};g_vehicleBag={0};
        expect(!SelectPreparedSpawnVehicle(&plan,readPlan)&&g_vehicleBag.empty(),
            "alpha50 all-incompatible one-model palette returns waiting without fallback beyond audio budget");
        nosUnlocked=false;
        expect(SelectPreparedSpawnVehicle(&plan,readPlan)->installed==&cars[0],
            "alpha50 all-incompatible wait is not a permanent latch");
        g_palette={900};
        expect(!SelectPreparedSpawnVehicle(&plan,readPlan),"alpha50 stale catalog indices rejected");
        g_palette={0,1,2,3,4,0};
        expect(!SelectPreparedSpawnVehicle(&plan,readPlan),"alpha50 oversized candidate window rejected");
        g_palette.clear();expect(!SelectPreparedSpawnVehicle(&plan,readPlan),"alpha50 empty palette stays empty");

        // SDK default ctor calls the game's 00581C20. This offline fixture
        // supplies record storage, never invokes that unavailable constructor.
        using Record=NFSPluginSDK::MW05::FECustomizationRecord;
        alignas(Record) std::array<unsigned char,sizeof(Record)> storage{};
        storage.fill(0xA5);auto& record=*reinterpret_cast<Record*>(storage.data());
        const auto before=storage;plan.maximum={3,4,3,4,4,3,3};plan.chosen={1,1,1,1,1,1,1};
        WriteCareerPerformancePlan(103,plan,&record);
        const auto* oldBytes=before.data();
        const auto* newBytes=reinterpret_cast<const unsigned char*>(&record);
        bool onlyPhysics=true;
        for(std::size_t i=0;i<sizeof(record);++i)
            if(i<0x118||i>=0x138)onlyPhysics=onlyPhysics&&oldBytes[i]==newBytes[i];
        PerformanceLevels written{};std::memcpy(written.data(),&record.mInstalledPhysics,sizeof(written));
        expect(onlyPhysics&&written==plan.chosen&&record.mInstalledPhysics.mJunkman==NFSPluginSDK::MW05::JunkmanParts::None,
            "alpha50 selected plan changes private physics package only and preserves appearance bytes");
        ResetDynamicCatalog();ResetSpawnEligibility();
    }
