#include "Runtime.h"

#include "Logging.h"
#include "VehicleCatalog.h"
#include "EncounterBattleModel.h"
#include "EncounterRouteTrail.h"
#include "EncounterPursuit.h"
#include "EncounterPursuitPath.h"
#include "EncounterCommittedPath.h"
#include <NFSPluginSDK/Game.MW05/Types/WRoadNav.h>
#include "EncounterText.h"
#include "EncounterWave.h"

#include <Windows.h>
#include <Wincrypt.h>
#include <mmsystem.h>
#include <d3d9.h>
#include <MinHook.h>
#include <intrin.h>

#include <NFSPluginSDK/Game.MW05/Types/AIGoal.h>
#include <NFSPluginSDK/Game.MW05/Types/VehicleBehavior.h>
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
constexpr std::uintptr_t kGetHeading = 0x00688250;
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
constexpr std::size_t kSlotHeading = 45;
constexpr std::size_t kSlotAIResetVehicleToRoadNav = 22;
constexpr std::size_t kSlotAISetSpawned = 34;
constexpr std::size_t kSlotAIGetCurrentRoad = 45;
constexpr std::size_t kGoalChooseActionSlot = 1;

constexpr std::uint32_t kGameFlowRacing = 6;
constexpr std::uint32_t kDriverHuman = 0;
constexpr std::uint32_t kDriverTraffic = 1;
constexpr std::uint32_t kDriverRacer = 3;
constexpr std::uint32_t kMaximumVehicles = 256;
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
constexpr std::uint64_t kExpectedExecutableSize = 6029312;
constexpr char kExpectedExecutableSha256[] =
    "B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06";

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
    unsigned freeRoamAudioSlots = 4;
    std::size_t vehicleVariety = 5;
    bool randomAppearance = true;
    unsigned appearanceAttempts = 3;
    std::size_t maximumRacers = kDefaultMaximumRacers;
    float populationRadiusMeters = 600.0f;
    float markerRadiusMeters = 200.0f;
    float anchorMinimumMeters = 350.0f;
    float anchorMaximumMeters = 700.0f;
    float anchorMinimumSpeedMps = 1.0f;
    float anchorHeadingDotMinimum = 0.25f;
    float spawnIntervalSeconds = 5.0f;
    float missingGraceSeconds = 2.0f;
    float telemetryIntervalSeconds = 5.0f;
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
    int encounterActor = -1;
};

using DisplayFrameFn = int(__cdecl*)();
using CacheQueryFn = std::uint32_t(__thiscall*)(void*, const void*, const void*);

std::uintptr_t g_base = 0;
DisplayFrameFn g_originalDisplayFrame = nullptr;
CacheQueryFn g_originalCacheQuery = nullptr;
Settings g_settings{};
std::vector<ManagedRacer> g_racers;
std::vector<VehicleSnapshot> g_vehicleScratch;
NFSPluginSDK::MW05::GManager* g_markerManager = nullptr;
std::array<MarkerSlot, kMaximumRacersLimit> g_markers{};
std::array<std::atomic<std::uintptr_t>, kMaximumRacersLimit>
    g_protectedSimables{};
std::array<std::atomic<std::uintptr_t>, kMaximumRacersLimit>
    g_protectedVehicles{};
std::atomic<bool> g_streamRetentionEnabled{false};
std::array<std::atomic<std::uint32_t>, kMaximumRacersLimit> g_protectedKeys{};
std::atomic<std::uint32_t> g_cacheWanted{0};
std::atomic<std::uint32_t> g_cacheChangedVotes{0};
std::atomic<std::uintptr_t> g_lastCacheVehicle{0};
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

    g_settings.enabled =
        GetPrivateProfileIntW(L"Population", L"Enabled", 1, modulePath) != 0;
    g_settings.encounterSignalEnabled =
        GetPrivateProfileIntW(L"Encounter", L"SignalEnabled", 1, modulePath) != 0;
    wchar_t encounterLanguage[16]{};
    GetPrivateProfileStringW(L"Encounter", L"Language", L"ja", encounterLanguage,16,modulePath);
    g_settings.encounterEnglish = _wcsicmp(encounterLanguage,L"en")==0;
    LoadEncounterPowerSettings(modulePath);
    g_settings.backgroundPoliceEnabled = GetPrivateProfileIntW(L"BackgroundPolice", L"Enabled", 1, modulePath) != 0;
    g_settings.freeRoamAudioSlots = static_cast<unsigned>(std::clamp(
        static_cast<int>(GetPrivateProfileIntW(L"Audio", L"FreeRoamSlots", 4, modulePath)), 0, 4));
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
}

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
        const auto* heading =
            reinterpret_cast<const NFSPluginSDK::MW05::UMath::Vector3* (
                __thiscall*)(void*)>(vtable[kSlotHeading])(vehicle);
        if (position == nullptr || heading == nullptr) return false;
        output->pointer = vehicle;
        output->position = FromUMath(*position);
        output->heading = FromUMath(*heading);
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

void EnumerateVehicles(std::vector<VehicleSnapshot>& result) {
    result.clear(); // Keep the allocation reserved at installation.
    std::uint32_t count = 0;
    void** data = nullptr;
    if (!SafeRead(reinterpret_cast<void*>(Address(kVehicleListCount)), &count) ||
        !SafeRead(reinterpret_cast<void*>(Address(kVehicleListData)), &data) ||
        data == nullptr || count > kMaximumVehicles)
        return;
    for (std::uint32_t index = 0; index < count; ++index) {
        void* vehicle = nullptr;
        if (!SafeRead(data + index, &vehicle)) continue;
        VehicleSnapshot snapshot{};
        if (ReadVehicle(vehicle, &snapshot)) result.push_back(snapshot);
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

std::uint32_t __fastcall CacheQueryHook(void* cache, void*,
                                      const void* vehicle,
                                      const void* askingCache) noexcept {
    // Native OnQueryVehicleCache is a read-only vote, before OnRemoved and
    // any component/destructor work. Want=0 removes this candidate from the
    // eviction list at 0x6879EE. All unrelated votes retain native behavior.
    const std::uint32_t original = g_originalCacheQuery(cache, vehicle, askingCache);
    if (!IsManagedCacheVehicle(vehicle)) return original;
    g_cacheWanted.fetch_add(1, std::memory_order_relaxed);
    if (original != 0) g_cacheChangedVotes.fetch_add(1, std::memory_order_relaxed);
    g_lastCacheVehicle.store(reinterpret_cast<std::uintptr_t>(vehicle),
                             std::memory_order_relaxed);
    return 0; // eVehicleCacheResult::Want
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
        reinterpret_cast<void(__thiscall*)(void*)>(simableVtable[2])(simable);
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
    __try {
        const auto mapPosition = ToMath(position);
        if (slot.icon == nullptr) {
            using AllocateFn = NFSPluginSDK::MW05::GIcon*(__thiscall*)(
                NFSPluginSDK::MW05::GManager*,
                NFSPluginSDK::MW05::GIcon::Type,
                const NFSPluginSDK::MW05::Math::Vector3&, float, bool);
            slot.icon = reinterpret_cast<AllocateFn>(Address(kGManagerAllocIcon))(
                g_markerManager,
                NFSPluginSDK::MW05::GIcon::Type::RaceRival, mapPosition, 0.0f,
                false);
            if (slot.icon == nullptr) {
                Log(LogLevel::Error, "RaceRival marker allocation failed");
                return;
            }
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
        Log(LogLevel::Error, "RaceRival marker update faulted code=%08X",
            static_cast<unsigned int>(GetExceptionCode()));
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

bool SpawnAtAnchor(const VehicleSnapshot& anchor,bool* candidateWait) {
    *candidateWait=false;
    NFSPluginSDK::MW05::WRoadNav* anchorRoad = nullptr;
    if (!GetTrafficRoad(anchor.pointer, &anchorRoad)) return false;
    void* anchorSimable=GetSimablePointer(anchor.pointer);
    AnchorRoadSeed anchorSeed;
    if(!AnchorGenerationMatches(anchor,anchorSimable)||!CaptureAnchorRoadSeed(anchorRoad,anchorSeed)) {
        Log(LogLevel::Info,"ANCHOR_SEED skipped reason=invalid-road-or-generation anchor=%p nativeCopyCalls=0",anchor.pointer);
        return false;
    }
    if (!g_appearanceAvailable) return false;
    CareerPerformancePlan performancePlan{};
    const auto* selectedCar = SelectPreparedSpawnVehicle(&performancePlan,ReadCareerPerformancePlan);
    if (!selectedCar) { *candidateWait=true;return false; }
    const std::uint32_t vehicleKey = selectedCar->installed->key;
    const char* preset = selectedCar->installed->model;
    NFSPluginSDK::MW05::FECustomizationRecord customization;
    if (!MakeAppearance(*selectedCar, &customization)) return false;
    WriteCareerPerformancePlan(vehicleKey,performancePlan,&customization);
    void* rawStatus = nullptr;
    if (!SafeRead(reinterpret_cast<void*>(Address(kRaceStatus)), &rawStatus) ||
        rawStatus == nullptr)
        return false;

    const auto nativeDirection = ToUMath(anchor.heading);
    const auto nativePosition = ToUMath(anchor.position);
    auto* created = ConstructTrafficSeed(rawStatus, vehicleKey, nativeDirection,
                                         nativePosition, &customization);
    // Target 0x68934D allocates 0x198 bytes; 0x689361 copies all 0x66 DWORDs.
    // PVehicle owns its copy. No pointer to this stack record survives Construct.
    if (created == nullptr) return false;
    auto* vehicle = static_cast<NFSPluginSDK::MW05::IVehicle*>(created);
    if (!IsExpectedVehicle(vehicle) ||
        !VerifySpawnPerformance(created, customization) ||
        !ActivateNativeRacer(vehicle, anchorSeed)) {
        KillVehicle(vehicle, "native action activation failed");
        return false;
    }
    SpawnIdentity identity{};
    if (!CaptureSpawnIdentity(vehicle, &identity)) {
        KillVehicle(vehicle, "spawn identity unavailable");
        return false;
    }
    if (AnchorGenerationMatches(anchor,anchorSimable) &&
        !KillVehicle(anchor.pointer, "replaced by native free-roam racer")) {
        KillVehicle(vehicle, "anchor replacement aborted");
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
    g_racers.push_back(std::move(racer));
    ProtectManagedVehicle(markerSlot, g_racers.back().pointer,
                          g_racers.back().simable, g_racers.back().vehicleKey);
    Log(LogLevel::Info,
        "SPAWNED_NATIVE_RACER preset=%s modelKey=%08X key=%08X vehicle=%08X anchor=%08X anchorSpeed=%.1fkmh population=%u/%u",
        preset, vehicleKey, identity.key,
        static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(vehicle)),
        static_cast<unsigned int>(
            reinterpret_cast<std::uintptr_t>(anchor.pointer)),
        anchor.speed * 3.6f, static_cast<unsigned int>(g_racers.size()),
        static_cast<unsigned int>(g_settings.maximumRacers));
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
    AdvanceVehicleCatalog();
    if (!EnsureVehiclePalette(player)) return;
    if (g_spawnFailureBackoffSeconds > 0.0f) {
        g_spawnFailureBackoffSeconds =
            std::max(0.0f, g_spawnFailureBackoffSeconds - dt);
        return;
    }
    if (g_racers.size() >= g_settings.maximumRacers) return;
    g_spawnTimer += dt;
    if (g_spawnTimer < g_settings.spawnIntervalSeconds) return;
    g_spawnTimer = 0.0f;

    const VehicleSnapshot* selected = nullptr;
    std::size_t candidateCount = 0;
    for (const auto& vehicle : vehicles) {
        if (vehicle.driverClass != kDriverTraffic ||
            vehicle.speed < g_settings.anchorMinimumSpeedMps)
            continue;
        const float distance = Distance(player.position, vehicle.position);
        if (distance < g_settings.anchorMinimumMeters ||
            distance > g_settings.anchorMaximumMeters ||
            HorizontalHeadingDot(player.heading, vehicle.heading) <
                g_settings.anchorHeadingDotMinimum)
            continue;
        // Reservoir sampling: uniform choice without allocating or shuffling.
        ++candidateCount;
        if (std::uniform_int_distribution<std::size_t>(1, candidateCount)(g_random) == 1)
            selected = &vehicle;
    }
    if (selected == nullptr) return;
    Log(LogLevel::Info,
        "SPAWN_ATTEMPT_BEGIN candidateCount=%u population=%u/%u failurePolicy=retry-with-bounded-backoff consecutiveFailures=%u",
        static_cast<unsigned int>(candidateCount),
        static_cast<unsigned int>(g_racers.size()),
        static_cast<unsigned int>(g_settings.maximumRacers),
        g_consecutiveSpawnFailures);
    bool candidateWait=false;
    if (!SpawnAtAnchor(*selected,&candidateWait)) {
        if(candidateWait) {
            // No allocation attempted: this is not an engine spawn failure.
            g_consecutiveSpawnFailures=0;g_spawnFailureBackoffSeconds=0.0f;return;
        }
        ++g_consecutiveSpawnFailures;
        const std::uint32_t shift =
            std::min<std::uint32_t>(g_consecutiveSpawnFailures, 4);
        g_spawnFailureBackoffSeconds = std::min(
            60.0f, 5.0f * static_cast<float>(1u << shift));
        Log(LogLevel::Error,
            "Population spawn failed; retry deferred %.1fs consecutiveFailures=%u permanentLatch=0",
            g_spawnFailureBackoffSeconds, g_consecutiveSpawnFailures);
    } else {
        g_consecutiveSpawnFailures = 0;
        g_spawnFailureBackoffSeconds = 0.0f;
    }
}

void LogTelemetry(const VehicleSnapshot& player,
                  const std::vector<VehicleSnapshot>& vehicles,
                  const float dt) {
    g_telemetryTimer += dt;
    if (g_telemetryTimer < g_settings.telemetryIntervalSeconds) return;
    g_telemetryTimer = 0.0f;
    std::size_t visibleMarkers = 0;
    for (const auto& marker : g_markers)
        if (marker.visible) ++visibleMarkers;
    Log(LogLevel::Info, "TELEMETRY managed=%u/%u markers=%u vehicles=%u",
        static_cast<unsigned int>(g_racers.size()),
        static_cast<unsigned int>(g_settings.maximumRacers),
        static_cast<unsigned int>(visibleMarkers),
        static_cast<unsigned int>(vehicles.size()));
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

void ClearWorld(const bool killManaged) noexcept {
    ResetSpawnEligibility();
    ResetBackgroundPolice();
    // No cached vehicle dereference after the world has gone away.
    EndEncounterBattle(false,true);
    ResetEncounterSignal();
    g_streamRetentionEnabled.store(false, std::memory_order_release);
    for (std::size_t index = g_racers.size(); index-- > 0;)
        RemoveRacer(index, "free-roam world ended", killManaged);
    for (auto& marker : g_markers) marker = {};
    g_markerManager = nullptr;
    g_palette.clear();
    g_vehicleBag.clear();
    g_palettePlayerKey = 0;
    g_spawnTimer = 0.0f;
    g_telemetryTimer = 0.0f;
    g_managementPending = 0.0f;
    g_vehicleScratch.clear();
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
    UpdateEncounterVoiceVolume();
    if (g_faulted) return;
    CaptureFrontendCatalog(); // Must run before the free-roam-only guard.
    const float elapsed = DeltaSeconds();
    if (!IsFreeRoam()) {
        if (g_worldReady) {
            ClearWorld(true);
            Log(LogLevel::Info, "Free-roam world ended and pilot racer released");
        }
        g_worldReady = false;
        return;
    }
    if (!g_worldReady) {
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
    EnumerateVehicles(g_vehicleScratch);
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
    RefreshManaged(*player, vehicles, dt);
    Replenish(*player, vehicles, dt);
    UpdateEncounterCandidate(*player, vehicles, dt);
    UpdateBackgroundPolice(*player, vehicles, dt);
    LogTelemetry(*player, vehicles, dt);
}

#include "FrameTimingDiagnostics.inl"

int __cdecl DisplayFrameHook() {
    const auto start = GetTickCount64();
    const int result = g_originalDisplayFrame();
    const auto nativeEnd = GetTickCount64();
    SafeAudioDiagnosticTick();
    const auto audioEnd = GetTickCount64();
    __try {
        Update();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_faulted = true;
        Log(LogLevel::Error,
            "Frame update exception code=%08X; runtime latched disabled",
            static_cast<unsigned int>(GetExceptionCode()));
    }
    ObserveFrameTiming(start, nativeEnd, audioEnd, GetTickCount64(), g_worldReady);
    return result;
}

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
        reinterpret_cast<std::uintptr_t>(vehicleVtable[kSlotHeading]) !=
            Address(kGetHeading) ||
        reinterpret_cast<std::uintptr_t>(
            goalVtable[kGoalChooseActionSlot]) != Address(kGoalChooseAction)) {
        if (error != nullptr) *error = "vtable guard failed";
        return false;
    }
    return true;
}

}  // namespace

bool VerifyHostExecutable() noexcept {
    wchar_t executablePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) == 0) {
        Log(LogLevel::Error, "Could not resolve host executable");
        return false;
    }
    const wchar_t* fileName = wcsrchr(executablePath, L'\\');
    fileName = fileName == nullptr ? executablePath : fileName + 1;
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (_wcsicmp(fileName, kExpectedExecutableName) != 0 ||
        !GetFileAttributesExW(executablePath, GetFileExInfoStandard,
                              &attributes)) {
        Log(LogLevel::Error, "Unsupported host name; fail-closed");
        return false;
    }
    const std::uint64_t size =
        (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) |
        attributes.nFileSizeLow;
    std::string hash;
    if (size != kExpectedExecutableSize ||
        !ComputeSha256(executablePath, &hash) ||
        hash != kExpectedExecutableSha256) {
        Log(LogLevel::Error,
            "Unsupported executable; fail-closed size=%llu sha256=%s",
            static_cast<unsigned long long>(size),
            hash.empty() ? "unavailable" : hash.c_str());
        return false;
    }
    g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    Log(LogLevel::Info,
        "Supported NFSPatcher LAA target accepted size=%llu sha256=%s",
        static_cast<unsigned long long>(size), hash.c_str());
    return true;
}

bool InstallRuntime() noexcept {
    LoadSettings();
    if (!g_settings.enabled) {
        Log(LogLevel::Info,
            "Population disabled by configuration; no hook installed");
        return true;
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
    LARGE_INTEGER seed{};
    try { PrepareCatalogPersistence(); }
    catch(...) { Log(LogLevel::Warning,"CALIBRATION_CACHE_DISABLED initialization=failed"); }
    g_audioDiagnosticsEnabled = ValidateAudioDiagnosticSurface();
    g_audioDetailEnabled = g_audioDiagnosticsEnabled && ValidateAudioDetailSurface();
    Log(LogLevel::Info, "AUDIO_DETAIL enabled=%u writes=0 calls=0 registryLimit=128",
        static_cast<unsigned>(g_audioDetailEnabled));
    Log(LogLevel::Info, "AUDIO_DIAGNOSTICS enabled=%u exactAudioSurface=%u writes=0 calls=0 intervalMs=5000 maxRows=32 populationUnaffectedOnMismatch=1",
        static_cast<unsigned>(g_audioDiagnosticsEnabled),
        static_cast<unsigned>(g_audioDiagnosticsEnabled));
    g_racers.reserve(kMaximumRacersLimit);
    g_vehicleScratch.reserve(kMaximumVehicles);
    g_audioVehicles.reserve(kMaximumVehicles);
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
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(reinterpret_cast<void*>(Address(kRaceStatusCacheQuery)));
        MH_RemoveHook(reinterpret_cast<void*>(Address(kMainDisplayFrame)));
        MH_Uninitialize();
        return false;
    }
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
    InstallEncounterPathHook();
    InstallEncounterSignalHooks();
    Log(LogLevel::Info,
        "Runtime installed mode=alpha.50-spawn-eligibility max=%u populationRadius=%.1fm markerRadius=%.1fm managementMaxHz=20 nativeAIUnthrottled=1 failurePolicy=retry-with-bounded-backoff cacheWantRetention=1 managedKillRetention=0 managedDeactivateRetention=0 destructorRetention=0 battleRouteAdapterMaxHz=1 passTransitionImmediate=1 directSteeringWrites=0 borrowedAnchorRoadAcrossAllocation=0",
        static_cast<unsigned int>(g_settings.maximumRacers),
        g_settings.populationRadiusMeters, g_settings.markerRadiusMeters);
    return true;
}

void StopEncounterVoiceForProcessExit() noexcept {
    // DllMain must never join or wait for an audio thread/device teardown.
    if(g_voice) {g_voice->stop=true;SetEvent(g_voice->wake);}
}

}  // namespace native_freeroam
