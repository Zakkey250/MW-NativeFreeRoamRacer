#include "CarStreamingPolicy.h"
// Extend the known 115 MiB Unlimiter arena when it is selected. Keep the
// native allocator and give both the world budget and CarLoader the same size.
unsigned g_carStreamingUnits=car_streaming_capacity::Units(car_streaming_capacity::DefaultMiB);
void __declspec(naked) CarStreamingInitialSize(){
    __asm {
        mov esi,dword ptr ds:[008F7EF0h]
        pushfd
        cmp esi,01CC00h
        jne initialSizeReady
        mov esi,dword ptr [g_carStreamingUnits]
    initialSizeReady:
        popfd
        push 00664A2Ah
        ret
    }
}
void __declspec(naked) CarStreamingReloadSize(){
    __asm {
        mov eax,dword ptr ds:[008F7EF0h]
        pushfd
        cmp eax,01CC00h
        jne reloadSizeReady
        mov eax,dword ptr [g_carStreamingUnits]
    reloadSizeReady:
        popfd
        push 00666EC6h
        ret
    }
}
void InstallCarStreamingCapacity(const std::wstring& ini){
    const unsigned requested=GetPrivateProfileIntW(L"VehicleResources",L"RpdStreamingPoolMiB",car_streaming_capacity::DefaultMiB,ini.c_str());
    const unsigned selected=car_streaming_capacity::SelectMiB(requested);
    g_carStreamingUnits=car_streaming_capacity::Units(selected);
    if(selected!=requested)Log(L"CAR_STREAMING_CAPACITY invalid_mib=%u fallback_mib=%u",requested,selected);
    const auto image=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
    if(!(nt->FileHeader.Characteristics&IMAGE_FILE_LARGE_ADDRESS_AWARE))return;
    constexpr BYTE initialBytes[]={0x8B,0x35,0xF0,0x7E,0x8F,0,0x8B,0x0D,0x60,0x60,0x92,0,0xC1,0xE6,0x0A};
    constexpr BYTE reloadBytes[]={0xA1,0xF0,0x7E,0x8F,0,0x0F,0x94,0xC3,0x84,0xDB};
    auto initial=reinterpret_cast<void*>(0x664A24);
    auto reload=reinterpret_cast<void*>(0x666EC1);
    if(memcmp(initial,initialBytes,sizeof(initialBytes))||memcmp(reload,reloadBytes,sizeof(reloadBytes))){
        Log(L"CAR_STREAMING_CAPACITY inactive=owner-conflict native_preserved=1");
        return;
    }
    const auto init=MH_Initialize();
    if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return;
    if(MH_CreateHook(initial,CarStreamingInitialSize,nullptr)!=MH_OK)return;
    if(MH_CreateHook(reload,CarStreamingReloadSize,nullptr)!=MH_OK){
        MH_RemoveHook(initial);
        return;
    }
    if(MH_QueueEnableHook(initial)==MH_OK&&MH_QueueEnableHook(reload)==MH_OK&&MH_ApplyQueued()==MH_OK){
        Log(L"CAR_STREAMING_CAPACITY installed=1 input_mib=115 expanded_mib=%u native_budget_and_reload=1",selected);
        return;
    }
    MH_DisableHook(initial);MH_DisableHook(reload);
    MH_RemoveHook(initial);MH_RemoveHook(reload);
}
