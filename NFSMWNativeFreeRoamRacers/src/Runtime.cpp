#include "../../integration/InstalledCarType.h"
#include "Runtime.h"
#include "RuntimeConfig.h"
#include "../../integration/AppearanceSafety.h"
#include "MarkerRetry.h"
#include "PromptCallSurface.h"
#include "../../NFSMWEventLoader/src/NeoDriverBridge.h"
#include "RacePowerPolicy.h"
#include "../include/JunkmanMirrorPolicy.h"
#include "ExecutableTargets.h"

#include "Logging.h"
#include "VehicleCatalog.h"
#include "EncounterBattleModel.h"
#include "EncounterRouteTrail.h"
#include "EncounterPursuit.h"
#include "EncounterPursuitPath.h"
#include "EncounterCommittedPath.h"
#include "EncounterCustomAI.h"
#include "EncounterDirectionalTrail.h"
#include "EncounterChaseSpeed.h"
#include "RoamingPace.h"
#include "PopulationPolicy.h"
#include "RefillPolicy.h"
#include "AnchorSearch.h"
#include "AudioExpansionPolicy.h"
#include "AdaptiveSpawnPolicy.h"
#include <NFSPluginSDK/Game.MW05/Types/WRoadNav.h>
#include "EncounterText.h"
#include "EncounterReward.h"
#include "EncounterWager.h"
#include "EncounterWeapons.h"
#include "EncounterWave.h"

#include <Windows.h>
#include <Wincrypt.h>
#include <mmsystem.h>
#include <d3d9.h>
#include <MinHook.h>
#include <intrin.h>

#include <NFSPluginSDK/Game.MW05/Types/AIGoal.h>
#include <NFSPluginSDK/Game.MW05/Types/VehicleBehavior.h>
#include <NFSPluginSDK/Game.MW05/Types/AIVehicle.h>
#include <NFSPluginSDK/Game.MW05/Types/GIcon.h>
#include <NFSPluginSDK/Game.MW05/Types/GManager.h>
#include <NFSPluginSDK/Game.MW05/Types/GRaceStatus.h>
#include <NFSPluginSDK/Game.MW05/Types/PVehicle.h>
#include <NFSPluginSDK/Game.MW05/Types/VehicleParams.h>
#include <NFSPluginSDK/Game.MW05/Types/RideInfo.h>
#include <NFSPluginSDK/Game.MW05/Types/FECustomizationRecord.h>
#include <NFSPluginSDK/Game.MW05/Types/cFrontendDatabase.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <limits>
#include <optional>
#include <type_traits>
#include <random>
#include <string>
#include <vector>
#include <deque>
#include <NFSPluginSDK/Game.MW05/Types/Attrib/Database.h>
#include <NFSPluginSDK/Game.MW05/Types/Attrib/Class.h>
#include <NFSPluginSDK/Game.MW05/Types/Attrib/Collection.h>
#include <NFSPluginSDK/Game.MW05/Types/Attrib/RefSpec.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Winmm.lib")

namespace native_freeroam {
namespace {

constexpr std::uintptr_t kPreferredBase = 0x00400000;
constexpr std::uintptr_t kMainDisplayFrame = 0x0064A7C0;
constexpr std::uintptr_t kGameFlowState = 0x00925E90;
constexpr std::uintptr_t kRaceStatus = 0x0091E000;
constexpr std::uintptr_t kVehicleListData = 0x0092CD1C;
constexpr std::uintptr_t kVehicleListCount = 0x0092CD24;
constexpr std::uintptr_t kGManagerGlobal = 0x0091E00C;
constexpr std::uintptr_t kIVehicleVtable = 0x008AA828;
constexpr std::uintptr_t kISimableVtable = 0x008AA940;
constexpr std::uintptr_t kTrafficIVehicleAIVtable = 0x00891C18;
constexpr std::uintptr_t kRacecarPrimaryVtable = 0x00892720;
constexpr std::uintptr_t kRacecarIVehicleAIVtable = 0x00892640;
constexpr std::uintptr_t kAIGoalRacerVtable = 0x00892D30;
constexpr std::uintptr_t kAIActionRaceVtable = 0x00891090;
constexpr std::uintptr_t kAIVehicleSetGoal = 0x00422480;
constexpr std::uintptr_t kGoalChooseAction = 0x0042B070;
constexpr std::uintptr_t kGetPosition = 0x00688340;
constexpr std::uintptr_t kGetSpeed = 0x006881A0;
constexpr std::uintptr_t kComputeHeading = 0x00670700;
constexpr std::uintptr_t kSetDriverClass = 0x006876E0;
constexpr std::uintptr_t kGetDriverClass = 0x006880B0;
constexpr std::uintptr_t kGetOffScreenTime = 0x00688120;
constexpr std::uintptr_t kGetOnScreenTime = 0x00688130;
constexpr std::uintptr_t kGetVehicleKey = 0x006880A0;
constexpr std::uintptr_t kGetAIVehicle = 0x00688230;
constexpr std::uintptr_t kGetSimable = 0x00688030;
constexpr std::uintptr_t kActivateVehicle = 0x006693A0;
constexpr std::uintptr_t kDeactivateVehicle = 0x006693C0;
constexpr std::uintptr_t kKillSimable = 0x006851D0;
constexpr std::uintptr_t kRaceStatusCacheVtable = 0x008A3A60;
constexpr std::uintptr_t kRaceStatusCacheQuery = 0x005E8A20;
constexpr std::uintptr_t kAIResetVehicleToRoadNav = 0x00422690;
constexpr std::uintptr_t kAISetSpawned = 0x00415D00;
constexpr std::uintptr_t kAIGetCurrentRoad = 0x00442A70;
constexpr std::uintptr_t kGManagerAllocIcon = 0x005E9EC0;
constexpr std::uintptr_t kGIconUnspawn = 0x005E5A00;
constexpr std::uintptr_t kGIconSetPosition = 0x005E5A90;
constexpr std::uintptr_t kGIconSpawn = 0x005EC270;
constexpr std::uintptr_t kVehicleBudgetCompareImmediate = 0x00687819;
constexpr std::uintptr_t kVehicleBudgetSubtractDisplacement = 0x0068781E;
constexpr std::uintptr_t kVehicleEvictionReserveImmediate = 0x00687855;
constexpr std::uintptr_t kTrafficCapacityCompareImmediate = 0x0041EC15;
constexpr std::uintptr_t kTrafficPopulationAddImmediate = 0x00426332;
constexpr std::uint8_t kNativeVehicleBudget = 20;
// Alpha.60's observed AI/driver-pool boundary remains the fallback. Alpha.64
// relaxes it only with the optional provider's verified relocated companion
// vectors, actual vacancy and expiring traffic reservation.
constexpr std::size_t kObservedRacerConstructionLiveCeiling = 21;

constexpr std::size_t kSlotSimable = 2;
constexpr std::size_t kSlotPosition = 3;
constexpr std::size_t kSlotVehicleKey = 20;
constexpr std::size_t kSlotSetDriverClass = 21;
constexpr std::size_t kSlotDriverClass = 22;
constexpr std::size_t kSlotOffScreenTime = 24;
constexpr std::size_t kSlotOnScreenTime = 25;
constexpr std::size_t kSlotActivateVehicle = 32;
constexpr std::size_t kSlotDeactivateVehicle = 33;
constexpr std::size_t kSlotSpeed = 35;
constexpr std::size_t kSlotAIVehicle = 43;
constexpr std::size_t kSlotComputeHeading = 46;
constexpr std::size_t kSlotAIResetVehicleToRoadNav = 22;
constexpr std::size_t kSlotAISetSpawned = 34;
constexpr std::size_t kSlotAIGetCurrentRoad = 45;
constexpr std::size_t kGoalChooseActionSlot = 1;

constexpr std::uint32_t kGameFlowRacing = 6;
constexpr std::uint32_t kDriverHuman = 0;
constexpr std::uint32_t kDriverTraffic = 1;
constexpr std::uint32_t kDriverRacer = 3;
constexpr std::uint32_t kInitialVehicleReserve = 256; // Preallocation, NOT an admission limit.
constexpr std::uint32_t kMaximumIcons = 200;
constexpr std::size_t kMaximumRacersLimit = 15;
constexpr std::size_t kDefaultMaximumRacers = 6;
constexpr float kManagementIntervalSeconds = 0.05f;
constexpr std::size_t kGManagerIconCountOffset = 0x2BC;
constexpr std::size_t kGManagerIconTableOffset = 0x2C4;
constexpr std::size_t kRacecarPrimaryFromAIOffset = 0x4C;
constexpr std::size_t kRacecarGoalOffset = 0xB8;
constexpr std::size_t kRacecarGoalNameOffset = 0xC4;
constexpr std::size_t kGoalActionOffset = 0x04;
constexpr std::size_t kAIDriveNavOffset = 0x24;
constexpr std::size_t kAIDriveFlagsOffset = 0x80;
constexpr std::size_t kRoadNavValidOffset = 0x50;
constexpr std::size_t kRoadNavTypeOffset = 0x78;
constexpr std::size_t kRoadNavPathTypeOffset = 0x7C;
constexpr std::uint32_t kNavDirection = 2;
constexpr std::uint32_t kPathRacer = 2;
constexpr std::uint32_t kAIGoalRacerHash = 0x08D0D8A7;

constexpr wchar_t kExpectedExecutableName[] = L"speed.exe";
// Default only supports isolated offline tests. Live initialization verifies and
// selects the actual identity before installing hooks or reading the catalog.
const char* g_verifiedExecutableSha256 = nfrr_target::targets[0].sha256;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct VehicleSnapshot {
    void* pointer = nullptr;
    Vec3 position{};
    Vec3 heading{};
    float speed = 0.0f;
    float offScreenTime = 0.0f;
    float onScreenTime = 0.0f;
    std::uint32_t vehicleKey = 0;
    std::uint32_t driverClass = std::numeric_limits<std::uint32_t>::max();
};

struct Settings {
    bool enabled = true;
    bool encounterSignalEnabled = true;
    bool encounterEnglish = false;
    bool backgroundPoliceEnabled = true;
    std::array<float,3> encounterPowerScales{1.50f,1.75f,2.00f};
    encounter_custom::Mode encounterAIMode=encounter_custom::Mode::Stable;
    bool encounterWinStreak=false;
    float customAILeaderPowerScale=1.25f;
    float cruisingSpeedPercent=60.0f;
    float startSpeedToleranceKmh=10.0f;
    unsigned freeRoamAudioSlots = 4;
    bool lbEngineAudioExpansion = false;
    bool lbVehicleVarietyExpansion = false;
    unsigned lbNonRacerSlots = 0;
    bool adaptiveSpawnPacing = false;
    bool retainPreparedAppearance = false;
    std::size_t vehicleVariety = 5;
    bool randomAppearance = true;
    unsigned appearanceAttempts = 3;
    std::size_t maximumRacers = kDefaultMaximumRacers;
    std::uint8_t totalVehicleBudget = kNativeVehicleBudget;
    float populationRadiusMeters = 600.0f;
    float markerRadiusMeters = 200.0f;
    float anchorMinimumMeters = 350.0f;
    float anchorMaximumMeters = 700.0f;
    float anchorMinimumSpeedMps = 1.0f;
    float anchorHeadingDotMinimum = 0.25f;
    float spawnIntervalSeconds = 5.0f;
    float missingGraceSeconds = 2.0f;
    float telemetryIntervalSeconds = 5.0f;
    bool deepAudioDiagnostics = false;
    bool frameTimingDiagnostics = false;
    float retirementOffScreenSeconds = 1.0f;
    float hardRetirementMarginMeters = 500.0f;
};

struct MarkerSlot {
    NFSPluginSDK::MW05::GIcon* icon = nullptr;
    void* owner = nullptr;
    bool visible = false;
};

struct ManagedRacer {
    void* pointer = nullptr;
    void* simable = nullptr;
    std::uint32_t vehicleKey = 0;
    std::size_t markerSlot = kMaximumRacersLimit;
    std::string preset;
    float ageSeconds = 0.0f;
    float missingSeconds = 0.0f;
    float lastDistanceMeters = std::numeric_limits<float>::infinity();
    bool retirementDeferred = false;
    bool customExterior = false;
    int encounterActor = -1;
};

using DisplayFrameFn = int(__cdecl*)();
using CacheQueryFn = std::uint32_t(__thiscall*)(void*, const void*, const void*);
using AdvancedCivilianQueryFn = int(__cdecl*)(const void*);
using ExternalPopulationTickFn = void(__cdecl*)();

std::uintptr_t g_base = 0;
DisplayFrameFn g_originalDisplayFrame = nullptr;
CacheQueryFn g_originalCacheQuery = nullptr;
Settings g_settings{};
std::vector<ManagedRacer> g_racers;
std::vector<VehicleSnapshot> g_vehicleScratch;
AnchorSearch g_anchorSearch;
NFSPluginSDK::MW05::GManager* g_markerManager = nullptr;
std::array<MarkerSlot, kMaximumRacersLimit> g_markers{};
nfrr_marker::Retry g_markerRetry{};
std::array<std::atomic<std::uintptr_t>, kMaximumRacersLimit>
    g_protectedSimables{};
std::array<std::atomic<std::uintptr_t>, kMaximumRacersLimit>
    g_protectedVehicles{};
std::atomic<bool> g_streamRetentionEnabled{false};
std::array<std::atomic<std::uint32_t>, kMaximumRacersLimit> g_protectedKeys{};
std::atomic<std::uint32_t> g_cacheWanted{0};
std::atomic<std::uint32_t> g_cacheChangedVotes{0};
std::atomic<std::uintptr_t> g_lastCacheVehicle{0};
SRWLOCK g_constructionSetLock=SRWLOCK_INIT;
std::vector<std::uintptr_t> g_externalConstructionProtectedVehicles;
std::atomic<std::uint32_t> g_externalConstructionProtectedCount{0};
std::atomic<bool> g_externalConstructionProtectionClaimed{false};
std::atomic<bool> g_externalConstructionProtectionActive{false};
std::atomic<std::uint32_t> g_externalConstructionProtectedVotes{0};
std::atomic<std::uint32_t> g_externalConstructionProtectionStartVotes{0};
std::mt19937 g_random{};
LARGE_INTEGER g_frequency{};
LARGE_INTEGER g_lastTick{};
float g_spawnTimer = 0.0f;
float g_telemetryTimer = 0.0f;
float g_managementPending = 0.0f;
bool g_worldReady = false;
bool g_faulted = false;
std::uint32_t g_consecutiveSpawnFailures = 0;
float g_spawnFailureBackoffSeconds = 0.0f;
std::atomic<ExternalPopulationTickFn> g_externalPopulationTick{nullptr};
// Keep the population provider's V1 slot independent from frame observers.
// World adapters must keep updating during races and frontend transitions too.
std::array<std::atomic<ExternalPopulationTickFn>, 4> g_frameObservers{};
std::atomic<bool> g_frameObserverReady{false};

bool IsAdvancedCivilianVehicle(const void* vehicle) noexcept {
    // Optional, lazy bridge.  The civilian ASI may load before or after this
    // plugin; absence must preserve the existing anchor-selection behavior.
    const auto module = GetModuleHandleW(L"MWAdvancedCivilianVehicles.asi");
    if (module == nullptr) return false;
    const auto query = reinterpret_cast<AdvancedCivilianQueryFn>(
        GetProcAddress(module, "MWACV_IsManagedVehicleV1"));
    return query != nullptr && query(vehicle) != 0;
}

static_assert(sizeof(NFSPluginSDK::MW05::UMath::Vector3) == 12,
              "UMath vector ABI changed");
static_assert(sizeof(NFSPluginSDK::MW05::GIcon) == 32, "GIcon ABI changed");
static_assert(sizeof(NFSPluginSDK::MW05::RideInfo) == 0x310, "RideInfo ABI changed");
static_assert(sizeof(NFSPluginSDK::MW05::FECustomizationRecord) == 0x198, "Customization ABI changed");
static_assert(offsetof(NFSPluginSDK::MW05::RideInfo, mPartsTable) == 0x48, "RideInfo parts ABI changed");

std::uintptr_t Address(const std::uintptr_t absolute) noexcept {
    return g_base + (absolute - kPreferredBase);
}

template <typename T>
bool SafeRead(const void* address, T* output) noexcept {
    if (address == nullptr || output == nullptr) return false;
    __try {
        *output = *static_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

Vec3 FromUMath(const NFSPluginSDK::MW05::UMath::Vector3& value) noexcept {
    return {value.x, value.y, value.z};
}

NFSPluginSDK::MW05::UMath::Vector3 ToUMath(const Vec3& value) noexcept {
    NFSPluginSDK::MW05::UMath::Vector3 result{};
    result.x = value.x;
    result.y = value.y;
    result.z = value.z;
    return result;
}

NFSPluginSDK::MW05::Math::Vector3 ToMath(const Vec3& value) noexcept {
    NFSPluginSDK::MW05::Math::Vector3 result{};
    result.x = value.x;
    result.y = value.y;
    result.z = value.z;
    return result;
}

float Distance(const Vec3& a, const Vec3& b) noexcept {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float HorizontalHeadingDot(const Vec3& a, const Vec3& b) noexcept {
    const float aLength = std::sqrt(a.x * a.x + a.z * a.z);
    const float bLength = std::sqrt(b.x * b.x + b.z * b.z);
    if (aLength < 0.001f || bLength < 0.001f) return -1.0f;
    return (a.x * b.x + a.z * b.z) / (aLength * bLength);
}

std::size_t ParseMaximumRacers(const wchar_t* text) noexcept {
    if (text == nullptr) return kDefaultMaximumRacers;
    wchar_t* end = nullptr;
    const long parsed = std::wcstol(text, &end, 10);
    if (end == text) return kDefaultMaximumRacers;
    while (*end == L' ' || *end == L'\t') ++end;
    if (*end != L'\0') return kDefaultMaximumRacers;
    return static_cast<std::size_t>(std::clamp<long>(parsed, 1, 15));
}

std::size_t ParseVehicleVariety(const wchar_t* text) noexcept {
    if (!text) return 5;
    wchar_t* end = nullptr;
    const long parsed = std::wcstol(text, &end, 10);
    if (end == text) return 5;
    while (*end == L' ' || *end == L'\t') ++end;
    if (*end != L'\0') return 5;
    return static_cast<std::size_t>(std::clamp<long>(parsed, 1, 5));
}

// One management pass at most per rendered frame, with no catch-up bursts.
bool ConsumeManagementTick(float elapsed, float* pending, float* step) noexcept {
    *pending += elapsed;
    if (*pending < kManagementIntervalSeconds) return false;
    *step = *pending;
    *pending = 0.0f;
    return true;
}

float ReadFloatSetting(const wchar_t* path, const wchar_t* section,
                       const wchar_t* key, const float fallback) noexcept {
    wchar_t fallbackText[64]{};
    wchar_t value[64]{};
    swprintf_s(fallbackText, L"%.3f", fallback);
    GetPrivateProfileStringW(section, key, fallbackText, value,
                             static_cast<DWORD>(std::size(value)), path);
    wchar_t* end = nullptr;
    const float parsed = wcstof(value, &end);
    return end != value && std::isfinite(parsed) ? parsed : fallback;
}

#include "EncounterPowerSettings.inl"

unsigned ParseNativeHeadroom(const wchar_t* text) noexcept {
    if(!text)return 0;
    wchar_t* end=nullptr;const long value=std::wcstol(text,&end,10);
    if(end==text)return 0;
    while(*end==L' '||*end==L'\t')++end;
    return *end?0u:static_cast<unsigned>(std::clamp<long>(value,0,64));
}
bool ReadLbAudioExpansionSetting(const wchar_t* path) noexcept {
    // New section wins, including explicit OFF. Keep old user INIs compatible.
    const auto legacy=GetPrivateProfileIntW(L"Audio",L"LBEngineAudioExpansion",0,path);
    return GetPrivateProfileIntW(L"LimitBreaker",L"LBEngineAudioExpansion",legacy,path)==1;
}

static bool g_eventSelectionOnly=false;
void LoadSettings() noexcept {
    wchar_t modulePath[MAX_PATH]{};
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&LoadSettings), &self);
    if (self == nullptr ||
        GetModuleFileNameW(self, modulePath, MAX_PATH) == 0)
        return;
    wchar_t* extension = wcsrchr(modulePath, L'.');
    if (extension != nullptr)
        wcscpy_s(extension, MAX_PATH - (extension - modulePath), L".ini");

    const auto editorPath=nfrr_config::EditorPath(modulePath);
    const auto settingsPath=nfrr_config::SettingsPath(modulePath);
    wcscpy_s(modulePath,settingsPath.c_str());
    const bool editorRequested=GetPrivateProfileIntW(L"Editor",L"EventSelectionOnly",0,modulePath)!=0;
    g_settings.enabled =
        GetPrivateProfileIntW(L"Population", L"Enabled", editorRequested?0:1, modulePath) != 0;
    const bool editorFallback=GetPrivateProfileIntW(L"Editor",L"EventSelectionOnly",0,editorPath.c_str())!=0;
    g_eventSelectionOnly=nfrr_config::SelectionOnly(editorRequested||editorFallback,g_settings.enabled);
    Log(LogLevel::Info,"NFRR_CONFIG population=%u editorOnly=%u settings=%ls",
        unsigned(g_settings.enabled),unsigned(g_eventSelectionOnly),modulePath);
    Log(LogLevel::Info,"UI_RECOVERY_POLICY markerRetryMs=1000-8000 markerWarningMs=30000 sharedManagerBackoff=1 fullTableNativeCalls=0 promptEventLoaderAdapter=1 versionGate=0");
    g_settings.encounterSignalEnabled =
        GetPrivateProfileIntW(L"Encounter", L"SignalEnabled", 1, modulePath) != 0;
    wchar_t encounterLanguage[16]{};
    GetPrivateProfileStringW(L"Encounter", L"Language", L"ja", encounterLanguage,16,modulePath);
    g_settings.encounterEnglish = _wcsicmp(encounterLanguage,L"en")==0;
    LoadEncounterPowerSettings(modulePath);
    wchar_t cruiseText[128]{},toleranceText[128]{};
    const auto cruiseCount=GetPrivateProfileStringW(L"Population",L"CruisingSpeedPercent",L"60",cruiseText,128,modulePath);
    const auto toleranceCount=GetPrivateProfileStringW(L"Encounter",L"StartSpeedToleranceKmh",L"10",toleranceText,128,modulePath);
    g_settings.cruisingSpeedPercent=cruiseCount>=127?60.f:roaming_pace::Parse(cruiseText,60,1,100);
    g_settings.startSpeedToleranceKmh=toleranceCount>=127?10.f:roaming_pace::Parse(toleranceText,10,0,100);
    Log(LogLevel::Info,"ROAMING_PACE config cruisePercent=%.1f startSpeedToleranceKmh=%.1f restartRequired=1",
        g_settings.cruisingSpeedPercent,g_settings.startSpeedToleranceKmh);
    g_settings.backgroundPoliceEnabled = GetPrivateProfileIntW(L"BackgroundPolice", L"Enabled", 1, modulePath) != 0;
    g_settings.freeRoamAudioSlots = static_cast<unsigned>(std::clamp(
        static_cast<int>(GetPrivateProfileIntW(L"Audio", L"FreeRoamSlots", 4, modulePath)), 0, 4));
    g_settings.lbEngineAudioExpansion=ReadLbAudioExpansionSetting(modulePath);
    g_settings.lbVehicleVarietyExpansion=GetPrivateProfileIntW(L"LimitBreaker",L"LBVehicleVarietyExpansion",0,modulePath)==1;
    wchar_t headroomText[64]{};
    const auto headroomLength=GetPrivateProfileStringW(L"LimitBreaker",L"LBNonRacerSlots",L"0",headroomText,64,modulePath);
    g_settings.lbNonRacerSlots=headroomLength>=63?0u:ParseNativeHeadroom(headroomText);
    g_settings.adaptiveSpawnPacing=GetPrivateProfileIntW(L"Population",L"AdaptiveSpawnPacing",0,modulePath)==1;
    g_settings.retainPreparedAppearance=GetPrivateProfileIntW(L"Population",L"RetainPreparedAppearance",0,modulePath)==1;
    Log(LogLevel::Info,"SPAWN_PREPARATION_POLICY retainAppearance=%u readyLifetimeMs=%u incompleteLifetimeMs=5000 freshCareerCheck=1 extraReservation=0 lbRequired=0",
        g_settings.retainPreparedAppearance?1u:0u,g_settings.retainPreparedAppearance?15000u:5000u);
    Log(LogLevel::Info,"SPAWN_LOAD_POLICY enabled=%u thresholdUs=50000 cooldownMs=750 maxDeferralMs=2000 newPreparationOnly=1 lbRequired=0 diagnosticSwitchIndependent=1",
        g_settings.adaptiveSpawnPacing?1u:0u);
    wchar_t varietyText[64]{};
    GetPrivateProfileStringW(L"Vehicles", L"Variety", L"5", varietyText, 64, modulePath);
    g_settings.vehicleVariety = ParseVehicleVariety(varietyText);
    g_settings.randomAppearance = GetPrivateProfileIntW(L"Vehicles", L"RandomAppearance", 1, modulePath) != 0;
    g_settings.appearanceAttempts = static_cast<unsigned>(std::clamp(
        static_cast<int>(GetPrivateProfileIntW(L"Vehicles", L"AppearanceAttempts", 3, modulePath)), 1, 5));
    wchar_t maximumText[64]{};
    GetPrivateProfileStringW(L"Population", L"MaximumRacers", L"6",
                            maximumText, 64, modulePath);
    g_settings.maximumRacers = ParseMaximumRacers(maximumText);
    const int requestedVehicleBudget = static_cast<int>(GetPrivateProfileIntW(
        L"Population", L"TotalVehicleBudget", kNativeVehicleBudget,
        modulePath));
    g_settings.totalVehicleBudget = kNativeVehicleBudget;
    if (requestedVehicleBudget != kNativeVehicleBudget) {
        Log(LogLevel::Error,
            "VEHICLE_BUDGET_CLAMPED requested=%d effective=%u reason=fixed-pvehicle-array-capacity",
            requestedVehicleBudget,
            static_cast<unsigned>(kNativeVehicleBudget));
    }
    g_settings.populationRadiusMeters = std::clamp(
        ReadFloatSetting(modulePath, L"Population", L"RadiusMeters", 600.0f),
        100.0f, 1000.0f);
    g_settings.markerRadiusMeters = std::clamp(
        ReadFloatSetting(modulePath, L"Markers", L"RadiusMeters", 200.0f),
        25.0f, g_settings.populationRadiusMeters);
    g_settings.anchorMinimumMeters = std::clamp(
        ReadFloatSetting(modulePath, L"Spawning", L"AnchorMinimumMeters", 350.0f),
        g_settings.markerRadiusMeters + 25.0f,
        g_settings.populationRadiusMeters - 25.0f);
    g_settings.anchorMaximumMeters = std::clamp(
        ReadFloatSetting(modulePath, L"Spawning", L"AnchorMaximumMeters", 700.0f),
        g_settings.anchorMinimumMeters, g_settings.populationRadiusMeters);
    g_settings.anchorMinimumSpeedMps = std::clamp(
        ReadFloatSetting(modulePath, L"Spawning", L"MinimumAnchorSpeedKmh", 3.6f) /
            3.6f,
        0.0f, 20.0f);
    g_settings.anchorHeadingDotMinimum = std::clamp(
        ReadFloatSetting(modulePath, L"Spawning", L"MinimumHeadingDot", 0.25f),
        -1.0f, 1.0f);
    g_settings.spawnIntervalSeconds = std::clamp(
        ReadFloatSetting(modulePath, L"Spawning", L"IntervalSeconds", 5.0f),
        0.5f, 10.0f);
    g_settings.telemetryIntervalSeconds = std::clamp(
        ReadFloatSetting(modulePath, L"Diagnostics", L"TelemetryIntervalSeconds",
                         5.0f),
        1.0f, 60.0f);
    const auto diagnosticFlag=[&](const wchar_t* key) noexcept {
        if(!LoggingEnabled())return false;
        wchar_t value[8]{};
        GetPrivateProfileStringW(L"Diagnostics",key,L"0",value,8,modulePath);
        return wcscmp(value,L"1")==0;
    };
    g_settings.deepAudioDiagnostics = diagnosticFlag(L"DeepAudioDiagnostics");
    g_settings.frameTimingDiagnostics = diagnosticFlag(L"FrameTiming");
}

#include "PerformanceDiagnostics.inl"
#include "AdaptiveSpawnPacing.inl"

bool ComputeSha256(const wchar_t* path, std::string* output) noexcept {
    if (path == nullptr || output == nullptr) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    bool success =
        CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES,
                             CRYPT_VERIFYCONTEXT) != FALSE &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) != FALSE;
    std::array<BYTE, 64 * 1024> buffer{};
    while (success) {
        DWORD read = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()),
                      &read, nullptr)) {
            success = false;
            break;
        }
        if (read == 0) break;
        success = CryptHashData(hash, buffer.data(), read, 0) != FALSE;
    }
    std::array<BYTE, 32> digest{};
    DWORD digestSize = static_cast<DWORD>(digest.size());
    if (success)
        success = CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &digestSize,
                                    0) != FALSE &&
                  digestSize == digest.size();
    if (hash != 0) CryptDestroyHash(hash);
    if (provider != 0) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    if (!success) return false;
    char text[65]{};
    for (std::size_t index = 0; index < digest.size(); ++index)
        sprintf_s(text + index * 2, 3, "%02X", digest[index]);
    *output = text;
    return true;
}

bool WriteGuardedCodeByte(const std::uintptr_t absolute,
                          const std::uint8_t expected,
                          const std::uint8_t replacement) noexcept {
    auto* address = reinterpret_cast<std::uint8_t*>(Address(absolute));
    std::uint8_t observed = 0;
    if (!SafeRead(address, &observed) || observed != expected) return false;
    DWORD previous = 0;
    if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &previous))
        return false;
    *address = replacement;
    const bool flushed =
        FlushInstructionCache(GetCurrentProcess(), address, 1) != FALSE;
    DWORD unused = 0;
    const bool restored = VirtualProtect(address, 1, previous, &unused) != FALSE;
    if (!flushed || !restored) {
        DWORD writable = 0;
        if (VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &writable)) {
            *address = expected;
            FlushInstructionCache(GetCurrentProcess(), address, 1);
            VirtualProtect(address, 1, writable, &unused);
        }
        return false;
    }
    return true;
}

bool InstallVehicleBudgetPatch() noexcept {
    const auto budget = g_settings.totalVehicleBudget;
    if (budget == kNativeVehicleBudget) {
        Log(LogLevel::Info,
            "VEHICLE_BUDGET native=%u configured=%u patched=0 preservation=1",
            kNativeVehicleBudget, budget);
        return true;
    }
    struct Site {
        std::uintptr_t address;
        std::uint8_t expected;
        std::uint8_t replacement;
    };
    const std::array<Site, 5> sites{{
        {kVehicleBudgetCompareImmediate, kNativeVehicleBudget, budget},
        {kVehicleBudgetSubtractDisplacement,
         static_cast<std::uint8_t>(-static_cast<int>(kNativeVehicleBudget)),
         static_cast<std::uint8_t>(-static_cast<int>(budget))},
        {kVehicleEvictionReserveImmediate, kNativeVehicleBudget, budget},
        {kTrafficCapacityCompareImmediate, kNativeVehicleBudget, budget},
        {kTrafficPopulationAddImmediate, kNativeVehicleBudget, budget},
    }};
    for (const auto& site : sites) {
        std::uint8_t observed = 0;
        if (!SafeRead(reinterpret_cast<void*>(Address(site.address)), &observed) ||
            observed != site.expected) {
            Log(LogLevel::Error,
                "VEHICLE_BUDGET_REJECTED site=%08X expected=%02X observed=%02X",
                static_cast<unsigned>(site.address), site.expected, observed);
            return false;
        }
    }
    std::size_t applied = 0;
    for (; applied < sites.size(); ++applied) {
        const auto& site = sites[applied];
        if (!WriteGuardedCodeByte(site.address, site.expected,
                                  site.replacement))
            break;
    }
    if (applied != sites.size()) {
        while (applied-- > 0) {
            const auto& site = sites[applied];
            WriteGuardedCodeByte(site.address, site.replacement,
                                 site.expected);
        }
        Log(LogLevel::Error,
            "VEHICLE_BUDGET_REJECTED reason=write-failed rollback=attempted");
        return false;
    }
    Log(LogLevel::Info,
        "VEHICLE_BUDGET native=%u configured=%u patched=1 sites=%u preservation=1 initialStage=1",
        kNativeVehicleBudget, budget, static_cast<unsigned>(sites.size()));
    return true;
}

bool IsExpectedVehicle(void* vehicle) noexcept {
    void** vtable = nullptr;
    return vehicle != nullptr && SafeRead(vehicle, &vtable) && vtable != nullptr &&
           reinterpret_cast<std::uintptr_t>(vtable) == Address(kIVehicleVtable);
}

bool ReadVehicle(void* vehicle, VehicleSnapshot* output) noexcept {
    if (output == nullptr || !IsExpectedVehicle(vehicle)) return false;
    void** vtable = nullptr;
    if (!SafeRead(vehicle, &vtable)) return false;
    __try {
        const auto* position =
            reinterpret_cast<const NFSPluginSDK::MW05::UMath::Vector3* (
                __thiscall*)(void*)>(vtable[kSlotPosition])(vehicle);
        // Slot 45 returns LOCAL VELOCITY, not a world-space heading. Its
        // magnitude is speed and its sign does not encode the car's world yaw.
        // ComputeHeading (slot 46) writes a normalized world vector via out-param.
        NFSPluginSDK::MW05::UMath::Vector3 heading{};
        reinterpret_cast<void(__thiscall*)(void*,NFSPluginSDK::MW05::UMath::Vector3*)>(
            vtable[kSlotComputeHeading])(vehicle,&heading);
        const auto direction=FromUMath(heading);
        const float norm=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
        if(position==nullptr||!std::isfinite(norm)||norm<.9f||norm>1.1f)return false;
        output->pointer = vehicle;
        output->position = FromUMath(*position);
        output->heading = {direction.x/norm,direction.y/norm,direction.z/norm};
        output->speed = std::abs(reinterpret_cast<float(__thiscall*)(void*)>(
            vtable[kSlotSpeed])(vehicle));
        output->vehicleKey =
            reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(
                vtable[kSlotVehicleKey])(vehicle);
        output->driverClass =
            reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(
                vtable[kSlotDriverClass])(vehicle);
        output->offScreenTime =
            reinterpret_cast<float(__thiscall*)(void*)>(
                vtable[kSlotOffScreenTime])(vehicle);
        output->onScreenTime =
            reinterpret_cast<float(__thiscall*)(void*)>(
                vtable[kSlotOnScreenTime])(vehicle);
        return std::isfinite(output->position.x) &&
               std::isfinite(output->position.y) &&
               std::isfinite(output->position.z) &&
               std::isfinite(output->speed) &&
               std::isfinite(output->offScreenTime) &&
               std::isfinite(output->onScreenTime);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

#include "NativeVehicleList.inl"
void EnumerateVehicles(std::vector<VehicleSnapshot>& result) {
    result.clear(); // Keep the allocation reserved at installation.
    std::uint32_t count = 0;
    const void* const* data = nullptr;
    if (!ReadNativeVehicleList(data,count)) return;
    try {result.reserve(count);} catch (...) {return;}
    for (std::uint32_t index = 0; index < count; ++index) {
        const void* vehicle = nullptr;
        if (!SafeRead(data + index, &vehicle)) continue;
        VehicleSnapshot snapshot{};
        if (ReadVehicle(const_cast<void*>(vehicle), &snapshot)) result.push_back(snapshot);
    }
}

bool IsFreeRoam() noexcept {
    std::uint32_t flow = 0;
    void* rawStatus = nullptr;
    if (!SafeRead(reinterpret_cast<void*>(Address(kGameFlowState)), &flow) ||
        flow != kGameFlowRacing ||
        !SafeRead(reinterpret_cast<void*>(Address(kRaceStatus)), &rawStatus) ||
        rawStatus == nullptr)
        return false;
    __try {
        const auto* status =
            static_cast<const NFSPluginSDK::MW05::GRaceStatus*>(rawStatus);
        return status->mPlayMode ==
               NFSPluginSDK::MW05::GRaceStatus::PlayMode::Roaming;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

#include "AudioDiagnostics.inl"

float DeltaSeconds() noexcept {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    if (g_lastTick.QuadPart == 0 || g_frequency.QuadPart <= 0) {
        g_lastTick = now;
        return 1.0f / 60.0f;
    }
    const float dt = static_cast<float>(now.QuadPart - g_lastTick.QuadPart) /
                     static_cast<float>(g_frequency.QuadPart);
    g_lastTick = now;
    return std::clamp(dt, 0.0f, 0.25f);
}

void* GetSimablePointer(void* vehicle) noexcept {
    if (!IsExpectedVehicle(vehicle)) return nullptr;
    void** vehicleVtable = nullptr;
    if (!SafeRead(vehicle, &vehicleVtable) || vehicleVtable == nullptr)
        return nullptr;
    __try {
        return reinterpret_cast<void*(__thiscall*)(void*)>(
            vehicleVtable[kSlotSimable])(vehicle);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

void ProtectManagedVehicle(const std::size_t slot, void* vehicle,
                           void* simable, const std::uint32_t vehicleKey) noexcept {
    if (slot >= g_protectedSimables.size() || vehicle == nullptr ||
        simable == nullptr)
        return;
    g_protectedKeys[slot].store(vehicleKey, std::memory_order_relaxed);
    g_protectedSimables[slot].store(
        reinterpret_cast<std::uintptr_t>(simable), std::memory_order_release);
    g_protectedVehicles[slot].store(
        reinterpret_cast<std::uintptr_t>(vehicle), std::memory_order_release);
}

void UnprotectManagedVehicle(const std::size_t slot, void* vehicle,
                             void* simable) noexcept {
    if (slot >= g_protectedSimables.size()) return;
    const auto expectedVehicle = reinterpret_cast<std::uintptr_t>(vehicle);
    auto currentVehicle = expectedVehicle;
    g_protectedVehicles[slot].compare_exchange_strong(
        currentVehicle, 0, std::memory_order_acq_rel,
        std::memory_order_acquire);
    const auto expectedSimable = reinterpret_cast<std::uintptr_t>(simable);
    auto currentSimable = expectedSimable;
    g_protectedSimables[slot].compare_exchange_strong(
        currentSimable, 0, std::memory_order_acq_rel,
        std::memory_order_acquire);
}

bool IsManagedCacheVehicle(const void* vehicle) noexcept {
    if (vehicle == nullptr ||
        !g_streamRetentionEnabled.load(std::memory_order_acquire)) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(vehicle);
    for (std::size_t slot = 0; slot < g_protectedVehicles.size(); ++slot) {
        if (g_protectedVehicles[slot].load(std::memory_order_acquire) != address)
            continue;
        // Pointer equality alone can retain an unrelated, recycled Traffic car.
        void** vtable = nullptr;
        std::uint32_t key = 0;
        std::uint32_t driver = 0;
        if (!SafeRead(vehicle, &vtable) ||
            reinterpret_cast<std::uintptr_t>(vtable) != Address(kIVehicleVtable))
            return false;
        __try {
            key = reinterpret_cast<std::uint32_t(__thiscall*)(const void*)>(
                vtable[kSlotVehicleKey])(vehicle);
            driver = reinterpret_cast<std::uint32_t(__thiscall*)(const void*)>(
                vtable[kSlotDriverClass])(vehicle);
            const auto simable = reinterpret_cast<void*(__thiscall*)(const void*)>(
                vtable[kSlotSimable])(vehicle);
            return driver == kDriverRacer &&
                key == g_protectedKeys[slot].load(std::memory_order_relaxed) &&
                reinterpret_cast<std::uintptr_t>(simable) ==
                    g_protectedSimables[slot].load(std::memory_order_acquire);
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    return false;
}

bool IsExternalConstructionProtectedVehicle(const void* vehicle) noexcept {
    if (vehicle == nullptr ||
        !g_externalConstructionProtectionActive.load(std::memory_order_acquire))
        return false;
    const auto address = reinterpret_cast<std::uintptr_t>(vehicle);
    AcquireSRWLockShared(&g_constructionSetLock);
    const auto count = std::min<std::uint32_t>(
        g_externalConstructionProtectedCount.load(std::memory_order_acquire),
        static_cast<std::uint32_t>(g_externalConstructionProtectedVehicles.size()));
    bool found=false;
    for (std::uint32_t index = 0; index < count; ++index) {
        if (g_externalConstructionProtectedVehicles[index] == address){found=true;break;}
    }
    ReleaseSRWLockShared(&g_constructionSetLock);
    return found;
}

bool BeginConstructionProtection(const void* const* vehicles,
                                 const unsigned count) noexcept {
    if (count == 0 || !ReadablePointerArray(vehicles,count))
        return false;
    bool expected = false;
    if (!g_externalConstructionProtectionClaimed.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel,
            std::memory_order_acquire))
        return false;
    AcquireSRWLockExclusive(&g_constructionSetLock);
    bool prepared=true;
    try {g_externalConstructionProtectedVehicles.resize(count);} catch (...) {prepared=false;}
    for (unsigned index = 0; prepared&&index < count; ++index) {
        const void* vehicle=nullptr;
        if(!SafeRead(vehicles+index,&vehicle)||!vehicle){prepared=false;break;}
        g_externalConstructionProtectedVehicles[index]=reinterpret_cast<std::uintptr_t>(vehicle);
    }
    if(!prepared){
        g_externalConstructionProtectedCount.store(0,std::memory_order_release);
        g_externalConstructionProtectionClaimed.store(false,std::memory_order_release);
        ReleaseSRWLockExclusive(&g_constructionSetLock);
        return false;
    }
    g_externalConstructionProtectionStartVotes.store(
        g_externalConstructionProtectedVotes.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
    g_externalConstructionProtectedCount.store(count, std::memory_order_release);
    g_externalConstructionProtectionActive.store(true,
                                                  std::memory_order_release);
    ReleaseSRWLockExclusive(&g_constructionSetLock);
    return true;
}

void EndConstructionProtection(const char* scope) noexcept {
    AcquireSRWLockExclusive(&g_constructionSetLock);
    g_externalConstructionProtectionActive.store(false,
                                                  std::memory_order_release);
    const auto protectedCount =
        g_externalConstructionProtectedCount.load(std::memory_order_acquire);
    const auto votes = g_externalConstructionProtectedVotes.load(
                           std::memory_order_relaxed) -
        g_externalConstructionProtectionStartVotes.load(
            std::memory_order_relaxed);
    g_externalConstructionProtectedCount.store(0, std::memory_order_release);
    g_externalConstructionProtectionClaimed.store(false,
                                                   std::memory_order_release);
    ReleaseSRWLockExclusive(&g_constructionSetLock);
    Log(LogLevel::Info,
        "Construction protection ended scope=%s protected=%u votes=%u",
        scope != nullptr ? scope : "unknown", protectedCount, votes);
}

std::uint32_t __fastcall CacheQueryHook(void* cache, void*,
                                      const void* vehicle,
                                      const void* askingCache) noexcept {
    // Native OnQueryVehicleCache is a read-only vote, before OnRemoved and
    // any component/destructor work. Want=0 removes this candidate from the
    // eviction list at 0x6879EE. All unrelated votes retain native behavior.
    const std::uint32_t original = g_originalCacheQuery(cache, vehicle, askingCache);
    if (IsExternalConstructionProtectedVehicle(vehicle)) {
        g_externalConstructionProtectedVotes.fetch_add(1,
                                                        std::memory_order_relaxed);
        return 0; // eVehicleCacheResult::Want
    }
    if (!IsManagedCacheVehicle(vehicle)) return original;
    g_cacheWanted.fetch_add(1, std::memory_order_relaxed);
    if (original != 0) g_cacheChangedVotes.fetch_add(1, std::memory_order_relaxed);
    g_lastCacheVehicle.store(reinterpret_cast<std::uintptr_t>(vehicle),
                             std::memory_order_relaxed);
    return 0; // eVehicleCacheResult::Want
}

// Diagnostic only: native headers are captured without changing allocation,
// eviction, skin ownership or calling native resource functions. Raw words are
// deliberately not labelled as counters until their live layout is validated.
void LogRenderResources(const char* stage,const void* vehicle=nullptr) noexcept {
    if(!LoggingEnabled())return;
    std::uint32_t skinMask=0,arenaBase=0,arenaBytes=0;
    const bool maskOK=SafeRead(reinterpret_cast<void*>(Address(0x903690)),&skinMask);
    const bool arenaOK=SafeRead(reinterpret_cast<void*>(Address(0x9B2C60)),&arenaBase)&&
        SafeRead(reinterpret_cast<void*>(Address(0x9B2C64)),&arenaBytes);
    Log(LogLevel::Info,"RENDER_RESOURCE stage=%s vehicle=%p skinMaskValid=%u freeSkinMask=%08X arenaValid=%u arena=%08X arenaBytes=%u readOnly=1",
        stage,vehicle,unsigned(maskOK),skinMask,unsigned(arenaOK),arenaBase,arenaBytes);
    for(unsigned i=0;i<4;++i) {
        std::uintptr_t pool=0;std::array<std::uint32_t,16> header{};
        const bool valid=SafeRead(reinterpret_cast<void*>(Address(0x9B0A00+i*4)),&pool)&&pool&&
            SafeRead(reinterpret_cast<void*>(pool),&header);
        Log(LogLevel::Info,"RENDER_POOL stage=%s index=%u valid=%u pool=%08X raw=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X",
            stage,i,unsigned(valid),unsigned(pool),header[0],header[1],header[2],header[3],header[4],header[5],header[6],header[7],
            header[8],header[9],header[10],header[11],header[12],header[13],header[14],header[15]);
    }
}

bool KillVehicle(void* vehicle, const char* reason) noexcept {
    if (!IsExpectedVehicle(vehicle)) return false;
    void** vehicleVtable = nullptr;
    if (!SafeRead(vehicle, &vehicleVtable)) return false;
    __try {
        auto* simable = reinterpret_cast<void*(__thiscall*)(void*)>(
            vehicleVtable[kSlotSimable])(vehicle);
        void** simableVtable = nullptr;
        if (simable == nullptr || !SafeRead(simable, &simableVtable) ||
            reinterpret_cast<std::uintptr_t>(simableVtable) !=
                Address(kISimableVtable) ||
            reinterpret_cast<std::uintptr_t>(simableVtable[2]) !=
                Address(kKillSimable))
            return false;
        LogRenderResources("before-retire",vehicle);
        reinterpret_cast<void(__thiscall*)(void*)>(simableVtable[2])(simable);
        LogRenderResources("after-retire",vehicle); // Address only; never dereferenced.
        Log(LogLevel::Info, "Released managed vehicle=%08X reason=%s",
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(vehicle)),
            reason != nullptr ? reason : "unspecified");
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool IsIconRegistered(NFSPluginSDK::MW05::GManager* manager,
                      NFSPluginSDK::MW05::GIcon* icon) noexcept {
    if (manager == nullptr || icon == nullptr) return false;
    std::uint32_t count = 0;
    NFSPluginSDK::MW05::GIcon** icons = nullptr;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(manager);
    if (!SafeRead(bytes + kGManagerIconCountOffset, &count) ||
        !SafeRead(bytes + kGManagerIconTableOffset, &icons) ||
        count > kMaximumIcons || (count != 0 && icons == nullptr))
        return false;
    for (std::uint32_t index = 0; index < count; ++index) {
        NFSPluginSDK::MW05::GIcon* current = nullptr;
        if (SafeRead(icons + index, &current) && current == icon) return true;
    }
    return false;
}

void HideMarker(MarkerSlot& slot) noexcept {
    if (!slot.visible || g_markerManager == nullptr || slot.icon == nullptr ||
        !IsIconRegistered(g_markerManager, slot.icon)) {
        slot.visible = false;
        return;
    }
    __try {
        auto flags = static_cast<std::uint16_t>(slot.icon->mFlags);
        flags &= ~static_cast<std::uint16_t>(
            NFSPluginSDK::MW05::GIcon::Flags::ShowInWorld);
        flags &= ~static_cast<std::uint16_t>(
            NFSPluginSDK::MW05::GIcon::Flags::ShowOnMap);
        slot.icon->mFlags =
            static_cast<NFSPluginSDK::MW05::GIcon::Flags>(flags);
        if ((flags & static_cast<std::uint16_t>(
                         NFSPluginSDK::MW05::GIcon::Flags::Spawned)) != 0)
            reinterpret_cast<void(__thiscall*)(NFSPluginSDK::MW05::GIcon*)>(
                Address(kGIconUnspawn))(slot.icon);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        slot.icon = nullptr;
    }
    if (slot.visible)
        Log(LogLevel::Info, "RaceRival marker hidden owner=%08X",
            static_cast<unsigned int>(
                reinterpret_cast<std::uintptr_t>(slot.owner)));
    slot.visible = false;
}

void RefreshMarkerManager() noexcept {
    NFSPluginSDK::MW05::GManager* manager = nullptr;
    SafeRead(reinterpret_cast<void*>(Address(kGManagerGlobal)), &manager);
    if (manager == g_markerManager) return;
    for (auto& slot : g_markers) {
        slot.icon = nullptr;
        slot.visible = false;
    }
    g_markerManager = manager;
    g_markerRetry.Reset();
    Log(LogLevel::Info, "Gameplay marker manager=%08X",
        static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(manager)));
}

void UpdateMarker(MarkerSlot& slot, const Vec3& position,
                  const bool show) noexcept {
    RefreshMarkerManager();
    if (!show || g_markerManager == nullptr) {
        HideMarker(slot);
        return;
    }
    if (slot.icon != nullptr &&
        !IsIconRegistered(g_markerManager, slot.icon)) {
        slot.icon = nullptr;
        slot.visible = false;
    }
    const auto now = GetTickCount64();
    if (slot.icon == nullptr && !g_markerRetry.Ready(now)) return;
    __try {
        const auto mapPosition = ToMath(position);
        if (slot.icon == nullptr) {
            std::uint32_t count = 0;
            if (!SafeRead(reinterpret_cast<std::uint8_t*>(g_markerManager) +
                          kGManagerIconCountOffset, &count) || count >= kMaximumIcons) {
                if (g_markerRetry.Fail(now))
                    Log(LogLevel::Warning, "MARKER_RETRY reason=icon-table-unavailable count=%u limit=%u retryMs=%llu failures=%u nativeAllocation=0", count, kMaximumIcons,
                        g_markerRetry.nextAttempt - now, g_markerRetry.failures);
                return;
            }
            using AllocateFn = NFSPluginSDK::MW05::GIcon*(__thiscall*)(
                NFSPluginSDK::MW05::GManager*,
                NFSPluginSDK::MW05::GIcon::Type,
                const NFSPluginSDK::MW05::Math::Vector3&, float, bool);
            slot.icon = reinterpret_cast<AllocateFn>(Address(kGManagerAllocIcon))(
                g_markerManager,
                NFSPluginSDK::MW05::GIcon::Type::RaceRival, mapPosition, 0.0f,
                false);
            if (slot.icon == nullptr) {
                if (g_markerRetry.Fail(now))
                    Log(LogLevel::Warning, "MARKER_RETRY reason=allocation-failed count=%u limit=%u retryMs=%llu failures=%u", count, kMaximumIcons,
                        g_markerRetry.nextAttempt - now, g_markerRetry.failures);
                return;
            }
            if (g_markerRetry.failures)
                Log(LogLevel::Info, "MARKER_RETRY recovered=1 failures=%u", g_markerRetry.failures);
            g_markerRetry.Reset();
        }
        slot.icon->mPosition = mapPosition;
        auto flags = static_cast<std::uint16_t>(slot.icon->mFlags);
        flags &= ~static_cast<std::uint16_t>(
            NFSPluginSDK::MW05::GIcon::Flags::ShowInWorld);
        flags |= static_cast<std::uint16_t>(
            NFSPluginSDK::MW05::GIcon::Flags::ShowOnMap);
        slot.icon->mFlags =
            static_cast<NFSPluginSDK::MW05::GIcon::Flags>(flags);
        if ((flags & static_cast<std::uint16_t>(
                         NFSPluginSDK::MW05::GIcon::Flags::Spawned)) == 0)
            reinterpret_cast<void(__thiscall*)(NFSPluginSDK::MW05::GIcon*)>(
                Address(kGIconSpawn))(slot.icon);
        reinterpret_cast<void(__thiscall*)(NFSPluginSDK::MW05::GIcon*)>(
            Address(kGIconSetPosition))(slot.icon);
        if (!slot.visible)
            Log(LogLevel::Info, "RaceRival marker shown owner=%08X",
                static_cast<unsigned int>(
                    reinterpret_cast<std::uintptr_t>(slot.owner)));
        slot.visible = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (g_markerRetry.Fail(now))
            Log(LogLevel::Warning, "MARKER_RETRY reason=update-fault code=%08X retryMs=%llu",
                static_cast<unsigned int>(GetExceptionCode()), g_markerRetry.nextAttempt - now);
        slot.icon = nullptr;
        slot.visible = false;
    }
}

std::size_t AcquireMarkerSlot(void* owner) noexcept {
    for (std::size_t index = 0; index < g_markers.size(); ++index) {
        if (g_markers[index].owner == owner) return index;
    }
    for (std::size_t index = 0; index < g_markers.size(); ++index) {
        if (g_markers[index].owner == nullptr) {
            g_markers[index].owner = owner;
            return index;
        }
    }
    return g_markers.size();
}

void ReleaseMarkerSlot(const std::size_t index) noexcept {
    if (index >= g_markers.size()) return;
    HideMarker(g_markers[index]);
    g_markers[index].owner = nullptr;
}

#include "VehicleVariation.inl"
#include "VehiclePerformance.inl"
#include "SpawnIdentity.inl"

bool GetTrafficRoad(void* vehicle,
                    NFSPluginSDK::MW05::WRoadNav** road) noexcept {
    if (road == nullptr || !IsExpectedVehicle(vehicle)) return false;
    *road = nullptr;
    void** vehicleVtable = nullptr;
    if (!SafeRead(vehicle, &vehicleVtable)) return false;
    __try {
        auto* ai = reinterpret_cast<NFSPluginSDK::MW05::IVehicleAI* (
            __thiscall*)(void*)>(vehicleVtable[kSlotAIVehicle])(vehicle);
        void** aiVtable = nullptr;
        if (ai == nullptr || !SafeRead(ai, &aiVtable) ||
            reinterpret_cast<std::uintptr_t>(aiVtable) !=
                Address(kTrafficIVehicleAIVtable) ||
            reinterpret_cast<std::uintptr_t>(
                aiVtable[kSlotAIGetCurrentRoad]) != Address(kAIGetCurrentRoad))
            return false;
        *road = reinterpret_cast<NFSPluginSDK::MW05::WRoadNav* (
            __thiscall*)(NFSPluginSDK::MW05::IVehicleAI*)>(
            aiVtable[kSlotAIGetCurrentRoad])(ai);
        return *road != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

#include "AnchorRoadSeed.inl"

NFSPluginSDK::MW05::PVehicle* ConstructTrafficSeed(
    void* rawStatus, const std::uint32_t vehicleKey,
    const NFSPluginSDK::MW05::UMath::Vector3& direction,
    const NFSPluginSDK::MW05::UMath::Vector3& position,
    NFSPluginSDK::MW05::FECustomizationRecord* customization) noexcept {
    __try {
        auto* status = static_cast<NFSPluginSDK::MW05::GRaceStatus*>(rawStatus);
        auto* cache = static_cast<NFSPluginSDK::MW05::IVehicleCache*>(status);
        void** cacheVtable = nullptr;
        if (!SafeRead(cache, &cacheVtable) ||
            reinterpret_cast<std::uintptr_t>(cacheVtable) !=
                Address(kRaceStatusCacheVtable)) {
            Log(LogLevel::Error, "CACHE_OWNER_REJECTED unexpected GRaceStatus cache interface");
            return nullptr;
        }
        NFSPluginSDK::MW05::VehicleParams params(
            NFSPluginSDK::MW05::DriverClass::Traffic, vehicleKey, direction,
            position, customization,
            NFSPluginSDK::MW05::eVehicleParamFlags::SnapToGround |
                NFSPluginSDK::MW05::eVehicleParamFlags::CalcPerformance,
            cache, nullptr);
        return NFSPluginSDK::MW05::PVehicle::Construct(params);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool ActivateNativeRacer(void* vehicle,
                         const AnchorRoadSeed& anchorSeed) noexcept {
    if (!IsExpectedVehicle(vehicle) || !ValidateAnchorRoadNetwork(anchorSeed)) return false;
    void** vehicleVtable = nullptr;
    if (!SafeRead(vehicle, &vehicleVtable)) return false;
    const char* stage = "activate";
    const auto fail = [&stage, vehicle]() noexcept {
        std::uint32_t driverClass = std::numeric_limits<std::uint32_t>::max();
        void* ai = nullptr;
        void** aiVtable = nullptr;
        void** primaryVtable = nullptr;
        void* goal = nullptr;
        void** goalVtable = nullptr;
        void* action = nullptr;
        void** actionVtable = nullptr;
        void* driveNav = nullptr;
        std::uint32_t navType = 0;
        std::uint32_t pathType = 0;
        std::uint32_t driveFlags = 0;
        SafeRead(static_cast<std::uint8_t*>(vehicle) + 0x94, &driverClass);
        SafeRead(static_cast<std::uint8_t*>(vehicle) + 0x54, &ai);
        if (ai != nullptr) {
            SafeRead(ai, &aiVtable);
            const std::uintptr_t aiAddress =
                reinterpret_cast<std::uintptr_t>(ai);
            if (aiAddress >= kRacecarPrimaryFromAIOffset) {
                const std::uintptr_t primary =
                    aiAddress - kRacecarPrimaryFromAIOffset;
                SafeRead(reinterpret_cast<void*>(primary), &primaryVtable);
                SafeRead(reinterpret_cast<void*>(primary + kRacecarGoalOffset),
                         &goal);
            }
            SafeRead(static_cast<std::uint8_t*>(ai) + kAIDriveNavOffset,
                     &driveNav);
            SafeRead(static_cast<std::uint8_t*>(ai) + kAIDriveFlagsOffset,
                     &driveFlags);
        }
        if (goal != nullptr) {
            SafeRead(goal, &goalVtable);
            SafeRead(static_cast<std::uint8_t*>(goal) + kGoalActionOffset,
                     &action);
        }
        if (action != nullptr) SafeRead(action, &actionVtable);
        if (driveNav != nullptr) {
            SafeRead(static_cast<std::uint8_t*>(driveNav) + kRoadNavTypeOffset,
                     &navType);
            SafeRead(static_cast<std::uint8_t*>(driveNav) +
                         kRoadNavPathTypeOffset,
                     &pathType);
        }
        Log(LogLevel::Error,
            "ACTIVATION_REJECTED stage=%s vehicle=%08X driver=%u ai=%08X aiVtable=%08X primaryVtable=%08X goal=%08X goalVtable=%08X action=%08X actionVtable=%08X driveNav=%08X navType=%u pathType=%u driveFlags=%08X spawnRetryLatched=1",
            stage,
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(vehicle)),
            driverClass,
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(ai)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(aiVtable)),
            static_cast<unsigned int>(
                reinterpret_cast<std::uintptr_t>(primaryVtable)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(goal)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(goalVtable)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(action)),
            static_cast<unsigned int>(
                reinterpret_cast<std::uintptr_t>(actionVtable)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(driveNav)),
            navType, pathType, driveFlags);
        return false;
    };
    __try {
        using GetAIFn = NFSPluginSDK::MW05::IVehicleAI*(__thiscall*)(void*);
        LogAudioVehicle(vehicle, "constructed-traffic", "mod");
        reinterpret_cast<void(__thiscall*)(void*)>(
            vehicleVtable[kSlotActivateVehicle])(vehicle);
        LogAudioVehicle(vehicle, "activated-traffic", "mod");

        stage = "traffic seed validation";
        auto* ai = reinterpret_cast<GetAIFn>(
            vehicleVtable[kSlotAIVehicle])(vehicle);
        void** aiVtable = nullptr;
        if (ai == nullptr || !SafeRead(ai, &aiVtable) ||
            reinterpret_cast<std::uintptr_t>(aiVtable) !=
                Address(kTrafficIVehicleAIVtable))
            return fail();

        stage = "SetDriverClass Racer";
        reinterpret_cast<void(__thiscall*)(void*, std::uint32_t)>(
            vehicleVtable[kSlotSetDriverClass])(vehicle, kDriverRacer);
        LogAudioVehicle(vehicle, "converted-racer", "mod");
        if (reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(
                vehicleVtable[kSlotDriverClass])(vehicle) != kDriverRacer)
            return fail();

        stage = "racecar AI validation";
        ai = reinterpret_cast<GetAIFn>(vehicleVtable[kSlotAIVehicle])(vehicle);
        aiVtable = nullptr;
        if (ai == nullptr || !SafeRead(ai, &aiVtable) ||
            reinterpret_cast<std::uintptr_t>(aiVtable) !=
                Address(kRacecarIVehicleAIVtable) ||
            reinterpret_cast<std::uintptr_t>(
                aiVtable[kSlotAIResetVehicleToRoadNav]) !=
                Address(kAIResetVehicleToRoadNav) ||
            reinterpret_cast<std::uintptr_t>(aiVtable[kSlotAISetSpawned]) !=
                Address(kAISetSpawned))
            return fail();

        stage = "copy owned anchor road seed";
        if(!ValidateAnchorRoadNetwork(anchorSeed)) return fail();
        reinterpret_cast<void(__thiscall*)(NFSPluginSDK::MW05::IVehicleAI*,
                                           NFSPluginSDK::MW05::WRoadNav*)>(
            aiVtable[kSlotAIResetVehicleToRoadNav])(ai,
                reinterpret_cast<NFSPluginSDK::MW05::WRoadNav*>(const_cast<unsigned char*>(anchorSeed.bytes.data())));
        reinterpret_cast<void(__thiscall*)(NFSPluginSDK::MW05::IVehicleAI*)>(
            aiVtable[kSlotAISetSpawned])(ai);

        // The alpha.2 trace proved that free-roam SetDriverClass creates a valid
        // AIVehicleRacecar but leaves mCurrentGoal null.  Install the same native
        // AIGoalRacer that the Racecar constructor requests, now that the owner,
        // road navigation and spawned state are all ready.
        stage = "native Racecar primary validation";
        const std::uintptr_t aiAddress =
            reinterpret_cast<std::uintptr_t>(ai);
        if (aiAddress < kRacecarPrimaryFromAIOffset) return fail();
        const std::uintptr_t primaryAddress =
            aiAddress - kRacecarPrimaryFromAIOffset;
        void** primaryVtable = nullptr;
        NFSPluginSDK::MW05::AIGoal* goal = nullptr;
        if (!SafeRead(reinterpret_cast<void*>(primaryAddress), &primaryVtable) ||
            reinterpret_cast<std::uintptr_t>(primaryVtable) !=
                Address(kRacecarPrimaryVtable))
            return fail();

        stage = "install native AIGoalRacer";
        if (!SafeRead(reinterpret_cast<void*>(primaryAddress +
                                             kRacecarGoalOffset),
                      &goal))
            return fail();
        std::uint32_t goalCreationCalls = 0;
        if (goal == nullptr) {
            const std::uint32_t goalHash = kAIGoalRacerHash;
            reinterpret_cast<void(__thiscall*)(void*, const std::uint32_t&)>(
                Address(kAIVehicleSetGoal))(
                reinterpret_cast<void*>(primaryAddress), goalHash);
            goalCreationCalls = 1;
            if (!SafeRead(reinterpret_cast<void*>(primaryAddress +
                                                 kRacecarGoalOffset),
                          &goal) ||
                goal == nullptr)
                return fail();
        }

        stage = "validate native AIGoalRacer";
        void** goalVtable = nullptr;
        if (!SafeRead(goal, &goalVtable) ||
            reinterpret_cast<std::uintptr_t>(goalVtable) !=
                Address(kAIGoalRacerVtable) ||
            reinterpret_cast<std::uintptr_t>(
                goalVtable[kGoalChooseActionSlot]) !=
                Address(kGoalChooseAction))
            return fail();

        void* actionBefore = nullptr;
        SafeRead(reinterpret_cast<std::uint8_t*>(goal) + kGoalActionOffset,
                 &actionBefore);
        std::uint32_t actionSelectionCalls = 0;
        if (actionBefore == nullptr) {
            stage = "native goal Action selection";
            reinterpret_cast<void(__thiscall*)(NFSPluginSDK::MW05::AIGoal*,
                                                float)>(
                goalVtable[kGoalChooseActionSlot])(goal, 0.0f);
            actionSelectionCalls = 1;
        }

        void* action = nullptr;
        void** actionVtable = nullptr;
        void* driveNav = nullptr;
        std::uint32_t navType = 0;
        std::uint32_t pathType = 0;
        std::uint32_t driveFlags = 0;
        std::uint32_t goalName = 0;
        std::uint8_t navValid = 0;
        if (!SafeRead(reinterpret_cast<std::uint8_t*>(goal) +
                          kGoalActionOffset,
                      &action) ||
            action == nullptr || !SafeRead(action, &actionVtable) ||
            reinterpret_cast<std::uintptr_t>(actionVtable) !=
                Address(kAIActionRaceVtable) ||
            !SafeRead(reinterpret_cast<std::uint8_t*>(ai) +
                          kAIDriveNavOffset,
                      &driveNav) ||
            driveNav == nullptr ||
            !SafeRead(reinterpret_cast<std::uint8_t*>(driveNav) +
                          kRoadNavTypeOffset,
                      &navType) ||
            !SafeRead(reinterpret_cast<std::uint8_t*>(driveNav) +
                          kRoadNavPathTypeOffset,
                      &pathType) ||
            !SafeRead(reinterpret_cast<std::uint8_t*>(driveNav) +
                          kRoadNavValidOffset,
                      &navValid) ||
            !SafeRead(reinterpret_cast<std::uint8_t*>(ai) +
                          kAIDriveFlagsOffset,
                      &driveFlags) ||
            !SafeRead(reinterpret_cast<void*>(primaryAddress +
                                             kRacecarGoalNameOffset),
                      &goalName)) {
            Log(LogLevel::Error,
                "Native Action selection rejected vehicle=%08X goal=%08X actionBefore=%08X actionAfter=%08X actionVtable=%08X",
                static_cast<unsigned int>(
                    reinterpret_cast<std::uintptr_t>(vehicle)),
                static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(goal)),
                static_cast<unsigned int>(
                    reinterpret_cast<std::uintptr_t>(actionBefore)),
                static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(action)),
                static_cast<unsigned int>(
                    reinterpret_cast<std::uintptr_t>(actionVtable)));
            return fail();
        }
        if (navType != kNavDirection || pathType != kPathRacer) {
            Log(LogLevel::Error,
                "Native Action selected but navigation differs vehicle=%08X navType=%u pathType=%u flags=%08X",
                static_cast<unsigned int>(
                    reinterpret_cast<std::uintptr_t>(vehicle)),
                navType, pathType, driveFlags);
            return fail();
        }
        Log(LogLevel::Info,
            "NATIVE_ACTION_READY vehicle=%08X ai=%08X goalName=%08X goal=%08X action=%08X roadNav=%08X navValid=%u navType=Direction pathType=Racer driveFlags=%08X goalCreationCalls=%u actionSelectionCalls=%u externalPathWrites=0 externalDriveWrites=0",
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(vehicle)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(ai)),
            goalName,
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(goal)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(action)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(driveNav)),
            navValid, driveFlags, goalCreationCalls, actionSelectionCalls);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log(LogLevel::Error,
            "Native racer activation exception stage=%s code=%08X vehicle=%08X",
            stage, static_cast<unsigned int>(GetExceptionCode()),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(vehicle)));
        return false;
    }
}

#include "PopulationBridge.inl"
#include "ConstructionLiveSet.inl"
#include "SpawnPreparation.inl"
RefillPacing g_refillPacing;
ULONGLONG g_refillDiagnosticAt=0;
const VehicleSnapshot* FindLive(const std::vector<VehicleSnapshot>&,const ManagedRacer&) noexcept;
mwappearance::RoamingPopulation AppearancePopulation() {
    return mwappearance::CountRoaming(g_racers,[](const ManagedRacer& racer){
        return FindLive(g_vehicleScratch,racer)!=nullptr;
    });
}

bool SpawnAtAnchor(const VehicleSnapshot& anchor,const VehicleSnapshot& player,bool* candidateWait) {
    *candidateWait=false;
    const auto pendingTransaction=g_populationTransaction;
    const bool awaitingVacancy=pendingTransaction.Awaiting(GetTickCount64());
    g_populationTransaction.Reset();
    PopulationAttemptScope populationScope;
    NFSPluginSDK::MW05::WRoadNav* anchorRoad = nullptr;
    if (!GetTrafficRoad(anchor.pointer, &anchorRoad)) return false;
    void* anchorSimable=GetSimablePointer(anchor.pointer);
    AnchorRoadSeed anchorSeed;
    if(!AnchorGenerationMatches(anchor,anchorSimable)||!CaptureAnchorRoadSeed(anchorRoad,anchorSeed)) {
        Log(LogLevel::Info,"ANCHOR_SEED skipped reason=invalid-road-or-generation anchor=%p nativeCopyCalls=0",anchor.pointer);
        return false;
    }
    if (!g_appearanceAvailable) return false;
    const auto prepared=AdvanceSpawnPreparation(player.vehicleKey,GetTickCount64(),
        [](CareerPerformancePlan* plan){return SelectPreparedSpawnVehicle(plan,ReadCareerPerformancePlan);},
        [](const RankedVehicle& car){
            g_preparedCustomization.emplace();
            const auto live=AppearancePopulation();
            return MakeAppearance(car,&*g_preparedCustomization,
                mwappearance::RoamingExterior(live.live,live.customized),&g_spawnPreparation.customExterior);
        });
    if(prepared!=SpawnPreparationResult::Ready) {
        *candidateWait=prepared!=SpawnPreparationResult::Failed;return false;
    }
    if(!g_preparedCustomization) {ResetSpawnPreparation();*candidateWait=true;return false;}
    const std::uint32_t vehicleKey=g_spawnPreparation.vehicleKey;
    const char* preset=g_spawnPreparation.model.c_str();
    auto& customization=*g_preparedCustomization;
    // Career state is not cached admission. Re-read this selected model just
    // before allocation (no player/global customization writes).
    const auto finalStart=PerformanceNow();
    CareerPerformancePlan performancePlan{};
    const auto eligibility=ReadCareerPerformancePlan(vehicleKey,&performancePlan);
    if(eligibility==PerformanceEligibility::Ready) {
        WriteCareerPerformancePlan(vehicleKey,performancePlan,&customization);
        std::uint32_t playerJunkman=0;
        if(ReadVehicleJunkman(player.pointer,&playerJunkman)) {
            const auto aiJunkman=junkman::Plan(playerJunkman,0,[&](unsigned type) {
                return (performancePlan.junkmanEligible&(1u<<type))!=0;
            });
            customization.mInstalledPhysics.mJunkman=static_cast<NFSPluginSDK::MW05::JunkmanParts>(aiJunkman);
            Log(LogLevel::Info,"PERFORMANCE_JUNKMAN_SPAWN player_parts=%u ai_parts=%u private_record=1",
                junkman::Count(playerJunkman),junkman::Count(aiJunkman));
        }
    }
    RecordPerformance(PerfStage::SpawnFinalize,finalStart,PerformanceNow());
    if(eligibility!=PerformanceEligibility::Ready) {
        ResetSpawnPreparation();*candidateWait=true;return false;
    }
    void* rawStatus = nullptr;
    if (!SafeRead(reinterpret_cast<void*>(Address(kRaceStatus)), &rawStatus) ||
        rawStatus == nullptr)
        return false;

    nfsmw_population::Status population;
    // Preparation has succeeded. Reserve one immediately needed slot only.
    const auto access=PopulationCapacity(population,1);
    if(DeferForNativeHeadroom(access,population)) {*candidateWait=true;return false;}
    if(access==PopulationAccess::Full||access==PopulationAccess::Unsafe||
       (access==PopulationAccess::Legacy&&g_vehicleScratch.size()>=kObservedRacerConstructionLiveCeiling)) {
        *candidateWait=true;
        Log(LogLevel::Info,"SPAWN_DEFERRED reason=population-capacity access=%u live=%u limit=%u free=%u reserved=%u",
            static_cast<unsigned>(access),population.live,population.limit,population.freeSlots,population.reserved);
        if(access==PopulationAccess::Full&&population.live==population.limit) {
            if(awaitingVacancy) {
                // Do not retire a second car while native removal is pending.
                g_populationTransaction=pendingTransaction;
                g_spawnTimer=g_settings.spawnIntervalSeconds;
            } else if(ReclaimOneTraffic(player,anchor)) {
                g_populationTransaction.Begin(GetTickCount64());
                g_spawnTimer=g_settings.spawnIntervalSeconds;
            }
        }
        return false;
    }
    const auto nativeDirection = ToUMath(anchor.heading);
    const auto nativePosition = ToUMath(anchor.position);
    auto& protectedVehicles=g_constructionScratch;
    unsigned protectedCount=0;
    if(!CaptureConstructionLiveSet(protectedVehicles,protectedCount)||
       !ConstructionContains(protectedVehicles,protectedCount,anchor.pointer)||
       !AnchorGenerationMatches(anchor,anchorSimable)||
       (access==PopulationAccess::Legacy&&protectedCount>=kObservedRacerConstructionLiveCeiling)||
       (access==PopulationAccess::Ready&&protectedCount!=population.live)) {
        *candidateWait=true;
        Log(LogLevel::Info,"SPAWN_DEFERRED reason=construction-live-set-changed fresh=%u frame=%u provider=%u construction=0",
            protectedCount,unsigned(g_vehicleScratch.size()),population.live);return false;
    }
    if(protectedCount!=g_vehicleScratch.size())
        Log(LogLevel::Info,"CONSTRUCTION_LIVE_SET frame=%u fresh=%u source=native-after-retirement staleFrameIgnored=1",
            unsigned(g_vehicleScratch.size()),protectedCount);
    const void* pendingVehicle=nullptr;
    const auto lifecycle=ValidateConstructionLifecycles(protectedVehicles,protectedCount,&pendingVehicle);
    if(lifecycle!=ConstructionLifecycle::Ready){
        *candidateWait=true;
        Log(LogLevel::Info,"SPAWN_DEFERRED reason=%s vehicle=%p fresh=%u construction=0 reservation=release battleUpdate=continue",
            lifecycle==ConstructionLifecycle::RetirementPending?"native-retirement-pending":"invalid-native-lifecycle",
            pendingVehicle,protectedCount);
        return false;
    }
    // Preparation can outlive several population samples. Re-evaluate from
    // current native identities before allocating; never rewrite an existing car.
    const auto liveAppearance=mwappearance::CountRoaming(g_racers,[&](const ManagedRacer& racer){
        return FindLive(g_vehicleScratch,racer) && ConstructionContains(protectedVehicles,protectedCount,racer.pointer);
    });
    if(g_spawnPreparation.customExterior && !mwappearance::RoamingExterior(liveAppearance.live,liveAppearance.customized)) {
        const auto* car=FindPreparedSpawnVehicle();
        if(!car || !StockExterior(*car,&customization)) {ResetSpawnPreparation();*candidateWait=true;return false;}
        g_spawnPreparation.customExterior=false;
    }
    if (protectedCount == 0 ||
        !BeginConstructionProtection(protectedVehicles.data(),
                                     static_cast<unsigned>(protectedCount))) {
        Log(LogLevel::Error,
            "SPAWN_REJECTED stage=construction-protection-begin protected=%u",
            static_cast<unsigned>(protectedCount));
        return false;
    }
    LogRenderResources("before-construct",player.pointer);
    const auto constructStart = PerformanceNow();
    auto* created = ConstructTrafficSeed(rawStatus, vehicleKey, nativeDirection,
                                         nativePosition, &customization);
    RecordPerformance(PerfStage::Construct, constructStart, PerformanceNow());
    EndConstructionProtection("native-racer");
    // Target 0x68934D allocates 0x198 bytes; 0x689361 copies all 0x66 DWORDs.
    // PVehicle owns its copy. No pointer to the prepared record survives Construct.
    LogRenderResources("after-construct",player.pointer);
    if (created == nullptr) return false;
    auto* vehicle = static_cast<NFSPluginSDK::MW05::IVehicle*>(created);
    if (ConstructionContains(protectedVehicles,protectedCount,vehicle)) {
        Log(LogLevel::Error,
            "NATIVE_REPLACEMENT_GUARD_TRIPPED vehicle=%08X anchor=%08X protected=%u action=fault-without-release",
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(vehicle)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(anchor.pointer)),
            static_cast<unsigned>(protectedCount));
        g_faulted = true;
        return false;
    }
    if (!AnchorGenerationMatches(anchor, anchorSimable)) {
        KillVehicle(vehicle, "anchor generation changed after protected construction");
        return false;
    }
    const auto activateStart = PerformanceNow();
    const bool activated = IsExpectedVehicle(vehicle) &&
        VerifySpawnPerformance(created, customization, vehicleKey) &&
        ActivateNativeRacer(vehicle, anchorSeed);
    RecordPerformance(PerfStage::Activate, activateStart, PerformanceNow());
    if (!activated) {
        KillVehicle(vehicle, "native action activation failed");
        return false;
    }
    SpawnIdentity identity{};
    if (!CaptureSpawnIdentity(vehicle, &identity)) {
        KillVehicle(vehicle, "spawn identity unavailable");
        return false;
    }
    // A PVehicle storage address can be recycled.  Remove stale generations
    // before the new key becomes authoritative.
    for (std::size_t index = g_racers.size(); index-- > 0;) {
        if (g_racers[index].pointer != vehicle) continue;
        UnprotectManagedVehicle(g_racers[index].markerSlot,
                                g_racers[index].pointer,
                                g_racers[index].simable);
        ReleaseMarkerSlot(g_racers[index].markerSlot);
        g_racers.erase(g_racers.begin() + index);
    }
    const std::size_t markerSlot = AcquireMarkerSlot(vehicle);
    if (markerSlot >= g_markers.size()) {
        KillVehicle(vehicle, "marker slot unavailable");
        return false;
    }
    ManagedRacer racer{};
    racer.pointer = vehicle;
    racer.simable = identity.simable;
    if (racer.simable == nullptr) {
        ReleaseMarkerSlot(markerSlot);
        KillVehicle(vehicle, "managed simable unavailable");
        return false;
    }
    racer.vehicleKey = identity.key;
    racer.markerSlot = markerSlot;
    racer.preset = preset;
    racer.customExterior = g_spawnPreparation.customExterior;
    g_racers.push_back(std::move(racer));
    Log(LogLevel::Info,"APPEARANCE_DENSITY liveBefore=%u customBefore=%u newCustom=%u threshold=5 naturalReplacement=1 paintPreserved=1",
        liveAppearance.live,liveAppearance.customized,unsigned(g_racers.back().customExterior));
    ProtectManagedVehicle(markerSlot, g_racers.back().pointer,
                          g_racers.back().simable, g_racers.back().vehicleKey);
    Log(LogLevel::Info,
        "SPAWNED_NATIVE_RACER preset=%s modelKey=%08X key=%08X vehicle=%08X anchor=%08X anchorSpeed=%.1fkmh population=%u/%u replacement=0 protectedExisting=%u",
        preset, vehicleKey, identity.key,
        static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(vehicle)),
        static_cast<unsigned int>(
            reinterpret_cast<std::uintptr_t>(anchor.pointer)),
        anchor.speed * 3.6f, static_cast<unsigned int>(g_racers.size()),
        static_cast<unsigned int>(g_settings.maximumRacers),
        static_cast<unsigned>(protectedCount));
    return true;
}

void RemoveRacer(const std::size_t index, const char* reason,
                 const bool kill) noexcept {
    if (index >= g_racers.size()) return;
    const ManagedRacer racer = g_racers[index];
    UnprotectManagedVehicle(racer.markerSlot, racer.pointer, racer.simable);
    ReleaseMarkerSlot(racer.markerSlot);
    if (kill) KillVehicle(racer.pointer, reason);
    Log(LogLevel::Info, "Managed racer removed vehicle=%08X reason=%s",
        static_cast<unsigned int>(
            reinterpret_cast<std::uintptr_t>(racer.pointer)),
        reason != nullptr ? reason : "unspecified");
    g_racers.erase(g_racers.begin() + index);
}

const VehicleSnapshot* FindLive(
    const std::vector<VehicleSnapshot>& vehicles,
    const ManagedRacer& racer) noexcept {
    const auto found = std::find_if(
        vehicles.begin(), vehicles.end(), [&racer](const VehicleSnapshot& item) {
            return item.pointer == racer.pointer &&
                   item.vehicleKey == racer.vehicleKey &&
                   item.driverClass == kDriverRacer;
        });
    return found == vehicles.end() ? nullptr : &*found;
}

bool EncounterMarkerAllowed(const ManagedRacer& racer) noexcept;

void RefreshManaged(const VehicleSnapshot& player,
                    const std::vector<VehicleSnapshot>& vehicles,
                    const float dt) {
    for (std::size_t index = 0; index < g_racers.size();) {
        auto& racer = g_racers[index];
        racer.ageSeconds += dt;
        const auto* live = FindLive(vehicles, racer);
        if (live == nullptr) {
            HideMarker(g_markers[racer.markerSlot]);
            racer.missingSeconds += dt;
            if (racer.missingSeconds >= g_settings.missingGraceSeconds) {
                const auto pointerMatch = std::find_if(
                    vehicles.begin(), vehicles.end(),
                    [&racer](const VehicleSnapshot& item) {
                        return item.pointer == racer.pointer;
                    });
                const bool pointerPresent = pointerMatch != vehicles.end();
                Log(LogLevel::Info,
                    "MISSING_FINAL vehicle=%08X expectedKey=%08X pointerPresent=%u actualKey=%08X actualDriver=%u lastDistance=%.1fm",
                    static_cast<unsigned int>(
                        reinterpret_cast<std::uintptr_t>(racer.pointer)),
                    racer.vehicleKey, pointerPresent ? 1u : 0u,
                    pointerPresent ? pointerMatch->vehicleKey : 0u,
                    pointerPresent ? pointerMatch->driverClass
                                   : std::numeric_limits<std::uint32_t>::max(),
                    racer.lastDistanceMeters);
                RemoveRacer(index, "vehicle streamed out", false);
                continue;
            }
            ++index;
            continue;
        }
        racer.missingSeconds = 0.0f;
        const float distance = Distance(player.position, live->position);
        racer.lastDistanceMeters = distance;
        if (distance > g_settings.populationRadiusMeters) {
            const bool onScreen = live->onScreenTime > 0.0f &&
                                  live->offScreenTime <= 0.05f;
            const bool hardLimit =
                distance > g_settings.populationRadiusMeters +
                               g_settings.hardRetirementMarginMeters;
            if (hardLimit ||
                (!onScreen && live->offScreenTime >=
                                  g_settings.retirementOffScreenSeconds)) {
                RemoveRacer(index,
                            hardLimit ? "outside hard retirement radius"
                                      : "outside configured radius and off-screen",
                            true);
                continue;
            }
            if (!racer.retirementDeferred) {
                racer.retirementDeferred = true;
                Log(LogLevel::Info,
                    "RETIRE_DEFERRED vehicle=%08X distance=%.1fm onScreen=%.2fs offScreen=%.2fs",
                    static_cast<unsigned int>(
                        reinterpret_cast<std::uintptr_t>(racer.pointer)),
                    distance, live->onScreenTime, live->offScreenTime);
            }
        } else {
            racer.retirementDeferred = false;
        }
        UpdateMarker(g_markers[racer.markerSlot], live->position,
                     distance <= g_settings.markerRadiusMeters && EncounterMarkerAllowed(racer));
        ++index;
    }
}

void Replenish(const VehicleSnapshot& player,
               const std::vector<VehicleSnapshot>& vehicles, const float dt) {
    g_refillPacing.Tick(dt,g_racers.size(),g_settings.maximumRacers);
    AdvanceVehicleCatalog();
    if (!EnsureVehiclePalette(player)) {ResetSpawnPreparation();ReleasePopulationReservation();return;}
    const auto now=GetTickCount64();
    if(g_racers.size()>=g_settings.maximumRacers) {ResetSpawnPreparation();ReleasePopulationReservation();return;}
    if(g_populationTransaction.Expired(now)) {
        ResetSpawnPreparation();
        ReleasePopulationReservation();g_spawnTimer=0.0f;
        Log(LogLevel::Info,"POPULATION_RESERVATION_END reason=vacancy-timeout trafficAdmissionRestored=1");return;
    }
    if (g_spawnFailureBackoffSeconds > 0.0f) {
        ResetSpawnPreparation();
        ReleasePopulationReservation();
        g_spawnFailureBackoffSeconds =
            std::max(0.0f, g_spawnFailureBackoffSeconds - dt);
        return;
    }
    g_spawnTimer += dt;
    const bool pendingVacancy=g_populationTransaction.Awaiting(now);
    if(!pendingVacancy&&!g_spawnPreparation.continuation&&(g_spawnTimer<std::min(1.0f,g_settings.spawnIntervalSeconds)||
       !g_refillPacing.Ready(g_settings.spawnIntervalSeconds,g_racers.size(),g_settings.maximumRacers)))return;
    // Never prolong a one-slot lease or break the three-sample preparation chain.
    if(DeferAdaptiveSpawn(pendingVacancy||g_spawnPreparation.phase!=SpawnPreparationPhase::Empty,now)){
        ReleasePopulationReservation();g_spawnTimer=0.0f;return;
    }
    // Fresh capability/capacity only when a spawn search is due, or while the
    // bounded one-slot transaction is pending. Never reuse it for construction:
    // SpawnAtAnchor must still reserve/check again immediately before allocation.
    nfsmw_population::Status population;
    const auto access=PopulationCapacity(population,g_populationTransaction.Requested(now));
    if (access==PopulationAccess::Unsafe||
        (access==PopulationAccess::Legacy&&vehicles.size() >= kObservedRacerConstructionLiveCeiling)) {
        ResetSpawnPreparation();
        ReleasePopulationReservation();
        g_spawnTimer = 0.0f;
        g_spawnFailureBackoffSeconds = 5.0f;
        Log(LogLevel::Info,
            "SPAWN_DEFERRED reason=unverified-population-capacity live=%u fallbackCeiling=%u action=no-construction access=%u providerLimit=%u providerLive=%u free=%u reserved=%u capabilities=%X",
            static_cast<unsigned int>(vehicles.size()),
            static_cast<unsigned int>(kObservedRacerConstructionLiveCeiling),unsigned(access),
            population.limit,population.live,population.freeSlots,population.reserved,population.capabilities);
        return;
    }
    g_spawnTimer = 0.0f;
    if(DeferForNativeHeadroom(access,population)) {
        ResetSpawnPreparation();ReleasePopulationReservation();return;
    }

    const VehicleSnapshot* selected = nullptr;
    std::size_t candidateCount = 0;
    int bestRank=99;
    unsigned moving=0,inRange=0,roadRejected=0;
    g_anchorSearch.Clear();
    for (std::size_t index=0;index<vehicles.size();++index) {
        const auto& vehicle=vehicles[index];
        if (vehicle.driverClass != kDriverTraffic ||
            vehicle.speed < g_settings.anchorMinimumSpeedMps ||
            IsAdvancedCivilianVehicle(vehicle.pointer))
            continue;
        ++moving;
        const float distance = Distance(player.position, vehicle.position);
        if(distance>=g_settings.anchorMinimumMeters&&distance<=g_settings.anchorMaximumMeters)++inRange;
        const Vec3 offset{vehicle.position.x-player.position.x,0.0f,vehicle.position.z-player.position.z};
        const int rank=RefillAnchorRank(distance,HorizontalHeadingDot(player.heading,offset),
            HorizontalHeadingDot(player.heading,vehicle.heading),g_settings.anchorMinimumMeters,
            g_settings.anchorMaximumMeters,g_settings.anchorHeadingDotMinimum,g_refillPacing.shortage,
            vehicle.onScreenTime<=0.0f&&vehicle.offScreenTime>=1.0f);
        g_anchorSearch.Add(index,rank);
    }
    const auto anchorResult=g_anchorSearch.Select(g_random,[&](std::size_t index) {
        const auto& vehicle=vehicles[index];
        NFSPluginSDK::MW05::WRoadNav* road=nullptr;AnchorRoadSeed seed;
        return GetTrafficRoad(vehicle.pointer,&road)&&CaptureAnchorRoadSeed(road,seed);
    });
    roadRejected=static_cast<unsigned>(anchorResult.rejected);
    if(anchorResult.index!=AnchorSearch::none) {
        selected=&vehicles[anchorResult.index];bestRank=anchorResult.rank;
        candidateCount=anchorResult.candidates; // Coarse tier, not all road-validated.
    }
    if (selected == nullptr) {
        ResetSpawnPreparation();
        ReleasePopulationReservation();
        if(now>=g_refillDiagnosticAt) {
            g_refillDiagnosticAt=now+5000;
            Log(LogLevel::Info,"SPAWN_DEFERRED reason=no-eligible-moving-anchor live=%u population=%u/%u moving=%u inRange=%u roadRejected=%u shortageSeconds=%.1f reserved=0 trafficAdmissionUnrestricted=1 searchHz=1",
                static_cast<unsigned>(vehicles.size()),static_cast<unsigned>(g_racers.size()),static_cast<unsigned>(g_settings.maximumRacers),moving,inRange,roadRejected,g_refillPacing.shortage);
        }
        return;
    }
    if(g_populationTransaction.Awaiting(now)&&access==PopulationAccess::Full) {
        // Poll for the ONE requested native retirement; no new preparation,
        // reservation deadline renewal or second reclaim during this wait.
        g_spawnTimer=g_settings.spawnIntervalSeconds;return;
    }
    Log(LogLevel::Info,
        "SPAWN_ATTEMPT_BEGIN candidateCount=%u population=%u/%u failurePolicy=retry-with-bounded-backoff consecutiveFailures=%u",
        static_cast<unsigned int>(candidateCount),
        static_cast<unsigned int>(g_racers.size()),
        static_cast<unsigned int>(g_settings.maximumRacers),
        g_consecutiveSpawnFailures);
    bool candidateWait=false;
    if(now>=g_refillDiagnosticAt) {
        g_refillDiagnosticAt=now+5000;
        Log(LogLevel::Info,"REFILL_POLICY rank=%d interval=%.1f population=%u/%u anchorDistance=%.1f shortageSeconds=%.1f coarseCandidates=%u roadProbes=%u roadRejected=%u",
            bestRank,RefillPacing::Interval(g_settings.spawnIntervalSeconds,g_racers.size(),g_settings.maximumRacers),
            unsigned(g_racers.size()),unsigned(g_settings.maximumRacers),Distance(player.position,selected->position),g_refillPacing.shortage,
            unsigned(candidateCount),unsigned(anchorResult.probes),roadRejected);
    }
    if (!SpawnAtAnchor(*selected,player,&candidateWait)) {
        if(candidateWait) {
            // No allocation attempted: this is not an engine spawn failure.
            g_consecutiveSpawnFailures=0;g_spawnFailureBackoffSeconds=0.0f;return;
        }
        ResetSpawnPreparation();
        ++g_consecutiveSpawnFailures;
        const std::uint32_t shift =
            std::min<std::uint32_t>(g_consecutiveSpawnFailures, 4);
        g_spawnFailureBackoffSeconds = std::min(
            60.0f, 5.0f * static_cast<float>(1u << shift));
        Log(LogLevel::Error,
            "Population spawn failed; retry deferred %.1fs consecutiveFailures=%u permanentLatch=0",
            g_spawnFailureBackoffSeconds, g_consecutiveSpawnFailures);
    } else {
        ResetSpawnPreparation();
        g_refillPacing.Spawned();
        g_consecutiveSpawnFailures = 0;
        g_spawnFailureBackoffSeconds = 0.0f;
    }
}

// Keep the native SEH probe in a separate function from the RAII batch scope.
void LogTelemetryRows(const VehicleSnapshot& player,
                      const std::vector<VehicleSnapshot>& vehicles) {
    std::size_t visibleMarkers = 0;
    LogAudioPoolHealth();
    LogRenderResources("periodic-player",player.pointer);
    for (const auto& marker : g_markers)
        if (marker.visible) ++visibleMarkers;
    Log(LogLevel::Info, "TELEMETRY managed=%u/%u markers=%u vehicles=%u populationIdleCalls=%llu populationReserveCalls=%llu populationReleaseCalls=%llu populationReleaseSkipped=%llu",
        static_cast<unsigned int>(g_racers.size()),
        static_cast<unsigned int>(g_settings.maximumRacers),
        static_cast<unsigned int>(visibleMarkers),
        static_cast<unsigned int>(vehicles.size()),
        g_populationApiCounters.idle,g_populationApiCounters.reserve,
        g_populationApiCounters.release,g_populationApiCounters.releaseSkipped);
    const auto wanted = g_cacheWanted.exchange(0, std::memory_order_acq_rel);
    const auto changed = g_cacheChangedVotes.exchange(0, std::memory_order_acq_rel);
    if (wanted != 0) {
        Log(LogLevel::Info,
            "CACHE_RETENTION wanted=%u changedNativeVotes=%u lastVehicle=%08X policy=Want-before-eviction lifecycleSuppression=0",
            wanted, changed,
            static_cast<unsigned int>(
                g_lastCacheVehicle.load(std::memory_order_relaxed)));
    }
    for (const auto& racer : g_racers) {
        const auto* live = FindLive(vehicles, racer);
        if (live == nullptr) continue;
        void* ai = nullptr;
        void* goal = nullptr;
        void* action = nullptr;
        void* driveNav = nullptr;
        std::uint32_t navType = 0;
        std::uint32_t pathType = 0;
        std::uint32_t driveFlags = 0;
        std::uint8_t navValid = 0;
        void** vehicleVtable = nullptr;
        if (SafeRead(live->pointer, &vehicleVtable) && vehicleVtable != nullptr) {
            __try {
                ai = reinterpret_cast<void*(__thiscall*)(void*)>(
                    vehicleVtable[kSlotAIVehicle])(live->pointer);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                ai = nullptr;
            }
        }
        if (ai != nullptr) {
            const std::uintptr_t primary =
                reinterpret_cast<std::uintptr_t>(ai) -
                kRacecarPrimaryFromAIOffset;
            SafeRead(reinterpret_cast<void*>(primary + kRacecarGoalOffset),
                     &goal);
            SafeRead(reinterpret_cast<std::uint8_t*>(ai) + kAIDriveNavOffset,
                     &driveNav);
            SafeRead(reinterpret_cast<std::uint8_t*>(ai) + kAIDriveFlagsOffset,
                     &driveFlags);
        }
        if (goal != nullptr)
            SafeRead(reinterpret_cast<std::uint8_t*>(goal) + kGoalActionOffset,
                     &action);
        if (driveNav != nullptr) {
            SafeRead(reinterpret_cast<std::uint8_t*>(driveNav) +
                         kRoadNavValidOffset,
                     &navValid);
            SafeRead(reinterpret_cast<std::uint8_t*>(driveNav) +
                         kRoadNavTypeOffset,
                     &navType);
            SafeRead(reinterpret_cast<std::uint8_t*>(driveNav) +
                         kRoadNavPathTypeOffset,
                     &pathType);
        }
        Log(LogLevel::Info,
            "RACER_STATE vehicle=%08X preset=%s age=%.1fs distance=%.1fm speed=%.1fkmh onScreen=%.2fs offScreen=%.2fs ai=%08X goal=%08X action=%08X nav=%08X navValid=%u navType=%u pathType=%u driveFlags=%08X",
            static_cast<unsigned int>(
                reinterpret_cast<std::uintptr_t>(live->pointer)),
            racer.preset.c_str(), racer.ageSeconds,
            Distance(player.position, live->position), live->speed * 3.6f,
            live->onScreenTime, live->offScreenTime,
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(ai)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(goal)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(action)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(driveNav)),
            navValid, navType, pathType, driveFlags);
    }
}

#include "TelemetryDispatch.inl"

template<typename T> bool AudioRead(const void* address,T* output) noexcept {
    return AudioRead(reinterpret_cast<std::uintptr_t>(address),0,output);
}
#include "EncounterVoice.inl"
#include "EncounterGauge.inl"
bool EncounterBattleBusy() noexcept;
bool QueueEncounterBattle(const VehicleSnapshot&,const VehicleSnapshot&) noexcept;
#include "EncounterSignal.inl"
#include "EncounterBattle.inl"
#include "BackgroundPolice.inl"
#include "EncounterWeaponHits.inl"
#include "EncounterArmedRacers.inl"

void ClearWorld(const bool killManaged) noexcept {
    ResetSpawnPreparation();
    g_refillPacing={};g_refillDiagnosticAt=0;
    ReleasePopulationReservation();
    g_pendingEncounterMessage={};ResetEncounterArmedRacers();
    ResetSpawnEligibility();
    g_performanceRetry={};
    ResetVehicleVarietyExpansion();
    ResetAdaptiveSpawnPacing();
    ResetBackgroundPolice();
    // No cached vehicle dereference after the world has gone away.
    EndEncounterBattle(false,true);
    ResetEncounterWager();
    ResetEncounterSignal();
    g_streamRetentionEnabled.store(false, std::memory_order_release);
    for (std::size_t index = g_racers.size(); index-- > 0;)
        RemoveRacer(index, "free-roam world ended", killManaged);
    for (auto& marker : g_markers) marker = {};
    g_markerManager = nullptr;
    g_markerRetry.Reset();
    g_palette.clear();
    g_vehicleBag.clear();
    g_palettePlayerKey = 0;
    g_spawnTimer = 0.0f;
    g_telemetryTimer = 0.0f;
    g_managementPending = 0.0f;
    ResetAudioPoolHealth();
    g_vehicleScratch.clear();
    g_anchorSearch.Clear();
    g_consecutiveSpawnFailures = 0;
    g_spawnFailureBackoffSeconds = 0.0f;
    g_cacheWanted.store(0, std::memory_order_relaxed);
    g_cacheChangedVotes.store(0, std::memory_order_relaxed);
    g_lastCacheVehicle.store(0, std::memory_order_relaxed);
    for (auto& address : g_protectedVehicles)
        address.store(0, std::memory_order_relaxed);
    for (auto& address : g_protectedSimables)
        address.store(0, std::memory_order_relaxed);
}

void Update() {
    if(g_eventSelectionOnly){if(!g_faulted){CaptureFrontendCatalog();AdvanceVehicleCatalog();}return;}
    UpdateEncounterVoiceVolume();
    if (g_faulted) return;
    CaptureFrontendCatalog(); // Must run before the free-roam-only guard.
    const float elapsed = DeltaSeconds();
    if (!IsFreeRoam()) {
        // Direct menu -> event also needs the calibrated catalog. Each call
        // advances at most one collection; never scan inside a spawn hook.
        AdvanceVehicleCatalog();
        if (g_worldReady) {
            ClearWorld(true);
            Log(LogLevel::Info, "Free-roam world ended and pilot racer released");
        }
        g_worldReady = false;
        return;
    }
    if (!g_worldReady) {
        LockAudioExpansionForWorld();
        ClearWorld(false);
        g_worldReady = true;
        g_streamRetentionEnabled.store(true, std::memory_order_release);
        g_spawnTimer = g_settings.spawnIntervalSeconds;
        g_managementPending = kManagementIntervalSeconds;
        Log(LogLevel::Info,
            "Free-roam pilot armed maximum=%u radius=%.1fm markerRadius=%.1fm anchorRange=%.1f-%.1fm sameDirectionDot>=%.2f",
            static_cast<unsigned int>(g_settings.maximumRacers),
            g_settings.populationRadiusMeters, g_settings.markerRadiusMeters,
            g_settings.anchorMinimumMeters, g_settings.anchorMaximumMeters,
            g_settings.anchorHeadingDotMinimum);
    }
    float dt = 0.0f;
    if (!ConsumeManagementTick(elapsed, &g_managementPending, &dt)) return;
    auto phase = PerformanceNow();
    EnumerateVehicles(g_vehicleScratch);
    PerformanceCheckpoint(PerfStage::Enumerate, &phase);
    const auto& vehicles = g_vehicleScratch;
    const auto player = std::find_if(
        vehicles.begin(), vehicles.end(), [](const VehicleSnapshot& item) {
            return item.driverClass == kDriverHuman;
        });
    if (player == vehicles.end()) {
        EncounterMissingSample(dt);
        ResetEncounterSignal();
        return;
    }
    UpdateEncounterBattle(*player, vehicles, dt);
    FlushEncounterMessage();
    PerformanceCheckpoint(PerfStage::Battle, &phase);
    RefreshManaged(*player, vehicles, dt);
    PerformanceCheckpoint(PerfStage::Refresh, &phase);
    Replenish(*player, vehicles, dt);
    PerformanceCheckpoint(PerfStage::Replenish, &phase);
    UpdateEncounterCandidate(*player, vehicles, dt);
    PerformanceCheckpoint(PerfStage::Candidate, &phase);
    UpdateBackgroundPolice(*player, vehicles, dt);
    UpdateEncounterArmedRacers(*player,vehicles,dt);
    PerformanceCheckpoint(PerfStage::Police, &phase);
    LogTelemetry(*player, vehicles, dt);
    PerformanceCheckpoint(PerfStage::Telemetry, &phase);
}

#include "FrameTimingDiagnostics.inl"

int __cdecl DisplayFrameHook() {
    const auto start = FrameClockNow();
    const int result = g_originalDisplayFrame();
    const auto nativeEnd = FrameClockNow();
    SafeAudioDiagnosticTick();
    const auto audioEnd = FrameClockNow();
    __try {
        Update();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_faulted = true;
        Log(LogLevel::Error,
            "Frame update exception code=%08X; runtime latched disabled",
            static_cast<unsigned int>(GetExceptionCode()));
    }
    const auto updateEnd = FrameClockNow();
    // Event ASI owns its runtime lifecycle. This existing display-frame
    // owner provides a late-loaded callback without another native detour.
    using EventFrame = void(__cdecl*)();
    static EventFrame eventFrame = nullptr;
    if (!eventFrame) {
        const auto events = GetModuleHandleW(L"MWEventLoader.asi");
        if (events) eventFrame = reinterpret_cast<EventFrame>(GetProcAddress(events,"MWEL_FrameV1"));
    }
    if (eventFrame) {
        __try { eventFrame(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { eventFrame = nullptr; }
    }
    for (auto& slot : g_frameObservers) {
        if (const auto observer = slot.load(std::memory_order_acquire)) {
            __try { observer(); }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                auto expected = observer;
                slot.compare_exchange_strong(expected, nullptr);
                Log(LogLevel::Error, "Frame observer faulted code=%08X; observer detached",
                    static_cast<unsigned>(GetExceptionCode()));
            }
        }
    }
    if (const auto externalTick =
            g_externalPopulationTick.load(std::memory_order_acquire)) {
        __try {
            externalTick();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_externalPopulationTick.store(nullptr, std::memory_order_release);
            Log(LogLevel::Error,
                "External population tick faulted code=%08X; callback detached",
                static_cast<unsigned int>(GetExceptionCode()));
        }
    }
    ObserveFrameTiming(start, nativeEnd, audioEnd, updateEnd, FrameClockNow(), g_worldReady);
    return result;
}

#include "SolidLifetime.inl"

bool ValidateSurface(std::string* error) noexcept {
    if (g_base != kPreferredBase) {
        if (error != nullptr) *error = "relocated image base unsupported";
        return false;
    }
    const auto guard = [error](const std::uintptr_t address,
                               const std::uint8_t* expected,
                               const std::size_t size,
                               const char* name) noexcept {
        if (std::memcmp(reinterpret_cast<void*>(Address(address)), expected,
                        size) == 0)
            return true;
        if (error != nullptr) *error = std::string(name) + " byte guard failed";
        return false;
    };
    static constexpr std::array<std::uint8_t, 16> displayBytes = {
        0xA1, 0x50, 0x58, 0x92, 0x00, 0x85, 0xC0, 0x7E,
        0x07, 0x48, 0xA3, 0x50, 0x58, 0x92, 0x00, 0xC3};
    static constexpr std::array<std::uint8_t, 12> setDriverBytes = {
        0x8B, 0x44, 0x24, 0x04, 0x39, 0x81, 0x94, 0x00,
        0x00, 0x00, 0x74, 0x1C};
    static constexpr std::array<std::uint8_t, 8> chooseActionBytes = {
        0x83, 0xEC, 0x08, 0x53, 0x55, 0x57, 0x8B, 0xF9};
    static constexpr std::array<std::uint8_t, 8> setGoalBytes = {
        0x6A, 0xFF, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00};
    static constexpr std::array<std::uint8_t, 8> killSimableBytes = {
        0x51, 0x56, 0x8B, 0xF1, 0x57, 0x8D, 0x46, 0xD4};
    static constexpr std::array<std::uint8_t, 4> retirementFlagBytes = {
        0xC6, 0x46, 0x1C, 0x01};
    static constexpr std::array<std::uint8_t, 8> deactivateVehicleBytes = {
        0x8B, 0x01, 0x6A, 0x00, 0xFF, 0x50, 0x34, 0xC3};
    static constexpr std::array<std::uint8_t, 10> cacheQueryBytes = {
        0x55, 0x8B, 0xE9, 0x83, 0xBD, 0x60, 0x19, 0x00, 0x00, 0x01};
    if (!guard(kMainDisplayFrame, displayBytes.data(), displayBytes.size(),
               "MainDisplayFrame") ||
        !guard(kSetDriverClass, setDriverBytes.data(), setDriverBytes.size(),
               "SetDriverClass") ||
        !guard(kAIVehicleSetGoal, setGoalBytes.data(), setGoalBytes.size(),
               "AIVehicle::SetGoal") ||
        !guard(kKillSimable, killSimableBytes.data(), killSimableBytes.size(),
               "KillSimable") ||
        !guard(kKillSimable+0x0E, retirementFlagBytes.data(), retirementFlagBytes.size(),
               "KillSimable retirement flag") ||
        !guard(kDeactivateVehicle, deactivateVehicleBytes.data(),
               deactivateVehicleBytes.size(), "PVehicle::Deactivate") ||
        !guard(kRaceStatusCacheQuery, cacheQueryBytes.data(),
               cacheQueryBytes.size(), "GRaceStatus::OnQueryVehicleCache") ||
        !guard(kGoalChooseAction, chooseActionBytes.data(),
               chooseActionBytes.size(), "AIGoal::ChooseAction"))
        return false;

    auto** vehicleVtable = reinterpret_cast<void**>(Address(kIVehicleVtable));
    auto** goalVtable = reinterpret_cast<void**>(Address(kAIGoalRacerVtable));
    auto** cacheVtable = reinterpret_cast<void**>(Address(kRaceStatusCacheVtable));
    if (reinterpret_cast<std::uintptr_t>(cacheVtable[2]) !=
            Address(kRaceStatusCacheQuery) ||
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotSimable]) !=
            Address(kGetSimable) ||
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotPosition]) !=
            Address(kGetPosition) ||
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotVehicleKey]) !=
            Address(kGetVehicleKey) ||
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotSetDriverClass]) !=
            Address(kSetDriverClass) ||
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotDriverClass]) !=
            Address(kGetDriverClass) ||
        reinterpret_cast<std::uintptr_t>(
            vehicleVtable[kSlotOffScreenTime]) != Address(kGetOffScreenTime) ||
        reinterpret_cast<std::uintptr_t>(
            vehicleVtable[kSlotOnScreenTime]) != Address(kGetOnScreenTime) ||
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotActivateVehicle]) !=
            Address(kActivateVehicle) ||
        reinterpret_cast<std::uintptr_t>(
            vehicleVtable[kSlotDeactivateVehicle]) !=
            Address(kDeactivateVehicle) ||
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotSpeed]) !=
            Address(kGetSpeed) ||
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotAIVehicle]) !=
            Address(kGetAIVehicle) ||
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotComputeHeading]) !=
            Address(kComputeHeading) ||
        reinterpret_cast<std::uintptr_t>(
            goalVtable[kGoalChooseActionSlot]) != Address(kGoalChooseAction)) {
        if (error != nullptr) *error = "vtable guard failed";
        return false;
    }
    return true;
}

}  // namespace

extern "C" __declspec(dllexport) int __cdecl
NFRR_RegisterPopulationTickV1(void(__cdecl* tick)()) noexcept {
    g_externalPopulationTick.store(tick, std::memory_order_release);
    Log(LogLevel::Info, "External population tick registered active=%u",
        tick != nullptr ? 1u : 0u);
    return 1;
}

extern "C" __declspec(dllexport) int __cdecl
NFRR_RegisterFrameObserverV1(void(__cdecl* tick)()) noexcept {
    if (!tick || !g_frameObserverReady.load(std::memory_order_acquire)) return 0;
    for (auto& slot : g_frameObservers)
        if (slot.load(std::memory_order_acquire) == tick) return 1;
    for (auto& slot : g_frameObservers) {
        ExternalPopulationTickFn empty = nullptr;
        if (slot.compare_exchange_strong(empty, tick)) return 1;
    }
    return 0;
}

extern "C" __declspec(dllexport) int __cdecl
NFRR_BeginVehicleConstructionProtectionV1(
    const void* const* vehicles, const unsigned count) noexcept {
    return BeginConstructionProtection(vehicles, count) ? 1 : 0;
}

extern "C" __declspec(dllexport) void __cdecl
NFRR_EndVehicleConstructionProtectionV1() noexcept {
    EndConstructionProtection("external");
}

bool StartupNoticeAllowed() noexcept {
    unsigned flow=0;
    // Never interrupt a race/free roam (flow 6), or invent UI on an unreadable host.
    return SafeRead(reinterpret_cast<void*>(Address(kGameFlowState)),&flow)&&flow<6;
}

#include "../../integration/World2MWPortableHost.h"
bool VerifyHostExecutable() noexcept {
    wchar_t executablePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) == 0) {
        Log(LogLevel::Error, "Could not resolve host executable");
        return false;
    }
    const wchar_t* fileName = wcsrchr(executablePath, L'\\');
    fileName = fileName == nullptr ? executablePath : fileName + 1;
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    // The private W2MW installation uses an alias for the identical verified
    // NFSPatcher executable. Keep the production name and full byte checks.
    const bool worldAlias = _wcsicmp(fileName, L"World2MW.exe") == 0 &&
        (wcsstr(executablePath, L"\\World2MW\\sandbox\\Game\\World2MW.exe") != nullptr || World2MWPortableHost());
    if ((_wcsicmp(fileName, kExpectedExecutableName) != 0 && !worldAlias) ||
        !GetFileAttributesExW(executablePath, GetFileExInfoStandard,
                              &attributes)) {
        Log(LogLevel::Error, "Unsupported host name; fail-closed");
        return false;
    }
    const std::uint64_t size =
        (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) |
        attributes.nFileSizeLow;
    std::string hash;
    if (!nfrr_target::KnownSize(size) ||
        !ComputeSha256(executablePath, &hash) ||
        !nfrr_target::Find(size,hash)) {
        Log(LogLevel::Error,
            "Unsupported executable; fail-closed size=%llu sha256=%s",
            static_cast<unsigned long long>(size),
            hash.empty() ? "unavailable" : hash.c_str());
        return false;
    }
    const auto* target=nfrr_target::Find(size,hash);
    g_verifiedExecutableSha256=target->sha256;
    g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    Log(LogLevel::Info,
        "Supported executable accepted target=%s size=%llu sha256=%s",
        target->id,static_cast<unsigned long long>(size), hash.c_str());
    return true;
}

bool InstallRuntime() noexcept {
    LoadSettings();
    if (!g_settings.enabled && !g_eventSelectionOnly) {
        Log(LogLevel::Info,
            "Population disabled by configuration; no hook installed");
        return true;
    }
    if(g_eventSelectionOnly){
        const unsigned char guard[]={0xA1,0x50,0x58,0x92,0,0x85,0xC0,0x7E,7,0x48,0xA3,0x50,0x58,0x92,0,0xC3};
        auto target=reinterpret_cast<void*>(Address(kMainDisplayFrame));
        if(g_base!=kPreferredBase||memcmp(target,guard,sizeof(guard)))return false;
        g_catalog.reserve(1024);g_palette.reserve(5);g_vehicleBag.reserve(5);
        auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
        if(MH_CreateHook(target,&DisplayFrameHook,reinterpret_cast<void**>(&g_originalDisplayFrame))!=MH_OK)return false;
        if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);return false;}
        g_frameObserverReady.store(true,std::memory_order_release);
        // The Web route provider also needs direct guidance when installed
        // without the free-roam population feature. Keep cache/audio untouched.
        g_battleSurface=ValidateEncounterBattleSurface();
        if(g_battleSurface){InstallEncounterPathHook(true);InstallCustomDriveHook();InstallEncounterPowerBoost(true);}
        Log(LogLevel::Info,"MW_EDITOR_EVENT_SELECTION_ACTIVE population=0 native_cache_and_audio_preserved=1 map_route_drive=%u",unsigned(g_customDriveInstalled&&g_encounterPathInstalled));return true;
    }
    std::string error;
    if (!ValidateSurface(&error)) {
        Log(LogLevel::Error, "Exact target surface rejected: %s",
            error.c_str());
        return false;
    }
    if(!ValidateAnchorCopySurface()) {
        Log(LogLevel::Error,"Anchor road copy ABI guard rejected; no population hook installed");
        return false;
    }
    if (!VerifyVariationAssets()) {
        Log(LogLevel::Error, "Variation asset or Unlimiter guard rejected; no population hook installed");
        return false;
    }
    if(!ValidateSolidLifetimeSurface()) {
        Log(LogLevel::Error,"SOLID_LIFETIME byte guard rejected; no population hook installed");
        return false;
    }
    if (!InstallVehicleBudgetPatch()) {
        Log(LogLevel::Error,
            "Vehicle budget extension failed; no population hook installed");
        return false;
    }
    LARGE_INTEGER seed{};
    try { PrepareCatalogPersistence(); }
    catch(...) { Log(LogLevel::Warning,"CALIBRATION_CACHE_DISABLED initialization=failed"); }
    g_audioDiagnosticsEnabled = g_settings.deepAudioDiagnostics && ValidateAudioDiagnosticSurface();
    g_audioDetailEnabled = g_audioDiagnosticsEnabled && ValidateAudioDetailSurface();
    Log(LogLevel::Info, "AUDIO_DETAIL enabled=%u writes=0 calls=0 registryLimit=128",
        static_cast<unsigned>(g_audioDetailEnabled));
    Log(LogLevel::Info, "DIAGNOSTIC_POLICY deepAudio=%u frameTiming=%u timingIntervalSeconds=30 restartRequired=1 audioPlaybackUnaffected=1 lbRequired=0",
        static_cast<unsigned>(g_settings.deepAudioDiagnostics), static_cast<unsigned>(g_settings.frameTimingDiagnostics));
    Log(LogLevel::Info, "AUDIO_DIAGNOSTICS enabled=%u exactAudioSurface=%u writes=0 calls=0 intervalMs=5000 maxRows=32 populationUnaffectedOnMismatch=1",
        static_cast<unsigned>(g_audioDiagnosticsEnabled),
        static_cast<unsigned>(g_audioDiagnosticsEnabled));
    g_racers.reserve(kMaximumRacersLimit);
    g_vehicleScratch.reserve(kInitialVehicleReserve);
    g_anchorSearch.Reserve(kInitialVehicleReserve);
    g_audioVehicles.reserve(kInitialVehicleReserve);
    g_catalog.reserve(1024);
    g_palette.reserve(5);
    g_vehicleBag.reserve(5);
    QueryPerformanceCounter(&seed);
    g_random.seed(static_cast<std::uint32_t>(
        seed.LowPart ^ seed.HighPart ^ GetTickCount()));
    QueryPerformanceFrequency(&g_frequency);
    g_lastTick = {};
    g_battleSurface = ValidateEncounterBattleSurface();
    InitializeBackgroundPolice();
    if (g_settings.encounterSignalEnabled && g_battleSurface) {
        try { InitializeEncounterVoice(); }
        catch (...) { Log(LogLevel::Warning,"ENCOUNTER_VOICE initialization failed; battle remains silent"); }
    }
    if (MH_Initialize() != MH_OK) return false;
    if (MH_CreateHook(reinterpret_cast<void*>(Address(kMainDisplayFrame)),
                      &DisplayFrameHook,
                      reinterpret_cast<void**>(&g_originalDisplayFrame)) !=
        MH_OK) {
        MH_Uninitialize();
        return false;
    }
    if (MH_CreateHook(reinterpret_cast<void*>(Address(kRaceStatusCacheQuery)),
                      &CacheQueryHook,
                      reinterpret_cast<void**>(&g_originalCacheQuery)) !=
        MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(Address(kMainDisplayFrame)));
        MH_Uninitialize();
        return false;
    }
    if(MH_CreateHook(reinterpret_cast<void*>(Address(kSolidUnload)),&SolidUnloadHook,
                     reinterpret_cast<void**>(&g_originalSolidUnload))!=MH_OK) {
        MH_Uninitialize();return false;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(reinterpret_cast<void*>(Address(kRaceStatusCacheQuery)));
        MH_RemoveHook(reinterpret_cast<void*>(Address(kMainDisplayFrame)));
        MH_Uninitialize();
        return false;
    }
    Log(LogLevel::Info,"SOLID_LIFETIME ACTIVE nativeUnload=00507460 exactPointerAliases=1 modelBlacklist=0 lbRequired=0");
    g_frameObserverReady.store(true,std::memory_order_release);
    // Optional observation hook is enabled separately: failure must not stop population.
    if (CreateAudioFlowHook()) {
        g_audioFlowEnabled = MH_EnableHook(reinterpret_cast<void*>(Address(kAudioFlowUpdate))) == MH_OK;
        if (!g_audioFlowEnabled) MH_RemoveHook(reinterpret_cast<void*>(Address(kAudioFlowUpdate)));
    }
    Log(LogLevel::Info, "AUDIO_FLOW_DIAGNOSTICS enabled=%u hook=00694700 capacity=64 captureIntervalMs=250 gameStateWrites=0 extraNativeCalls=0 originalForwardedOnce=1",
        static_cast<unsigned>(g_audioFlowEnabled));
    InstallAudioSourceHooks();
    InstallAudioLifeHooks();
    InstallAudioPoolHook();
    InstallEncounterRaceSkillHook();
    InitializeEncounterGuide();
    InitializeEncounterCatchupProbe();
    InstallEncounterPowerBoost();
    InstallEncounterMinimapHook();
    // Event guidance and NeoAI road ownership are independent of battles.
    InstallEncounterPathHook(true);
    InstallCustomDriveHook();
    InstallCustomSpeedHook();
    InstallEncounterSignalHooks();
    Log(LogLevel::Info,
        "Runtime installed mode=dual-target max=%u totalVehicleBudget=%u racerConstructionLiveCeiling=%u populationRadius=%.1fm markerRadius=%.1fm managementMaxHz=20 nativeAITickUnthrottled=1 failurePolicy=retry-with-bounded-backoff cacheWantRetention=1 constructionProtection=all-live replacement=0 managedKillRetention=0 managedDeactivateRetention=0 destructorRetention=0 battleRouteAdapterStableHz=1 customManagementHz=4 customDriveHook=%u customSpeedHook=%u passTransitionImmediate=1 directSteeringWrites=0 borrowedAnchorRoadAcrossAllocation=0 nativeStartTimeoutMs=1000 limitBreakerRequired=0 populationApi=1 idleReserved=0 constructionReserved=1 vacancyTimeoutMs=1500 nativeStartOwnedRoadReseed=1 anchorSearchHz=1 deficitCadence=adaptive frontAnchorPriority=1 constructionLiveSet=native-after-retirement nativeRetirementBarrier=1 providerLimitPolicy=capabilities-and-consistency dynamicVehicleStorage=1",
        static_cast<unsigned int>(g_settings.maximumRacers),
        static_cast<unsigned>(g_settings.totalVehicleBudget),
        static_cast<unsigned>(kObservedRacerConstructionLiveCeiling),
        g_settings.populationRadiusMeters, g_settings.markerRadiusMeters,unsigned(g_customDriveInstalled),unsigned(g_customSpeedInstalled));
    return true;
}

void StopEncounterVoiceForProcessExit() noexcept {
    // DllMain must never join or wait for an audio thread/device teardown.
    if(g_voice) {g_voice->stop=true;SetEvent(g_voice->wake);}
}

// Event adapter shares only validated selection values. It creates no cars,
// does not modify the roaming shuffle bag, and holds no world references.
std::uint32_t ReadEventCareerPlayerKey() noexcept {
    __try {
        auto* database=NFSPluginSDK::MW05::cFrontEndDatabase::Get();
        if(!database || !database->bProfileLoaded){Log(LogLevel::Info,"EVENT_PLAYER_LOOKUP database=%p loaded=%u",database,unsigned(database&&database->bProfileLoaded));return 0;}
        auto* profile=database->GetUserProfile();
        if(!profile){Log(LogLevel::Info,"EVENT_PLAYER_LOOKUP profile=missing");return 0;}
        auto* record=profile->mPlayersCarStable.GetCarRecordByHandle(profile->mTheCareerSettings.CurrentCar);
        Log(LogLevel::Info,"EVENT_PLAYER_LOOKUP profile=%p handle=%08X record=%p key=%08X",profile,profile->mTheCareerSettings.CurrentCar,record,record?record->mVehicleKey:0);
        return record ? record->mVehicleKey : 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) {Log(LogLevel::Info,"EVENT_PLAYER_LOOKUP exception=%08X",GetExceptionCode());return 0;}
}

std::uint32_t SelectEventOpponent() {
    if(g_faulted||!g_catalogFinished||g_catalog.empty())return 0;
    std::vector<VehicleSnapshot> vehicles;EnumerateVehicles(vehicles);
    auto human=std::find_if(vehicles.begin(),vehicles.end(),[](const auto& v){return v.driverClass==kDriverHuman;});
    g_eventPlayerKey=human!=vehicles.end()?human->vehicleKey:ReadEventCareerPlayerKey();
    if(!g_eventPlayerKey){Log(LogLevel::Info,"EVENT_OPPONENT_DEFER reason=player-key-not-ready human=%u",unsigned(human!=vehicles.end()));return 0;}
    // Human cars can use preset/derived pvehicle collections. The roaming
    // palette resolves their model before looking up the ranked stock row.
    char playerModel[32]{};
    if(!InspectVehicleAttributes(g_eventPlayerKey,playerModel,nullptr))return 0;
    auto player=std::find_if(g_catalog.begin(),g_catalog.end(),[&](const auto& c){return c.installed&&_stricmp(c.installed->model,playerModel)==0;});
    if(player==g_catalog.end()){Log(LogLevel::Info,"EVENT_OPPONENT_DEFER reason=player-not-in-catalog key=%08X",g_eventPlayerKey);return 0;}
    const auto center=std::size_t(player-g_catalog.begin());
    const auto width=std::min(EffectiveVehicleVariety(),g_catalog.size());
    const auto start=WindowStart(g_catalog.size(),center,width);
    static std::uint32_t lastPlayer=0;static std::vector<std::size_t> bag;
    if(lastPlayer!=g_eventPlayerKey){bag.clear();lastPlayer=g_eventPlayerKey;}
    std::vector<std::size_t> eligible;
    for(std::size_t i=start;i<start+width;++i){CareerPerformancePlan plan{};if(ReadCareerPerformancePlan(g_catalog[i].installed->key,&plan)==PerformanceEligibility::Ready)eligible.push_back(i);}
    bag.erase(std::remove_if(bag.begin(),bag.end(),[&](auto i){return std::find(eligible.begin(),eligible.end(),i)==eligible.end();}),bag.end());
    if(bag.empty()){bag=eligible;std::shuffle(bag.begin(),bag.end(),g_random);}
    if(bag.empty())return 0;
    auto chosen=bag.back();bag.pop_back();
    Log(LogLevel::Info,"EVENT_OPPONENT_SELECTED player=%s model=%s key=%08X window=%u-%u careerChecked=1 price50Performance50=1 modelBudget=%u",
        player->installed->model,g_catalog[chosen].installed->model,g_catalog[chosen].installed->key,unsigned(start+1),unsigned(start+width),unsigned(width));
    return g_catalog[chosen].installed->key;
}
}  // namespace native_freeroam

#pragma comment(linker,"/EXPORT:NFR_PlayerSpikeHitV1=_NFR_PlayerSpikeHitV1")
extern "C" __declspec(dllexport) void __cdecl NFR_PlayerSpikeHitV1(std::uintptr_t vehicle,std::uintptr_t simable) noexcept {
    native_freeroam::EncounterSpikeHit(vehicle,simable);
}
#pragma comment(linker,"/EXPORT:NFR_PlayerSpikeHitV2=_NFR_PlayerSpikeHitV2")
extern "C" __declspec(dllexport) void __cdecl NFR_PlayerSpikeHitV2(std::uintptr_t vehicle,std::uintptr_t simable,
    std::uintptr_t victim,std::uintptr_t victimSimable) noexcept {
    native_freeroam::EncounterSpikeHitV2(vehicle,simable,victim,victimSimable);
}

#pragma comment(linker,"/EXPORT:NFR_SelectEventOpponentV1=_NFR_SelectEventOpponentV1")
extern "C" __declspec(dllexport) std::uint32_t __cdecl NFR_SelectEventOpponentV1() noexcept {
    try{return native_freeroam::SelectEventOpponent();}catch(...){return 0;}
}
