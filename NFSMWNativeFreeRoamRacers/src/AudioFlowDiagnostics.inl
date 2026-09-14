// alpha.14: observe the verified SoundRacer parameter update, never synthesize a call.
constexpr std::uintptr_t kAudioFlowUpdate = 0x00694700;
constexpr std::size_t kAudioFlowCapacity = 64;
constexpr ULONGLONG kAudioFlowInterval = 250;
// Preserve the native EAX result as well as this/stack arguments; its semantic type is unknown.
using AudioFlowUpdateFn = std::uintptr_t(__thiscall*)(void*, void*);
AudioFlowUpdateFn g_originalAudioFlowUpdate = nullptr;
bool g_audioFlowEnabled = false;
SRWLOCK g_audioFlowLock = SRWLOCK_INIT;
std::atomic<unsigned> g_audioFlowEntered{0}, g_audioFlowReturned{0}, g_audioFlowDropped{0};

struct AudioFlowIdentity {
    std::uintptr_t sound = 0, owner = 0, engine = 0, connection = 0, object = 0;
    bool operator==(const AudioFlowIdentity&) const = default;
};
struct AudioFlowValues {
    unsigned valid = 0;
    std::uintptr_t packet = 0, transmission = 0, attributes = 0;
    float feedbackBefore = kAudioUnknownFloat, feedbackAfter = kAudioUnknownFloat;
    float normalizedBefore = kAudioUnknownFloat, normalizedAfter = kAudioUnknownFloat;
    float soundBefore = kAudioUnknownFloat, soundAfter = kAudioUnknownFloat;
    float lowerRpm = kAudioUnknownFloat, upperRpm = kAudioUnknownFloat;
    float liveRpm = kAudioUnknownFloat, gasAfter = kAudioUnknownFloat;
    float objectFeedbackBefore = kAudioUnknownFloat;
};
struct AudioFlowRecord {
    AudioFlowIdentity identity{};
    unsigned generation = 0, entered = 0, returned = 0, samples = 0, identityChanges = 0;
    ULONGLONG lastCall = 0, lastSample = 0;
    AudioFlowValues values{};
    bool feedbackSeen = false, normalizedSeen = false;
    float feedbackMin = 0, feedbackMax = 0, normalizedMin = 0, normalizedMax = 0;
};
std::array<AudioFlowRecord, kAudioFlowCapacity> g_audioFlowRecords{};
unsigned g_audioFlowGeneration = 0;
struct AudioFlowTicket {
    std::size_t index = kAudioFlowCapacity;
    unsigned generation = 0;
    bool sample = false;
};

bool ValidateAudioFlowSurface() noexcept {
    const std::uint8_t entry[] = {0x56,0x57,0x8B,0x7C,0x24,0x0C,0x57,0x8B,0xF1,
        0xE8,0x52,0xFA,0xFF,0xFF,0x8B,0x86,0x98,0x00,0x00,0x00};
    const std::uint8_t feedback[] = {0xD9,0x87,0x94,0,0,0,0xD8,0x4C,0x24,0x14,
        0xD8,0x44,0x24,0x18,0xD9,0x5E,0x7C,0xD9,0x5F,0x04};
    const std::uint8_t exit[] = {0x5F,0x5E,0xC2,0x04,0x00};
    std::uintptr_t method = 0;
    return AudioCodeMatches(kAudioFlowUpdate, entry) && AudioCodeMatches(0x00694210, feedback) &&
        AudioCodeMatches(0x00694746, exit) &&
        AudioRead(Address(kSoundRacerPrimaryVtable), 0x28, &method) && method == Address(kAudioFlowUpdate);
}

bool ReadAudioFlowIdentity(std::uintptr_t sound, AudioFlowIdentity* result) noexcept {
    AudioFlowIdentity r;
    r.sound = sound;
    std::uintptr_t vt = 0, connectionVT = 0;
    std::uint8_t invalid = 1;
    if (!AudioRead(sound, 0, &vt) || vt != Address(kSoundRacerPrimaryVtable) ||
        !AudioRead(sound, 0x48, &r.owner) || !IsExpectedVehicle(reinterpret_cast<void*>(r.owner)) ||
        !AudioRead(sound, 0x60, &r.engine) || !AudioRead(sound, 0x5C, &r.connection) ||
        !AudioRead(r.connection, 0, &connectionVT) || connectionVT != Address(kCarSoundConnVtable) ||
        !AudioRead(r.connection, 0x0C, &invalid) || invalid ||
        !AudioRead(r.connection, 0x14, &r.object) || !r.object) return false;
    *result = r;
    return true;
}

// Caller holds the short diagnostic lock. No game reads or allocation here.
AudioFlowTicket BeginAudioFlow(const AudioFlowIdentity& identity, ULONGLONG now) noexcept {
    std::size_t slot = kAudioFlowCapacity;
    for (std::size_t i = 0; i < g_audioFlowRecords.size(); ++i)
        if (g_audioFlowRecords[i].generation && g_audioFlowRecords[i].identity == identity) { slot = i; break; }
    if (slot == kAudioFlowCapacity) {
        slot = 0;
        for (std::size_t i = 0; i < g_audioFlowRecords.size(); ++i) {
            if (!g_audioFlowRecords[i].generation) { slot = i; break; }
            if (g_audioFlowRecords[i].lastCall < g_audioFlowRecords[slot].lastCall) slot = i;
        }
        g_audioFlowRecords[slot] = {};
        auto& r = g_audioFlowRecords[slot];
        r.identity = identity;
        if (++g_audioFlowGeneration == 0) ++g_audioFlowGeneration;
        r.generation = g_audioFlowGeneration;
    }
    auto& r = g_audioFlowRecords[slot];
    ++r.entered;
    r.lastCall = now;
    const bool sample = r.entered == 1 || now - r.lastSample >= kAudioFlowInterval;
    if (sample) r.lastSample = now;
    return {slot, r.generation, sample};
}

void ReadAudioFlowBefore(const AudioFlowIdentity& id, std::uintptr_t packet, AudioFlowValues* v) noexcept {
    v->packet = packet;
    if (AudioFiniteRead(packet, 0x94, &v->feedbackBefore)) v->valid |= 1;
    if (AudioFiniteRead(packet, 4, &v->normalizedBefore)) v->valid |= 4;
    if (AudioFiniteRead(id.sound, 0x7C, &v->soundBefore)) v->valid |= 16;
    if (AudioFiniteRead(id.object, 0x248, &v->objectFeedbackBefore)) v->valid |= 1024;
    AudioRead(id.sound, 0x68, &v->transmission);
    AudioRead(id.sound, 0x8C, &v->attributes);
    if (AudioFiniteRead(v->attributes, 0x5C, &v->lowerRpm)) v->valid |= 64;
    if (AudioFiniteRead(v->attributes, 0x58, &v->upperRpm)) v->valid |= 128;
    std::uintptr_t vt = 0;
    if (ReadVerifiedEngineRpm(id.engine, id.owner, &vt, &v->liveRpm)) v->valid |= 256;
}
void ReadAudioFlowAfter(const AudioFlowIdentity& id, AudioFlowValues* v) noexcept {
    if (AudioFiniteRead(v->packet, 0x94, &v->feedbackAfter)) v->valid |= 2;
    if (AudioFiniteRead(v->packet, 4, &v->normalizedAfter)) v->valid |= 8;
    if (AudioFiniteRead(id.sound, 0x7C, &v->soundAfter)) v->valid |= 32;
    if (AudioFiniteRead(v->packet, 8, &v->gasAfter)) v->valid |= 512;
}
void AudioFlowRange(float value, bool* seen, float* low, float* high) noexcept {
    if (!*seen) { *low = *high = value; *seen = true; }
    else { *low = (std::min)(*low, value); *high = (std::max)(*high, value); }
}
void EndAudioFlow(const AudioFlowTicket& ticket, const AudioFlowValues& values, bool sameIdentity) noexcept {
    if (ticket.index >= g_audioFlowRecords.size()) return;
    auto& r = g_audioFlowRecords[ticket.index];
    if (r.generation != ticket.generation) return;
    ++r.returned;
    if (!ticket.sample) return;
    ++r.samples;
    if (!sameIdentity) ++r.identityChanges;
    r.values = values;
    if (values.valid & 1) AudioFlowRange(values.feedbackBefore, &r.feedbackSeen, &r.feedbackMin, &r.feedbackMax);
    if (values.valid & 8) AudioFlowRange(values.normalizedAfter, &r.normalizedSeen, &r.normalizedMin, &r.normalizedMax);
}

std::uintptr_t __fastcall AudioFlowUpdateHook(void* sound, void*, void* packet) {
    g_audioFlowEntered.fetch_add(1, std::memory_order_relaxed);
    AudioFlowIdentity identity;
    AudioFlowTicket ticket;
    AudioFlowValues values;
    if (ReadAudioFlowIdentity(reinterpret_cast<std::uintptr_t>(sound), &identity)) {
        if (TryAcquireSRWLockExclusive(&g_audioFlowLock)) {
            ticket = BeginAudioFlow(identity, GetTickCount64());
            ReleaseSRWLockExclusive(&g_audioFlowLock);
        } else g_audioFlowDropped.fetch_add(1, std::memory_order_relaxed);
    }
    if (ticket.sample) ReadAudioFlowBefore(identity, reinterpret_cast<std::uintptr_t>(packet), &values);
    // Exactly one unmodified native call, including when diagnostics cannot read or lock.
    // Do not catch exceptions raised by the native function.
    const auto nativeResult = g_originalAudioFlowUpdate(sound, packet);
    g_audioFlowReturned.fetch_add(1, std::memory_order_relaxed);
    bool sameIdentity = true;
    if (ticket.sample) {
        AudioFlowIdentity after;
        sameIdentity = ReadAudioFlowIdentity(identity.sound, &after) && after == identity;
        if (sameIdentity) ReadAudioFlowAfter(identity, &values);
    }
    if (ticket.index < kAudioFlowCapacity) {
        if (TryAcquireSRWLockExclusive(&g_audioFlowLock)) {
            EndAudioFlow(ticket, values, sameIdentity);
            ReleaseSRWLockExclusive(&g_audioFlowLock);
        } else g_audioFlowDropped.fetch_add(1, std::memory_order_relaxed);
    }
    return nativeResult;
}

void LogAudioFlow(const AudioSnapshot& s, const char* source, const char* phase, unsigned key) noexcept {
    if (!g_audioFlowEnabled) return;
    const AudioFlowIdentity identity{s.primary, s.owner, s.engine, s.connection, s.object};
    AudioFlowRecord record;
    const char* status = "not-observed";
    if (TryAcquireSRWLockShared(&g_audioFlowLock)) {
        for (const auto& r : g_audioFlowRecords)
            if (r.generation && r.identity == identity) { record = r; status = "observed"; break; }
        ReleaseSRWLockShared(&g_audioFlowLock);
    } else status = "busy";
    const auto& v = record.values;
    const auto now = GetTickCount64();
    Log(LogLevel::Info,
        "AUDIO_FLOW phase=%s source=%s vehicle=%08X key=%08X status=%s generation=%u entered=%u returned=%u samples=%u identityChanges=%u ageMs=%llu sampleAgeMs=%llu totalEntered=%u totalReturned=%u lockDrops=%u valid=%03X packet=%08X transmission=%08X attributes=%08X feedbackBefore=%.5f feedbackAfter=%.5f normalizedBefore=%.5f normalizedAfter=%.5f soundBefore=%.2f soundAfter=%.2f lowerRpm=%.2f upperRpm=%.2f liveRpm=%.2f gasAfter=%.3f feedbackRangeValid=%u feedbackMin=%.5f feedbackMax=%.5f normalizedRangeValid=%u normalizedMin=%.5f normalizedMax=%.5f objectFeedbackBefore=%.5f",
        phase, source, static_cast<unsigned>(s.owner), key, status, record.generation,
        record.entered, record.returned, record.samples, record.identityChanges,
        record.generation ? now - record.lastCall : 0ULL, record.samples ? now - record.lastSample : 0ULL,
        g_audioFlowEntered.load(std::memory_order_relaxed), g_audioFlowReturned.load(std::memory_order_relaxed),
        g_audioFlowDropped.load(std::memory_order_relaxed), v.valid,
        static_cast<unsigned>(v.packet), static_cast<unsigned>(v.transmission), static_cast<unsigned>(v.attributes),
        v.feedbackBefore, v.feedbackAfter, v.normalizedBefore, v.normalizedAfter,
        v.soundBefore, v.soundAfter, v.lowerRpm, v.upperRpm, v.liveRpm, v.gasAfter,
        static_cast<unsigned>(record.feedbackSeen), record.feedbackMin, record.feedbackMax,
        static_cast<unsigned>(record.normalizedSeen), record.normalizedMin, record.normalizedMax, v.objectFeedbackBefore);
}

bool CreateAudioFlowHook() noexcept {
    if (!g_audioDetailEnabled || !ValidateAudioFlowSurface()) return false;
    return MH_CreateHook(reinterpret_cast<void*>(Address(kAudioFlowUpdate)), &AudioFlowUpdateHook,
        reinterpret_cast<void**>(&g_originalAudioFlowUpdate)) == MH_OK;
}
