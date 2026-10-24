// Management-thread-only one-item pipeline. Retain owned scalar/index data,
// never a vehicle, road nav, RideInfo part pointer or catalog-row pointer.
enum class SpawnPreparationPhase { Empty, Appearance, Ready };
enum class SpawnPreparationResult { Working, Ready, Retry, Failed };
constexpr std::uint64_t SpawnPreparationLifetimeMs(SpawnPreparationPhase phase,bool retain) noexcept {
    return retain&&phase==SpawnPreparationPhase::Ready?15000u:5000u;
}
struct SpawnPreparation {
    SpawnPreparationPhase phase=SpawnPreparationPhase::Empty;
    std::uint32_t playerKey=0,vehicleKey=0;
    std::uint64_t catalogGeneration=0,started=0,lastAdvance=0;
    std::string model;
    bool continuation=false;
    bool customExterior=false;
};
SpawnPreparation g_spawnPreparation;
// The SDK constructor calls native code! optional keeps DLL/static startup
// inert. It is emplaced only during the free-roam appearance stage.
std::optional<NFSPluginSDK::MW05::FECustomizationRecord> g_preparedCustomization;
static_assert(std::is_trivially_destructible_v<NFSPluginSDK::MW05::FECustomizationRecord>);
void ResetSpawnPreparation() noexcept {
    g_preparedCustomization.reset();g_spawnPreparation={};
}
const RankedVehicle* FindPreparedSpawnVehicle() noexcept {
    for(const auto index:g_palette) {
        if(index>=g_catalog.size())return nullptr;
        const auto& car=g_catalog[index];
        if(car.installed && car.installed->key==g_spawnPreparation.vehicleKey &&
           car.installed->model && g_spawnPreparation.model==car.installed->model)return &car;
    }
    return nullptr;
}
bool SpawnPreparationContextMatches(std::uint32_t playerKey,std::uint64_t now) noexcept {
    const auto& p=g_spawnPreparation;
    return g_catalogFinished && p.playerKey==playerKey && g_palettePlayerKey==playerKey &&
        p.catalogGeneration==g_catalogGeneration && now>=p.started &&
        now-p.started<SpawnPreparationLifetimeMs(p.phase,g_settings.retainPreparedAppearance) &&
        FindPreparedSpawnVehicle()!=nullptr;
}
template<class Select,class Appearance>
SpawnPreparationResult AdvanceSpawnPreparation(std::uint32_t playerKey,std::uint64_t now,
                                               Select select,Appearance appearance) {
    auto& p=g_spawnPreparation;
    if(p.phase!=SpawnPreparationPhase::Empty && !SpawnPreparationContextMatches(playerKey,now))
        ResetSpawnPreparation();
    if(p.phase==SpawnPreparationPhase::Empty) {
        CareerPerformancePlan plan{};
        const auto start=PerformanceNow();
        const auto* car=select(&plan);
        RecordPerformance(PerfStage::SpawnSelect,start,PerformanceNow());
        if(!car || !car->installed || !car->installed->model)return SpawnPreparationResult::Retry;
        p.playerKey=playerKey;p.vehicleKey=car->installed->key;p.model=car->installed->model;
        p.catalogGeneration=g_catalogGeneration;p.started=now;p.lastAdvance=now;
        p.phase=SpawnPreparationPhase::Appearance;p.continuation=true;
        return SpawnPreparationResult::Working;
    }
    // Never chain expensive stages within the same management sample.
    if(now==p.lastAdvance)return SpawnPreparationResult::Working;
    p.lastAdvance=now;
    if(p.phase==SpawnPreparationPhase::Appearance) {
        const auto* car=FindPreparedSpawnVehicle();
        const auto start=PerformanceNow();
        const bool ready=car && appearance(*car);
        RecordPerformance(PerfStage::SpawnAppearance,start,PerformanceNow());
        if(!ready){ResetSpawnPreparation();return SpawnPreparationResult::Failed;}
        p.phase=SpawnPreparationPhase::Ready;
        return SpawnPreparationResult::Working; // Construct on a later sample.
    }
    p.continuation=false; // Further capacity/lifecycle waits use existing cadence.
    return SpawnPreparationResult::Ready;
}
