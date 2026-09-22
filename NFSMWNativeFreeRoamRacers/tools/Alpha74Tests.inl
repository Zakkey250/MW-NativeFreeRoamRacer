    {
        const auto savedCatalog=g_catalog;
        const auto savedPalette=g_palette;
        const auto savedFinished=g_catalogFinished;
        const auto savedPlayer=g_palettePlayerKey;
        const auto savedGeneration=g_catalogGeneration;
        const auto beforeCalls=g_populationApiCounters;
        expect(!g_preparedCustomization,"alpha74 optional record has no startup native constructor");
        const InstalledVehicle models[]={{"A",0x41,10000},{"B",0x42,20000}};
        g_catalog={{&models[0],0,{0,0,0},0},{&models[1],1,{0,0,0},0}};
        g_palette={0,1};g_palettePlayerKey=123;g_catalogFinished=true;
        unsigned selections=0,appearances=0;bool eligible=true,validAppearance=true;
        auto select=[&](CareerPerformancePlan*)->const RankedVehicle* {
            ++selections;return eligible?&g_catalog[g_palette.front()]:nullptr;
        };
        auto appearance=[&](const RankedVehicle& car){
            ++appearances;return validAppearance && car.installed->key==g_spawnPreparation.vehicleKey;
        };
        ResetSpawnPreparation();eligible=false;
        expect(AdvanceSpawnPreparation(123,10,select,appearance)==SpawnPreparationResult::Retry &&
            g_spawnPreparation.phase==SpawnPreparationPhase::Empty && !g_spawnPreparation.continuation,
            "alpha74 no eligible model waits without allocation or permanent exclusion");
        eligible=true;selections=0;
        expect(AdvanceSpawnPreparation(123,100,select,appearance)==SpawnPreparationResult::Working &&
            selections==1 && appearances==0 && g_spawnPreparation.continuation,
            "alpha74 first sample selects only, no appearance or construction");
        expect(AdvanceSpawnPreparation(123,100,select,appearance)==SpawnPreparationResult::Working &&
            selections==1 && appearances==0,"alpha74 same sample cannot advance twice");
        expect(AdvanceSpawnPreparation(123,150,select,appearance)==SpawnPreparationResult::Working &&
            selections==1 && appearances==1 && g_spawnPreparation.phase==SpawnPreparationPhase::Ready,
            "alpha74 second sample builds appearance but cannot construct");
        expect(AdvanceSpawnPreparation(123,200,select,appearance)==SpawnPreparationResult::Ready &&
            !g_spawnPreparation.continuation && selections==1 && appearances==1,
            "alpha74 third sample permits fresh validation and construction");
        expect(AdvanceSpawnPreparation(123,250,select,appearance)==SpawnPreparationResult::Ready &&
            selections==1 && appearances==1,"alpha74 bounded vacancy retry reuses owned appearance");
        // Vector reallocation is safe: no selected row pointer survives a sample.
        g_catalog.reserve(4000);
        expect(FindPreparedSpawnVehicle()==&g_catalog[0],"alpha74 catalog vector relocation resolved by key each time");
        ++g_catalogGeneration;
        expect(AdvanceSpawnPreparation(123,300,select,appearance)==SpawnPreparationResult::Working &&
            selections==2 && appearances==1,"alpha74 same model keys after catalog reset invalidate preparation");
        g_palettePlayerKey=456;
        expect(AdvanceSpawnPreparation(456,350,select,appearance)==SpawnPreparationResult::Working &&
            selections==3 && g_spawnPreparation.playerKey==456,"alpha74 player car change restarts preparation");
        g_palette={1};
        expect(AdvanceSpawnPreparation(456,400,select,appearance)==SpawnPreparationResult::Working &&
            selections==4 && g_spawnPreparation.vehicleKey==0x42,"alpha74 palette removal rejects old model");
        expect(AdvanceSpawnPreparation(456,5400,select,appearance)==SpawnPreparationResult::Working &&
            selections==5 && g_spawnPreparation.started==5400,"alpha74 five-second expiry starts a fresh selection");
        validAppearance=false;
        expect(AdvanceSpawnPreparation(456,5450,select,appearance)==SpawnPreparationResult::Failed &&
            g_spawnPreparation.phase==SpawnPreparationPhase::Empty && !g_preparedCustomization,
            "alpha74 invalid appearance clears pipeline for existing bounded backoff");
        validAppearance=true;
        AdvanceSpawnPreparation(456,5500,select,appearance);
        ResetSpawnPreparation();
        expect(g_spawnPreparation.phase==SpawnPreparationPhase::Empty &&
            g_spawnPreparation.model.empty() && !g_spawnPreparation.continuation && !g_preparedCustomization,
            "alpha74 world/no-anchor/full-population reset drops all prepared state");
        expect(g_populationApiCounters.idle==beforeCalls.idle &&
            g_populationApiCounters.reserve==beforeCalls.reserve && g_populationApiCounters.release==beforeCalls.release,
            "alpha74 preparation itself never reserves LB slots or assumes LB presence");
        g_catalog=savedCatalog;g_palette=savedPalette;g_catalogFinished=savedFinished;
        g_palettePlayerKey=savedPlayer;g_catalogGeneration=savedGeneration;
    }
