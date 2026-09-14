// alpha.12: read-only audio diagnostics. No hooks or calls into the audio engine.
// Included in Runtime.cpp's anonymous namespace after vehicle read helpers.
constexpr std::uintptr_t kSoundRacerAudibleVtable = 0x008AC0E4;
constexpr std::uintptr_t kSoundRacerPrimaryVtable = 0x008AC0FC;
constexpr std::uintptr_t kCarSoundConnVtable = 0x00897740;
constexpr std::size_t kAudibleFromVehicle = 0x60; // PVehicle+10C - IVehicle+AC
constexpr std::size_t kSoundPrimaryFromAudible = 0x54;
static_assert(offsetof(NFSPluginSDK::MW05::PVehicle, mAudible) == 0x10C,
              "PVehicle audible layout changed");
constexpr ULONGLONG kAudioSampleMilliseconds = 5000;
bool g_audioDiagnosticsEnabled = true;
ULONGLONG g_audioNextSample = 0;
std::vector<VehicleSnapshot> g_audioVehicles;

struct AudioSnapshot {
    const char* status = "unreadable-vehicle";
    std::uintptr_t audible = 0, audibleVtable = 0, primary = 0;
    std::uintptr_t primaryVtable = 0, owner = 0, connection = 0;
    std::uintptr_t connectionVtable = 0, object = 0, engine = 0;
    int invalid = -1, connected = -1, flag22C = -1, flag22D = -1;
    int nativeAudible = -1;
    std::uint32_t aiFlag228 = 0xFFFFFFFF;
    std::int32_t selection1FC = -999, resource1F4 = -999, resource250 = -999;
};

// Do not use SEH as an ordinary probe: first-chance AVs stop attached loggers.
// No cached page permissions (objects can be released); no 2 GiB cutoff (LAA).
bool AudioReadableRange(std::uintptr_t address, std::size_t size) noexcept {
    if (address < 0x10000 || !size || size - 1 >
        std::numeric_limits<std::uintptr_t>::max() - address) return false;
    const auto last = address + size - 1;
    while (address <= last) {
        MEMORY_BASIC_INFORMATION page{};
        if (!VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) ||
            page.State != MEM_COMMIT || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto protection = page.Protect & 0xff;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
            protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
            protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY) return false;
        const auto start = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
        if (!page.RegionSize || page.RegionSize - 1 >
            std::numeric_limits<std::uintptr_t>::max() - start) return false;
        const auto end = start + page.RegionSize - 1;
        if (end >= last) return true;
        if (end < address) return false;
        address = end + 1;
    }
    return false;
}

template <typename T>
bool AudioRead(std::uintptr_t base, std::size_t offset, T* output) noexcept {
    if (!output || !base || base > std::numeric_limits<std::uintptr_t>::max() - offset ||
        !AudioReadableRange(base + offset, sizeof(T)))
        return false;
    // Final defense for an unmap/protection race after VirtualQuery.
    return SafeRead(reinterpret_cast<void*>(base + offset), output);
}

template <std::size_t N>
bool AudioCodeMatches(std::uintptr_t va, const std::uint8_t (&bytes)[N]) noexcept {
    for (std::size_t i = 0; i < N; ++i) {
        std::uint8_t actual = 0;
        if (!AudioRead(Address(va), i, &actual) || actual != bytes[i]) return false;
    }
    return true;
}

bool ValidateAudioDiagnosticSurface() noexcept {
    const std::uint8_t audible[] = {0x8B,0x41,0x08,0x50,0x83,0xC1,0xAC,0xE8,
        0xE4,0xAA,0x04,0x00,0x48,0xF7,0xD8,0x1B,0xC0,0x40,0xC3};
    const std::uint8_t connection[] = {0x8B,0x4C,0x24,0x04,0x85,0xC9,0x74,0x0F,
        0x8A,0x41,0x0C,0x84,0xC0,0x75,0x08,0x8B,0x01,0xFF,0x50,0x08,
        0xC2,0x04,0x00,0x83,0xC8,0xFF,0xC2,0x04,0x00};
    const std::uint8_t state[] = {0x8A,0x41,0x11,0x84,0xC0,0x74,0x17,0x8B,0x41,
        0x14,0x85,0xC0,0x74,0x10,0x8A,0x88,0x2D,0x02,0x00,0x00,0x84,0xC9,
        0x74,0x06,0xB8,0x01,0x00,0x00,0x00,0xC3,0x33,0xC0,0xC3};
    std::uintptr_t audibleMethod = 0, stateMethod = 0;
    return AudioCodeMatches(0x006A3F00, audible) &&
        AudioCodeMatches(0x006EE9F0, connection) && AudioCodeMatches(0x004B15B0, state) &&
        AudioRead(Address(kSoundRacerAudibleVtable), 4, &audibleMethod) &&
        audibleMethod == Address(0x006A3F00) &&
        AudioRead(Address(kCarSoundConnVtable), 8, &stateMethod) &&
        stateMethod == Address(0x004B15B0);
}

AudioSnapshot ReadAudioSnapshot(void* vehicle) noexcept {
    AudioSnapshot s;
    if (!IsExpectedVehicle(vehicle)) return s;
    if (!AudioRead(reinterpret_cast<std::uintptr_t>(vehicle), kAudibleFromVehicle,
                   &s.audible)) return s;
    s.status = "no-audible";
    if (!s.audible) return s;
    s.status = "unreadable-audible";
    if (!AudioRead(s.audible, 0, &s.audibleVtable)) return s;
    s.status = "other-audible-type";
    if (s.audibleVtable != Address(kSoundRacerAudibleVtable) ||
        s.audible < kSoundPrimaryFromAudible) return s;
    s.primary = s.audible - kSoundPrimaryFromAudible;
    s.status = "unreadable-sound";
    if (!AudioRead(s.primary, 0, &s.primaryVtable) ||
        !AudioRead(s.primary, 0x48, &s.owner)) return s;
    s.status = "sound-owner-or-type-mismatch";
    if (s.primaryVtable != Address(kSoundRacerPrimaryVtable) ||
        s.owner != reinterpret_cast<std::uintptr_t>(vehicle)) return s;
    AudioRead(s.primary, 0x60, &s.engine); // Raw cached interface; never dereference it.
    s.status = "unreadable-connection-slot";
    if (!AudioRead(s.primary, 0x5C, &s.connection)) return s;
    s.status = "no-connection";
    if (!s.connection) { s.nativeAudible = 0; return s; }
    s.status = "unreadable-connection";
    if (!AudioRead(s.connection, 0, &s.connectionVtable)) return s;
    s.status = "other-connection-type";
    if (s.connectionVtable != Address(kCarSoundConnVtable)) return s;
    std::uint8_t invalid = 0, connected = 0;
    s.status = "unreadable-connection-fields";
    if (!AudioRead(s.connection, 0x0C, &invalid) ||
        !AudioRead(s.connection, 0x11, &connected) ||
        !AudioRead(s.connection, 0x14, &s.object)) return s;
    s.invalid = invalid;
    s.connected = connected;
    // 6EE9F0 returns -1 for invalid connections; do not follow a retiring object.
    if (invalid) { s.status = "invalid-connection"; s.nativeAudible = 0; return s; }
    if (!s.object) { s.status = "no-audio-object"; s.nativeAudible = 0; return s; }
    std::uint8_t flag22C = 0, flag22D = 0;
    s.status = "unreadable-audio-object";
    if (!AudioRead(s.object, 0x22C, &flag22C) ||
        !AudioRead(s.object, 0x22D, &flag22D)) return s;
    s.flag22C = flag22C;
    s.flag22D = flag22D;
    AudioRead(s.object, 0x228, &s.aiFlag228);
    AudioRead(s.object, 0x1FC, &s.selection1FC);
    AudioRead(s.object, 0x1F4, &s.resource1F4);
    AudioRead(s.object, 0x250, &s.resource250);
    // Mirror 4B15B0 -> 6EE9F0 -> 6A3F00, without invoking any virtual method.
    // This native boolean is NOT proof that an audible engine sample reached output.
    s.nativeAudible = connected && flag22D ? 1 : 0;
    s.status = s.nativeAudible ? "native-audible" : "native-not-audible";
    return s;
}

#include "AudioDetailDiagnostics.inl"

void LogAudioVehicleImpl(void* vehicle, const char* phase, const char* source,
                        std::uint32_t key, float distance, float speed) noexcept {
    if (!g_audioDiagnosticsEnabled) return;
    const auto s = ReadAudioSnapshot(vehicle);
    std::uint32_t driver = 0xFFFFFFFF;
    AudioRead(reinterpret_cast<std::uintptr_t>(vehicle), 0x94, &driver);
    Log(LogLevel::Info,
        "AUDIO_VEHICLE phase=%s source=%s vehicle=%08X key=%08X driver=%u distance=%.1fm speed=%.1fkmh status=%s audible=%08X audibleVT=%08X sound=%08X soundVT=%08X soundOwner=%08X cachedEngine=%08X conn=%08X connVT=%08X invalid=%d connected=%d object=%08X flag22C=%d flag22D=%d nativeAudible=%d aiFlag228=%u selection1FC=%d resource1F4=%d resource250=%d",
        phase, source, static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(vehicle)),
        key, driver, distance, speed, s.status,
        static_cast<unsigned>(s.audible), static_cast<unsigned>(s.audibleVtable),
        static_cast<unsigned>(s.primary), static_cast<unsigned>(s.primaryVtable),
        static_cast<unsigned>(s.owner), static_cast<unsigned>(s.engine),
        static_cast<unsigned>(s.connection), static_cast<unsigned>(s.connectionVtable),
        s.invalid, s.connected, static_cast<unsigned>(s.object), s.flag22C, s.flag22D,
        s.nativeAudible, s.aiFlag228, s.selection1FC, s.resource1F4, s.resource250);
    LogAudioDetail(vehicle, s, source, phase, key);
}

void LogAudioVehicle(void* vehicle, const char* phase, const char* source,
                     std::uint32_t key = 0, float distance = -1.0f,
                     float speed = -1.0f) noexcept {
    __try { LogAudioVehicleImpl(vehicle, phase, source, key, distance, speed); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        g_audioDiagnosticsEnabled = false;
        Log(LogLevel::Warning, "AUDIO_DIAGNOSTICS_DISABLED exception=%08X populationUnchanged=1",
            static_cast<unsigned>(GetExceptionCode()));
    }
}

void AudioDiagnosticTick() {
    if (!g_audioDiagnosticsEnabled) return;
    const ULONGLONG now = GetTickCount64();
    if (now < g_audioNextSample) return;
    g_audioNextSample = now + kAudioSampleMilliseconds;
    std::uint32_t flow = 0;
    NFSPluginSDK::MW05::GRaceStatus* race = nullptr;
    if (!SafeRead(reinterpret_cast<void*>(Address(kGameFlowState)), &flow) ||
        flow != kGameFlowRacing ||
        !SafeRead(reinterpret_cast<void*>(Address(kRaceStatus)), &race) || !race) return;
    NFSPluginSDK::MW05::GRaceStatus::PlayMode playMode{};
    if (!SafeRead(&race->mPlayMode, &playMode)) return;
    const bool roaming = playMode == NFSPluginSDK::MW05::GRaceStatus::PlayMode::Roaming;
    const char* scope = roaming ? "freeroam" : "non-roaming";
    EnumerateVehicles(g_audioVehicles);
    const auto player = std::find_if(g_audioVehicles.begin(), g_audioVehicles.end(),
        [](const VehicleSnapshot& v) { return v.driverClass == kDriverHuman; });
    if (player == g_audioVehicles.end()) return;
    std::uintptr_t subsystem = 0, mixer = 0;
    SafeRead(reinterpret_cast<void*>(Address(0x008F86F8)), &subsystem);
    SafeRead(reinterpret_cast<void*>(Address(0x00911FA8)), &mixer);
    Log(LogLevel::Info, "AUDIO_SAMPLE scope=%s playMode=%u vehicles=%u managed=%u subsystem=%08X mixer=%08X readOnly=1 audioCalls=0",
        scope, static_cast<unsigned>(playMode), static_cast<unsigned>(g_audioVehicles.size()),
        static_cast<unsigned>(g_racers.size()), static_cast<unsigned>(subsystem),
        static_cast<unsigned>(mixer));
    unsigned trafficSamples = 0, rows = 0;
    for (const auto& v : g_audioVehicles) {
        if (rows >= 32) break;
        const bool managed = std::any_of(g_racers.begin(), g_racers.end(),
            [&v](const ManagedRacer& r) { return r.pointer == v.pointer && r.vehicleKey == v.vehicleKey; });
        const bool traffic = v.driverClass == kDriverTraffic;
        if (traffic) {
            if (trafficSamples >= 2 || Distance(player->position, v.position) > 300.0f) continue;
            ++trafficSamples;
        } else if (!managed && v.driverClass != kDriverHuman && v.driverClass != kDriverRacer) continue;
        const char* source = managed ? "mod" : v.driverClass == kDriverHuman ? "player" :
                             traffic ? "traffic" : "native-racer";
        LogAudioVehicle(v.pointer, scope, source, v.vehicleKey,
                        Distance(player->position, v.position), v.speed * 3.6f);
        ++rows;
    }
}

// A diagnostic failure must not latch off the population or touch vehicle state.
void SafeAudioDiagnosticTick() noexcept {
    __try { AudioDiagnosticTick(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        g_audioDiagnosticsEnabled = false;
        Log(LogLevel::Warning, "AUDIO_DIAGNOSTICS_DISABLED exception=%08X populationUnchanged=1",
            static_cast<unsigned>(GetExceptionCode()));
    }
}
