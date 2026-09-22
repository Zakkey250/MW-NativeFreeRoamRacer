// Keep this fixture out of the legacy all-in-one test function's stack frame.
template<class Expect>
__declspec(noinline) void RunAlpha76Tests(Expect expect) {
        const auto savedSettings=g_settings;
        const auto savedLocked=g_vehicleVarietyLocked,savedExpanded=g_vehicleVarietyExpanded;
        const auto savedFrozen=g_audioPreparationFrozen.load(),savedHook=g_audioPoolHookEnabled;
        const auto savedCeiling=g_audioSlotCeiling.load();
        const auto savedCatalog=g_catalog;const auto savedPalette=g_palette,savedBag=g_vehicleBag;
        const auto savedKey=g_palettePlayerKey;
        AudioPoolContext c{};c.valid=0x7FF;c.raceStatus=1;c.budget=6;
        AudioPoolHealth h{};h.complete=true;h.slots=6;
        auto accepts=[&](bool opt=true,bool audio=true,unsigned slots=4,bool frozen=true,unsigned ceiling=6,bool hook=true){
            return CanExpandVehicleVariety(opt,audio,slots,frozen,ceiling,hook,c,h);
        };
        expect(accepts(),"alpha76 verified real six-slot pool permits one extra model");
        expect(!accepts(false)&&!accepts(true,false)&&!accepts(true,true,0)&&
            !accepts(true,true,4,false)&&!accepts(true,true,4,true,4)&&!accepts(true,true,4,true,6,false),
            "alpha76 opt-in audio disabled missing capability and invalid hook retain baseline");
        for(unsigned bit:{7u,8u,9u,10u}){c.valid&=~(1u<<bit);
            expect(!accepts(),"alpha76 unreadable event or budget data rejects expansion");c.valid|=1u<<bit;}
        c.raceStatus=0;expect(!accepts(),"alpha76 absent world is not free roam");c.raceStatus=1;
        c.playMode=1;expect(!accepts(),"alpha76 native race context never authorizes variety");c.playMode=0;
        c.raceParms=1;expect(!accepts(),"alpha76 pending native event rejects variety");c.raceParms=0;
        for(unsigned budget:{0u,4u,65u,0xFFFFFFFFu}){c.budget=budget;expect(!accepts(),"alpha76 invalid audio budget rejects variety");}c.budget=6;
        for(unsigned count:{0u,4u,5u,7u,16u}){h.slots=count;expect(!accepts(),"alpha76 requested six is insufficient without exact actual six");}h.slots=6;
        h.complete=false;expect(!accepts(),"alpha76 incomplete pool enumeration rejects variety");h.complete=true;
        g_settings.lbVehicleVarietyExpansion=true;g_settings.lbEngineAudioExpansion=true;
        g_settings.freeRoamAudioSlots=4;g_audioPreparationFrozen=true;g_audioSlotCeiling=6;g_audioPoolHookEnabled=true;
        unsigned reads=0;
        auto readPool=[&](AudioPoolContext* outC,AudioPoolHealth* outH){++reads;*outC=c;*outH=h;};
        ResetVehicleVarietyExpansion();h.complete=false;LockVehicleVarietyWith(readPool);
        expect(VehicleModelLimit()==5&&reads==1,"alpha76 failed initial observation retains five");
        h.complete=true;LockVehicleVarietyWith(readPool);
        expect(VehicleModelLimit()==5&&reads==1,"alpha76 no late promotion or per-frame pool probe");
        ResetVehicleVarietyExpansion();LockVehicleVarietyWith(readPool);
        expect(VehicleModelLimit()==6&&reads==2,"alpha76 new world may negotiate verified six");
        LockVehicleVarietyWith(readPool);expect(reads==2,"alpha76 successful decision also frozen");
        ResetVehicleVarietyExpansion();g_audioSlotCeiling=4;LockVehicleVarietyWith(readPool);
        expect(VehicleModelLimit()==5&&reads==2,"alpha76 missing LB capability does not probe pool");
        ResetVehicleVarietyExpansion();g_settings.lbVehicleVarietyExpansion=false;LockVehicleVarietyWith(readPool);
        expect(!g_vehicleVarietyLocked&&reads==2,"alpha76 default OFF has no new pool work");
        for(std::size_t width:{0u,1u,3u,5u,99u}){g_settings.vehicleVariety=width;
            expect(EffectiveVehicleVariety()==std::clamp<std::size_t>(width,1,5),"alpha76 legacy variety settings preserved");}
        g_settings.vehicleVariety=5;g_settings.lbVehicleVarietyExpansion=true;g_audioSlotCeiling=6;
        LockVehicleVarietyWith(readPool);
        const InstalledVehicle cars[]={{"A",1,100},{"B",2,200},{"C",3,300},{"D",4,400},
            {"E",5,500},{"F",6,600},{"G",7,700},{"H",8,800}};
        g_catalog.clear();for(unsigned i=0;i<8;++i)g_catalog.push_back({&cars[i],static_cast<int>(i),{1,1,1}});
        for(unsigned center=0;center<8;++center){
            expect(BuildVehiclePalette(center+1,cars[center].model)&&g_palette.size()==6&&
                std::find(g_palette.begin(),g_palette.end(),center)!=g_palette.end(),
                "alpha76 six-model shifted neighborhood includes player at every list position");
        }
        expect(g_palette==std::vector<std::size_t>{2,3,4,5,6,7},"alpha76 upper edge shifts six-model window inward");
        BuildVehiclePalette(1,"A");
        expect(g_palette==std::vector<std::size_t>{0,1,2,3,4,5},"alpha76 lower edge retains six near-ranked models");
        g_vehicleBag.clear();CareerPerformancePlan plan{};unsigned calls=0;
        auto ready=[&](unsigned key,CareerPerformancePlan* p){++calls;p->chosen[0]=key;return PerformanceEligibility::Ready;};
        std::array<unsigned,6> counts{};
        for(unsigned n=0;n<12;++n){auto* car=SelectPreparedSpawnVehicle(&plan,ready);
            expect(car&&car->installed->key>=1&&car->installed->key<=6,"alpha76 prepared selector reaches only six-model palette");
            if(car&&car->installed->key>=1&&car->installed->key<=6)++counts[car->installed->key-1];}
        expect(counts==std::array<unsigned,6>{2,2,2,2,2,2}&&calls==72,"alpha76 sixth candidate participates in balanced shuffle with six career checks");
        g_vehicleBag={5};
        auto noNitrous=[&](unsigned key,CareerPerformancePlan* p){return key==6?PerformanceEligibility::NoNitrous:ready(key,p);};
        auto* selected=SelectPreparedSpawnVehicle(&plan,noNitrous);
        expect(selected&&selected->installed->key!=6,"alpha76 sixth model still obeys unlocked nitrous eligibility");
        g_palette.push_back(6);
        expect(!SelectPreparedSpawnVehicle(&plan,ready),"alpha76 seven models remain rejected");
        g_palette={0,1,2,3,4,0};
        expect(!SelectPreparedSpawnVehicle(&plan,ready),"alpha76 duplicate candidate keys rejected");
        g_catalog.resize(3);BuildVehiclePalette(3,"C");
        expect(g_palette.size()==3,"alpha76 small catalog never fabricates extra models");
        ResetVehicleVarietyExpansion();g_settings.lbVehicleVarietyExpansion=false;
        g_catalog.clear();for(unsigned i=0;i<8;++i)g_catalog.push_back({&cars[i],static_cast<int>(i),{1,1,1}});
        expect(BuildVehiclePalette(1,"A")&&g_palette.size()==5,"alpha76 next world with expansion off restores five-model palette");
        g_palette={0,1,2,3,4,5};expect(!SelectPreparedSpawnVehicle(&plan,ready),"alpha76 baseline refuses stale six-model candidates");
        g_settings=savedSettings;g_vehicleVarietyLocked=savedLocked;g_vehicleVarietyExpanded=savedExpanded;
        g_audioPreparationFrozen=savedFrozen;g_audioSlotCeiling=savedCeiling;g_audioPoolHookEnabled=savedHook;
        g_catalog=savedCatalog;g_palette=savedPalette;g_vehicleBag=savedBag;g_palettePlayerKey=savedKey;
        ResetSpawnEligibility();
    }
