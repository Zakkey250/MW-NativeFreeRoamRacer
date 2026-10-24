#include "../../NFSMWNativeFreeRoamRacers/include/ExecutableTargets.h"
#include <windows.h>
#include <bcrypt.h>
#include <dbghelp.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include "SkinResources.h"

#include "VehicleLimitPlan.h"
#include "PopulationApi.h"
#include "PopulationLease.h"
#include "ResourceApi.h"
#include "../third_party/minhook/include/MinHook.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "dbghelp.lib")

namespace {

using namespace limitbreaker;

HMODULE g_module = nullptr;
bool g_loggingEnabled = false;
StorageBlock* g_storage = nullptr;
void* g_allocation = nullptr;
PVOID g_exceptionHandler = nullptr;
volatile LONG g_exceptionCaptureCount = 0;
volatile LONG g_victimAllocations = 0;
volatile LONG g_victimReuses = 0;
volatile LONG g_victimFrees = 0;
volatile LONG g_victimAllocationFailures = 0;
volatile LONG g_managerAllocations = 0;
volatile LONG g_managerReuses = 0;
volatile LONG g_managerFrees = 0;
volatile LONG g_managerAllocationFailures = 0;
std::uint32_t g_logicalLimit = 0;
wchar_t g_dumpDirectory[MAX_PATH]{};
std::array<std::uintptr_t, 6> g_victimVtable{};
std::array<std::uintptr_t, 6> g_managerVtable{};
std::uintptr_t g_executableImageBegin = 0;
std::uintptr_t g_executableImageEnd = 0;
std::uint32_t g_audioMixerPoolMask = 0;
std::uint32_t g_audioMixerPoolBytes = 0;
std::uint32_t g_audioAllocatorDescriptor = 0;
volatile LONG g_resourceCapabilities = 0;
std::uint32_t g_resourceLogicalLimit = 0;

constexpr wchar_t kVersion[] = L"0.1.3-resource-headroom.5";
constexpr DWORD kVectorInitializationTimeoutMs = 10 * 60 * 1000;

std::wstring ModuleSiblingPath(const wchar_t* name) {
    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(g_module, path, MAX_PATH) == 0) return name;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash == nullptr) return name;
    *(slash + 1) = L'\0';
    return std::wstring(path) + name;
}

void Log(const wchar_t* format, ...) {
    if (!g_loggingEnabled) return;
    wchar_t message[2048]{};
    va_list arguments;
    va_start(arguments, format);
    _vsnwprintf_s(message, _countof(message), _TRUNCATE, format, arguments);
    va_end(arguments);

    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t line[2304]{};
    _snwprintf_s(
        line, _countof(line), _TRUNCATE,
        L"%04u-%02u-%02uT%02u:%02u:%02u.%03u %s\r\n",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
        time.wSecond, time.wMilliseconds, message);

    const std::wstring path = ModuleSiblingPath(L"NFSMWLimitBreaker.log");
    HANDLE file = CreateFileW(
        path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    const int characterCount = static_cast<int>(wcslen(line));
    const int byteCount = WideCharToMultiByte(
        CP_UTF8, 0, line, characterCount, nullptr, 0, nullptr, nullptr);
    if (byteCount <= 0) {
        CloseHandle(file);
        return;
    }
    std::string utf8(static_cast<std::size_t>(byteCount), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, line, characterCount, utf8.data(), byteCount, nullptr, nullptr);
    DWORD written = 0;
    WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(file);
}

bool HashFileSha256(const wchar_t* path, std::string& output) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    HANDLE file = INVALID_HANDLE_VALUE;
    bool result = false;
    DWORD objectLength = 0;
    DWORD hashLength = 0;
    DWORD received = 0;
    std::vector<unsigned char> object;
    std::vector<unsigned char> digest;
    std::vector<unsigned char> buffer(64 * 1024);

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &received, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &received, 0) < 0) {
        goto cleanup;
    }
    object.resize(objectLength);
    digest.resize(hashLength);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectLength, nullptr, 0, 0) < 0) {
        goto cleanup;
    }
    file = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) goto cleanup;
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) {
            goto cleanup;
        }
        if (count == 0) break;
        if (BCryptHashData(hash, buffer.data(), count, 0) < 0) goto cleanup;
    }
    if (BCryptFinishHash(hash, digest.data(), hashLength, 0) < 0) goto cleanup;
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        output.clear();
        output.reserve(digest.size() * 2);
        for (const unsigned char value : digest) {
            output.push_back(digits[value >> 4]);
            output.push_back(digits[value & 0x0F]);
        }
    }
    result = true;

cleanup:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (hash != nullptr) BCryptDestroyHash(hash);
    if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
    return result;
}

bool IsSupportedExecutable(std::wstring& executablePath, std::string& digest) {
    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) return false;
    executablePath = path;
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attributes)) return false;
    const std::uint64_t size =
        (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) |
        attributes.nFileSizeLow;
    if (!nfrr_target::KnownSize(size) ||
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) != 0x00400000u ||
        !HashFileSha256(path, digest)) {
        return false;
    }
    return nfrr_target::Find(size,digest)!=nullptr;
}

template <typename T>
T& At(const std::uintptr_t address) {
    return *reinterpret_cast<T*>(address);
}

template <std::size_t Bytes>
struct alignas(16) ReusableStorageSlot {
    volatile LONG owner;
    std::array<std::uint8_t, Bytes> bytes;
};

template <std::size_t Slots, std::size_t Bytes>
using ReusableStoragePool = std::array<ReusableStorageSlot<Bytes>, Slots>;

ReusableStoragePool<kVictimPoolSlots,
    kStorageCapacity * kVictimRecordSize> g_victimPool{};
ReusableStoragePool<kAIVehicleManagerPoolSlots,
    kStorageCapacity * kPointerRecordSize> g_managerPool{};

template <std::size_t Slots, std::size_t Bytes>
void* AcquireReusableStorage(ReusableStoragePool<Slots, Bytes>& pool,
    void* vectorObject, volatile LONG& acquisitions, volatile LONG& reuses,
    volatile LONG& failures) {
    const LONG owner = static_cast<LONG>(reinterpret_cast<std::uintptr_t>(vectorObject));
    if (owner == 0) {
        InterlockedIncrement(&failures);
        return nullptr;
    }
    for (auto& slot : pool) {
        if (InterlockedCompareExchange(&slot.owner, owner, owner) == owner) {
            const auto liveData = *reinterpret_cast<std::uintptr_t*>(
                static_cast<std::uint8_t*>(vectorObject) + 4);
            if (liveData != reinterpret_cast<std::uintptr_t>(slot.bytes.data())) {
                std::memset(slot.bytes.data(), 0, slot.bytes.size());
            }
            InterlockedIncrement(&reuses);
            return slot.bytes.data();
        }
    }
    for (auto& slot : pool) {
        if (InterlockedCompareExchange(&slot.owner, owner, 0) == 0) {
            std::memset(slot.bytes.data(), 0, slot.bytes.size());
            InterlockedIncrement(&acquisitions);
            return slot.bytes.data();
        }
    }
    InterlockedIncrement(&failures);
    return nullptr;
}

template <std::size_t Slots, std::size_t Bytes>
bool ReleaseReusableStorage(ReusableStoragePool<Slots, Bytes>& pool,
    void* vectorObject, void* storage) {
    const LONG owner = static_cast<LONG>(reinterpret_cast<std::uintptr_t>(vectorObject));
    for (auto& slot : pool) {
        if (slot.bytes.data() == storage &&
            InterlockedCompareExchange(&slot.owner, 0, owner) == owner) {
            return true;
        }
    }
    return false;
}

template <std::size_t Slots, std::size_t Bytes>
bool StorageBelongsToPool(const ReusableStoragePool<Slots, Bytes>& pool,
    const std::uintptr_t storage) {
    for (const auto& slot : pool) {
        if (reinterpret_cast<std::uintptr_t>(slot.bytes.data()) == storage) return true;
    }
    return false;
}

void* __fastcall AllocateVictimVectorStorage(
    void* vectorObject, void*, const std::uint32_t capacity, const std::uint32_t) {
    if (capacity == 0 || capacity > kStorageCapacity) {
        InterlockedIncrement(&g_victimAllocationFailures);
        return nullptr;
    }
    return AcquireReusableStorage(g_victimPool, vectorObject, g_victimAllocations,
        g_victimReuses, g_victimAllocationFailures);
}

void __fastcall FreeVictimVectorStorage(
    void* vectorObject, void*, void* allocation, const std::uint32_t) {
    const auto inlineStorage = reinterpret_cast<std::uintptr_t>(vectorObject) + 16;
    if (allocation != nullptr && reinterpret_cast<std::uintptr_t>(allocation) != inlineStorage) {
        if (ReleaseReusableStorage(g_victimPool, vectorObject, allocation)) {
            InterlockedIncrement(&g_victimFrees);
        }
    }
}

std::uint32_t __fastcall GrowVictimVector(
    void*, void*, const std::uint32_t requiredCapacity) {
    return requiredCapacity <= kStorageCapacity ? kStorageCapacity : requiredCapacity;
}

std::uint32_t __fastcall MaxVictimVectorCapacity(void*, void*) {
    return kStorageCapacity;
}

void* __fastcall AllocateManagerVectorStorage(
    void* vectorObject, void*, const std::uint32_t capacity, const std::uint32_t) {
    if (capacity == 0 || capacity > kStorageCapacity) {
        InterlockedIncrement(&g_managerAllocationFailures);
        return nullptr;
    }
    return AcquireReusableStorage(g_managerPool, vectorObject, g_managerAllocations,
        g_managerReuses, g_managerAllocationFailures);
}

void __fastcall FreeManagerVectorStorage(
    void* vectorObject, void*, void* allocation, const std::uint32_t) {
    const auto inlineStorage = reinterpret_cast<std::uintptr_t>(vectorObject) + 16;
    if (allocation != nullptr && reinterpret_cast<std::uintptr_t>(allocation) != inlineStorage &&
        ReleaseReusableStorage(g_managerPool, vectorObject, allocation)) {
        InterlockedIncrement(&g_managerFrees);
    }
}

std::uint32_t __fastcall GrowManagerVector(
    void*, void*, const std::uint32_t requiredCapacity) {
    return requiredCapacity <= kStorageCapacity ? kStorageCapacity : requiredCapacity;
}

std::uint32_t __fastcall MaxManagerVectorCapacity(void*, void*) {
    return kStorageCapacity;
}

bool ResolveModuleImageRange(HMODULE module, std::uintptr_t& begin,
    std::uintptr_t& end) {
    if (module == nullptr) return false;
    const auto moduleAddress = reinterpret_cast<std::uintptr_t>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleAddress);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        moduleAddress + static_cast<std::uintptr_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.SizeOfImage == 0) {
        return false;
    }
    begin = moduleAddress;
    end = moduleAddress + nt->OptionalHeader.SizeOfImage;
    return end > begin;
}

LONG WINAPI CaptureVehicleLimitException(EXCEPTION_POINTERS* exception) {
    if (exception == nullptr || exception->ExceptionRecord == nullptr ||
        exception->ContextRecord == nullptr || g_storage == nullptr ||
        exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        At<std::uint32_t>(kVehicleCount) < DiagnosticThresholdFor(g_logicalLimit)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const auto exceptionAddress = reinterpret_cast<std::uintptr_t>(
        exception->ExceptionRecord->ExceptionAddress);
    // This handler is diagnostic only. Other ASIs may deliberately use handled
    // first-chance AVs; dumping those exceptions causes visible stalls and does
    // not diagnose the executable-side vehicle-limit patch.
    if (!IsAddressInHalfOpenRange(exceptionAddress, g_executableImageBegin,
            g_executableImageEnd) ||
        InterlockedCompareExchange(&g_exceptionCaptureCount, 1, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    constexpr LONG capture = 1;

    wchar_t dumpPath[MAX_PATH]{};
    _snwprintf_s(dumpPath, _countof(dumpPath), _TRUNCATE,
        L"%sNFSMWLimitBreaker-crash-%lu-%ld.dmp", g_dumpDirectory,
        GetCurrentProcessId(), capture);
    HANDLE file = CreateFileW(dumpPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    BOOL dumped = FALSE;
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION information{};
        information.ThreadId = GetCurrentThreadId();
        information.ExceptionPointers = exception;
        information.ClientPointers = FALSE;
        const auto dumpType = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithDataSegs | MiniDumpWithHandleData |
            MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo);
        dumped = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
            dumpType, &information, nullptr, nullptr);
        CloseHandle(file);
    }

    const ULONG_PTR operation = exception->ExceptionRecord->NumberParameters >= 1
        ? exception->ExceptionRecord->ExceptionInformation[0] : ~ULONG_PTR{0};
    const ULONG_PTR target = exception->ExceptionRecord->NumberParameters >= 2
        ? exception->ExceptionRecord->ExceptionInformation[1] : 0;
    Log(L"EXCEPTION_CAPTURED sequence=%ld code=0x%08lX address=0x%08lX operation=%llu target=0x%08lX eip=0x%08lX eax=0x%08lX ebx=0x%08lX ecx=0x%08lX edx=0x%08lX esi=0x%08lX edi=0x%08lX ebp=0x%08lX esp=0x%08lX primary_count=%lu dumped=%u path=%s",
        capture, exception->ExceptionRecord->ExceptionCode,
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(
            exception->ExceptionRecord->ExceptionAddress)),
        static_cast<unsigned long long>(operation), static_cast<unsigned long>(target),
        exception->ContextRecord->Eip, exception->ContextRecord->Eax,
        exception->ContextRecord->Ebx, exception->ContextRecord->Ecx,
        exception->ContextRecord->Edx, exception->ContextRecord->Esi,
        exception->ContextRecord->Edi, exception->ContextRecord->Ebp,
        exception->ContextRecord->Esp,
        static_cast<unsigned long>(At<std::uint32_t>(kVehicleCount)),
        dumped ? 1u : 0u, dumpPath);
    return EXCEPTION_CONTINUE_SEARCH;
}

void LogRemainingFixedVectorCandidates(
    const wchar_t* milestone, const std::uint32_t primaryCount) {
    constexpr std::uintptr_t scanBegin = 0x008D0000u;
    constexpr std::uintptr_t scanEnd = 0x00970000u;
    std::size_t matches = 0;
    std::uintptr_t regionAddress = scanBegin;
    while (regionAddress < scanEnd) {
        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(reinterpret_cast<const void*>(regionAddress), &information,
                sizeof(information)) == 0) {
            break;
        }
        const std::uintptr_t rawRegionEnd =
            reinterpret_cast<std::uintptr_t>(information.BaseAddress) +
            static_cast<std::uintptr_t>(information.RegionSize);
        const std::uintptr_t regionEnd = rawRegionEnd < scanEnd ? rawRegionEnd : scanEnd;
        const DWORD disallowed = PAGE_NOACCESS | PAGE_GUARD;
        if (information.State == MEM_COMMIT && (information.Protect & disallowed) == 0) {
            for (std::uintptr_t address = regionAddress;
                address + 16 <= regionEnd; address += 4) {
                const auto vtable = At<std::uint32_t>(address);
                const auto data = At<std::uintptr_t>(address + 4);
                const auto capacity = At<std::uint32_t>(address + 8);
                const auto count = At<std::uint32_t>(address + 12);
                if (vtable < 0x00800000u || vtable >= 0x008D0000u ||
                    data != address + 16 || capacity != kStockLimit ||
                    count == 0 || count > 64) {
                    continue;
                }
                ++matches;
                Log(L"FIXED_VECTOR_CANDIDATE object=0x%08lX vtable=0x%08lX data=0x%08lX capacity=%lu count=%lu",
                    static_cast<unsigned long>(address), static_cast<unsigned long>(vtable),
                    static_cast<unsigned long>(data), static_cast<unsigned long>(capacity),
                    static_cast<unsigned long>(count));
            }
        }
        if (regionEnd <= regionAddress) break;
        regionAddress = regionEnd;
    }
    Log(L"FIXED_VECTOR_SCAN_COMPLETE milestone=%s primary_count=%lu matches=%zu range=0x%08lX..0x%08lX",
        milestone, static_cast<unsigned long>(primaryCount), matches,
        static_cast<unsigned long>(scanBegin), static_cast<unsigned long>(scanEnd));
}

bool IsReadableProcessAddress(const std::uintptr_t address) {
    if (address == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &information,
            sizeof(information)) == 0 || information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xFFu;
    return protection == PAGE_READONLY || protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

void LogPrimaryPointerValidation(
    const wchar_t* milestone, const std::uint32_t primaryCount) {
    const std::uint32_t checked = primaryCount < kStorageCapacity
        ? primaryCount : kStorageCapacity;
    std::uint32_t invalid = 0;
    for (std::uint32_t index = 0; index < checked; ++index) {
        const std::uintptr_t pointer = g_storage->vectors[0].pointers[index];
        if (IsReadableProcessAddress(pointer)) continue;
        if (invalid < 4) {
            Log(L"POINTER_VALIDATION_FAILURE milestone=%s index=%lu pointer=0x%08lX",
                milestone, static_cast<unsigned long>(index),
                static_cast<unsigned long>(pointer));
        }
        ++invalid;
    }
    Log(L"POINTER_VALIDATION_COMPLETE milestone=%s primary_count=%lu checked=%lu invalid=%lu",
        milestone, static_cast<unsigned long>(primaryCount),
        static_cast<unsigned long>(checked), static_cast<unsigned long>(invalid));
}

bool BytesEqual(const std::uintptr_t address, const std::uint8_t* expected, const std::size_t size) {
    return std::memcmp(reinterpret_cast<const void*>(address), expected, size) == 0;
}

bool WriteBytes(const std::uintptr_t address, const void* source, const std::size_t size) {
    DWORD oldProtection = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtection)) {
        return false;
    }
    std::memcpy(reinterpret_cast<void*>(address), source, size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);
    DWORD ignored = 0;
    return VirtualProtect(reinterpret_cast<void*>(address), size, oldProtection, &ignored) != FALSE;
}

constexpr std::uintptr_t kAudioAllocatorDescriptor = 0x0090E0B8u;
constexpr std::uintptr_t kAllocatorNameOffset = 0x00u;
constexpr std::uintptr_t kAllocatorSizeOffset = 0x20u;
constexpr std::uintptr_t kAllocatorAllocatedOffset = 0x30u;
constexpr std::uintptr_t kAllocatorPeakOffset = 0x34u;
constexpr std::uintptr_t kAllocatorFreeOffset = 0x38u;
constexpr std::uintptr_t kAllocatorMinFreeOffset = 0x3Cu;
constexpr std::uint32_t kExpectedAudioAllocatorName = 0x00896800u;

std::uintptr_t FindAudioAllocatorDescriptor() {
    return At<std::uint32_t>(
        kAudioAllocatorDescriptor + kAllocatorNameOffset) ==
            kExpectedAudioAllocatorName ? kAudioAllocatorDescriptor : 0;
}

bool InstallEngineAudioExpansion() {
    constexpr auto poolMiB = kExpandedAudioMixerPoolMiB;
    constexpr auto requestedMask = AudioMixerPoolMaskForMiB(poolMiB);
    constexpr auto requestedBytes = AudioMixerPoolBytesForMiB(poolMiB);
    const auto opcode = At<std::uint8_t>(kAudioMixerPoolMaskInstruction);
    const auto ownerMask = At<std::uint32_t>(kAudioMixerPoolMaskOperand);
    const auto liveArena = At<std::uintptr_t>(kAudioMixerArenaGlobal);
    const auto liveBase = At<std::uintptr_t>(kAudioMixerPoolBaseGlobal);
    const auto liveBytes = At<std::uint32_t>(kAudioMixerPoolSizeGlobal);
    if (!IsPreinitializedAudioArena(liveArena, liveBase, liveBytes)) {
        Log(L"AUDIO_FEATURE_INACTIVE reason=audio-mixer-pool-too-late arena=0x%08lX base=0x%08lX bytes=0x%08lX vehicle_feature_continues=1",
            static_cast<unsigned long>(liveArena),
            static_cast<unsigned long>(liveBase),
            static_cast<unsigned long>(liveBytes));
        return false;
    }
    if (opcode != kExpectedAudioMixerPoolMaskOpcode ||
        !IsKnownAudioMixerPoolOwnerValue(ownerMask, requestedMask)) {
        Log(L"AUDIO_FEATURE_INACTIVE reason=patch-conflict site=audio-mixer-pool address=0x%08lX opcode=0x%02X live_mask=0x%08lX stock_mask=0x%08lX requested_mask=0x%08lX vehicle_feature_continues=1",
            static_cast<unsigned long>(kAudioMixerPoolMaskInstruction),
            static_cast<unsigned>(opcode),
            static_cast<unsigned long>(ownerMask),
            static_cast<unsigned long>(kStockAudioMixerPoolMask),
            static_cast<unsigned long>(requestedMask));
        return false;
    }
    if (ownerMask != requestedMask &&
        (!WriteBytes(kAudioMixerPoolMaskOperand, &requestedMask,
                sizeof(requestedMask)) ||
            At<std::uint32_t>(kAudioMixerPoolMaskOperand) != requestedMask)) {
        if (ownerMask == kStockAudioMixerPoolMask &&
            IsPreinitializedAudioArena(
                At<std::uintptr_t>(kAudioMixerArenaGlobal),
                At<std::uintptr_t>(kAudioMixerPoolBaseGlobal),
                At<std::uint32_t>(kAudioMixerPoolSizeGlobal))) {
            WriteBytes(kAudioMixerPoolMaskOperand, &ownerMask,
                sizeof(ownerMask));
        }
        Log(L"AUDIO_FEATURE_INACTIVE reason=patch-write-failed site=audio-mixer-pool address=0x%08lX transaction_rollback_attempted=1 vehicle_feature_continues=1",
            static_cast<unsigned long>(kAudioMixerPoolMaskOperand));
        return false;
    }
    g_audioMixerPoolMask = requestedMask;
    g_audioMixerPoolBytes = requestedBytes;
    Log(L"AUDIO_PATCH_INSTALLED feature=LBEngineAudioExpansion site=audio-mixer-pool instruction=0x%08lX old_mask=0x%08lX new_mask=0x%08lX requested_mib=%lu expected_bytes=0x%08lX consumer_slots_unchanged=1 restart_required=1",
        static_cast<unsigned long>(kAudioMixerPoolMaskInstruction),
        static_cast<unsigned long>(ownerMask),
        static_cast<unsigned long>(requestedMask),
        static_cast<unsigned long>(poolMiB),
        static_cast<unsigned long>(requestedBytes));
    return true;
}

bool VerifyEngineAudioExpansion() {
    const ULONGLONG deadline = GetTickCount64() + 10000;
    std::uintptr_t arena = 0;
    std::uintptr_t base = 0;
    std::uint32_t bytes = 0;
    do {
        arena = At<std::uintptr_t>(kAudioMixerArenaGlobal);
        base = At<std::uintptr_t>(kAudioMixerPoolBaseGlobal);
        bytes = At<std::uint32_t>(kAudioMixerPoolSizeGlobal);
        if (arena != 0 && base != 0 && bytes != 0) break;
        Sleep(1);
    } while (GetTickCount64() < deadline);

    const auto descriptor = FindAudioAllocatorDescriptor();
    const auto descriptorBytes = descriptor == 0 ? 0u :
        At<std::uint32_t>(descriptor + kAllocatorSizeOffset);
    if (arena == 0 || base == 0 || bytes != g_audioMixerPoolBytes ||
        descriptor == 0 || descriptorBytes != g_audioMixerPoolBytes) {
        Log(L"AUDIO_FEATURE_INACTIVE reason=runtime-verification-failed arena=0x%08lX base=0x%08lX live_bytes=0x%08lX expected_bytes=0x%08lX descriptor=0x%08lX descriptor_bytes=0x%08lX capability_published=0 vehicle_feature_continues=1",
            static_cast<unsigned long>(arena),
            static_cast<unsigned long>(base),
            static_cast<unsigned long>(bytes),
            static_cast<unsigned long>(g_audioMixerPoolBytes),
            static_cast<unsigned long>(descriptor),
            static_cast<unsigned long>(descriptorBytes));
        return false;
    }
    g_audioAllocatorDescriptor = static_cast<std::uint32_t>(descriptor);
    InterlockedExchange(&g_resourceCapabilities,
        nfsmw_limitbreaker::ResourceAudioMixerPoolExpanded);
    Log(L"AUDIO_FEATURE_ACTIVE feature=LBEngineAudioExpansion arena=0x%08lX base=0x%08lX bytes=0x%08lX mib=%lu allocated=0x%08lX peak=0x%08lX free=0x%08lX min_free=0x%08lX capability=0x%08X restart_required=1",
        static_cast<unsigned long>(arena),
        static_cast<unsigned long>(base),
        static_cast<unsigned long>(bytes),
        static_cast<unsigned long>(kExpandedAudioMixerPoolMiB),
        static_cast<unsigned long>(At<std::uint32_t>(
            descriptor + kAllocatorAllocatedOffset)),
        static_cast<unsigned long>(At<std::uint32_t>(
            descriptor + kAllocatorPeakOffset)),
        static_cast<unsigned long>(At<std::uint32_t>(
            descriptor + kAllocatorFreeOffset)),
        static_cast<unsigned long>(At<std::uint32_t>(
            descriptor + kAllocatorMinFreeOffset)),
        static_cast<unsigned>(nfsmw_limitbreaker::ResourceAudioMixerPoolExpanded));
    return true;
}

struct AudioMonitorState {
    bool previousFault = false;
    std::uint32_t previousMinFree = 0xFFFFFFFFu;
    ULONGLONG nextCheckTick = 0;
};

void MonitorEngineAudioExpansion(AudioMonitorState& state) {
    const ULONGLONG now = GetTickCount64();
    if (state.nextCheckTick != 0 && now < state.nextCheckTick) return;
    state.nextCheckTick = now + kAudioMonitorIntervalMs;
    if ((InterlockedCompareExchange(&g_resourceCapabilities, 0, 0) &
            nfsmw_limitbreaker::ResourceAudioMixerPoolExpanded) == 0) {
        return;
    }
    const auto liveMask = At<std::uint32_t>(kAudioMixerPoolMaskOperand);
    const auto liveBytes = At<std::uint32_t>(kAudioMixerPoolSizeGlobal);
    const auto liveArena = At<std::uintptr_t>(kAudioMixerArenaGlobal);
    const auto liveDescriptor = FindAudioAllocatorDescriptor();
    const bool fault =
        At<std::uint8_t>(kAudioMixerPoolMaskInstruction) !=
            kExpectedAudioMixerPoolMaskOpcode ||
        liveMask != g_audioMixerPoolMask ||
        liveBytes != g_audioMixerPoolBytes || liveArena == 0 ||
        liveDescriptor != g_audioAllocatorDescriptor ||
        liveDescriptor == 0 ||
        At<std::uint32_t>(liveDescriptor + kAllocatorSizeOffset) !=
            g_audioMixerPoolBytes;
    if (fault != state.previousFault) {
        Log(L"AUDIO_INVARIANT state=%s mask=0x%08lX expected_mask=0x%08lX bytes=0x%08lX expected_bytes=0x%08lX arena=0x%08lX descriptor=0x%08lX expected_descriptor=0x%08lX",
            fault ? L"fault" : L"recovered",
            static_cast<unsigned long>(liveMask),
            static_cast<unsigned long>(g_audioMixerPoolMask),
            static_cast<unsigned long>(liveBytes),
            static_cast<unsigned long>(g_audioMixerPoolBytes),
            static_cast<unsigned long>(liveArena),
            static_cast<unsigned long>(liveDescriptor),
            static_cast<unsigned long>(g_audioAllocatorDescriptor));
    }
    state.previousFault = fault;
    if (fault) return;

    const auto minFree = At<std::uint32_t>(
        liveDescriptor + kAllocatorMinFreeOffset);
    if (state.previousMinFree == 0xFFFFFFFFu ||
        minFree + kAudioMinFreeLogStepBytes <= state.previousMinFree) {
        Log(L"AUDIO_MIXER_POOL_STATS allocated=0x%08lX peak=0x%08lX free=0x%08lX min_free=0x%08lX total=0x%08lX",
            static_cast<unsigned long>(At<std::uint32_t>(
                liveDescriptor + kAllocatorAllocatedOffset)),
            static_cast<unsigned long>(At<std::uint32_t>(
                liveDescriptor + kAllocatorPeakOffset)),
            static_cast<unsigned long>(At<std::uint32_t>(
                liveDescriptor + kAllocatorFreeOffset)),
            static_cast<unsigned long>(minFree),
            static_cast<unsigned long>(g_audioMixerPoolBytes));
        state.previousMinFree = minFree;
    }
}

bool VictimVectorVtableOperandMatches(const std::uint32_t expected) {
    return At<std::uint32_t>(kVictimVectorVtableOperand) == expected;
}

bool ManagerVectorVtableOperandMatches(const std::uint32_t expected) {
    return At<std::uint32_t>(kAIVehicleManagerVectorVtableOperand) == expected;
}

void InitializeVictimVectorVtable() {
    std::memcpy(g_victimVtable.data(),
        reinterpret_cast<const void*>(kExpectedVictimVectorVtable),
        sizeof(g_victimVtable));
    g_victimVtable[1] = reinterpret_cast<std::uintptr_t>(&AllocateVictimVectorStorage);
    g_victimVtable[2] = reinterpret_cast<std::uintptr_t>(&FreeVictimVectorStorage);
    g_victimVtable[3] = reinterpret_cast<std::uintptr_t>(&GrowVictimVector);
    g_victimVtable[4] = reinterpret_cast<std::uintptr_t>(&MaxVictimVectorCapacity);
}

void InitializeManagerVectorVtable() {
    std::memcpy(g_managerVtable.data(),
        reinterpret_cast<const void*>(kExpectedAIVehicleManagerVectorVtable),
        sizeof(g_managerVtable));
    g_managerVtable[1] = reinterpret_cast<std::uintptr_t>(&AllocateManagerVectorStorage);
    g_managerVtable[2] = reinterpret_cast<std::uintptr_t>(&FreeManagerVectorStorage);
    g_managerVtable[3] = reinterpret_cast<std::uintptr_t>(&GrowManagerVector);
    g_managerVtable[4] = reinterpret_cast<std::uintptr_t>(&MaxManagerVectorCapacity);
}

bool SelfTestVictimVectorVtable() {
    using AllocateMethod = void* (__thiscall*)(void*, std::uint32_t, std::uint32_t);
    using FreeMethod = void (__thiscall*)(void*, void*, std::uint32_t);
    using GrowMethod = std::uint32_t (__thiscall*)(void*, std::uint32_t);
    using MaxMethod = std::uint32_t (__thiscall*)(void*);
    alignas(16) std::array<std::uint8_t,
        16 + kStockLimit * kVictimRecordSize> fakeVector{};
    void* object = fakeVector.data();
    const auto allocate = reinterpret_cast<AllocateMethod>(g_victimVtable[1]);
    const auto free = reinterpret_cast<FreeMethod>(g_victimVtable[2]);
    const auto grow = reinterpret_cast<GrowMethod>(g_victimVtable[3]);
    const auto maximum = reinterpret_cast<MaxMethod>(g_victimVtable[4]);
    void* allocation = allocate(object, kReleaseLimit, 16);
    *reinterpret_cast<void**>(fakeVector.data() + 4) = allocation;
    if (allocation != nullptr) static_cast<std::uint8_t*>(allocation)[0] = 0xA5;
    void* reused = allocate(object, kStorageCapacity, 16);
    const bool passed = allocation != nullptr && reused == allocation &&
        static_cast<std::uint8_t*>(allocation)[0] == 0xA5 &&
        reinterpret_cast<std::uintptr_t>(allocation) !=
            reinterpret_cast<std::uintptr_t>(object) + 16 &&
        grow(object, kReleaseLimit + 1) == kStorageCapacity &&
        maximum(object) == kStorageCapacity;
    if (allocation != nullptr) free(object, allocation, kReleaseLimit);
    Log(L"VICTIM_VECTOR_SELF_TEST passed=%u allocation=%p requested_bytes=%zu storage_bytes=%zu acquisitions=%ld reuses=%ld frees=%ld failures=%ld",
        passed ? 1u : 0u, allocation, VictimAllocationBytes(kReleaseLimit),
        VictimAllocationBytes(kStorageCapacity),
        g_victimAllocations, g_victimReuses, g_victimFrees,
        g_victimAllocationFailures);
    InterlockedExchange(&g_victimAllocations, 0);
    InterlockedExchange(&g_victimReuses, 0);
    InterlockedExchange(&g_victimFrees, 0);
    InterlockedExchange(&g_victimAllocationFailures, 0);
    return passed;
}

bool SelfTestManagerVectorVtable() {
    using AllocateMethod = void* (__thiscall*)(void*, std::uint32_t, std::uint32_t);
    using FreeMethod = void (__thiscall*)(void*, void*, std::uint32_t);
    using GrowMethod = std::uint32_t (__thiscall*)(void*, std::uint32_t);
    using MaxMethod = std::uint32_t (__thiscall*)(void*);
    alignas(16) std::array<std::uint8_t, 16 + kStockLimit * kPointerRecordSize>
        fakeVector{};
    void* object = fakeVector.data();
    const auto allocate = reinterpret_cast<AllocateMethod>(g_managerVtable[1]);
    const auto free = reinterpret_cast<FreeMethod>(g_managerVtable[2]);
    const auto grow = reinterpret_cast<GrowMethod>(g_managerVtable[3]);
    const auto maximum = reinterpret_cast<MaxMethod>(g_managerVtable[4]);
    void* allocation = allocate(object, kStockLimit, 16);
    *reinterpret_cast<void**>(fakeVector.data() + 4) = allocation;
    if (allocation != nullptr) static_cast<std::uint32_t*>(allocation)[0] = 0xA55AA55Au;
    void* reused = allocate(object, kStorageCapacity, 16);
    const bool passed = allocation != nullptr && reused == allocation &&
        static_cast<std::uint32_t*>(allocation)[0] == 0xA55AA55Au &&
        reinterpret_cast<std::uintptr_t>(allocation) !=
            reinterpret_cast<std::uintptr_t>(object) + 16 &&
        grow(object, kStockLimit + 1) == kStorageCapacity &&
        maximum(object) == kStorageCapacity;
    if (allocation != nullptr) free(object, allocation, kStockLimit);
    Log(L"MANAGER_VECTOR_SELF_TEST passed=%u allocation=%p bytes=%zu acquisitions=%ld reuses=%ld frees=%ld failures=%ld",
        passed ? 1u : 0u, allocation, PointerAllocationBytes(kStorageCapacity),
        g_managerAllocations, g_managerReuses, g_managerFrees,
        g_managerAllocationFailures);
    InterlockedExchange(&g_managerAllocations, 0);
    InterlockedExchange(&g_managerReuses, 0);
    InterlockedExchange(&g_managerFrees, 0);
    InterlockedExchange(&g_managerAllocationFailures, 0);
    return passed;
}

bool InstallVictimVectorVtable() {
    const std::uint32_t replacement = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(g_victimVtable.data()));
    if (!WriteBytes(kVictimVectorVtableOperand, &replacement, sizeof(replacement)) ||
        !VictimVectorVtableOperandMatches(replacement)) {
        WriteBytes(kVictimVectorVtableOperand, &kExpectedVictimVectorVtable,
            sizeof(kExpectedVictimVectorVtable));
        Log(L"PATCH_FAILED site=victim-vector-vtable address=0x%08lX",
            static_cast<unsigned long>(kVictimVectorVtableOperand));
        return false;
    }
    Log(L"PATCH_INSTALLED site=victim-vector-vtable address=0x%08lX old=0x%08lX new=0x%08lX record_size=%zu",
        static_cast<unsigned long>(kVictimVectorVtableOperand),
        static_cast<unsigned long>(kExpectedVictimVectorVtable),
        static_cast<unsigned long>(replacement), kVictimRecordSize);
    return true;
}

void RestoreVictimVectorVtable() {
    WriteBytes(kVictimVectorVtableOperand, &kExpectedVictimVectorVtable,
        sizeof(kExpectedVictimVectorVtable));
}

bool InstallManagerVectorVtable() {
    const std::uint32_t replacement = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(g_managerVtable.data()));
    if (!WriteBytes(kAIVehicleManagerVectorVtableOperand, &replacement,
            sizeof(replacement)) || !ManagerVectorVtableOperandMatches(replacement)) {
        WriteBytes(kAIVehicleManagerVectorVtableOperand,
            &kExpectedAIVehicleManagerVectorVtable,
            sizeof(kExpectedAIVehicleManagerVectorVtable));
        Log(L"PATCH_FAILED site=ai-vehicle-manager-vector-vtable address=0x%08lX",
            static_cast<unsigned long>(kAIVehicleManagerVectorVtableOperand));
        return false;
    }
    Log(L"PATCH_INSTALLED site=ai-vehicle-manager-vector-vtable address=0x%08lX old=0x%08lX new=0x%08lX record_size=%zu",
        static_cast<unsigned long>(kAIVehicleManagerVectorVtableOperand),
        static_cast<unsigned long>(kExpectedAIVehicleManagerVectorVtable),
        static_cast<unsigned long>(replacement), kPointerRecordSize);
    return true;
}

void RestoreManagerVectorVtable() {
    WriteBytes(kAIVehicleManagerVectorVtableOperand,
        &kExpectedAIVehicleManagerVectorVtable,
        sizeof(kExpectedAIVehicleManagerVectorVtable));
}

bool VerifyAllPatchSites() {
    if (!VictimVectorVtableOperandMatches(kExpectedVictimVectorVtable)) {
        Log(L"INACTIVE reason=patch-conflict site=victim-vector-vtable address=0x%08lX",
            static_cast<unsigned long>(kVictimVectorVtableOperand));
        return false;
    }
    if (!ManagerVectorVtableOperandMatches(kExpectedAIVehicleManagerVectorVtable)) {
        Log(L"INACTIVE reason=patch-conflict site=ai-vehicle-manager-vector-vtable address=0x%08lX",
            static_cast<unsigned long>(kAIVehicleManagerVectorVtableOperand));
        return false;
    }
    for (const PatchSite& site : kPatchSites) {
        if (!BytesEqual(site.address, site.stock.data(), site.size)) {
            Log(L"INACTIVE reason=patch-conflict site=%S address=0x%08lX", site.name,
                static_cast<unsigned long>(site.address));
            return false;
        }
    }
    return true;
}

bool ApplyBudgetPatches(const std::uint32_t limit, std::size_t& appliedCount) {
    appliedCount = 0;
    for (const PatchSite& site : kPatchSites) {
        if (!BytesEqual(site.address, site.stock.data(), site.size)) {
            Log(L"PATCH_FAILED reason=late-conflict site=%S address=0x%08lX", site.name,
                static_cast<unsigned long>(site.address));
            return false;
        }
        std::array<std::uint8_t, 3> replacement = site.stock;
        replacement[site.operandOffset] = OperandFor(site, limit);
        if (!WriteBytes(site.address, replacement.data(), site.size) ||
            !BytesEqual(site.address, replacement.data(), site.size)) {
            WriteBytes(site.address, site.stock.data(), site.size);
            Log(L"PATCH_FAILED site=%S address=0x%08lX", site.name,
                static_cast<unsigned long>(site.address));
            return false;
        }
        ++appliedCount;
        Log(L"PATCH_INSTALLED site=%S address=0x%08lX limit=%lu", site.name,
            static_cast<unsigned long>(site.address), static_cast<unsigned long>(limit));
    }
    return true;
}

bool BudgetPatchesMatch(const std::uint32_t limit) {
    for (const PatchSite& site : kPatchSites) {
        std::array<std::uint8_t, 3> replacement = site.stock;
        replacement[site.operandOffset] = OperandFor(site, limit);
        if (!BytesEqual(site.address, replacement.data(), site.size)) return false;
    }
    return true;
}

void RestoreBudgetPatches(const std::size_t appliedCount) {
    for (std::size_t index = appliedCount; index > 0; --index) {
        const PatchSite& site = kPatchSites[index - 1];
        WriteBytes(site.address, site.stock.data(), site.size);
    }
}

#include "PopulationBridge.inl"

bool WaitForVehicleVectors(const DWORD timeoutMs) {
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    do {
        bool allReady = true;
        for (const FixedVectorSpec& spec : kRelocatedVectors) {
            const auto vtable = At<std::uint32_t>(spec.object);
            const auto data = At<std::uintptr_t>(VectorDataAddress(spec));
            const auto capacity = At<std::uint32_t>(VectorCapacityAddress(spec));
            const auto count = At<std::uint32_t>(VectorCountAddress(spec));
            if (IsExpectedEmptyFixedVectorState(spec, vtable, data, capacity, count)) continue;
            allReady = false;
            if (vtable == spec.expectedVtable && count != 0) {
                Log(L"INACTIVE reason=vehicle-vector-already-active name=%S object=0x%08lX data=0x%08lX capacity=%lu count=%lu",
                    spec.name, static_cast<unsigned long>(spec.object),
                    static_cast<unsigned long>(data), static_cast<unsigned long>(capacity),
                    static_cast<unsigned long>(count));
                return false;
            }
        }
        if (allReady) return true;
        Sleep(10);
    } while (GetTickCount64() < deadline);
    for (const FixedVectorSpec& spec : kRelocatedVectors) {
        const auto vtable = At<std::uint32_t>(spec.object);
        const auto data = At<std::uintptr_t>(VectorDataAddress(spec));
        const auto capacity = At<std::uint32_t>(VectorCapacityAddress(spec));
        const auto count = At<std::uint32_t>(VectorCountAddress(spec));
        if (!IsExpectedEmptyFixedVectorState(spec, vtable, data, capacity, count)) {
            Log(L"INACTIVE reason=vehicle-vector-init-timeout name=%S object=0x%08lX vtable=0x%08lX data=0x%08lX capacity=%lu count=%lu",
                spec.name, static_cast<unsigned long>(spec.object),
                static_cast<unsigned long>(vtable), static_cast<unsigned long>(data),
                static_cast<unsigned long>(capacity), static_cast<unsigned long>(count));
        }
    }
    return false;
}

bool AllocateStorage() {
    SYSTEM_INFO information{};
    GetSystemInfo(&information);
    const std::size_t pageSize = information.dwPageSize;
    if (pageSize < sizeof(StorageBlock)) return false;
    g_allocation = VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE, PAGE_NOACCESS);
    if (g_allocation == nullptr) return false;
    auto* middle = static_cast<std::uint8_t*>(g_allocation) + pageSize;
    if (VirtualAlloc(middle, pageSize, MEM_COMMIT, PAGE_READWRITE) == nullptr) return false;
    g_storage = reinterpret_cast<StorageBlock*>(middle + (pageSize - sizeof(StorageBlock)) / 2);
    std::memset(g_storage, 0, sizeof(StorageBlock));
    for (GuardedPointerStorage& storage : g_storage->vectors) {
        storage.guardBefore = kGuardBefore;
        storage.guardAfter = kGuardAfter;
    }
    return true;
}

#include "SkinCapacity.inl"
#include "SkinRecolourCache.inl"
#include "CarPartCapacity.inl"
#include "NativeAiDispatchQueue.inl"
DWORD WINAPI Initialize(LPVOID) {
    wchar_t logging[16]{};
    GetPrivateProfileStringW(L"Diagnostics",L"Logging",L"",logging,16,ModuleSiblingPath(L"NFSMWLimitBreaker.ini").c_str());
    g_loggingEnabled=wcscmp(logging,L"1")==0;
    Log(L"NFSMWLimitBreaker %s initialization started pid=%lu", kVersion, GetCurrentProcessId());
    const std::wstring iniPath = ModuleSiblingPath(L"NFSMWLimitBreaker.ini");
    if (GetPrivateProfileIntW(L"VehicleLimit", L"Enabled", 1, iniPath.c_str()) == 0) {
        Log(L"INACTIVE reason=disabled-in-ini");
        return 0;
    }
    const std::uint32_t limit = static_cast<std::uint32_t>(
        GetPrivateProfileIntW(L"VehicleLimit", L"MaxVehicles", kReleaseLimit, iniPath.c_str()));
    const int audioExpansionSetting = GetPrivateProfileIntW(
        L"Audio", L"LBEngineAudioExpansion", 0, iniPath.c_str());
    const bool audioExpansionRequested = audioExpansionSetting == 1;
    const DWORD monitorInterval = static_cast<DWORD>(
        GetPrivateProfileIntW(L"Diagnostics", L"MonitorIntervalMs", 100, iniPath.c_str()));
    if (!IsSupportedLimit(limit) || monitorInterval < 10 || monitorInterval > 5000) {
        Log(L"INACTIVE reason=invalid-config limit=%lu monitor_ms=%lu supported_limits=20,24,28,32 release_limit=%lu",
            static_cast<unsigned long>(limit), static_cast<unsigned long>(monitorInterval),
            static_cast<unsigned long>(kReleaseLimit));
        return 0;
    }

    std::wstring executablePath;
    std::string digest;
    if (!IsSupportedExecutable(executablePath, digest)) {
        Log(L"INACTIVE reason=unsupported-executable path=%s sha256=%S", executablePath.c_str(), digest.c_str());
        return 0;
    }
    if (!VerifyAllPatchSites()) return 0;
    InstallCarPartCapacity(iniPath);
    if(GetPrivateProfileIntW(L"VehicleResources",L"AdditionalSkins",1,iniPath.c_str())==1)InstallSkinCapacity();
    InstallSkinRecolourCache();
    g_resourceLogicalLimit = limit;
    bool audioExpansionInstalled = false;
    if (audioExpansionSetting != 0 && audioExpansionSetting != 1) {
        Log(L"AUDIO_FEATURE_INACTIVE reason=invalid-config feature=LBEngineAudioExpansion value=%d accepted=0,1 vehicle_feature_continues=1",
            audioExpansionSetting);
    } else if (audioExpansionRequested) {
        audioExpansionInstalled = InstallEngineAudioExpansion();
    } else {
        Log(L"AUDIO_FEATURE_DISABLED feature=LBEngineAudioExpansion restart_required=1 vehicle_feature_continues=1");
    }
    Log(L"WAITING reason=vehicle-vectors-not-yet-active timeout_ms=%lu",
        static_cast<unsigned long>(kVectorInitializationTimeoutMs));
    if (!WaitForVehicleVectors(kVectorInitializationTimeoutMs)) return 0;
    if (!InstallAiDispatchQueue()) return 0;
    if (!AllocateStorage()) {
        RestoreAiDispatchQueue();
        Log(L"INACTIVE reason=storage-allocation-failed error=%lu", GetLastError());
        return 0;
    }
    g_logicalLimit = limit;
    if (!ResolveModuleImageRange(GetModuleHandleW(nullptr), g_executableImageBegin,
            g_executableImageEnd)) {
        Log(L"INACTIVE reason=executable-image-range-unavailable");
        return 0;
    }
    const std::wstring dumpDirectory = ModuleSiblingPath(L"");
    wcsncpy_s(g_dumpDirectory, dumpDirectory.c_str(), _TRUNCATE);
    if(g_loggingEnabled) g_exceptionHandler = AddVectoredExceptionHandler(1, CaptureVehicleLimitException);
    Log(L"CRASH_CAPTURE_ACTIVE handler=%p threshold=%lu configured_limit=%lu scope=main-executable image=0x%08lX..0x%08lX max_dumps=1 directory=%s",
        g_exceptionHandler,
        static_cast<unsigned long>(DiagnosticThresholdFor(limit)),
        static_cast<unsigned long>(limit),
        static_cast<unsigned long>(g_executableImageBegin),
        static_cast<unsigned long>(g_executableImageEnd), g_dumpDirectory);

    std::array<std::array<std::uint32_t, kStockLimit>, kRelocatedVectors.size()> oldInline{};
    std::array<std::uintptr_t, kRelocatedVectors.size()> oldData{};
    std::array<std::uint32_t, kRelocatedVectors.size()> oldCapacity{};
    for (std::size_t index = 0; index < kRelocatedVectors.size(); ++index) {
        const FixedVectorSpec& spec = kRelocatedVectors[index];
        std::memcpy(oldInline[index].data(),
            reinterpret_cast<const void*>(VectorInlineData(spec)), sizeof(oldInline[index]));
        oldData[index] = At<std::uintptr_t>(VectorDataAddress(spec));
        oldCapacity[index] = At<std::uint32_t>(VectorCapacityAddress(spec));
        InterlockedExchange(reinterpret_cast<volatile LONG*>(VectorDataAddress(spec)),
            static_cast<LONG>(reinterpret_cast<std::uintptr_t>(
                g_storage->vectors[index].pointers.data())));
        InterlockedExchange(reinterpret_cast<volatile LONG*>(VectorCapacityAddress(spec)),
            static_cast<LONG>(limit));
    }

    InitializeVictimVectorVtable();
    InitializeManagerVectorVtable();
    const auto managerBeforePatch = At<std::uintptr_t>(kAIVehicleManagerSingleton);
    if (!SelfTestVictimVectorVtable() || !SelfTestManagerVectorVtable() ||
        managerBeforePatch != 0 || !InstallVictimVectorVtable() ||
        !InstallManagerVectorVtable()) {
        RestoreAiDispatchQueue();
        RestoreVictimVectorVtable();
        RestoreManagerVectorVtable();
        for (std::size_t index = kRelocatedVectors.size(); index > 0; --index) {
            const FixedVectorSpec& spec = kRelocatedVectors[index - 1];
            InterlockedExchange(reinterpret_cast<volatile LONG*>(VectorCapacityAddress(spec)),
                static_cast<LONG>(oldCapacity[index - 1]));
            InterlockedExchange(reinterpret_cast<volatile LONG*>(VectorDataAddress(spec)),
                static_cast<LONG>(oldData[index - 1]));
        }
        Log(L"INACTIVE reason=local-vector-vtable-transaction-rolled-back manager=0x%08lX",
            static_cast<unsigned long>(managerBeforePatch));
        return 0;
    }

    std::size_t appliedCount = 0;
    if (!ApplyBudgetPatches(limit, appliedCount)) {
        RestoreBudgetPatches(appliedCount);
        RestoreAiDispatchQueue();
        RestoreVictimVectorVtable();
        RestoreManagerVectorVtable();
        for (std::size_t index = kRelocatedVectors.size(); index > 0; --index) {
            const FixedVectorSpec& spec = kRelocatedVectors[index - 1];
            InterlockedExchange(reinterpret_cast<volatile LONG*>(VectorCapacityAddress(spec)),
                static_cast<LONG>(oldCapacity[index - 1]));
            InterlockedExchange(reinterpret_cast<volatile LONG*>(VectorDataAddress(spec)),
                static_cast<LONG>(oldData[index - 1]));
        }
        Log(L"INACTIVE reason=patch-transaction-rolled-back");
        return 0;
    }

    if (audioExpansionInstalled) {
        VerifyEngineAudioExpansion();
    }
    Log(L"RESOURCE_CAPABILITY export=NFSMWLB_QueryResourceLimitsV1 abi=1 capabilities=0x%08lX audio_mixer_bytes=0x%08lX logical_vehicle_limit=%lu",
        static_cast<unsigned long>(InterlockedCompareExchange(
            &g_resourceCapabilities, 0, 0)),
        static_cast<unsigned long>(
            InterlockedCompareExchange(&g_resourceCapabilities, 0, 0) &
                nfsmw_limitbreaker::ResourceAudioMixerPoolExpanded ?
                g_audioMixerPoolBytes : 0),
        static_cast<unsigned long>(g_resourceLogicalLimit));
    Log(L"POPULATION_BRIDGE active=%u optional=1 lease_ms=2000 trafficOnly=1 logicalLimitUnchanged=1",InstallPopulationBridge()?1u:0u);
    Log(L"ACTIVE executable_sha256=%S vectors=%zu logical_capacity=%lu release_logical_limit=%lu storage_capacity=%lu allocation=%zu local_vectors=reusable-pools",
        digest.c_str(), kRelocatedVectors.size(), static_cast<unsigned long>(limit),
        static_cast<unsigned long>(kReleaseLimit),
        static_cast<unsigned long>(kStorageCapacity), sizeof(StorageBlock));
    for (std::size_t index = 0; index < kRelocatedVectors.size(); ++index) {
        const FixedVectorSpec& spec = kRelocatedVectors[index];
        Log(L"VECTOR_ACTIVE name=%S object=0x%08lX old_base=0x%08lX new_base=0x%08lX count=%lu",
            spec.name, static_cast<unsigned long>(spec.object),
            static_cast<unsigned long>(oldData[index]),
            static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(
                g_storage->vectors[index].pointers.data())),
            static_cast<unsigned long>(At<std::uint32_t>(VectorCountAddress(spec))));
    }

    if(!g_loggingEnabled) return 0; // No diagnostic polling worker when OFF.
    std::array<InvariantStatus, kRelocatedVectors.size()> previous{};
    previous.fill(InvariantStatus::Ok);
    std::array<bool, kRelocatedVectors.size()> previousOldRangeChanged{};
    bool previousPatchBytesChanged = false;
    bool previousVictimVtableChanged = false;
    bool previousManagerVtableChanged = false;
    LONG previousVictimAllocations = 0;
    LONG previousVictimReuses = 0;
    LONG previousVictimFailures = 0;
    LONG previousManagerAllocations = 0;
    LONG previousManagerReuses = 0;
    LONG previousManagerFailures = 0;
    std::uintptr_t previousManagedManager = 0;
    bool previousManagerInvariantFault = false;
    AudioMonitorState audioMonitor{};
    std::array<std::uint32_t, kRelocatedVectors.size()> highWater{};
    bool validatedMilestoneSnapshotTaken = false;
    bool configuredMilestoneSnapshotTaken = false;
    for (;;) {
        Sleep(monitorInterval);
        MonitorEngineAudioExpansion(audioMonitor);
        const bool patchBytesChanged = !BudgetPatchesMatch(limit);
        if (patchBytesChanged != previousPatchBytesChanged) {
            Log(L"INVARIANT_FAULT name=budget-patches patch_bytes_changed=%u",
                patchBytesChanged ? 1u : 0u);
        }
        previousPatchBytesChanged = patchBytesChanged;
        const auto expectedVictimVtable = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(g_victimVtable.data()));
        const bool victimVtableChanged =
            !VictimVectorVtableOperandMatches(expectedVictimVtable);
        if (victimVtableChanged != previousVictimVtableChanged) {
            Log(L"INVARIANT_FAULT name=victim-vector-vtable changed=%u live=0x%08lX expected=0x%08lX allocations=%ld frees=%ld failures=%ld",
                victimVtableChanged ? 1u : 0u,
                static_cast<unsigned long>(At<std::uint32_t>(kVictimVectorVtableOperand)),
                static_cast<unsigned long>(expectedVictimVtable), g_victimAllocations,
                g_victimFrees, g_victimAllocationFailures);
        }
        previousVictimVtableChanged = victimVtableChanged;
        const auto expectedManagerVtable = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(g_managerVtable.data()));
        const bool managerVtableChanged =
            !ManagerVectorVtableOperandMatches(expectedManagerVtable);
        if (managerVtableChanged != previousManagerVtableChanged) {
            Log(L"INVARIANT_FAULT name=ai-vehicle-manager-vector-vtable changed=%u live=0x%08lX expected=0x%08lX acquisitions=%ld reuses=%ld frees=%ld failures=%ld",
                managerVtableChanged ? 1u : 0u,
                static_cast<unsigned long>(At<std::uint32_t>(
                    kAIVehicleManagerVectorVtableOperand)),
                static_cast<unsigned long>(expectedManagerVtable),
                g_managerAllocations, g_managerReuses, g_managerFrees,
                g_managerAllocationFailures);
        }
        previousManagerVtableChanged = managerVtableChanged;
        const LONG victimAllocations = g_victimAllocations;
        const LONG victimReuses = g_victimReuses;
        const LONG victimFrees = g_victimFrees;
        const LONG victimFailures = g_victimAllocationFailures;
        if (victimFailures != previousVictimFailures) {
            Log(L"INVARIANT_FAULT name=victim-vector-pool allocation_failures=%ld acquisitions=%ld reuses=%ld frees=%ld",
                victimFailures, victimAllocations, victimReuses, victimFrees);
        }
        if (victimAllocations != previousVictimAllocations ||
            (victimReuses != previousVictimReuses &&
                (victimReuses <= 4 || victimReuses % 500 == 0))) {
            Log(L"VICTIM_VECTOR_STATS acquisitions=%ld reuses=%ld frees=%ld failures=%ld",
                victimAllocations, victimReuses, victimFrees, victimFailures);
        }
        previousVictimAllocations = victimAllocations;
        previousVictimReuses = victimReuses;
        previousVictimFailures = victimFailures;

        const LONG managerAllocations = g_managerAllocations;
        const LONG managerReuses = g_managerReuses;
        const LONG managerFrees = g_managerFrees;
        const LONG managerFailures = g_managerAllocationFailures;
        if (managerFailures != previousManagerFailures) {
            Log(L"INVARIANT_FAULT name=ai-vehicle-manager-vector-pool allocation_failures=%ld acquisitions=%ld reuses=%ld frees=%ld",
                managerFailures, managerAllocations, managerReuses, managerFrees);
        }
        if (managerAllocations != previousManagerAllocations ||
            managerReuses != previousManagerReuses) {
            Log(L"MANAGER_VECTOR_STATS acquisitions=%ld reuses=%ld frees=%ld failures=%ld",
                managerAllocations, managerReuses, managerFrees, managerFailures);
        }
        previousManagerAllocations = managerAllocations;
        previousManagerReuses = managerReuses;
        previousManagerFailures = managerFailures;

        const auto manager = At<std::uintptr_t>(kAIVehicleManagerSingleton);
        if (manager != 0) {
            const auto vector = manager + kAIVehicleManagerVectorOffset;
            const auto liveVtable = At<std::uint32_t>(vector);
            const auto liveData = At<std::uintptr_t>(vector + 4);
            const auto liveCapacity = At<std::uint32_t>(vector + 8);
            const auto liveCount = At<std::uint32_t>(vector + 12);
            const bool cold = liveData == 0 && liveCapacity == 0 && liveCount == 0;
            const bool managed = liveVtable == expectedManagerVtable;
            if (managed && manager != previousManagedManager) {
                Log(L"MANAGER_VECTOR_ACTIVE manager=0x%08lX object=0x%08lX vtable=0x%08lX data=0x%08lX capacity=%lu count=%lu",
                    static_cast<unsigned long>(manager),
                    static_cast<unsigned long>(vector),
                    static_cast<unsigned long>(liveVtable),
                    static_cast<unsigned long>(liveData),
                    static_cast<unsigned long>(liveCapacity),
                    static_cast<unsigned long>(liveCount));
            }
            const bool unexpectedActiveObject = !cold && !managed;
            const bool managedStorageFault = managed &&
                ((!cold && !StorageBelongsToPool(g_managerPool, liveData)) ||
                    liveCapacity > kStorageCapacity || liveCount > liveCapacity);
            const bool managerInvariantFault =
                unexpectedActiveObject || managedStorageFault;
            if (managerInvariantFault != previousManagerInvariantFault &&
                managerInvariantFault) {
                Log(L"INVARIANT_FAULT name=ai-vehicle-manager-vector manager=0x%08lX object=0x%08lX vtable=0x%08lX expected_vtable=0x%08lX data=0x%08lX pool_data=%u capacity=%lu count=%lu",
                    static_cast<unsigned long>(manager),
                    static_cast<unsigned long>(vector),
                    static_cast<unsigned long>(liveVtable),
                    static_cast<unsigned long>(expectedManagerVtable),
                    static_cast<unsigned long>(liveData),
                    StorageBelongsToPool(g_managerPool, liveData) ? 1u : 0u,
                    static_cast<unsigned long>(liveCapacity),
                    static_cast<unsigned long>(liveCount));
            }
            previousManagerInvariantFault = managerInvariantFault;
            previousManagedManager = managed ? manager : 0;
        } else {
            previousManagerInvariantFault = false;
            previousManagedManager = 0;
        }

        for (std::size_t index = 0; index < kRelocatedVectors.size(); ++index) {
            const FixedVectorSpec& spec = kRelocatedVectors[index];
            const auto liveVtable = At<std::uint32_t>(spec.object);
            const auto liveData = At<std::uintptr_t>(VectorDataAddress(spec));
            const auto liveCapacity = At<std::uint32_t>(VectorCapacityAddress(spec));
            const auto liveCount = At<std::uint32_t>(VectorCountAddress(spec));
            if (liveCount > highWater[index]) {
                highWater[index] = liveCount;
                Log(L"HIGH_WATER name=%S count=%lu limit=%lu", spec.name,
                    static_cast<unsigned long>(liveCount), static_cast<unsigned long>(limit));
            }
            if (index == 0 && liveCount >= kValidatedLimit &&
                !validatedMilestoneSnapshotTaken) {
                validatedMilestoneSnapshotTaken = true;
                LogPrimaryPointerValidation(L"validated-28", liveCount);
                LogRemainingFixedVectorCandidates(L"validated-28", liveCount);
            }
            if (index == 0 && liveCount >= limit &&
                !configuredMilestoneSnapshotTaken) {
                configuredMilestoneSnapshotTaken = true;
                LogPrimaryPointerValidation(L"configured-limit", liveCount);
                LogRemainingFixedVectorCandidates(L"configured-limit", liveCount);
            }
            const InvariantStatus status = CheckInvariants(
                g_storage->vectors[index], liveVtable, spec.expectedVtable, liveData,
                reinterpret_cast<std::uintptr_t>(g_storage->vectors[index].pointers.data()),
                liveCapacity, limit, liveCount);
            const bool oldRangeChanged = std::memcmp(oldInline[index].data(),
                reinterpret_cast<const void*>(VectorInlineData(spec)),
                sizeof(oldInline[index])) != 0;
            if (status != previous[index] ||
                oldRangeChanged != previousOldRangeChanged[index]) {
                if (status != InvariantStatus::Ok || oldRangeChanged) {
                    Log(L"INVARIANT_FAULT name=%S status=%s object=0x%08lX vtable=0x%08lX data=0x%08lX capacity=%lu count=%lu old_range_changed=%u",
                        spec.name, InvariantStatusName(status),
                        static_cast<unsigned long>(spec.object),
                        static_cast<unsigned long>(liveVtable),
                        static_cast<unsigned long>(liveData),
                        static_cast<unsigned long>(liveCapacity),
                        static_cast<unsigned long>(liveCount), oldRangeChanged ? 1u : 0u);
                }
            }
            previous[index] = status;
            previousOldRangeChanged[index] = oldRangeChanged;
        }
    }
}

} // namespace

#pragma comment(linker,"/EXPORT:NFSMWLB_ReservePopulationV1=_NFSMWLB_ReservePopulationV1")
#pragma comment(linker,"/EXPORT:NFSMWLB_QueryResourceLimitsV1=_NFSMWLB_QueryResourceLimitsV1")
extern "C" std::uint32_t __cdecl NFSMWLB_ReservePopulationV1(std::uint32_t owner,std::uint32_t missing,nfsmw_population::Status* status) noexcept {
    return ReservePopulation(owner,missing,status);
}

extern "C" std::uint32_t __cdecl NFSMWLB_QueryResourceLimitsV1(
    nfsmw_limitbreaker::ResourceLimitsV1* output,
    const std::uint32_t outputBytes) noexcept {
    if (output == nullptr || outputBytes < sizeof(*output)) return 0;
    const auto capabilities = static_cast<std::uint32_t>(
        InterlockedCompareExchange(&g_resourceCapabilities, 0, 0));
    *output = {};
    output->structSize = sizeof(*output);
    output->abiVersion = nfsmw_limitbreaker::kResourceLimitsAbiV1;
    output->capabilities = capabilities;
    output->audioMixerPoolBytes = capabilities &
        nfsmw_limitbreaker::ResourceAudioMixerPoolExpanded ?
        g_audioMixerPoolBytes : 0;
    output->logicalVehicleLimit = g_resourceLogicalLimit;
    return capabilities;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
        if (thread != nullptr) CloseHandle(thread);
    }
    return TRUE;
}
