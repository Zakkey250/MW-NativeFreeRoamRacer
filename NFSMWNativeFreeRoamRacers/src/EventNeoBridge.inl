// NFRR supplies vehicle validation and its normal frame callback. Race
// controller state, input hooks and engine-output ownership belong to MWEL.
static unsigned EventNeoQuery(const char* name,void* object) noexcept {
    auto module=GetModuleHandleW(L"MWEventLoader.asi");
    using Query=unsigned(__cdecl*)(void*);
    auto query=module?reinterpret_cast<Query>(GetProcAddress(module,name)):nullptr;
    return query?query(object):0;
}
static bool EventNeoRoadBlocked(void* nav) noexcept {
    return EventNeoQuery("MWEL_NeoRoadBlockedV1",nav)!=0;
}
static bool EventNeoPowerOwned(void* ai) noexcept {
    return EventNeoQuery("MWEL_NeoPowerOwnedV1",ai)!=0;
}
static bool EventNeoLocalPlayer() noexcept {
    auto module=GetModuleHandleW(L"MWEventLoader.asi");
    using Query=unsigned(__cdecl*)();
    auto query=module?reinterpret_cast<Query>(GetProcAddress(module,"MWEL_LocalRaceTestV1")):nullptr;
    return query&&query();
}
