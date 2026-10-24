#include "SkinResources.h"
#include "SkinSlotOwnership.h"
// Private native texture pack, prepared on the initialization thread and
// registered on the game's car-loading thread. Original files stay unchanged.
bool g_skinAssetsReady=false;
skin_resources::Bytes g_skinBundle;
void* g_skinResident=nullptr;
bool g_skinRegistrationAttempted=false;
bool g_externalSkinAllocator=false,g_externalSkinSlotsReported=false,g_externalSkinSlotsVerified=false;
std::vector<unsigned> g_privateSkinKeys;
void PrepareExternalSkinSlots(){
 if(!g_externalSkinAllocator)return;
 auto module=GetModuleHandleW(L"NFSMWUnlimiter.asi");std::uintptr_t begin=0,end=0;
 if(!module||!ResolveModuleImageRange(module,begin,end))return;
 // Validate the actual allocator's configured limit, not just the INI text.
 auto search=reinterpret_cast<const BYTE*>(0x75D29E);
 if(search[0]!=0xE9)return;
 auto code=std::uintptr_t(0x75D2A3)+*reinterpret_cast<const std::int32_t*>(search+1);
 if(code<begin||code+64>end)return;
 unsigned limit=0;
 for(unsigned i=0;i<58;++i){auto op=reinterpret_cast<const BYTE*>(code+i);
  if(op[0]!=0x3B||op[1]!=0x15)continue;
  auto variable=*reinterpret_cast<const std::uint32_t*>(op+2);
  if(variable>=begin&&variable<=end-4){limit=*reinterpret_cast<const unsigned*>(variable);break;}
 }
 if(limit!=20)return;
 // Unused composite slots are indexed but deliberately not resident yet.
 // Their source entries were validated on the initialization worker.
 auto data=*reinterpret_cast<BYTE***>(0x9B37CC);auto count=*reinterpret_cast<unsigned*>(0x9B37D4);
 if(!data||count>64)return;
 unsigned occupied=0;
 for(unsigned i=0;i<count;++i){auto conn=data[i];if(!conn||*reinterpret_cast<unsigned*>(conn)!=0x8B5254)continue;
  const auto state=*reinterpret_cast<unsigned*>(conn+0x28),slot=*reinterpret_cast<unsigned*>(conn+0x68);
  if(state&&slot>=1&&slot<=20)occupied|=1u<<(slot-1);
 }
 auto extra=skin_slots::SupplementalFreeBits(occupied,limit);
 InterlockedOr(reinterpret_cast<volatile LONG*>(0x903690),LONG(extra));
 if(!g_externalSkinSlotsReported){g_externalSkinSlotsReported=true;Log(L"SKIN_CAPACITY external_allocator=Unlimiter verified_slots=20 supplemental_free_bits=%08X occupied=%08X game_bitmap_only=1 third_party_patches=0",extra,occupied);}
}
void RegisterSkinResources(){
 if(g_skinRegistrationAttempted)return;g_skinRegistrationAttempted=true;
 if(g_skinBundle.empty())return;
 g_skinResident=VirtualAlloc(nullptr,g_skinBundle.size(),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!g_skinResident){Log(L"SKIN_CAPACITY inactive=allocation-failed");return;}
 memcpy(g_skinResident,g_skinBundle.data(),g_skinBundle.size());
 // Native registration retains/mutates the backing storage. Retain until
 // process exit, even on partial failure; no per-race release of shared skins.
 if(!reinterpret_cast<int(__cdecl*)(void*)>(0x507B50)(g_skinResident)){Log(L"SKIN_CAPACITY inactive=registration-failed");return;}
 for(auto key:g_privateSkinKeys){
  auto info=reinterpret_cast<BYTE*(__cdecl*)(unsigned,int,int)>(0x503400)(key,0,0);
  if(!info||*reinterpret_cast<unsigned*>(info+36)!=key){Log(L"SKIN_CAPACITY inactive=registration-incomplete");return;}
 }
 g_skinAssetsReady=true;if(!g_externalSkinAllocator)InterlockedOr(reinterpret_cast<volatile LONG*>(0x903690),0xFFFF0);
 Log(L"SKIN_CAPACITY resources=resident-private slots=up-to-20 source=current-game disk_edits=0");
 skin_resources::Bytes().swap(g_skinBundle);
}
bool (__thiscall* g_nativeRenderLoad)(void*,unsigned,int,bool,void*)=nullptr;
void (__thiscall* g_nativeCompositeName)(void*,int)=nullptr;
bool __fastcall SkinRenderLoad(void* self,void*,unsigned world,int usage,bool commit,void* custom){
    RegisterSkinResources();if(g_externalSkinAllocator&&(g_externalSkinSlotsVerified||g_skinAssetsReady))PrepareExternalSkinSlots();
    const bool ready=g_skinAssetsReady;
    const bool result=g_nativeRenderLoad(self,world,usage,commit,custom);
    if(g_loggingEnabled)Log(L"SKIN_LOAD conn=%p slot=%u assets=%u result=%u",self,*reinterpret_cast<unsigned*>(static_cast<BYTE*>(self)+0x68),ready?1u:0u,result?1u:0u);
    return result;
}
void __fastcall SkinCompositeName(void* self,void*,int slot){
    g_nativeCompositeName(self,slot);
    if(g_skinAssetsReady&&!g_externalSkinAllocator&&slot>=5&&slot<=20){
        static_cast<BYTE*>(self)[7]=1;auto hashes=reinterpret_cast<unsigned*>(static_cast<BYTE*>(self)+60);
        unsigned i=0;for(auto family:{"SKIN","WHEEL","SPINNER"})hashes[i++]=skin_resources::Hash(skin_resources::Name(family,unsigned(slot)).c_str());
    }
}
// Entry retains the native precomposite flag and edi=free bitmap. EAX/ECX/EDX
// have the same scratch contract as the replaced search; native unload frees it.
void __declspec(naked) SkinSlotSearch(){
    __asm {
        jnz precomposite
        mov edx,1
    nextSlot:
        lea ecx,[edx-1]
        mov eax,1
        shl eax,cl
        test eax,edi
        jnz found
        inc edx
        cmp edx,5
        jne limitCheck
        cmp byte ptr [g_skinAssetsReady],0
        je none
    limitCheck:
        cmp edx,20
        jle nextSlot
    none:
        push 075D2E3h
        ret
    found:
        push 075D2D6h
        ret
    precomposite:
        push 075D2BBh
        ret
    }
}
bool InstallSkinCapacity(){
    // A known external skin allocator owns both allocation and release. Never
    // combine half of its patch with our half, regardless of ASI load order.
    wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto slash=wcsrchr(exe,L'\\');if(!slash)return false;*(slash+1)=0;
    // The loader may still be between ASI DLLs when this worker runs. File
    // presence also prevents installing just before Unlimiter overwrites it.
    const auto sibling=ModuleSiblingPath(L"NFSMWUnlimiter.asi");
    if(GetModuleHandleW(L"NFSMWUnlimiter.asi")||GetFileAttributesW(sibling.c_str())!=INVALID_FILE_ATTRIBUTES||
       GetFileAttributesW((std::wstring(exe)+L"NFSMWUnlimiter.asi").c_str())!=INVALID_FILE_ATTRIBUTES){
        try{auto source=skin_resources::Read((std::wstring(exe)+L"CARS\\TEXTURES.BIN").c_str());g_externalSkinSlotsVerified=skin_resources::HasExternalSlots(source);if(!g_externalSkinSlotsVerified)g_skinBundle=skin_resources::Build(source,true);}
        catch(const std::exception& error){Log(L"SKIN_CAPACITY delegated=NFSMWUnlimiter supplemental_bits=0 reason=%S",error.what());return false;}
        if(!g_skinBundle.empty())for(auto block:skin_resources::Fields(g_skinBundle,8,g_skinBundle.size()))if(block.tag==0xB3310000)for(auto field:skin_resources::Fields(g_skinBundle,block.at,block.end))if(field.tag==0x33310002)for(auto at=field.at;at<field.end;at+=8)g_privateSkinKeys.push_back(skin_resources::Word(g_skinBundle,at));
        const BYTE guard[]={0x56,0x8B,0xF1,0x8B,0x46,0x44};
        if(memcmp(reinterpret_cast<void*>(0x75D220),guard,sizeof(guard)))return false;
        auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
        auto load=reinterpret_cast<void*>(0x75D220);
        g_externalSkinAllocator=true;
        if(MH_CreateHook(load,SkinRenderLoad,reinterpret_cast<void**>(&g_nativeRenderLoad))!=MH_OK){g_externalSkinAllocator=false;return false;}
        if(MH_EnableHook(load)!=MH_OK){MH_RemoveHook(load);g_externalSkinAllocator=false;return false;}
        Log(L"SKIN_CAPACITY delegated=NFSMWUnlimiter existing_resources_preserved=1 verified_free_bitmap_bridge=1");return true;
    }
    const BYTE loadGuard[]={0x56,0x8B,0xF1,0x8B,0x46,0x44};
    const BYTE nameGuard[]={0x83,0xEC,0x40,0x56,0x57,0x8B,0x7C,0x24,0x4C};
    const BYTE searchGuard[]={0x75,0x1B,0xBA,0x01,0,0,0};
    const BYTE loaderGuard[]={0x53,0x56,0x57,0x8B,0x7C,0x24,0x10,0x8B,0x07};
    if(memcmp(reinterpret_cast<void*>(0x75D220),loadGuard,sizeof(loadGuard))||
       memcmp(reinterpret_cast<void*>(0x747F10),nameGuard,sizeof(nameGuard))||
       memcmp(reinterpret_cast<void*>(0x75D29E),searchGuard,sizeof(searchGuard))||
       memcmp(reinterpret_cast<void*>(0x507B50),loaderGuard,sizeof(loaderGuard))){Log(L"SKIN_CAPACITY inactive=owner-conflict native_preserved=1");return false;}
    try{g_skinBundle=skin_resources::Build(skin_resources::Read((std::wstring(exe)+L"CARS\\TEXTURES.BIN").c_str()));}
    catch(const std::exception& error){Log(L"SKIN_CAPACITY inactive=resource-preparation reason=%S",error.what());return false;}
    for(auto block:skin_resources::Fields(g_skinBundle,8,g_skinBundle.size()))if(block.tag==0xB3310000)for(auto field:skin_resources::Fields(g_skinBundle,block.at,block.end))if(field.tag==0x33310002)for(auto at=field.at;at<field.end;at+=8)g_privateSkinKeys.push_back(skin_resources::Word(g_skinBundle,at));
    auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
    auto load=reinterpret_cast<void*>(0x75D220),name=reinterpret_cast<void*>(0x747F10);
    if(MH_CreateHook(load,SkinRenderLoad,reinterpret_cast<void**>(&g_nativeRenderLoad))!=MH_OK)return false;
    if(MH_CreateHook(name,SkinCompositeName,reinterpret_cast<void**>(&g_nativeCompositeName))!=MH_OK){MH_RemoveHook(load);return false;}
    if(MH_EnableHook(load)!=MH_OK||MH_EnableHook(name)!=MH_OK){MH_DisableHook(load);MH_DisableHook(name);MH_RemoveHook(load);MH_RemoveHook(name);return false;}
    BYTE jump[7]={0xE9,0,0,0,0,0x90,0x90};
    auto delta=std::uint32_t(reinterpret_cast<std::uintptr_t>(&SkinSlotSearch)-0x75D2A3);
    memcpy(jump+1,&delta,4);
    if(!WriteBytes(0x75D29E,jump,sizeof(jump))){MH_DisableHook(load);MH_DisableHook(name);MH_RemoveHook(load);MH_RemoveHook(name);return false;}
    Log(L"SKIN_CAPACITY installed=1 stock_slots=4 additional_slots=16 resource_mode=automatic-private arena_expansion=0");return true;
}
