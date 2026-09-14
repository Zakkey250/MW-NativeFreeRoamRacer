// alpha.15: two verified producers of audioObject+248. Names remain address-based.
constexpr std::uintptr_t kAudioSourceFunctions[] = {0x004BC420, 0x004C8B40};
using AudioSourceFn = std::uintptr_t(__thiscall*)(void*, std::uintptr_t);
AudioSourceFn g_originalAudioSource[2]{};
bool g_audioSourceEnabled[2]{};
std::atomic<unsigned> g_audioSourceCalls[2]{}, g_audioSourceReturns[2]{}, g_audioSourceDrops{0};
SRWLOCK g_audioSourceLock = SRWLOCK_INIT;
struct AudioSourceIdentity {
    unsigned route = 0;
    std::uintptr_t source = 0, vt = 0, parent = 0, state = 0, object = 0;
    bool operator==(const AudioSourceIdentity&) const = default;
};
struct AudioSourceValues {
    unsigned valid = 0;
    float stateBefore = kAudioUnknownFloat, stateAfter = kAudioUnknownFloat;
    float objectBefore = kAudioUnknownFloat, objectAfter = kAudioUnknownFloat;
    std::uint32_t selection6C = 0, state84 = 0, parent14 = 0, context38 = 0;
    std::uintptr_t context = 0;
};
struct AudioSourceRecord {
    AudioSourceIdentity id{};
    unsigned generation = 0, entered = 0, returned = 0, samples = 0, changed = 0;
    ULONGLONG lastCall = 0, lastSample = 0;
    AudioSourceValues values{};
    bool rangeValid = false;
    float feedbackMin = 0, feedbackMax = 0;
};
std::array<AudioSourceRecord,64> g_audioSourceRecords{};
unsigned g_audioSourceGeneration = 0;

bool AudioSourceType(unsigned route, std::uintptr_t vt) noexcept {
    return (route == 0 && vt == Address(0x00896BF0)) ||
        (route == 1 && (vt == Address(0x00896D2C) || vt == Address(0x008974AC)));
}
bool ReadAudioSourceIdentity(unsigned route, std::uintptr_t source, AudioSourceIdentity* output) noexcept {
    AudioSourceIdentity r;
    r.route = route; r.source = source;
    if (!AudioRead(source,0,&r.vt) || !AudioSourceType(route,r.vt) ||
        !AudioRead(source,0x14,&r.parent) || !r.parent ||
        !AudioRead(source,0x18,&r.state) || !r.state ||
        !AudioRead(r.parent,0x34,&r.object) || !r.object) return false;
    *output = r;
    return true;
}
AudioFlowTicket BeginAudioSource(const AudioSourceIdentity& id, ULONGLONG now) noexcept {
    std::size_t slot = g_audioSourceRecords.size();
    for (std::size_t i=0;i<g_audioSourceRecords.size();++i)
        if (g_audioSourceRecords[i].generation && g_audioSourceRecords[i].id==id) {slot=i;break;}
    if (slot==g_audioSourceRecords.size()) {
        slot=0;
        for (std::size_t i=0;i<g_audioSourceRecords.size();++i) {
            if (!g_audioSourceRecords[i].generation) {slot=i;break;}
            if (g_audioSourceRecords[i].lastCall<g_audioSourceRecords[slot].lastCall) slot=i;
        }
        auto& r=g_audioSourceRecords[slot];
        r={}; r.id=id;
        if (++g_audioSourceGeneration==0) ++g_audioSourceGeneration;
        r.generation=g_audioSourceGeneration;
    }
    auto& r=g_audioSourceRecords[slot];
    ++r.entered; r.lastCall=now;
    const bool sample=r.entered==1 || now-r.lastSample>=250;
    if (sample) r.lastSample=now;
    return {slot,r.generation,sample};
}
void ReadAudioSourceBefore(const AudioSourceIdentity& id, AudioSourceValues* v) noexcept {
    if (AudioFiniteRead(id.state,0x74,&v->stateBefore)) v->valid|=1;
    if (AudioFiniteRead(id.object,0x248,&v->objectBefore)) v->valid|=4;
    if (AudioRead(id.state,0x6C,&v->selection6C)) v->valid|=16;
    if (AudioRead(id.state,0x84,&v->state84)) v->valid|=32;
    if (AudioRead(id.parent,0x14,&v->parent14)) v->valid|=64;
    // Only route A has the verified context+38 contract. Route B's +28 can be
    // AAAAAAAA (observed 008974AC variant); it is not a context pointer.
    if (id.route == 0 && id.vt == Address(0x00896BF0) &&
        AudioRead(id.source,0x28,&v->context) &&
        v->context != 0xAAAAAAAA && v->context != 0xCDCDCDCD &&
        v->context != 0xDDDDDDDD && v->context != 0xFEEEFEEE &&
        AudioRead(v->context,0x38,&v->context38)) v->valid|=128;
}
void ReadAudioSourceAfter(const AudioSourceIdentity& id, AudioSourceValues* v) noexcept {
    if (AudioFiniteRead(id.state,0x74,&v->stateAfter)) v->valid|=2;
    if (AudioFiniteRead(id.object,0x248,&v->objectAfter)) v->valid|=8;
}
void EndAudioSource(const AudioFlowTicket& ticket,const AudioSourceValues& values,bool same) noexcept {
    if (ticket.index>=g_audioSourceRecords.size()) return;
    auto& r=g_audioSourceRecords[ticket.index];
    if (r.generation!=ticket.generation) return;
    ++r.returned;
    if (!ticket.sample) return;
    ++r.samples;
    if (!same) ++r.changed;
    r.values=values;
    if (values.valid&8) AudioFlowRange(values.objectAfter,&r.rangeValid,&r.feedbackMin,&r.feedbackMax);
}
std::uintptr_t DispatchAudioSource(unsigned route,void* source,std::uintptr_t rawArg) {
    g_audioSourceCalls[route].fetch_add(1,std::memory_order_relaxed);
    AudioSourceIdentity id;
    AudioFlowTicket ticket;
    AudioSourceValues values;
    if (ReadAudioSourceIdentity(route,reinterpret_cast<std::uintptr_t>(source),&id)) {
        if (TryAcquireSRWLockExclusive(&g_audioSourceLock)) {
            ticket=BeginAudioSource(id,GetTickCount64());
            ReleaseSRWLockExclusive(&g_audioSourceLock);
        } else g_audioSourceDrops.fetch_add(1,std::memory_order_relaxed);
    }
    if (ticket.sample) ReadAudioSourceBefore(id,&values);
    const auto result=g_originalAudioSource[route](source,rawArg);
    g_audioSourceReturns[route].fetch_add(1,std::memory_order_relaxed);
    bool same=true;
    if (ticket.sample) {
        AudioSourceIdentity after;
        same=ReadAudioSourceIdentity(route,id.source,&after) && after==id;
        if (same) ReadAudioSourceAfter(id,&values);
    }
    if (ticket.index<g_audioSourceRecords.size()) {
        if (TryAcquireSRWLockExclusive(&g_audioSourceLock)) {
            EndAudioSource(ticket,values,same);
            ReleaseSRWLockExclusive(&g_audioSourceLock);
        } else g_audioSourceDrops.fetch_add(1,std::memory_order_relaxed);
    }
    return result;
}
std::uintptr_t __fastcall AudioSourceAHook(void* source,void*,std::uintptr_t arg) {return DispatchAudioSource(0,source,arg);}
std::uintptr_t __fastcall AudioSourceBHook(void* source,void*,std::uintptr_t arg) {return DispatchAudioSource(1,source,arg);}

bool ValidateAudioSourceSurface(unsigned route) noexcept {
    const std::uint8_t ctorCopy[]={0x8B,0x4C,0x24,0x20,0x89,0x9E,0xDC,0,0,0,0x89,0x8E,0x94,0,0,0};
    const std::uint8_t supplier[]={0x8B,0x80,0x48,0x02,0,0,0x50,0x8D,0x4C,0x24,0x14,0xE8,0x9E,0xCD,0xFF,0xFF};
    if (!AudioCodeMatches(0x004AE463,ctorCopy) || !AudioCodeMatches(0x004B1642,supplier)) return false;
    const std::uint8_t entryA[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x74,0x53,0x56,0x8B,0xF1};
    const std::uint8_t entryB[]={0x53,0x8B,0x5C,0x24,0x08,0x56,0x8B,0xF1,0x53,0x8D,0x8E,0xD4,0,0,0};
    const std::uint8_t writerA[]={0x8B,0x56,0x14,0x8B,0x4E,0x18,0x8B,0x42,0x34,0x8B,0x49,0x74,0x89,0x88,0x48,0x02,0,0};
    const std::uint8_t writerB[]={0x8B,0x4E,0x18,0x8B,0x56,0x14,0x8B,0x42,0x34,0x8B,0x49,0x74,0x5E,0x89,0x88,0x48,0x02,0,0};
    const std::uint8_t exitA[]={0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xC2,4,0};
    const std::uint8_t exitB[]={0x5B,0xC2,4,0};
    std::uintptr_t method=0, alternate=0;
    if (route==0) return AudioCodeMatches(kAudioSourceFunctions[0],entryA) &&
        AudioCodeMatches(0x004BC5C6,writerA) && AudioCodeMatches(0x004BC691,exitA) &&
        AudioRead(Address(0x00896BF0),0x24,&method) && method==Address(kAudioSourceFunctions[0]);
    return route==1 && AudioCodeMatches(kAudioSourceFunctions[1],entryB) &&
        AudioCodeMatches(0x004C8CB4,writerB) && AudioCodeMatches(0x004C8CC7,exitB) &&
        AudioRead(Address(0x00896D2C),0x24,&method) && method==Address(kAudioSourceFunctions[1]) &&
        AudioRead(Address(0x008974AC),0x24,&alternate) && alternate==method;
}
void InstallAudioSourceHooks() noexcept {
    for (unsigned route=0;route<2;++route) {
        void* target=reinterpret_cast<void*>(Address(kAudioSourceFunctions[route]));
        if (g_audioFlowEnabled && ValidateAudioSourceSurface(route) &&
            MH_CreateHook(target,route==0?&AudioSourceAHook:&AudioSourceBHook,
                reinterpret_cast<void**>(&g_originalAudioSource[route]))==MH_OK) {
            g_audioSourceEnabled[route]=MH_EnableHook(target)==MH_OK;
            if (!g_audioSourceEnabled[route]) MH_RemoveHook(target);
        }
        Log(LogLevel::Info,"AUDIO_SOURCE_DIAGNOSTICS route=%u enabled=%u hook=%08X gameStateWrites=0 extraNativeCalls=0 capacity=64 captureIntervalMs=250",
            route,static_cast<unsigned>(g_audioSourceEnabled[route]),static_cast<unsigned>(kAudioSourceFunctions[route]));
    }
}
void LogAudioSource(const AudioSnapshot& s,const char* source,const char* phase,unsigned key) noexcept {
    for (unsigned route=0;route<2;++route) {
        if (!g_audioSourceEnabled[route]) continue;
        AudioSourceRecord record;
        const char* status="not-observed";
        if (TryAcquireSRWLockShared(&g_audioSourceLock)) {
            for (const auto& r:g_audioSourceRecords)
                if (r.generation && r.id.route==route && r.id.object==s.object &&
                    (!record.generation || r.lastCall>record.lastCall)) record=r;
            ReleaseSRWLockShared(&g_audioSourceLock);
            if (record.generation) status=GetTickCount64()-record.lastCall<=1000?"recent":"stale";
        } else status="busy";
        const auto& v=record.values;
        Log(LogLevel::Info,"AUDIO_SOURCE phase=%s source=%s vehicle=%08X key=%08X object=%08X route=%u status=%s producer=%08X producerVT=%08X parent=%08X state=%08X generation=%u entered=%u returned=%u samples=%u changed=%u ageMs=%llu totalEntered=%u totalReturned=%u lockDrops=%u valid=%02X stateBefore=%.5f stateAfter=%.5f objectBefore=%.5f objectAfter=%.5f selection6C=%08X state84=%08X parent14=%08X context=%08X context38=%08X rangeValid=%u feedbackMin=%.5f feedbackMax=%.5f",
            phase,source,static_cast<unsigned>(s.owner),key,static_cast<unsigned>(s.object),route,status,
            static_cast<unsigned>(record.id.source),static_cast<unsigned>(record.id.vt),static_cast<unsigned>(record.id.parent),static_cast<unsigned>(record.id.state),
            record.generation,record.entered,record.returned,record.samples,record.changed,
            record.generation?GetTickCount64()-record.lastCall:0ULL,
            g_audioSourceCalls[route].load(std::memory_order_relaxed),g_audioSourceReturns[route].load(std::memory_order_relaxed),g_audioSourceDrops.load(std::memory_order_relaxed),
            v.valid,v.stateBefore,v.stateAfter,v.objectBefore,v.objectAfter,v.selection6C,v.state84,v.parent14,static_cast<unsigned>(v.context),v.context38,
            static_cast<unsigned>(record.rangeValid),record.feedbackMin,record.feedbackMax);
    }
}
