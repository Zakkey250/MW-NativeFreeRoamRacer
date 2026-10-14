#include "../include/MarkerRetry.h"
#include "../include/PromptCallSurface.h"
#undef NDEBUG
#include <cassert>
#include <iostream>

__declspec(noinline) void AdapterTarget() { OutputDebugStringA("test adapter"); }
int main() {
    nfrr_marker::Retry retry;
    unsigned attempts = 0, logs = 0;
    // Six racers, management tick every 50ms, native allocator continuously full.
    for (std::uint64_t now = 0; now < 120000; now += 50)
        for (unsigned racer = 0; racer < 6; ++racer)
            if (retry.Ready(now)) { ++attempts; logs += retry.Fail(now); }
    assert(attempts == 18 && logs == 4);
    assert(retry.nextAttempt == 127000);
    retry.Reset(); // manager replacement/world exit or allocation recovery
    assert(retry.Ready(0) && retry.failures == 0 && retry.nextLog == 0);
    assert(retry.Fail(100)); assert(retry.nextAttempt == 1100);
    assert(!retry.Ready(1099)); assert(retry.Ready(1100));
    assert(!retry.Fail(1100)); assert(retry.nextAttempt == 3100);
    retry.Reset(); assert(retry.Ready(1101));

    auto* code = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(code);
    const char expected[] = "\x53\x55\x56\x57\x8B\x7C\x24\x14";
    std::memcpy(code, expected, 8);
    const auto self = GetModuleHandleW(nullptr);
    assert(nfrr_prompt::CallSurface(code, expected, 8, nullptr));
    code[0] = 0x90;
    assert(!nfrr_prompt::CallSurface(code, expected, 8, self));
    code[0] = 0xE9;
    const auto delta = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(&AdapterTarget) -
        reinterpret_cast<std::uintptr_t>(code) - 5);
    std::memcpy(code + 1, &delta, 4);
    assert(!nfrr_prompt::CallSurface(code, expected, 8, nullptr));
    assert(nfrr_prompt::CallSurface(code, expected, 8, self));
    assert(!nfrr_prompt::CallSurface(code, expected, 8, GetModuleHandleW(L"kernel32.dll")));
    const std::int32_t bad = 0;
    std::memcpy(code + 1, &bad, 4);
    assert(!nfrr_prompt::CallSurface(code, expected, 8, self)); // private trampoline not trusted
    DWORD old = 0;
    assert(VirtualProtect(code,4096,PAGE_NOACCESS,&old));
    assert(!nfrr_prompt::CallSurface(code, expected, 8, self));
    assert(VirtualFree(code,0,MEM_RELEASE));
    std::cout << "PASS 120s/6-racer retry simulation: 18 probes, 4 warnings; reset/recovery; native/known-adapter/foreign/unreadable guards\n";
}
