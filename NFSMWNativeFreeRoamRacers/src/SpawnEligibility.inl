// Eligibility is career/session state, NOT persistent calibration metadata.
// Re-evaluate the existing <=5-model palette before each spawn, retaining the
// audio model budget and price/performance neighborhood. No car-name blacklist.
bool g_eligibilityLogged=false;
std::array<std::uint32_t,5> g_eligibilityKeys{};
std::array<PerformanceEligibility,5> g_eligibilityStates{};

void ResetSpawnEligibility() noexcept { g_eligibilityLogged=false; }

template<class ReadPlan>
const RankedVehicle* SelectPreparedSpawnVehicle(CareerPerformancePlan* plan,ReadPlan readPlan) {
    if(!plan||g_palette.empty()||g_palette.size()>5) return nullptr;
    std::array<std::uint32_t,5> keys{};
    std::array<PerformanceEligibility,5> states{};
    std::array<CareerPerformancePlan,5> plans{};
    std::array<std::size_t,5> eligible{};std::size_t count=0;
    for(std::size_t slot=0;slot<g_palette.size();++slot) {
        const auto index=g_palette[slot];
        if(index>=g_catalog.size()||!g_catalog[index].installed) {g_vehicleBag.clear();return nullptr;}
        keys[slot]=g_catalog[index].installed->key;
        states[slot]=readPlan(keys[slot],&plans[slot]);
        if(states[slot]==PerformanceEligibility::Ready) eligible[count++]=index;
    }
    if(!g_eligibilityLogged||keys!=g_eligibilityKeys||states!=g_eligibilityStates) {
        Log(LogLevel::Info,"SPAWN_ELIGIBILITY ready=%u palette=%u modelBudgetUnchanged=1 careerChecked=1",
            static_cast<unsigned>(count),static_cast<unsigned>(g_palette.size()));
        for(std::size_t slot=0;slot<g_palette.size();++slot)
            if(states[slot]==PerformanceEligibility::NoNitrous)
                Log(LogLevel::Info,"SPAWN_CANDIDATE_FILTERED model=%s key=%08X reason=nos-unsupported-career-unlocked allocation=0",
                    g_catalog[g_palette[slot]].installed->model,keys[slot]);
        if(!count) Log(LogLevel::Info,"SPAWN_WAIT reason=no-compatible-candidate retry=normal-interval permanentExclusion=0");
        g_eligibilityLogged=true;g_eligibilityKeys=keys;g_eligibilityStates=states;
    }
    const auto end=eligible.begin()+count;
    // Remove now-ineligible entries left in a partially consumed shuffle bag.
    g_vehicleBag.erase(std::remove_if(g_vehicleBag.begin(),g_vehicleBag.end(),
        [&](std::size_t index){return std::find(eligible.begin(),end,index)==end;}),g_vehicleBag.end());
    if(!count) return nullptr;
    if(g_vehicleBag.empty()) {
        g_vehicleBag.assign(eligible.begin(),end);
        std::shuffle(g_vehicleBag.begin(),g_vehicleBag.end(),g_random);
    }
    const auto selected=g_vehicleBag.back();g_vehicleBag.pop_back();
    const auto slot=std::find(g_palette.begin(),g_palette.end(),selected)-g_palette.begin();
    *plan=plans[slot];
    return &g_catalog[selected];
}
