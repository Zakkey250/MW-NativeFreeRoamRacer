#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace nfrr_prompt {
// Resource/texture adapters in EventLoader forward unrelated packs unchanged.
// Keep calling the public entry point, so the existing hook remains in chain.
inline bool CallSurface(const void* entry, const char* expected, std::size_t size,
                        HMODULE adapter) noexcept {
    MEMORY_BASIC_INFORMATION page{};
    if (!entry || !size || !VirtualQuery(entry, &page, sizeof(page)) ||
        page.State != MEM_COMMIT || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(page.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ||
        reinterpret_cast<std::uintptr_t>(entry) + size >
            reinterpret_cast<std::uintptr_t>(page.BaseAddress) + page.RegionSize) return false;
    if (std::memcmp(entry, expected, size) == 0) return true;
    const auto* bytes = static_cast<const unsigned char*>(entry);
    if (!adapter || size < 5 || bytes[0] != 0xE9) return false;
    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes + 1, sizeof(displacement));
    const auto target = reinterpret_cast<std::uintptr_t>(entry) + 5 + displacement;
    HMODULE owner = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(target), &owner) || owner != adapter) return false;
    if (!VirtualQuery(reinterpret_cast<const void*>(target), &page, sizeof(page)) ||
        page.State != MEM_COMMIT || page.Type != MEM_IMAGE ||
        (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(page.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
    return true;
}
}
