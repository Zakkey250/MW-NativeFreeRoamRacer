// One small native texture pack, loaded asynchronously on the UI thread.
// No game bank extraction, audio channel allocation or per-frame resource load.
constexpr std::uint32_t kEncounterReadyTexture=0xA808654B;
int g_promptAssetState=0; // 0 unrequested, 1 loading, 2 available, -1 disabled
char g_promptAssetPath[MAX_PATH]{};
void* g_promptResource=nullptr; // native ResourceFile owns a process-lifetime UI pack
ULONGLONG g_promptAssetRequestTick=0,g_promptAssetPollTick=0,g_promptSoundLastTick=0;
bool g_promptSoundPlayed=false,g_promptSoundDisabled=false;
bool g_promptAssetSlow=false;
bool PromptTexturePollDue(ULONGLONG now,ULONGLONG last,bool slowOrReady) noexcept {
    return now>=last&&now-last>=(slowOrReady?2000u:250u);
}
bool ObserveEncounterIconTexture(bool found,ULONGLONG now) noexcept {
    if(g_promptAssetState<1)return false;
    if(found) {
        if(g_promptAssetState!=2)Log(LogLevel::Info,"ENCOUNTER_ICON ready texture=A808654B native-renderer=1 late=%u",unsigned(g_promptAssetSlow));
        g_promptAssetState=2;g_promptAssetSlow=false;return true;
    }
    if(g_promptAssetState==2){g_promptAssetState=1;g_promptAssetRequestTick=now;Log(LogLevel::Info,"ENCOUNTER_ICON lookupMissing=1 repoll=1 allocations=1");}
    if(now>=g_promptAssetRequestTick&&now-g_promptAssetRequestTick>15000&&!g_promptAssetSlow) {
        g_promptAssetSlow=true;Log(LogLevel::Warning,"ENCOUNTER_ICON load delayed frame-retained=1 repollMs=2000 allocations=1 permanentDisable=0");
    }
    return false;
}

bool PromptBytes(std::uintptr_t va,const char* bytes,std::size_t size) noexcept {
    return size<=32 && AudioReadableRange(Address(va),size) &&
        std::memcmp(reinterpret_cast<void*>(Address(va)),bytes,size)==0;
}
bool PrepareEncounterIconPath() noexcept {
    HMODULE self=nullptr;wchar_t path[MAX_PATH]{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&PrepareEncounterIconPath),&self)||!GetModuleFileNameW(self,path,MAX_PATH))return false;
    auto* ext=wcsrchr(path,L'.');if(!ext)return false;
    wcscpy_s(ext,MAX_PATH-(ext-path),L".tpk");
    std::string hash;
    if(!ComputeSha256(path,&hash)||hash!="CA929293186A10867B65729CE78AF89C5F5DE880239A402EC68E35E0169D32D1")return false;
    // The native loader normally receives a short path relative to the EXE.
    // Avoid passing the long installation path to its internal filename fields.
    wchar_t executable[MAX_PATH]{};
    if(!GetModuleFileNameW(nullptr,executable,MAX_PATH))return false;
    auto* slash=wcsrchr(executable,L'\\');if(!slash)return false;
    const auto prefix=static_cast<std::size_t>(slash-executable+1);
    if(_wcsnicmp(path,executable,prefix)!=0||wcslen(path)<=prefix)return false;
    BOOL substituted=FALSE;
    const int length=WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,path+prefix,-1,g_promptAssetPath,MAX_PATH,nullptr,&substituted);
    return length>0&&length<=64&&!substituted;
}
bool EncounterIconReady() noexcept {
    if(g_promptAssetState<0)return false;
    const auto now=GetTickCount64();
    if(!g_promptAssetState) {
        // Fail closed before making native calls. Do not pump synchronous loading.
        if(!PromptBytes(0x0065FD30,"\x6A\xFF\x68\x5B\xA1\x87\x00\x64",8)||
           !PromptBytes(0x006616F0,"\x8B\x44\x24\x04\x56\x8B\xF1",7)||
           !PromptBytes(0x00503400,"\x53\x55\x56\x57\x8B\x7C\x24\x14",8)||
           !PromptBytes(0x00515B90,"\x8B\x44\x24\x04\x85\xC0\x74\x13",8)||!PrepareEncounterIconPath()) {
            g_promptAssetState=-1;Log(LogLevel::Warning,"ENCOUNTER_ICON disabled reason=surface-or-asset-identity frame-retained=1");return false;
        }
        g_promptAssetState=-1; // an exception must never make allocation retry
        __try {
            g_promptResource=reinterpret_cast<void*(__cdecl*)(const char*,int,int,int,int)>(Address(0x0065FD30))(g_promptAssetPath,0,0,0,0);
            if(!g_promptResource)return false;
            reinterpret_cast<void(__thiscall*)(void*,void*,void*)>(Address(0x006616F0))(g_promptResource,nullptr,nullptr);
            g_promptAssetState=1;g_promptAssetRequestTick=now;
            Log(LogLevel::Info,"ENCOUNTER_ICON loading texture=A808654B bytes=17152 allocations=1 async=1");
        } __except(EXCEPTION_EXECUTE_HANDLER) {Log(LogLevel::Warning,"ENCOUNTER_ICON resource exception=%08X",GetExceptionCode());}
        return false;
    }
    // This function is only polled while a challenge notification is wanted.
    // A long gap between notifications must not disable an already-loaded pack.
    if(!PromptTexturePollDue(now,g_promptAssetPollTick,g_promptAssetSlow||g_promptAssetState==2))return g_promptAssetState==2;
    g_promptAssetPollTick=now;
    __try {
        const auto* texture=reinterpret_cast<void*(__cdecl*)(std::uint32_t,int,int)>(Address(0x00503400))(kEncounterReadyTexture,0,0);
        std::uint32_t key=0;
        if(texture&&AudioRead(reinterpret_cast<std::uintptr_t>(texture),0x24,&key)&&key==kEncounterReadyTexture) {
            return ObserveEncounterIconTexture(true,now);
        }
        return ObserveEncounterIconTexture(false,now);
    } __except(EXCEPTION_EXECUTE_HANDLER) {g_promptAssetState=-1;}
    return false;
}
bool PromptSoundDue(ULONGLONG now,ULONGLONG last,bool played) noexcept {
    return !played || (now>=last&&now-last>=2500);
}
bool PlayEncounterPromptSound() noexcept {
    const auto now=GetTickCount64();
    if(g_promptSoundDisabled||!PromptSoundDue(now,g_promptSoundLastTick,g_promptSoundPlayed))return false;
    // Native ShowEvent 0057BD9B..0057BDA6 dispatches UI sound ID 19 here.
    if(!PromptBytes(0x004AE8F0,"\xA1\xF8\x86\x8F\x00\x85\xC0",7)) {g_promptSoundDisabled=true;return false;}
    void *manager=nullptr,*ui=nullptr,*table=nullptr,*play=nullptr;
    if(!AudioRead(reinterpret_cast<void*>(Address(0x00911FA8)),&manager)||!manager||
       !AudioRead(reinterpret_cast<std::uintptr_t>(manager),0x5c,&ui)||!ui||!AudioRead(ui,&table)||!table||
       !AudioRead(reinterpret_cast<std::uintptr_t>(table),0x0c,&play)||!play)return false;
    MEMORY_BASIC_INFORMATION page{};
    if(!VirtualQuery(play,&page,sizeof(page))||page.State!=MEM_COMMIT||
       (page.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
    __try {
        reinterpret_cast<void(__thiscall*)(void*,int)>(Address(0x004AE8F0))(manager,19);
        g_promptSoundLastTick=now;g_promptSoundPlayed=true;
        Log(LogLevel::Info,"ENCOUNTER_PROMPT_SOUND native=19 dispatch=1 cooldownMs=2500");return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {g_promptSoundDisabled=true;Log(LogLevel::Warning,"ENCOUNTER_PROMPT_SOUND disabled exception=%08X",GetExceptionCode());return false;}
}
