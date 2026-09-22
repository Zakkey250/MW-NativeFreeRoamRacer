template<class Expect>
__declspec(noinline) void RunAlpha83Tests(Expect expect) {
    const auto settings=g_settings;const auto catalog=g_catalog;const auto palette=g_palette;
    const auto finished=g_catalogFinished;const auto player=g_palettePlayerKey;
    const auto generation=g_catalogGeneration;const auto counters=g_populationApiCounters;
    const auto transaction=g_populationTransaction;const auto release=g_populationRelease;
    const auto preparation=g_spawnPreparation;
    expect(!g_preparedCustomization,"alpha83 offline fixture starts without native record constructor");
    const InstalledVehicle models[]={{"A",0x41,100},{"B",0x42,200}};
    g_catalog={{&models[0],0,{1,1,1},0},{&models[1],1,{1,1,1},0}};
    g_palette={0,1};g_palettePlayerKey=123;g_catalogFinished=true;
    unsigned selections=0,appearances=0;
    auto select=[&](CareerPerformancePlan*)->const RankedVehicle* {++selections;return &g_catalog[g_palette.front()];};
    auto appearance=[&](const RankedVehicle&){++appearances;return true;};
    auto begin=[&] {
        ResetSpawnPreparation();selections=appearances=0;
        AdvanceSpawnPreparation(123,100,select,appearance);
        AdvanceSpawnPreparation(123,150,select,appearance);
        AdvanceSpawnPreparation(123,200,select,appearance);
    };
    for(auto phase:{SpawnPreparationPhase::Empty,SpawnPreparationPhase::Appearance,SpawnPreparationPhase::Ready}) {
        expect(SpawnPreparationLifetimeMs(phase,false)==5000,"alpha83 OFF retains five-second limit for every phase");
        expect(SpawnPreparationLifetimeMs(phase,true)==(phase==SpawnPreparationPhase::Ready?15000u:5000u),
            "alpha83 only completed appearance gets extended lifetime");
    }
    g_settings.retainPreparedAppearance=false;begin();
    expect(AdvanceSpawnPreparation(123,5100,select,appearance)==SpawnPreparationResult::Working&&selections==2,
        "alpha83 OFF completed preparation expires at legacy boundary");
    g_settings.retainPreparedAppearance=true;begin();
    for(std::uint64_t now=1000;now<=15000;now+=1000)
        expect(AdvanceSpawnPreparation(123,now,select,appearance)==SpawnPreparationResult::Ready&&
            selections==1&&appearances==1&&g_spawnPreparation.started==100&&!g_spawnPreparation.continuation,
            "alpha83 bounded repeated waits reuse appearance without extending original deadline or fast retry");
    expect(AdvanceSpawnPreparation(123,15100,select,appearance)==SpawnPreparationResult::Working&&selections==2&&
        appearances==1&&g_spawnPreparation.phase==SpawnPreparationPhase::Appearance,
        "alpha83 exact fifteen-second boundary reselects without constructing");
    ResetSpawnPreparation();AdvanceSpawnPreparation(123,100,select,appearance);
    expect(!SpawnPreparationContextMatches(123,5100),"alpha83 incomplete appearance still expires at five seconds");
    begin();expect(!SpawnPreparationContextMatches(123,99),"alpha83 backwards time invalidates retained appearance");
    expect(!SpawnPreparationContextMatches(456,6000),"alpha83 player key change still invalidates immediately");
    ++g_catalogGeneration;
    expect(!SpawnPreparationContextMatches(123,6000),"alpha83 catalog rebuild still invalidates immediately");
    --g_catalogGeneration;g_palette={1};
    expect(!SpawnPreparationContextMatches(123,6000),"alpha83 palette removal still invalidates immediately");
    g_palette={0,1};g_catalogFinished=false;
    expect(!SpawnPreparationContextMatches(123,6000),"alpha83 unavailable catalog still rejects retained plan");
    g_catalogFinished=true;g_catalog.reserve(4096);
    expect(SpawnPreparationContextMatches(123,6000)&&FindPreparedSpawnVehicle()==&g_catalog[0],
        "alpha83 vector relocation resolves fresh row without cached native or catalog pointers");
    ResetSpawnPreparation();
    expect(g_spawnPreparation.phase==SpawnPreparationPhase::Empty&&!g_preparedCustomization,
        "alpha83 reset used by world exit no-anchor full population and failure still discards state");
    expect(g_populationApiCounters.idle==counters.idle&&g_populationApiCounters.reserve==counters.reserve&&
        g_populationApiCounters.release==counters.release&&g_populationTransaction.deadline==transaction.deadline&&
        g_populationRelease.heldUntil==release.heldUntil,
        "alpha83 retaining appearance performs no LB query/reservation or lease extension");
    g_settings=settings;g_catalog=catalog;g_palette=palette;g_catalogFinished=finished;
    g_palettePlayerKey=player;g_catalogGeneration=generation;g_spawnPreparation=preparation;
}
