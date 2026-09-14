// Read-only bridge to the active game's AudioSettings, also used by the native
// speech mixer at 004B5A7E. Worker threads never dereference game-owned objects.
std::atomic<float> g_encounterGameVoiceGain{0.0f};
std::atomic<bool> g_encounterGameVoiceValid{false};
bool g_encounterVolumeSurface=false;
static_assert(offsetof(NFSPluginSDK::MW05::AudioSettings,MasterVol)==0);
static_assert(offsetof(NFSPluginSDK::MW05::AudioSettings,SpeechVol)==4);

bool ValidateEncounterVolumeSurface() noexcept {
    const unsigned char chain[]={0x8B,0x15,0xA8,0x1F,0x91,0,0x8B,0x42,0x24,0xD9,0x40,4,0xD8,8};
    const unsigned char mastered[]={0xD9,0x41,4,0xD8,9,0xC3};
    return std::memcmp(reinterpret_cast<void*>(Address(0x004B5A7E)),chain,sizeof(chain))==0&&
        std::memcmp(reinterpret_cast<void*>(Address(0x004AC280)),mastered,sizeof(mastered))==0;
}
bool ReadEncounterGameVoiceGain(float& gain) noexcept {
    void* manager=nullptr;void* settings=nullptr;float master=0,speech=0;
    if(!g_encounterVolumeSurface||!AudioRead(reinterpret_cast<void*>(Address(0x00911FA8)),&manager)||!manager||
        !AudioRead(static_cast<unsigned char*>(manager)+0x24,&settings)||!settings||
        !AudioRead(settings,&master)||!AudioRead(static_cast<unsigned char*>(settings)+4,&speech)||
        !std::isfinite(master)||!std::isfinite(speech)||master<0||master>1||speech<0||speech>1) return false;
    gain=master*speech;return true;
}
void UpdateEncounterVoiceVolume() noexcept {
    static ULONGLONG next=0;const auto now=GetTickCount64();
    if(now<next) return;next=now+100;
    float gain=1;const bool valid=ReadEncounterGameVoiceGain(gain);
    // Keep the last known setting during transient manager teardown; before the
    // first valid setting stay silent instead of unexpectedly playing at 100%.
    if(valid) g_encounterGameVoiceGain.store(gain,std::memory_order_relaxed);
    g_encounterGameVoiceValid.store(valid,std::memory_order_release);
}
void ScaleEncounterPcm(std::vector<unsigned char>& bytes,const encounter_wave::Pcm& pcm,float gain) noexcept {
    if(pcm.offset>bytes.size()||pcm.bytes>bytes.size()-pcm.offset) return;
    gain=std::isfinite(gain)?std::clamp(gain,0.0f,1.0f):1.0f;
    if(gain==1) return;
    for(std::size_t i=pcm.offset;i+1<pcm.offset+pcm.bytes;i+=2) {
        std::int16_t sample=0;std::memcpy(&sample,bytes.data()+i,2);
        sample=static_cast<std::int16_t>(std::lround(float(sample)*gain));
        std::memcpy(bytes.data()+i,&sample,2);
    }
}
