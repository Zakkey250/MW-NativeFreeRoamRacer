namespace native_freeroam { namespace {
unsigned diagnosticAvCount = 0, diagnosticGuardCount = 0;
LONG CALLBACK CountDiagnosticExceptions(PEXCEPTION_POINTERS info) {
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) ++diagnosticAvCount;
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_GUARD_PAGE) ++diagnosticGuardCount;
    return EXCEPTION_CONTINUE_SEARCH;
}
std::array<std::array<std::uint8_t, 0x80>, 7> frameFixture{};
unsigned frameAnimations = 0;
void* frameAnimatedNode = nullptr;
std::uint32_t frameAnimation = 0;
void* __cdecl FindFrameFixture(void*, std::uint32_t hash) {
    for (auto& node : frameFixture) {
        std::uint32_t actual = 0; std::memcpy(&actual, node.data() + 0x10, 4);
        if (actual == hash) return node.data();
    }
    return nullptr;
}
void __cdecl AnimateFrameFixture(void* node, std::uint32_t hash, bool) {
    ++frameAnimations; frameAnimatedNode = node; frameAnimation = hash;
}
void __cdecl SetFrameFixtureTexture(void* node,std::uint32_t hash) {
    std::memcpy(static_cast<std::uint8_t*>(node)+0x24,&hash,4);
}
void __fastcall FrameNativePriority(void* widget, void*, void*) {
    const std::uint32_t event = 42;
    std::memcpy(static_cast<std::uint8_t*>(widget) + 0x48, &event, 4);
}
} }
