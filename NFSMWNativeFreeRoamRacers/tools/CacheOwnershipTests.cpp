// Offline x86 test of the production callback; no game process is accessed.
#include "../src/Runtime.cpp"
#include "Alpha47Fixtures.inl"
#include "EncounterHudAbiProbe.inl"
#include "Alpha23Fixtures.inl"
#include "Alpha32Fixtures.inl"
#include "Alpha38Fixtures.inl"
#include "../include/EncounterBattleModel.h"
#include <memory>
#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"user32.lib")

namespace native_freeroam {
namespace {
struct FakeVehicle {
    void** vtable;
    std::uint32_t key;
    std::uint32_t driver;
    void* simable;
};
std::uint32_t __fastcall FakeKey(const FakeVehicle* v, void*) { return v->key; }
std::uint32_t __fastcall FakeDriver(const FakeVehicle* v, void*) { return v->driver; }
void* __fastcall FakeSimable(const FakeVehicle* v, void*) { return v->simable; }
std::uint32_t nativeVote = 1;
unsigned nativeCalls = 0;
unsigned encounterSkillNativeCalls=0;
float __fastcall FakeEncounterSkill(void*,void*) {++encounterSkillNativeCalls;return 0.35f;}
const void* forwardedVehicle = nullptr;
const void* forwardedAsker = nullptr;
unsigned flowNativeCalls = 0;
void* flowForwardedSound = nullptr;
void* flowForwardedPacket = nullptr;
std::uintptr_t __fastcall FakeAudioFlowUpdate(void* sound, void*, void* packet) {
    ++flowNativeCalls;
    flowForwardedSound = sound;
    flowForwardedPacket = packet;
    if (packet) {
        const float normalized = 0.75f;
        std::memcpy(static_cast<std::uint8_t*>(packet) + 4, &normalized, sizeof(normalized));
    }
    return 0xA5B6C7D8;
}
unsigned sourceNativeCalls=0;
void* sourceNativeThis=nullptr;
std::uintptr_t sourceNativeArg=0;
std::uintptr_t __fastcall FakeAudioSource(void* source,void*,std::uintptr_t arg) {
    ++sourceNativeCalls; sourceNativeThis=source; sourceNativeArg=arg;
    if (source) {
        std::uintptr_t parent=0,state=0,object=0;
        AudioRead(reinterpret_cast<std::uintptr_t>(source),0x14,&parent);
        AudioRead(reinterpret_cast<std::uintptr_t>(source),0x18,&state);
        AudioRead(parent,0x34,&object);
        const float feedback=0.5f;
        std::memcpy(reinterpret_cast<void*>(state+0x74),&feedback,4);
        std::memcpy(reinterpret_cast<void*>(object+0x248),&feedback,4);
    }
    return arg ^ reinterpret_cast<std::uintptr_t>(source);
}
std::uint32_t __fastcall FakeNativeQuery(void*, void*, const void* vehicle,
                                      const void* asker) {
    ++nativeCalls;
    forwardedVehicle = vehicle;
    forwardedAsker = asker;
    return nativeVote;
}
}
int RunCacheTests() {
    auto* region = VirtualAlloc(nullptr, 0x120000,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (region == nullptr) return 2;
    // Relocate only the fabricated test vtable to avoid ASLR collisions.
    g_base = reinterpret_cast<std::uintptr_t>(region) -
             (kIVehicleVtable - kPreferredBase);
    auto** table = static_cast<void**>(region);
    table[kSlotVehicleKey] = reinterpret_cast<void*>(&FakeKey);
    table[kSlotDriverClass] = reinterpret_cast<void*>(&FakeDriver);
    table[kSlotSimable] = reinterpret_cast<void*>(&FakeSimable);
    g_originalCacheQuery = reinterpret_cast<CacheQueryFn>(&FakeNativeQuery);
    int simable = 0;
    FakeVehicle managed{table, 0x12345678, kDriverRacer, &simable};
    FakeVehicle other{table, 0x12345678, kDriverRacer, &simable};
    int asking = 0;
    unsigned failures = 0;
    const auto expect = [&failures](bool condition, const char* name) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
        if (!condition) ++failures;
    };
    ProtectManagedVehicle(0, &managed, &simable, managed.key);
    g_streamRetentionEnabled.store(true);
    expect(CacheQueryHook(nullptr, nullptr, &managed, &asking) == 0,
           "managed racer vetoes native DontCare before eviction");
    expect(nativeCalls == 1 && forwardedVehicle == &managed && forwardedAsker == &asking,
           "native query receives unchanged arguments exactly once");
    expect(g_cacheChangedVotes.load() == 1, "actual changed vote recorded");
    expect(CacheQueryHook(nullptr, nullptr, &other, &asking) == 1,
           "unmanaged same-model vehicle retains native vote");
    ++managed.key;
    expect(CacheQueryHook(nullptr, nullptr, &managed, nullptr) == 1,
           "recycled address with different key is not retained");
    --managed.key;
    managed.driver = kDriverTraffic;
    expect(CacheQueryHook(nullptr, nullptr, &managed, nullptr) == 1,
           "recycled traffic with same model is not retained");
    managed.driver = kDriverRacer;
    managed.simable = &asking;
    expect(CacheQueryHook(nullptr, nullptr, &managed, nullptr) == 1,
           "changed physical owner is not retained");
    managed.simable = &simable;
    managed.vtable = nullptr;
    expect(CacheQueryHook(nullptr, nullptr, &managed, nullptr) == 1,
           "invalid interface safely falls back to native vote");
    managed.vtable = table;
    g_streamRetentionEnabled.store(false);
    expect(CacheQueryHook(nullptr, nullptr, &managed, nullptr) == 1,
           "world exit releases cache retention");
    g_streamRetentionEnabled.store(true);
    UnprotectManagedVehicle(0, &managed, &simable);
    expect(CacheQueryHook(nullptr, nullptr, &managed, nullptr) == 1,
           "explicit retirement releases cache retention");
    nativeVote = 0;
    expect(CacheQueryHook(nullptr, nullptr, &other, nullptr) == 0,
           "native Want for unrelated vehicle remains Want");
    expect(CacheQueryHook(nullptr, nullptr, nullptr, nullptr) == 0,
           "null query preserves native behavior");
    expect(ParseMaximumRacers(nullptr) == 6 && ParseMaximumRacers(L"") == 6 &&
           ParseMaximumRacers(L"invalid") == 6 && ParseMaximumRacers(L"3cars") == 6,
           "missing and invalid population values use default six");
    expect(ParseMaximumRacers(L"-100") == 1 && ParseMaximumRacers(L"0") == 1 &&
           ParseMaximumRacers(L"1") == 1 && ParseMaximumRacers(L"6") == 6 &&
           ParseMaximumRacers(L"15") == 15 && ParseMaximumRacers(L"16") == 15 &&
           ParseMaximumRacers(L"99999999999999999") == 15,
           "population boundary and overflow clamp to one through fifteen");
    expect(ParseMaximumRacers(L" 6 \t") == 6,
           "population value accepts surrounding whitespace");
    std::array<FakeVehicle, 15> fleet{};
    std::array<int, 15> owners{};
    nativeVote = 1;
    bool fleetRetained = true;
    for (std::size_t i = 0; i < fleet.size(); ++i) {
        fleet[i] = {table, static_cast<std::uint32_t>(0x1000+i), kDriverRacer, &owners[i]};
        ProtectManagedVehicle(i, &fleet[i], &owners[i], fleet[i].key);
        fleetRetained &= AcquireMarkerSlot(&fleet[i]) == i;
    }
    for (const auto& vehicle : fleet)
        fleetRetained &= CacheQueryHook(nullptr, nullptr, &vehicle, nullptr) == 0;
    expect(fleetRetained, "all fifteen simultaneous ownership and marker slots are independent");
    expect(AcquireMarkerSlot(&other) == kMaximumRacersLimit,
           "sixteenth marker slot rejected without overwriting an owner");
    UnprotectManagedVehicle(7, &fleet[7], &owners[7]);
    ReleaseMarkerSlot(7);
    expect(CacheQueryHook(nullptr, nullptr, &fleet[7], nullptr) == 1 &&
           CacheQueryHook(nullptr, nullptr, &fleet[14], nullptr) == 0 &&
           AcquireMarkerSlot(&other) == 7,
           "retiring one of fifteen frees its slot without affecting peers");
    for (std::size_t i = 0; i < fleet.size(); ++i) {
        UnprotectManagedVehicle(i, &fleet[i], &owners[i]);
        ReleaseMarkerSlot(i);
    }
    float pending = 0.0f;
    float step = 0.0f;
    float accounted = 0.0f;
    unsigned passes = 0;
    for (unsigned frame = 0; frame < 1440; ++frame) {
        if (ConsumeManagementTick(1.0f/144.0f, &pending, &step)) {
            ++passes;
            accounted += step;
        }
    }
    expect(passes <= 200 && passes >= 170 &&
           std::abs(accounted + pending - 10.0f) < 0.001f,
           "144Hz frames bound management to at most 20Hz without losing elapsed time");
    pending = 0.0f;
    expect(ConsumeManagementTick(0.25f, &pending, &step) && step == 0.25f &&
           !ConsumeManagementTick(0.0f, &pending, &step),
           "long frame triggers one management pass without catch-up burst");
    g_vehicleScratch.reserve(kMaximumVehicles);
    const auto* scratchStorage = g_vehicleScratch.data();
    g_vehicleScratch.resize(kMaximumVehicles);
    g_vehicleScratch.clear();
    g_vehicleScratch.resize(20);
    expect(g_vehicleScratch.data() == scratchStorage &&
           g_vehicleScratch.capacity() >= kMaximumVehicles,
           "snapshot storage survives clear and reuse without reallocating");
    g_vehicleScratch.clear();
    expect(WindowStart(76, 2, 8) == 0 && WindowStart(76, 73, 8) == 68 &&
           WindowStart(76, 75, 8) == 68,
           "eight-model window shifts inward at both list edges");
    expect(WindowStart(76, 38, 6) == 36 && WindowStart(76, 38, 8) == 35 &&
           WindowStart(5, 4, 8) == 0 && WindowStart(0, 0, 8) == 0,
           "window supports six through eight and undersized or empty catalogs");
    bool boundsOK = true;
    for (std::size_t n = 1; n <= 100; ++n)
        for (std::size_t width = 1; width <= 5; ++width)
            for (std::size_t center = 0; center < n; ++center) {
                const auto start = WindowStart(n, center, width);
                const auto end = start + std::min(n, width);
                boundsOK &= start <= center && center < end && end <= n;
            }
    expect(boundsOK, "all one-to-one-hundred catalog sizes retain player and exact bounded width");
    expect(ParseVehicleVariety(nullptr)==5 && ParseVehicleVariety(L"")==5 &&
        ParseVehicleVariety(L"garbage")==5 && ParseVehicleVariety(L"3cars")==5,
        "model variety missing or invalid text defaults to five");
    expect(ParseVehicleVariety(L"-1")==1 && ParseVehicleVariety(L"0")==1 &&
        ParseVehicleVariety(L"1")==1 && ParseVehicleVariety(L" 3 \t")==3 &&
        ParseVehicleVariety(L"5")==5 && ParseVehicleVariety(L"8")==5 && ParseVehicleVariety(L"15")==5,
        "model variety clamps one through five including legacy eight-model settings");
    const InstalledVehicle budgetModels[]={{"A",1,100},{"B",2,200},{"C",3,300},{"D",4,400},
        {"E",5,500},{"F",6,600},{"G",7,700}};
    g_catalog.clear();
    for(unsigned i=0;i<7;++i)g_catalog.push_back({&budgetModels[i],static_cast<int>(i),{1,1,1}});
    bool paletteBudgetOK=true,singleModelOK=true;
    for(unsigned width=1;width<=5;++width) for(unsigned center=0;center<7;++center) {
        g_settings.vehicleVariety=width;
        paletteBudgetOK &= BuildVehiclePalette(center+1,budgetModels[center].model) && g_palette.size()==width &&
            std::find(g_palette.begin(),g_palette.end(),center)!=g_palette.end();
        for(unsigned draw=0;draw<20;++draw) {
            const auto* selected=SelectRandomVehicle();
            paletteBudgetOK &= selected && std::find(g_palette.begin(),g_palette.end(),
                static_cast<std::size_t>(selected-g_catalog.data()))!=g_palette.end();
            if(width==1)singleModelOK &= selected && selected->installed==&budgetModels[center];
        }
    }
    expect(paletteBudgetOK,"one through five model palettes include player at both edges and never draw outside budget");
    expect(singleModelOK,"one-model setting replenishes repeatedly using only the player's model");
    g_settings.vehicleVariety=99;
    expect(BuildVehiclePalette(4,"D") && g_palette.size()==5,
        "palette builder independently caps invalid in-memory variety at five");
    g_settings.vehicleVariety=0;
    expect(BuildVehiclePalette(4,"D") && g_palette.size()==1 && SelectRandomVehicle()->installed==&budgetModels[3],
        "palette builder independently clamps zero variety to player's model");
    g_settings.vehicleVariety=5;g_catalog.clear();g_palette.clear();g_vehicleBag.clear();g_palettePlayerKey=0;
    FrameTimingState timing;
    expect(!ShouldReportFrameDelay(false,1000,0,0,0,1000,&timing) &&
        !ShouldReportFrameDelay(true,249,99,99,99,1000,&timing),
        "frame delay diagnostics ignore menus and below-threshold durations");
    expect(ShouldReportFrameDelay(true,0,0,100,0,1000,&timing) &&
        !ShouldReportFrameDelay(true,1000,0,0,0,1100,&timing) &&
        ShouldReportFrameDelay(true,250,0,0,0,6000,&timing),
        "frame delay diagnostics report threshold crossing with five-second rate limit");
    expect(!ShouldReportFrameDelay(true,1000,0,0,0,5000,&timing),
        "frame delay diagnostics reject backward timing without unbounded logging");
    const InstalledVehicle examples[] = {{"LOW",1,10000}, {"MID",2,50000}, {"HIGH",3,200000}};
    std::vector<RankedVehicle> ranked = {{&examples[2],2,{0.9f,0.9f,0.9f}},
        {&examples[0],0,{0.1f,0.1f,0.1f}}, {&examples[1],1,{0.5f,0.5f,0.5f}}};
    RankVehicles(ranked);
    expect(ranked[0].installed->key == 1 && ranked[1].installed->key == 2 && ranked[2].installed->key == 3,
           "combined ranking orders price and all three performance ratings");
    ranked = {{&examples[0],0,{0.9f,0.9f,0.9f}}, {&examples[1],1,{0.1f,0.1f,0.1f}}};
    RankVehicles(ranked);
    expect(ranked[0].score == ranked[1].score,
           "price and performance have equal weight independent of numeric units");
    const InstalledVehicle bonus{"BONUS",4,0,false};
    ranked = {{&examples[0],0,{0.1f,0.1f,0.1f}}, {&bonus,3,{0.9f,0.9f,0.9f}}};
    RankVehicles(ranked);
    expect(ranked.back().installed->key == 4,
           "zero-price bonus vehicle is ranked by performance not treated as a cheap car");
    std::array<std::array<std::uint8_t, 0xD0>, 3> types{};
    std::memcpy(types[0].data(), "PLAYABLE", 9);
    std::memcpy(types[1].data(), "POLICE", 7);
    std::memcpy(types[2].data(), "TRAFFIC", 8);
    *reinterpret_cast<int*>(types[1].data()+0x94) = 1;
    *reinterpret_cast<int*>(types[2].data()+0x94) = 2;
    *reinterpret_cast<void**>(Address(0x9B09D8)) = types.data();
    *reinterpret_cast<int*>(Address(0x9B1334)) = 3;
    expect(FindPlayableType("playable") == 0 && FindPlayableType("POLICE") == -1 &&
           FindPlayableType("TRAFFIC") == -1 && FindPlayableType("ABSENT") == -1,
           "native car type table excludes police traffic and absent models");
    *reinterpret_cast<int*>(Address(0x9B1334)) = 1025;
    expect(FindPlayableType("PLAYABLE") == -1,
           "invalid native car count fails closed before walking the table");
    g_catalog = {{&examples[0],0,{1,1,1}}, {&examples[1],1,{2,2,2}}, {&examples[2],2,{3,3,3}}};
    g_palette = {0,1,2};
    g_vehicleBag.clear();
    std::array<unsigned, 4> seen{};
    for (int i = 0; i < 3; ++i) ++seen[SelectRandomVehicle()->installed->key];
    expect(seen[1] == 1 && seen[2] == 1 && seen[3] == 1 && g_vehicleBag.empty(),
           "vehicle shuffle bag draws each model once before reusing any");
    expect(SelectRandomVehicle() != nullptr && g_vehicleBag.size() == 2,
           "depleted vehicle bag automatically reshuffles and replenishes");
    g_palette.clear();
    expect(SelectRandomVehicle() == nullptr, "empty palette never falls back to unrelated preset");
    struct TestRide { int part; };
    const TestRide stock{7};
    TestRide chosen{0};
    unsigned attempts = 0, calls = 0;
    bool fresh = true;
    bool customized = ChooseAppearanceOrStock(stock, &chosen, true, 3, &attempts,
        [&](TestRide* ride) { fresh &= ride->part == 7; ride->part = 99; return ++calls == 2; });
    expect(customized && attempts == 2 && calls == 2 && chosen.part == 99 && fresh,
           "invalid cosmetic retries from fresh stock then accepts successful second attempt");
    calls = 0;
    customized = ChooseAppearanceOrStock(stock, &chosen, true, 3, &attempts,
        [&](TestRide* ride) { ++calls; ride->part = 99; return false; });
    expect(!customized && attempts == 3 && calls == 3 && chosen.part == 7,
           "cosmetic retry exhaustion restores stock without a fourth attempt");
    calls = 0;
    customized = ChooseAppearanceOrStock(stock, &chosen, false, 3, &attempts,
        [&](TestRide*) { ++calls; return true; });
    expect(!customized && calls == 0 && attempts == 0 && chosen.part == 7,
           "appearance disabled performs no randomization and uses stock");
    g_catalog.clear();
    g_vehicleBag.clear();
    expect(ValidStockPerformance(true, {0.0579f, 0.4706f, 0.0f}),
           "ALTO97 observed zero acceleration rating is valid");
    expect(ValidStockPerformance(true, {0.0f, 0.2824f, 0.3200f}),
           "BEAT observed zero top-speed rating is valid");
    expect(ValidStockPerformance(true, {0.0f, 0.0f, 0.0f}) &&
           !ValidStockPerformance(false, {0.0f, 0.0f, 0.0f}) &&
           !ValidStockPerformance(false, {0.5f, 0.5f, 0.5f}),
           "native success flag distinguishes valid zero ratings from estimation failure");
    bool invalidRatingsRejected = true;
    for (float invalid : {-1.0f, 10001.0f, std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN()})
        for (std::size_t slot = 0; slot < 3; ++slot) {
            std::array<float, 3> ratings{0.5f, 0.5f, 0.5f};
            ratings[slot] = invalid;
            invalidRatingsRejected &= !ValidStockPerformance(true, ratings);
        }
    expect(invalidRatingsRejected, "negative nonfinite and excessive ratings remain rejected in every slot");
    const InstalledVehicle transitionCars[] = {{"SVJ",1,365000}, {"ALTO97",2,12500}, {"BEAT",3,13800}};
    const std::array<float, 3> transitionRatings[] = {{0.9f,0.9f,0.9f}, {0.0579f,0.4706f,0.0f}, {0.0f,0.2824f,0.3200f}};
    for (std::size_t i = 0; i < 3; ++i)
        if (ValidStockPerformance(true, transitionRatings[i]))
            g_catalog.push_back({&transitionCars[i],static_cast<int>(i),transitionRatings[i]});
    RankVehicles(g_catalog);
    g_catalogFinished = true;
    expect(g_catalog.size() == 3 && BuildVehiclePalette(0x11111111, "SVJ") && SelectRandomVehicle(),
           "first free-roam builds and draws from the validated SVJ palette");
    g_consecutiveSpawnFailures = 4;
    g_spawnFailureBackoffSeconds = 60;
    ClearWorld(false);
    expect(g_palette.empty() && g_vehicleBag.empty() && g_palettePlayerKey == 0 &&
           g_catalog.size() == 3 && g_catalogFinished && g_spawnFailureBackoffSeconds == 0 &&
           g_consecutiveSpawnFailures == 0,
           "safehouse transition clears palette and backoff while retaining model catalog");
    expect(BuildVehiclePalette(0x1EB57883, "ALTO97") && SelectRandomVehicle(),
           "second free-roam after switching SVJ to ALTO97 resumes model selection");
    ClearWorld(false);
    expect(BuildVehiclePalette(0x7660CB44, "ALTO97") && SelectRandomVehicle(),
           "third free-roam with new dynamic key resolves the same ALTO97 model");
    ClearWorld(false);
    expect(BuildVehiclePalette(0x7660CB44, "ALTO97") && SelectRandomVehicle(),
           "same-car safehouse reentry with unchanged key rebuilds the palette");
    expect(BuildVehiclePalette(0x33333333, "BEAT") && SelectRandomVehicle() &&
           !BuildVehiclePalette(0x44444444, "NOT_INSTALLED") && !SelectRandomVehicle(),
           "zero-speed-rated BEAT remains selectable while unknown cars still fail closed");
    ClearWorld(false);
    g_catalog.clear();
    g_catalogFinished = false;
    // Audio tests use fabricated buffers, no game process or audio function calls.
    std::array<std::uint8_t, 0x180> audioVehicle{};
    std::array<std::uint8_t, 0xA0> sound{};
    std::array<std::uint8_t, 0x28> connection{};
    std::array<std::uint8_t, 0x260> audioObject{};
    const auto put = [](auto& buffer, std::size_t offset, auto value) {
        std::memcpy(buffer.data() + offset, &value, sizeof(value));
    };
    const auto vehicleAddress = reinterpret_cast<std::uintptr_t>(audioVehicle.data());
    put(audioVehicle, 0, Address(kIVehicleVtable));
    auto snapshot = [&]() { return ReadAudioSnapshot(audioVehicle.data()); };
    expect(std::strcmp(ReadAudioSnapshot(nullptr).status, "unreadable-vehicle") == 0,
           "audio null vehicle rejected without native calls");
    expect(std::strcmp(snapshot().status, "no-audible") == 0,
           "missing audible separated from nonplaying audio");
    put(audioVehicle, kAudibleFromVehicle, reinterpret_cast<std::uintptr_t>(sound.data()) + 0x54);
    put(sound, 0x54, Address(kSoundRacerAudibleVtable));
    put(sound, 0, Address(kSoundRacerPrimaryVtable));
    put(sound, 0x48, vehicleAddress);
    expect(std::strcmp(snapshot().status, "no-connection") == 0 && snapshot().nativeAudible == 0,
           "SoundRacer without a connection diagnosed");
    put(sound, 0x48, vehicleAddress + 4);
    expect(std::strcmp(snapshot().status, "sound-owner-or-type-mismatch") == 0,
           "sound owner mismatch blocks deeper reads");
    put(sound, 0x48, vehicleAddress);
    put(sound, 0x54, Address(0x008AC0EC));
    expect(std::strcmp(snapshot().status, "other-audible-type") == 0,
           "float-returning adjacent interface is never treated as IAudible");
    put(sound, 0x54, Address(kSoundRacerAudibleVtable));
    put(sound, 0x5C, reinterpret_cast<std::uintptr_t>(connection.data()));
    expect(std::strcmp(snapshot().status, "other-connection-type") == 0,
           "unknown connection vtable blocks object traversal");
    put(connection, 0, Address(kCarSoundConnVtable));
    expect(std::strcmp(snapshot().status, "no-audio-object") == 0,
           "valid connection with missing internal audio object separated");
    put(connection, 0x14, reinterpret_cast<std::uintptr_t>(audioObject.data()));
    bool flagsMatch = true;
    for (int invalid = 0; invalid <= 1; ++invalid)
        for (int connected = 0; connected <= 1; ++connected)
            for (int active = 0; active <= 1; ++active) {
                connection[0x0C] = static_cast<std::uint8_t>(invalid);
                connection[0x11] = static_cast<std::uint8_t>(connected);
                audioObject[0x22D] = static_cast<std::uint8_t>(active);
                flagsMatch &= snapshot().nativeAudible == (!invalid && connected && active ? 1 : 0);
            }
    expect(flagsMatch, "all eight native audible flag combinations match read-only mirror");
    connection[0x0C] = 0;
    const auto savedVehicle = audioVehicle;
    const auto savedSound = sound;
    const auto savedConnection = connection;
    const auto savedObject = audioObject;
    expect(snapshot().nativeAudible == 1 && audioVehicle == savedVehicle && sound == savedSound &&
           connection == savedConnection && audioObject == savedObject,
           "successful snapshot leaves all vehicle and sound bytes unchanged");
    put(connection, 0x14, static_cast<std::uintptr_t>(1));
    connection[0x0C] = 1;
    expect(std::strcmp(snapshot().status, "invalid-connection") == 0,
           "retiring connection does not follow stale object pointer");
    connection[0x0C] = 0;
    expect(std::strcmp(snapshot().status, "unreadable-audio-object") == 0 &&
           snapshot().nativeAudible == -1,
           "unreadable memory remains unknown rather than a false no-audio result");
    std::uint32_t unread = 0;
    expect(!AudioRead(std::numeric_limits<std::uintptr_t>::max(), 1, &unread),
           "audio address overflow rejected");
    std::array<std::uint8_t, 0x1AC> detailEngine{};
    const auto engineAddress = reinterpret_cast<std::uintptr_t>(detailEngine.data()) + 0x54;
    put(detailEngine, 0x54, Address(0x008AB6E0));
    put(detailEngine, 0x48, vehicleAddress);
    put(detailEngine, 0x180, 4321.0f);
    auto** engineTable = reinterpret_cast<void**>(Address(0x008AB6E0));
    engineTable[1] = reinterpret_cast<void*>(Address(0x006A03A0));
    std::uintptr_t observedVT = 0;
    float observedRpm = kAudioUnknownFloat;
    expect(ReadVerifiedEngineRpm(engineAddress, vehicleAddress, &observedVT, &observedRpm) && observedRpm == 4321.0f,
           "detail RPM mirrors verified native getter without executing it");
    expect(!ReadVerifiedEngineRpm(engineAddress, vehicleAddress+4, &observedVT, &observedRpm),
           "detail RPM rejects wrong engine owner");
    put(detailEngine, 0x180, std::numeric_limits<float>::infinity());
    expect(!ReadVerifiedEngineRpm(engineAddress, vehicleAddress, &observedVT, &observedRpm),
           "detail RPM rejects nonfinite input");
    put(detailEngine, 0x54, Address(0x008AC0EC));
    expect(!ReadVerifiedEngineRpm(engineAddress, vehicleAddress, &observedVT, &observedRpm),
           "detail RPM refuses unknown engine class");
    std::size_t controlsOffset = 0;
    expect(DecodeControlsOffset({0x8D,0x41,0x20,0xC3,0,0,0}, &controlsOffset) && controlsOffset==0x20 &&
           DecodeControlsOffset({0x8D,0x81,0,1,0,0,0xC3}, &controlsOffset) && controlsOffset==0x100,
           "GetControls address-only short and long forms decoded");
    expect(!DecodeControlsOffset({0x8D,0x41,0xFF,0xC3,0,0,0}, &controlsOffset) &&
           !DecodeControlsOffset({0x8D,0x81,0,3,0,0,0xC3}, &controlsOffset) &&
           !DecodeControlsOffset({0xE8,0,0,0,0,0xC3,0}, &controlsOffset),
           "negative excessive or call-based GetControls getters remain unknown");
    std::array<std::uint8_t, 0x24> detailManager{}, detailNode1{}, detailNode2{};
    const auto managerAddress = reinterpret_cast<std::uintptr_t>(detailManager.data());
    const auto node1 = reinterpret_cast<std::uintptr_t>(detailNode1.data());
    const auto node2 = reinterpret_cast<std::uintptr_t>(detailNode2.data());
    const auto objectAddress = reinterpret_cast<std::uintptr_t>(audioObject.data());
    put(detailManager, 0, Address(0x008AC000));
    expect(std::strcmp(ReadMixerMembership(managerAddress,objectAddress).status,"not-registered")==0,
           "empty mixer registry distinguished from unknown memory");
    put(detailManager, 0x10, node1);
    put(detailNode1, 0, Address(0x008AC000));
    put(detailNode1, 4, node2);
    put(detailNode2, 0, Address(0x008AC000));
    put(detailNode2, 0x1C, objectAddress);
    const auto savedNode1 = detailNode1, savedNode2 = detailNode2;
    auto membership = ReadMixerMembership(managerAddress,objectAddress);
    expect(std::strcmp(membership.status,"registered")==0 && membership.node==node2 && membership.visited==2 &&
           membership.validWords==63 && detailNode1==savedNode1 && detailNode2==savedNode2,
           "mixer registry finds exact audio object without modifying nodes");
    put(detailNode2, 0x1C, static_cast<std::uintptr_t>(0));
    put(detailNode2, 4, node1);
    expect(std::strcmp(ReadMixerMembership(managerAddress,objectAddress).status,"cycle")==0,
           "cyclic mixer list terminates and remains unknown");
    put(detailNode1, 4, static_cast<std::uintptr_t>(1));
    expect(std::strcmp(ReadMixerMembership(managerAddress,objectAddress).status,"unreadable-node")==0,
           "stale mixer link does not become not-registered");
    put(detailNode1, 0, static_cast<std::uintptr_t>(0));
    expect(std::strcmp(ReadMixerMembership(managerAddress,objectAddress).status,"unknown-node-type")==0,
           "foreign mixer vtable is not followed");
    std::array<std::array<std::uint8_t,0x24>,129> longList{};
    for(std::size_t i=0;i<longList.size();++i) {
        put(longList[i],0,Address(0x008AC000));
        if(i+1<longList.size()) put(longList[i],4,reinterpret_cast<std::uintptr_t>(longList[i+1].data()));
    }
    put(detailManager,0x10,reinterpret_cast<std::uintptr_t>(longList[0].data()));
    membership=ReadMixerMembership(managerAddress,objectAddress);
    expect(std::strcmp(membership.status,"limit")==0 && membership.visited==128,
           "mixer traversal hard limit prevents unbounded frame work");
    expect(std::strcmp(ReadMixerMembership(0,objectAddress).status,"no-manager")==0 &&
           std::strcmp(ReadMixerMembership(managerAddress,0).status,"no-object")==0,
           "absent mixer manager and absent audio object stay distinct");
    g_audioFlowRecords = {};
    const AudioFlowIdentity flowId{1,2,3,4,5};
    auto ticket = BeginAudioFlow(flowId, 1000);
    expect(ticket.sample && ticket.index == 0, "flow first observation requests one sample");
    AudioFlowValues flowValues;
    flowValues.valid = 9;
    flowValues.feedbackBefore = 0.0f;
    flowValues.normalizedAfter = 0.7f;
    EndAudioFlow(ticket, flowValues, true);
    expect(g_audioFlowRecords[0].entered == 1 && g_audioFlowRecords[0].returned == 1 &&
        g_audioFlowRecords[0].samples == 1, "flow entry return and sampled counts are independent");
    expect(!BeginAudioFlow(flowId,1249).sample && BeginAudioFlow(flowId,1250).sample,
        "flow detailed reads capped at one per 250ms per identity");
    flowValues.feedbackBefore = 0.4f;
    flowValues.normalizedAfter = 0.9f;
    ticket = BeginAudioFlow(flowId,1500);
    EndAudioFlow(ticket,flowValues,true);
    expect(g_audioFlowRecords[0].feedbackMin == 0 && g_audioFlowRecords[0].feedbackMax == 0.4f &&
        g_audioFlowRecords[0].normalizedMin == 0.7f && g_audioFlowRecords[0].normalizedMax == 0.9f,
        "flow sampled feedback and requested normalized RPM ranges preserve differences");
    auto otherId = flowId;
    otherId.object = 6;
    expect(BeginAudioFlow(otherId,1501).index != ticket.index,
        "flow recycled sound with different audio object has a separate identity");
    for (std::size_t i=0; i<kAudioFlowCapacity+1; ++i) {
        auto id=flowId; id.sound=100+i;
        BeginAudioFlow(id,2000+i);
    }
    const auto replacement = g_audioFlowRecords[ticket.index];
    EndAudioFlow(ticket, flowValues, true);
    expect(g_audioFlowRecords[ticket.index].generation == replacement.generation &&
        g_audioFlowRecords[ticket.index].returned == replacement.returned,
        "flow stale completion cannot corrupt bounded slot replacement");
    expect(g_audioFlowRecords.size()==64 && std::all_of(g_audioFlowRecords.begin(),g_audioFlowRecords.end(),
        [](const AudioFlowRecord& r){return r.generation!=0;}), "flow records remain bounded at sixty-four");

    g_audioFlowRecords = {};
    put(connection,0x14,objectAddress);
    connection[0x0C]=0;
    put(sound,0x60,engineAddress);
    put(detailEngine,0x54,Address(0x008AB6E0));
    put(detailEngine,0x180,5000.0f);
    std::array<std::uint8_t,0xA0> flowPacket{};
    std::array<std::uint8_t,0x60> flowAttributes{};
    put(sound,0x8C,reinterpret_cast<std::uintptr_t>(flowAttributes.data()));
    put(sound,0x7C,800.0f);
    put(flowAttributes,0x58,7800.0f);
    put(flowAttributes,0x5C,800.0f);
    AudioFlowIdentity measured;
    expect(ReadAudioFlowIdentity(reinterpret_cast<std::uintptr_t>(sound.data()),&measured),
        "flow accepts exact racer owner and valid audio connection");
    connection[0x0C]=1;
    expect(!ReadAudioFlowIdentity(reinterpret_cast<std::uintptr_t>(sound.data()),&measured),
        "flow skips invalid retiring connections");
    connection[0x0C]=0;
    g_originalAudioFlowUpdate=reinterpret_cast<AudioFlowUpdateFn>(&FakeAudioFlowUpdate);
    const auto beforeFlowSound=sound;
    const auto beforeFlowVehicle=audioVehicle;
    auto expectedPacket=flowPacket;
    put(expectedPacket,4,0.75f);
    const auto flowNativeResult=AudioFlowUpdateHook(sound.data(),nullptr,flowPacket.data());
    expect(flowNativeResult==0xA5B6C7D8, "flow preserves native EAX return value");
    expect(flowNativeCalls==1 && flowForwardedSound==sound.data() && flowForwardedPacket==flowPacket.data(),
        "flow hook forwards original this and packet exactly once");
    const auto& firstFlow=g_audioFlowRecords[0];
    expect(firstFlow.values.valid==0x7FF && firstFlow.values.feedbackBefore==0 &&
        firstFlow.values.normalizedBefore==0 && firstFlow.values.normalizedAfter==0.75f &&
        firstFlow.values.liveRpm==5000 && firstFlow.values.soundAfter==800,
        "flow captures native pre and post packet values with complete validity");
    expect(sound==beforeFlowSound && audioVehicle==beforeFlowVehicle && flowPacket==expectedPacket,
        "flow hook adds no game writes beyond fake native function's own output");
    AcquireSRWLockExclusive(&g_audioFlowLock);
    AudioFlowUpdateHook(sound.data(),nullptr,flowPacket.data());
    ReleaseSRWLockExclusive(&g_audioFlowLock);
    expect(flowNativeCalls==2 && g_audioFlowDropped.load()>0,
        "flow lock contention drops diagnostics without blocking native call");
    AudioFlowUpdateHook(nullptr,nullptr,nullptr);
    expect(flowNativeCalls==3 && flowForwardedSound==nullptr && flowForwardedPacket==nullptr,
        "flow unreadable identity still forwards native arguments exactly once");
    AudioFlowValues unknownFlow;
    ReadAudioFlowBefore(measured,1,&unknownFlow);
    expect(!(unknownFlow.valid & 5) && std::isnan(unknownFlow.feedbackBefore),
        "flow unreadable packet remains unknown rather than zero feedback");
    ticket=BeginAudioFlow(flowId,10000);
    EndAudioFlow(ticket,{},false);
    expect(g_audioFlowRecords[ticket.index].identityChanges==1 &&
        g_audioFlowRecords[ticket.index].values.valid==0,
        "flow identity change during native update rejects post sample");
    // Real MinHook/trampoline test in this disposable test process, never the game.
    // Same first six bytes as 694700, then EAX = packet[0] XOR this; ret 4.
    const std::uint8_t flowStub[] = {0x56,0x57,0x8B,0x7C,0x24,0x0C,
        0x8B,0x07,0x33,0xC1,0x5F,0x5E,0xC2,0x04,0x00};
    void* flowCode=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    bool flowHookInstalled=false;
    if (flowCode) {
        std::memcpy(flowCode,flowStub,sizeof(flowStub));
        FlushInstructionCache(GetCurrentProcess(),flowCode,sizeof(flowStub));
        if (MH_Initialize()==MH_OK) {
            flowHookInstalled=MH_CreateHook(flowCode,&AudioFlowUpdateHook,
                reinterpret_cast<void**>(&g_originalAudioFlowUpdate))==MH_OK &&
                MH_EnableHook(flowCode)==MH_OK;
        }
    }
    expect(flowHookInstalled,"flow native-shaped x86 prologue accepts real MinHook trampoline");
    if (flowHookInstalled) {
        put(flowPacket,0,std::uint32_t{0x10203040});
        const auto beforeEntered=g_audioFlowEntered.load();
        const auto stubResult=reinterpret_cast<AudioFlowUpdateFn>(flowCode)(sound.data(),flowPacket.data());
        expect(stubResult==(0x10203040u ^ reinterpret_cast<std::uintptr_t>(sound.data())) &&
            g_audioFlowEntered.load()==beforeEntered+1,
            "flow real trampoline preserves this packet EAX and x86 stack cleanup");
    }
    if (flowCode) {
        MH_DisableHook(flowCode);
        MH_RemoveHook(flowCode);
        MH_Uninitialize();
        VirtualFree(flowCode,0,MEM_RELEASE);
    }
    std::array<std::uint8_t,0x300> producer{};
    std::array<std::uint8_t,0x40> producerParent{}, producerContext{};
    std::array<std::uint8_t,0x88> producerState{};
    const auto producerAddress=reinterpret_cast<std::uintptr_t>(producer.data());
    put(producer,0,Address(0x00896BF0));
    put(producer,0x14,reinterpret_cast<std::uintptr_t>(producerParent.data()));
    put(producer,0x18,reinterpret_cast<std::uintptr_t>(producerState.data()));
    put(producer,0x28,reinterpret_cast<std::uintptr_t>(producerContext.data()));
    put(producerParent,0x34,objectAddress);
    put(producerState,0x74,0.25f);
    AudioSourceIdentity producerId;
    expect(ReadAudioSourceIdentity(0,producerAddress,&producerId) && producerId.object==objectAddress,
        "source follows verified producer-parent-audio object chain");
    expect(!ReadAudioSourceIdentity(1,producerAddress,&producerId) &&
        !ReadAudioSourceIdentity(0,1,&producerId), "source rejects wrong route and unreadable identity");
    expect(AudioSourceType(1,Address(0x00896D2C)) && AudioSourceType(1,Address(0x008974AC)) &&
        !AudioSourceType(0,Address(0x008974AC)), "source route B accepts exactly both verified native variants");
    g_audioSourceRecords={};
    g_originalAudioSource[0]=reinterpret_cast<AudioSourceFn>(&FakeAudioSource);
    const auto producerSaved=producer;
    const auto producerParentSaved=producerParent;
    auto producerStateExpected=producerState;
    auto sourceObjectExpected=audioObject;
    put(producerStateExpected,0x74,0.5f);
    put(sourceObjectExpected,0x248,0.5f);
    const auto sourceResult=AudioSourceAHook(producer.data(),nullptr,0x12345678);
    expect(sourceNativeCalls==1 && sourceNativeThis==producer.data() && sourceNativeArg==0x12345678 &&
        sourceResult==(0x12345678u^producerAddress), "source forwards this raw argument and EAX exactly once");
    const auto& sourceRecord=g_audioSourceRecords[0];
    expect(sourceRecord.values.valid==255 && sourceRecord.values.stateBefore==0.25f &&
        sourceRecord.values.stateAfter==0.5f && sourceRecord.values.objectAfter==0.5f && sourceRecord.rangeValid,
        "source captures writer inputs outputs and feedback range with validity");
    expect(producer==producerSaved && producerParent==producerParentSaved &&
        producerState==producerStateExpected && audioObject==sourceObjectExpected,
        "source observation adds no writes beyond native stub output");
    AcquireSRWLockExclusive(&g_audioSourceLock);
    AudioSourceAHook(producer.data(),nullptr,0xFFFFFFFF);
    ReleaseSRWLockExclusive(&g_audioSourceLock);
    expect(sourceNativeCalls==2 && g_audioSourceDrops.load()>0,
        "source contention never blocks or suppresses original call");
    expect(AudioSourceAHook(nullptr,nullptr,123)==123 && sourceNativeCalls==3,
        "source missing identity still preserves original call and result");
    g_audioSourceRecords={};
    auto sourceTicket=BeginAudioSource(producerId,1000);
    expect(sourceTicket.sample && !BeginAudioSource(producerId,1249).sample &&
        BeginAudioSource(producerId,1250).sample, "source detail reads are bounded to 250ms intervals");
    for (std::size_t i=0;i<65;++i) {
        auto id=producerId;id.source=100+i;BeginAudioSource(id,2000+i);
    }
    const auto sourceGeneration=g_audioSourceRecords[sourceTicket.index].generation;
    EndAudioSource(sourceTicket,{},true);
    expect(g_audioSourceRecords.size()==64 && g_audioSourceRecords[sourceTicket.index].generation==sourceGeneration &&
        g_audioSourceRecords[sourceTicket.index].returned==0,
        "source sixty-four-slot bound rejects replaced generation's late return");
    AudioSourceValues unreadSource;
    auto invalidSource=producerId;invalidSource.object=1;invalidSource.state=1;
    ReadAudioSourceBefore(invalidSource,&unreadSource);
    expect(!(unreadSource.valid&53) && std::isnan(unreadSource.objectBefore),
        "source invalid producer state is unknown rather than zero feedback");
    // Native-shaped A uses aligned EBP frame; B saves EBX/ESI. Exercise both detours.
    const std::uint8_t sourceStubA[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x74,
        0x8B,0xC1,0x33,0x45,0x08,0x8B,0xE5,0x5D,0xC2,4,0};
    const std::uint8_t sourceStubB[]={0x53,0x8B,0x5C,0x24,0x08,0x56,0x8B,0xF1,
        0x8B,0xC3,0x33,0xC1,0x5E,0x5B,0xC2,4,0};
    for (unsigned route=0;route<2;++route) {
        void* sourceCode=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
        bool installed=false;
        if (sourceCode) {
            std::memcpy(sourceCode,route==0?sourceStubA:sourceStubB,route==0?sizeof(sourceStubA):sizeof(sourceStubB));
            FlushInstructionCache(GetCurrentProcess(),sourceCode,64);
            if (MH_Initialize()==MH_OK)
                installed=MH_CreateHook(sourceCode,route==0?&AudioSourceAHook:&AudioSourceBHook,
                    reinterpret_cast<void**>(&g_originalAudioSource[route]))==MH_OK && MH_EnableHook(sourceCode)==MH_OK;
        }
        expect(installed,route==0?"source A aligned prologue real trampoline installs":"source B saved-register prologue real trampoline installs");
        if (installed) {
            put(producer,0,Address(route==0?0x00896BF0:0x00896D2C));
            const auto count=g_audioSourceCalls[route].load();
            const auto result=reinterpret_cast<AudioSourceFn>(sourceCode)(producer.data(),0xFE123456);
            expect(result==(0xFE123456u^producerAddress) && g_audioSourceCalls[route].load()==count+1,
                route==0?"source A trampoline preserves ABI and EAX":"source B trampoline preserves ABI and EAX");
        }
        if (sourceCode) {
            MH_DisableHook(sourceCode);MH_RemoveHook(sourceCode);MH_Uninitialize();VirtualFree(sourceCode,0,MEM_RELEASE);
        }
    }
#include "AudioLifecycleTests.inl"
#include "AudioPoolTests.inl"
#include "EncounterSignalTests.inl"
#include "Alpha23Tests.inl"
#include "EncounterBattleModelTests.inl"
#include "Alpha26Tests.inl"
#include "EncounterGaugeTests.inl"
#include "Alpha27Tests.inl"
#include "Alpha28Tests.inl"
#include "Alpha29Tests.inl"
#include "Alpha30Tests.inl"
#include "Alpha31Tests.inl"
#include "Alpha32Tests.inl"
#include "Alpha33Tests.inl"
#include "Alpha34Tests.inl"
#include "Alpha35Tests.inl"
#include "Alpha36Tests.inl"
#include "Alpha37Tests.inl"
#include "Alpha38Tests.inl"
#include "Alpha39Tests.inl"
#include "Alpha40Tests.inl"
#include "Alpha41Tests.inl"
#include "Alpha42Tests.inl"
#include "Alpha43Tests.inl"
#include "Alpha44Tests.inl"
#include "Alpha45Tests.inl"
#include "Alpha46Tests.inl"
#include "Alpha47Tests.inl"
#include "Alpha50Tests.inl"
#include "Alpha51Tests.inl"
#include "Alpha52Tests.inl"
#include "Alpha53Tests.inl"
#include "Alpha54Tests.inl"
#include "Alpha55Tests.inl"
#include "Alpha57Tests.inl"
#include "Alpha58Tests.inl"
#include "Alpha59Tests.inl"
    VirtualFree(region, 0, MEM_RELEASE);
    return failures == 0 ? 0 : 1;
}
}
int main() { return native_freeroam::RunCacheTests(); }
