// Included inside Runtime.cpp's private namespace. No driving or lifetime hooks.
struct RankedVehicle {
    const InstalledVehicle* installed = nullptr;
    int type = -1;
    std::array<float, 3> performance{};
    double score = 0;
};
std::vector<RankedVehicle> g_catalog;
std::vector<std::size_t> g_palette;
std::vector<std::size_t> g_vehicleBag;
std::size_t g_catalogCursor = 0;
bool g_catalogFinished = false;
std::uint32_t g_palettePlayerKey = 0;
bool g_appearanceAvailable = false;

template <class Ride, class Randomize>
bool ChooseAppearanceOrStock(const Ride& stock, Ride* chosen, bool enabled,
                             unsigned limit, unsigned* attempts, Randomize randomize) {
    *attempts = 0;
    *chosen = stock;
    if (enabled) {
        for (; *attempts < limit;) {
            ++*attempts;
            *chosen = stock;
            if (randomize(chosen)) return true;
        }
    }
    *chosen = stock;
    return false;
}

std::size_t WindowStart(std::size_t count, std::size_t center, std::size_t width) noexcept {
    if (!count) return 0;
    width = std::clamp<std::size_t>(width, 1, count);
    center = std::min(center, count - 1);
    const auto left = (width - 1) / 2;
    return std::min(center > left ? center - left : 0, count - width);
}

void RankVehicles(std::vector<RankedVehicle>& cars) {
    // Equal price/performance weights. Percentile ranks avoid mixing dollars
    // with physics units or letting one expensive add-on dominate the scale.
    for (auto& car : cars) {
        double price = 0, perf = 0;
        unsigned pricedCount = 0;
        for (const auto& other : cars) {
            if (other.installed->cost > 0) {
                ++pricedCount;
                price += (other.installed->cost < car.installed->cost ? 1.0 :
                          other.installed->cost == car.installed->cost ? 0.5 : 0.0);
            }
            for (std::size_t n = 0; n < 3; ++n)
                perf += other.performance[n] < car.performance[n] ? 1.0 :
                        other.performance[n] == car.performance[n] ? 0.5 : 0.0;
        }
        const auto performanceRank = perf / (3.0 * cars.size());
        // Zero-priced bonus vehicles (e.g. M3 GTR) have no retail price, not
        // economy-car prices. Place them by stock performance alone.
        car.score = car.installed->cost > 0 && pricedCount ?
            0.5 * price / pricedCount + 0.5 * performanceRank : performanceRank;
    }
    std::sort(cars.begin(), cars.end(), [](const auto& a, const auto& b) {
        if (a.score != b.score) return a.score < b.score;
        if (a.installed->cost != b.installed->cost) return a.installed->cost < b.installed->cost;
        return std::strcmp(a.installed->model, b.installed->model) < 0;
    });
}

bool InspectVehicleAttributes(std::uint32_t key, char* model,
                              float* performance) noexcept {
    // Instance owns a collection reference. Always release it, including when
    // the estimate fails. No PVehicle or render assets are created for ranking.
    NFSPluginSDK::MW05::Attrib::Gen::pvehicle instance{};
    bool initialized = false;
    bool success = false;
    __try {
        __try {
            reinterpret_cast<void(__thiscall*)(void*, std::uint32_t, int, int)>(
                Address(0x4E4EA0))(&instance, key, 0, 0);
            initialized = true;
            if (instance.mCollection && instance.mLayoutPtr) {
                const char* name = nullptr;
                if (SafeRead(static_cast<std::uint8_t*>(instance.mLayoutPtr) + 0x1C, &name) && name) {
                    const auto length = strnlen_s(name, 32);
                    if (length > 0 && length < 32) {
                        strcpy_s(model, 32, name);
                        success = !performance || reinterpret_cast<bool(__cdecl*)(void*, float*)>(
                            Address(0x67C570))(&instance, performance);
                    }
                }
            }
        } __finally {
            if (initialized) reinterpret_cast<void(__thiscall*)(void*)>(Address(0x45A430))(&instance);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log(LogLevel::Error, "CATALOG_ATTRIBUTE_EXCEPTION key=%08X code=%08X", key, GetExceptionCode());
        return false;
    }
    return success;
}

int FindPlayableType(const char* model) noexcept {
    std::uint8_t* table = nullptr;
    int count = 0;
    if (!SafeRead(reinterpret_cast<void*>(Address(0x9B09D8)), &table) || !table ||
        !SafeRead(reinterpret_cast<void*>(Address(0x9B1334)), &count) || count < 1 || count > 1024)
        return -1;
    for (int i = 0; i < count; ++i) {
        std::array<char, 16> name{};
        std::uint8_t usage = 0xFF;
        if (!SafeRead(table + i * 0xD0, &name) ||
            !SafeRead(table + i * 0xD0 + 0x94, &usage)) return -1;
        if (name.back() != '\0') continue;
        if (usage == 0 && _stricmp(name.data(), model) == 0) return i;
    }
    return -1;
}

bool ValidStockPerformance(bool estimateSucceeded, const std::array<float, 3>& ratings) noexcept {
    // These are UI performance ratings, not physical speed/acceleration.
    // Successful estimates may legitimately lie at zero (ALTO97 and BEAT).
    // Use the native success flag, not a positive-rating requirement.
    if (!estimateSucceeded) return false;
    for (const float value : ratings)
        if (!std::isfinite(value) || value < 0 || value > 10000) return false;
    return true;
}

#include "DynamicVehicleCatalog.inl"

bool BuildVehiclePalette(std::uint32_t playerKey, const char* model) {
    g_palette.clear();
    g_vehicleBag.clear();
    g_palettePlayerKey = playerKey;
    auto current = std::find_if(g_catalog.begin(), g_catalog.end(), [&](const auto& c) {
        return _stricmp(c.installed->model, model) == 0;
    });
    if (current == g_catalog.end()) {
        Log(LogLevel::Error, "PALETTE_UNAVAILABLE player=%s key=%08X not in validated playable catalog", model, playerKey);
        return false; // Do not silently substitute unrelated Blacklist cars.
    }
    const auto center = static_cast<std::size_t>(current - g_catalog.begin());
    // This is a model-type budget including the player's model, not a vehicle-count limit.
    const auto width = std::min(std::clamp<std::size_t>(g_settings.vehicleVariety, 1, 5), g_catalog.size());
    const auto start = WindowStart(g_catalog.size(), center, width);
    std::string names;
    for (std::size_t i = start; i < start + width; ++i) {
        g_palette.push_back(i);
        if (!names.empty()) names += ',';
        names += g_catalog[i].installed->model;
    }
    Log(LogLevel::Info, "VEHICLE_PALETTE player=%s rank=%u window=%u-%u count=%u models=%s playerIncluded=1 modelLimit=5",
        model, static_cast<unsigned>(center+1), static_cast<unsigned>(start+1),
        static_cast<unsigned>(start+width), static_cast<unsigned>(width), names.c_str());
    return !g_palette.empty();
}

bool EnsureVehiclePalette(const VehicleSnapshot& player) {
    if (!g_catalogFinished) return false;
    if (g_palettePlayerKey == player.vehicleKey) return !g_palette.empty();
    char model[32]{};
    if (!InspectVehicleAttributes(player.vehicleKey, model, nullptr)) return false;
    return BuildVehiclePalette(player.vehicleKey, model);
}

const RankedVehicle* SelectRandomVehicle() {
    if (g_palette.empty()) return nullptr;
    if (g_vehicleBag.empty()) {
        g_vehicleBag = g_palette;
        std::shuffle(g_vehicleBag.begin(), g_vehicleBag.end(), g_random);
    }
    const auto selected = g_vehicleBag.back();
    g_vehicleBag.pop_back();
    return &g_catalog[selected];
}

bool ValidateRideParts(const NFSPluginSDK::MW05::RideInfo& ride) noexcept {
    using namespace NFSPluginSDK::MW05;
    __try {
        for (const auto slot : {CarSlotId::Body, CarSlotId::FrontWheel, CarSlotId::RearWheel})
            if (!ride.mPartsTable[static_cast<unsigned>(slot)]) return false;
        for (auto* part : ride.mPartsTable) {
            if (!part) continue; // Optional stock slots can be absent.
            const auto index = reinterpret_cast<int(__thiscall*)(void*, void*)>(
                Address(0x747BB0))(reinterpret_cast<void*>(Address(0x9B26A8)), part);
            if (index < 0 || index >= 0xFFFF) return false;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool CreateStockRide(int type, NFSPluginSDK::MW05::RideInfo* ride) noexcept {
    __try {
        ride->Init(static_cast<NFSPluginSDK::MW05::CarType>(type), NFSPluginSDK::CarRenderUsage::Player);
        ride->SetStockParts(); // Use the installed Unlimiter's stock/dependency rules.
        return ValidateRideParts(*ride);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool RandomizeRide(NFSPluginSDK::MW05::RideInfo* ride) noexcept {
    using namespace NFSPluginSDK::MW05;
    __try {
        for (const auto slot : {CarSlotId::Body, CarSlotId::Hood, CarSlotId::Spoiler, CarSlotId::BasePaint}) {
            ride->SetRandomPart(slot, static_cast<eCareerUpgradeLevels>(-1));
        }
        return ValidateRideParts(*ride);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool WriteAppearanceRecord(const NFSPluginSDK::MW05::RideInfo& ride,
                           NFSPluginSDK::MW05::FECustomizationRecord* record) noexcept {
    __try {
        record->WriteRideIntoRecord(&ride);
        for (std::size_t i = 0; i < std::size(record->mInstalledParts); ++i) {
            if (ride.mPartsTable[i] && record->mInstalledParts[i] == -1) return false;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool MakeAppearance(const RankedVehicle& car, NFSPluginSDK::MW05::FECustomizationRecord* record) {
    using namespace NFSPluginSDK::MW05;
    RideInfo stock(static_cast<CarType>(car.type));
    if (!CreateStockRide(car.type, &stock)) {
        Log(LogLevel::Error, "APPEARANCE_STOCK_INVALID model=%s spawn skipped", car.installed->model);
        return false;
    }
    RideInfo chosen = stock;
    unsigned attempts = 0;
    const bool enabled = g_settings.randomAppearance && car.installed->customizable;
    const bool customized = ChooseAppearanceOrStock(stock, &chosen, enabled,
        g_settings.appearanceAttempts, &attempts, [&](RideInfo* candidate) {
            bool valid = RandomizeRide(candidate);
            // Wheels and all performance upgrades remain stock.
            for (const auto slot : {CarSlotId::FrontWheel, CarSlotId::RearWheel}) {
                const auto i = static_cast<unsigned>(slot);
                candidate->mPartsTable[i] = stock.mPartsTable[i];
                candidate->mPartsEnabled[i] = stock.mPartsEnabled[i];
            }
            valid = valid && WriteAppearanceRecord(*candidate, record);
            if (!valid) Log(LogLevel::Warning, "APPEARANCE_RETRY model=%s attempt=%u", car.installed->model, attempts);
            return valid;
        });
    if (!customized && !WriteAppearanceRecord(stock, record)) return false;
    unsigned changed = 0;
    for (std::size_t i = 0; i < std::size(chosen.mPartsTable); ++i)
        changed += chosen.mPartsTable[i] != stock.mPartsTable[i];
    Log(LogLevel::Info, "APPEARANCE_READY model=%s randomized=%u changedParts=%u attempts=%u stockFallback=%u wheels=stock",
        car.installed->model, customized, changed, attempts, enabled && !customized);
    return true;
}

bool VerifyVariationAssets() {
    struct Guard { unsigned address; std::array<std::uint8_t, 8> bytes; };
    const Guard guards[] = {
        {0x4E4EA0, {0x6A,0xFF,0x68,0x98,0xCD,0x86,0x00,0x64}},
        {0x45A430, {0x8B,0x41,0x04,0x85,0xC0,0x74,0x2A,0x8B}},
        {0x67C570, {0x83,0xEC,0x40,0x55,0x8B,0x6C,0x24,0x48}},
        {0x739A70, {0x8B,0x44,0x24,0x04,0x8B,0xD1,0x8A,0x4C}},
        {0x7596E0, {0x51,0x8B,0x44,0x24,0x0C,0x53,0x55,0x8B}},
        {0x747BB0, {0x53,0x55,0x56,0x57,0x8B,0x39,0x33,0xDB}},
        {0x56F2F0, {0x53,0x55,0x8B,0x6C,0x24,0x0C,0x56,0x57}},
        {0x581C20, {0x8B,0xD1,0x33,0xC0,0x53,0x55,0x56,0x8D}},
        {0x689361, {0xB9,0x66,0x00,0x00,0x00,0x8B,0xF8,0xF3}},
        {0x455BC0, {0x8B,0x44,0x24,0x04,0x56,0x8B,0x71,0x04}},
        {0x453FC0, {0x8B,0x41,0x08,0x8B,0x40,0x24,0xC3,0xCC}},
        {0x456B00, {0x8B,0x41,0x08,0x83,0xC0,0x1C,0x6A,0xFF}},
        {0x456B20, {0x8B,0x49,0x08,0x8B,0x44,0x24,0x04,0x56}},
        {0x455960, {0x8B,0x44,0x24,0x04,0x56,0x8B,0x71,0x08}},
        {0x454190, {0x8B,0x54,0x24,0x04,0x8D,0x44,0x24,0x04}},
        {0x454640, {0x8B,0x4C,0x24,0x04,0x85,0xC9,0x74,0x23}},
        {0x455FD0, {0xA1,0xBC,0xDC,0x90,0x00,0x56,0x8B,0x70}},
        {0x51E1A0, {0x6A,0xFF,0x68,0x58,0xE4,0x86,0x00,0x64}},
        {0x581745, {0x8B,0x4C,0x24,0x0C,0x8B,0x71,0x4C,0x8D}},
        {0x58164F, {0x8A,0x41,0x58,0x84,0xC0,0x74,0x08,0x8B}},
    };
    for (const auto& guard : guards) {
        std::array<std::uint8_t, 8> actual{};
        if (!SafeRead(reinterpret_cast<void*>(Address(guard.address)), &actual) || actual != guard.bytes) {
            Log(LogLevel::Error, "VARIATION_SURFACE_MISMATCH address=%08X", guard.address);
            return false;
        }
    }
    // Accept original RideInfo entrypoints or executable detours owned by
    // Unlimiter. No dependency on a particular car pack or Unlimiter file hash.
    // The native ABI/record layout and every produced part are still validated.
    HMODULE unlimiter = GetModuleHandleW(L"NFSMWUnlimiter.asi");
    const Guard rideGuards[] = {
        {0x7594A0,{0x83,0xEC,0x10,0x53,0x55,0x56,0x57,0x8B}},
        {0x756E90,{0x83,0xEC,0x44,0x53,0x55,0x56,0x8B,0xF1}},
    };
    unsigned nativeCount=0,hookCount=0;
    for (const auto& guard : rideGuards) {
        std::array<std::uint8_t,8> actual{};
        if(!SafeRead(reinterpret_cast<void*>(Address(guard.address)),&actual)) return false;
        if(actual==guard.bytes) {++nativeCount;continue;}
        if(!unlimiter || actual[0]!=0xE9) return false;
        std::int32_t displacement = 0;
        std::memcpy(&displacement,actual.data()+1,4);
        const auto target = Address(guard.address) + 5 + displacement;
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(target), &info, sizeof(info)) ||
            info.AllocationBase != unlimiter || info.State!=MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD|PAGE_NOACCESS)) ||
            !(info.Protect & (PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return false;
        ++hookCount;
    }
    g_appearanceAvailable = true;
    Log(LogLevel::Info, "VARIATION_ASSETS_OK catalog=runtime-vlt nativeRide=%u unlimiterOwnedRide=%u partValidation=required",nativeCount,hookCount);
    return true;
}
