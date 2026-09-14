// One independent, non-spatial PCM output. Never touches the native engine pool,
// PlaySound's process-global channel, game-owned WAVs, or a vehicle audio source.
#include "EncounterVolume.inl"
struct EncounterVoiceState {
    std::array<std::array<std::vector<std::wstring>,3>,100> clips;
    std::vector<int> actors;
    SRWLOCK lock=SRWLOCK_INIT;
    HANDLE wake=nullptr;
    std::atomic<bool> stop{false};
    std::atomic<unsigned> revision{0};
    int pendingActor=-1, pendingStage=0;
    std::mt19937 random{GetTickCount()};
};
EncounterVoiceState* g_voice=nullptr; // Process-lifetime pinned worker state.

bool ReadEncounterWave(const std::wstring& path, std::vector<unsigned char>& bytes,
                       encounter_wave::Pcm& pcm) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{}; DWORD got=0;
    bool ok=GetFileSizeEx(file,&size) && size.QuadPart>=44 && size.QuadPart<=8*1024*1024;
    if(ok) {
        bytes.resize(static_cast<std::size_t>(size.QuadPart));
        ok=ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr) && got==bytes.size();
    }
    CloseHandle(file);
    return ok && encounter_wave::Parse(bytes,pcm);
}

void PlayEncounterWave(EncounterVoiceState& state, const std::wstring& path, unsigned revision) {
    std::vector<unsigned char> bytes; encounter_wave::Pcm pcm{};
    if(!ReadEncounterWave(path,bytes,pcm)) {
        Log(LogLevel::Warning,"ENCOUNTER_VOICE invalid PCM file=%ls",path.c_str());return;
    }
    if(state.stop || revision!=state.revision) return;
    const bool gameValid=g_encounterGameVoiceValid.load(std::memory_order_acquire);
    const float gain=g_encounterGameVoiceGain.load(std::memory_order_relaxed);
    ScaleEncounterPcm(bytes,pcm,gain); // private PCM only; never change device/process volume
    Log(LogLevel::Info,"ENCOUNTER_VOICE_VOLUME source=game-master-speech gameValid=%u effective=%.3f settingIni=unused",
        unsigned(gameValid),gain);
    if(gain==0) return;
    WAVEFORMATEX format{};
    format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=static_cast<WORD>(pcm.channels);
    format.nSamplesPerSec=pcm.rate;format.wBitsPerSample=16;
    format.nBlockAlign=static_cast<WORD>(pcm.channels*2);format.nAvgBytesPerSec=pcm.rate*format.nBlockAlign;
    HWAVEOUT output=nullptr;
    HANDLE done=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!done) return;
    MMRESULT result=waveOutOpen(&output,WAVE_MAPPER,&format,reinterpret_cast<DWORD_PTR>(done),0,CALLBACK_EVENT);
    if(result==MMSYSERR_NOERROR) {
        WAVEHDR header{};header.lpData=reinterpret_cast<char*>(bytes.data()+pcm.offset);
        header.dwBufferLength=static_cast<DWORD>(pcm.bytes);
        result=waveOutPrepareHeader(output,&header,sizeof(header));
        const bool prepared=result==MMSYSERR_NOERROR;
        if(prepared) result=waveOutWrite(output,&header,sizeof(header));
        if(result==MMSYSERR_NOERROR) {
            Log(LogLevel::Info,"ENCOUNTER_VOICE playing spatial=0 nativeEngineSlots=0 file=%ls",path.c_str());
            const auto deadline=GetTickCount64()+65000;
            while(!(header.dwFlags&WHDR_DONE) && !state.stop && revision==state.revision && GetTickCount64()<deadline)
                WaitForSingleObject(done,50);
        }
        // Reset returns submitted buffers before either header or PCM is released.
        waveOutReset(output);
        if(prepared) waveOutUnprepareHeader(output,&header,sizeof(header));
        waveOutClose(output);
    }
    CloseHandle(done);
    if(result!=MMSYSERR_NOERROR && !state.stop)
        Log(LogLevel::Warning,"ENCOUNTER_VOICE waveOut error=%u battleUnaffected=1",unsigned(result));
}

DWORD WINAPI EncounterVoiceWorker(void* parameter) noexcept {
    auto& state=*static_cast<EncounterVoiceState*>(parameter);
    while(!state.stop) {
        WaitForSingleObject(state.wake,INFINITE);
        if(state.stop) break;
        AcquireSRWLockExclusive(&state.lock);
        const int actor=state.pendingActor,stage=state.pendingStage;
        const auto revision=state.revision.load();state.pendingActor=-1;
        ReleaseSRWLockExclusive(&state.lock);
        if(actor<0) continue;
        try {
            const auto& list=state.clips[actor][stage];
            if(!list.empty()) PlayEncounterWave(state,list[std::uniform_int_distribution<std::size_t>(0,list.size()-1)(state.random)],revision);
        } catch(...) {
            if(!state.stop) Log(LogLevel::Warning,"ENCOUNTER_VOICE worker exception; clip skipped");
        }
    }
    return 0;
}

void QueueEncounterVoice(int actor,int stage) noexcept {
    if(!g_voice || g_voice->stop) return;
    AcquireSRWLockExclusive(&g_voice->lock);
    g_voice->pendingActor=actor;g_voice->pendingStage=stage;
    ++g_voice->revision;
    ReleaseSRWLockExclusive(&g_voice->lock);
    SetEvent(g_voice->wake);
}

void InitializeEncounterVoice() {
    HMODULE self=nullptr;wchar_t path[MAX_PATH]{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&InitializeEncounterVoice),&self) || !GetModuleFileNameW(self,path,MAX_PATH)) return;
    const auto slash=wcsrchr(path,L'\\');if(!slash) return;slash[1]=0;
    const std::wstring root=std::wstring(path)+L"NativeFreeRoamRacers\\encounter\\";
    auto* state=new EncounterVoiceState;
    g_encounterVolumeSurface=ValidateEncounterVolumeSurface();
    UpdateEncounterVoiceVolume();
    const wchar_t* folders[]={L"start",L"player_victory",L"player_defeat"};
    unsigned count=0;
    for(unsigned stage=0;stage<3;++stage) {
        WIN32_FIND_DATAW entry{};
        const auto directory=root+folders[stage]+L"\\";
        HANDLE find=FindFirstFileW((directory+L"*.wav").c_str(),&entry);
        if(find==INVALID_HANDLE_VALUE) continue;
        do {
            const int actor=encounter_wave::Speaker(entry.cFileName);
            if(!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && actor>=0) {
                state->clips[actor][stage].push_back(directory+entry.cFileName);++count;
            }
        } while(FindNextFileW(find,&entry));
        FindClose(find);
    }
    for(int i=0;i<100;++i) {
        const auto& c=state->clips[i];
        if(!c[0].empty()&&!c[1].empty()&&!c[2].empty()) state->actors.push_back(i);
    }
    state->wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HANDLE thread=state->wake?CreateThread(nullptr,0,EncounterVoiceWorker,state,0,nullptr):nullptr;
    if(!thread) {if(state->wake) CloseHandle(state->wake);delete state;return;}
    CloseHandle(thread);g_voice=state;
    Log(LogLevel::Info,"ENCOUNTER_VOICE ready actors=%u files=%u spatial=0 workerIO=1 residentPcmLimit=8MiB",
        unsigned(state->actors.size()),count);
}
