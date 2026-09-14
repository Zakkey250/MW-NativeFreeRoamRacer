// alpha.13: bounded, passive reads only. No audio or physics function calls.
bool g_audioDetailEnabled = false;
constexpr std::size_t kAudioRegistryLimit = 128;
constexpr float kAudioUnknownFloat = std::numeric_limits<float>::quiet_NaN();

bool AudioNativeVtable(std::uintptr_t value) noexcept {
    return value >= Address(0x00890000) && value < Address(0x008F0000);
}

bool AudioFiniteRead(std::uintptr_t base, std::size_t offset, float* result) noexcept {
    float value = kAudioUnknownFloat;
    if (!AudioRead(base, offset, &value) || !std::isfinite(value)) return false;
    *result = value;
    return true;
}

bool ValidateAudioDetailSurface() noexcept {
    // EngineRacer IEngine::GetRPM is exactly fld [ecx+12C]; ret.
    const std::uint8_t rpm[] = {0xD9,0x81,0x2C,0x01,0x00,0x00,0xC3};
    // 4D96B0: native mixer registry lookup follows head+10, next+4, object+1C.
    const std::uint8_t registry[] = {0x8B,0x41,0x10,0x85,0xC0,0x74,0x15,
        0x8B,0x4C,0x24,0x04,0xEB,0x03,0x8D,0x49,0x00,0x3B,0x48,0x1C,
        0x74,0x09,0x8B,0x40,0x04,0x85,0xC0,0x75,0xF4,0x33,0xC0,0xC2,0x04,0x00};
    return AudioCodeMatches(0x006A03A0, rpm) && AudioCodeMatches(0x004D96B0, registry);
}

struct EngineAudioInput {
    std::uintptr_t liveEngine = 0, cachedEngineVT = 0, liveEngineVT = 0;
    std::uintptr_t liveInput = 0, cachedInput = 0, controlsGetter = 0;
    float liveRpm = kAudioUnknownFloat, cachedRpm = kAudioUnknownFloat;
    float soundRpm7C = kAudioUnknownFloat, gas = kAudioUnknownFloat, brake = kAudioUnknownFloat;
    unsigned valid = 0;
};

bool ReadVerifiedEngineRpm(std::uintptr_t engine, std::uintptr_t vehicle,
                           std::uintptr_t* vt, float* rpm) noexcept {
    if (!AudioRead(engine, 0, vt)) return false;
    if (*vt != Address(0x008AB6E0) && *vt != Address(0x008ABF88)) return false;
    if (engine < 0x54) return false;
    std::uintptr_t owner = 0, method = 0;
    if (!AudioRead(engine - 0x54, 0x48, &owner) || owner != vehicle ||
        !AudioRead(*vt, 4, &method) || method != Address(0x006A03A0)) return false;
    return AudioFiniteRead(engine, 0x12C, rpm);
}

// Decode ONLY the native trivial address-returning GetControls getter; never execute it.
bool DecodeControlsOffset(const std::array<std::uint8_t, 7>& code,
                          std::size_t* offset) noexcept {
    if (!offset || code[0] != 0x8D) return false;
    if (code[1] == 0x41 && code[3] == 0xC3 && code[2] < 0x80) {
        *offset = code[2]; return true;
    }
    if (code[1] == 0x81 && code[6] == 0xC3) {
        std::uint32_t displacement = 0;
        std::memcpy(&displacement, code.data() + 2, sizeof(displacement));
        if (displacement <= 0x200) { *offset = displacement; return true; }
    }
    return false;
}

bool ReadControlsOffset(std::uintptr_t method, std::size_t* offset) noexcept {
    if (method < Address(0x00401000) || method >= Address(0x00890000)) return false;
    std::array<std::uint8_t, 7> code{};
    return AudioRead(method, 0, &code) && DecodeControlsOffset(code, offset);
}

EngineAudioInput ReadEngineAudioInput(void* vehicle, const AudioSnapshot& s) noexcept {
    EngineAudioInput r;
    const auto v = reinterpret_cast<std::uintptr_t>(vehicle);
    if (!IsExpectedVehicle(vehicle) || !s.primary || s.owner != v ||
        s.primaryVtable != Address(kSoundRacerPrimaryVtable)) return r;
    if (AudioRead(v, 0x48, &r.liveEngine)) r.valid |= 1;
    if (ReadVerifiedEngineRpm(r.liveEngine, v, &r.liveEngineVT, &r.liveRpm)) r.valid |= 2;
    if (ReadVerifiedEngineRpm(s.engine, v, &r.cachedEngineVT, &r.cachedRpm)) r.valid |= 4;
    if (AudioFiniteRead(s.primary, 0x7C, &r.soundRpm7C)) r.valid |= 8;
    if (AudioRead(v, 0x3C, &r.liveInput) && AudioRead(s.primary, 0x64, &r.cachedInput)) r.valid |= 16;
    std::uintptr_t inputVT = 0;
    std::size_t controlsOffset = 0;
    if (r.cachedInput && r.cachedInput == r.liveInput &&
        AudioRead(r.cachedInput, 0, &inputVT) && AudioNativeVtable(inputVT) &&
        AudioRead(inputVT, 8, &r.controlsGetter) &&
        ReadControlsOffset(r.controlsGetter, &controlsOffset)) {
        // InputControls.mGas / mBrake; same offsets read by 69425B/6942B1.
        if (AudioFiniteRead(r.cachedInput, controlsOffset + 0x14, &r.gas)) r.valid |= 32;
        if (AudioFiniteRead(r.cachedInput, controlsOffset + 0x18, &r.brake)) r.valid |= 64;
    }
    return r;
}

struct MixerMembership {
    const char* status = "unknown";
    std::uintptr_t manager = 0, node = 0, vtable = 0;
    unsigned visited = 0, validWords = 0;
    std::array<std::uint32_t, 6> words{}; // Node+08,0C,10,14,18,20; raw, not channel IDs.
};

MixerMembership ReadMixerMembership(std::uintptr_t manager, std::uintptr_t object) noexcept {
    MixerMembership r;
    r.manager = manager;
    if (!object) { r.status = "no-object"; return r; }
    if (!manager) { r.status = "no-manager"; return r; }
    std::uintptr_t managerVT = 0, next = 0;
    r.status = "unreadable-manager";
    if (!AudioRead(manager, 0, &managerVT)) return r;
    r.status = "unknown-manager-type";
    if (!AudioNativeVtable(managerVT)) return r;
    r.status = "unreadable-head";
    if (!AudioRead(manager, 0x10, &next)) return r;
    std::array<std::uintptr_t, kAudioRegistryLimit> seen{};
    while (next) {
        if (r.visited >= seen.size()) { r.status = "limit"; return r; }
        for (unsigned i = 0; i < r.visited; ++i)
            if (seen[i] == next) { r.status = "cycle"; return r; }
        seen[r.visited++] = next;
        std::uintptr_t vt = 0, candidate = 0, following = 0;
        r.status = "unreadable-node";
        if (!AudioRead(next, 0, &vt)) return r;
        r.status = "unknown-node-type";
        if (!AudioNativeVtable(vt)) return r;
        r.status = "unreadable-node";
        if (!AudioRead(next, 0x1C, &candidate) || !AudioRead(next, 4, &following)) return r;
        if (candidate == object) {
            r.status = "registered"; r.node = next; r.vtable = vt;
            constexpr std::size_t offsets[] = {8,0x0C,0x10,0x14,0x18,0x20};
            for (std::size_t i = 0; i < r.words.size(); ++i)
                if (AudioRead(next, offsets[i], &r.words[i])) r.validWords |= 1u << i;
            return r;
        }
        next = following;
    }
    r.status = "not-registered";
    return r;
}

#include "AudioFlowDiagnostics.inl"
#include "AudioSourceDiagnostics.inl"
#include "AudioLifecycleDiagnostics.inl"
#include "AudioPoolSupport.inl"

void LogAudioDetail(void* vehicle, const AudioSnapshot& s, const char* source,
                    const char* phase, std::uint32_t key) noexcept {
    if (!g_audioDetailEnabled || s.invalid != 0 || s.connected < 0 ||
        !s.object || s.connectionVtable != Address(kCarSoundConnVtable)) return;
    // Do not expand logging within the creation/activation critical section.
    if (std::strcmp(phase, "freeroam") && std::strcmp(phase, "non-roaming")) return;
    LogAudioFlow(s, source, phase, key);
    LogAudioSource(s, source, phase, key);
    LogAudioLife(s, source, phase, key);
    LogAudioPool(s, source, phase, key);
    const auto input = ReadEngineAudioInput(vehicle, s);
    const auto v = static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(vehicle));
    Log(LogLevel::Info,
        "AUDIO_INPUT phase=%s source=%s vehicle=%08X key=%08X valid=%02X liveEngine=%08X cachedEngine=%08X liveEngineVT=%08X cachedEngineVT=%08X liveRpm=%.2f cachedRpm=%.2f soundRpm7C=%.2f liveInput=%08X cachedInput=%08X controlsGetter=%08X gas=%.3f brake=%.3f",
        phase, source, v, key, input.valid,
        static_cast<unsigned>(input.liveEngine), static_cast<unsigned>(s.engine),
        static_cast<unsigned>(input.liveEngineVT), static_cast<unsigned>(input.cachedEngineVT),
        input.liveRpm, input.cachedRpm, input.soundRpm7C,
        static_cast<unsigned>(input.liveInput), static_cast<unsigned>(input.cachedInput),
        static_cast<unsigned>(input.controlsGetter), input.gas, input.brake);
    std::uintptr_t manager = 0;
    MixerMembership mixer;
    if (SafeRead(reinterpret_cast<void*>(Address(0x00911F8C)), &manager))
        mixer = ReadMixerMembership(manager, s.object);
    Log(LogLevel::Info,
        "AUDIO_MIXER phase=%s source=%s vehicle=%08X key=%08X object=%08X manager=%08X status=%s visited=%u node=%08X nodeVT=%08X valid=%02X raw08=%08X raw0C=%08X raw10=%08X raw14=%08X raw18=%08X raw20=%08X",
        phase, source, v, key, static_cast<unsigned>(s.object), static_cast<unsigned>(manager),
        mixer.status, mixer.visited, static_cast<unsigned>(mixer.node), static_cast<unsigned>(mixer.vtable),
        mixer.validWords, mixer.words[0], mixer.words[1], mixer.words[2], mixer.words[3], mixer.words[4], mixer.words[5]);
    // Four 44-byte vehicle-state blocks, populated from the callback packet at 4B18D0.
    // These raw blocks are NOT proof of engine sound-bank loading (legacy AUDIO_BANK name).
    std::array<std::uint32_t, 20> bankWords{};
    unsigned bankValid = 0;
    for (std::size_t i = 0; i < bankWords.size(); ++i)
        if (AudioRead(s.object, 0xBC + (i / 5) * 0x44 + (i % 5) * 4, &bankWords[i])) bankValid |= 1u << i;
    Log(LogLevel::Info,
        "AUDIO_STATE_BLOCKS vehicle=%08X key=%08X valid=%05X offsets=00/04/08/0C/10 slot0=%08X/%08X/%08X/%08X/%08X slot1=%08X/%08X/%08X/%08X/%08X slot2=%08X/%08X/%08X/%08X/%08X slot3=%08X/%08X/%08X/%08X/%08X",
        v, key, bankValid, bankWords[0], bankWords[1], bankWords[2], bankWords[3], bankWords[4],
        bankWords[5], bankWords[6], bankWords[7], bankWords[8], bankWords[9],
        bankWords[10], bankWords[11], bankWords[12], bankWords[13], bankWords[14],
        bankWords[15], bankWords[16], bankWords[17], bankWords[18], bankWords[19]);
}
