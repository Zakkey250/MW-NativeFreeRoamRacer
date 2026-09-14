// Private spawn record only. Never edit the player's record, unlocks or global
// CarCustomizeManager (which is not initialized for a selected car in free roam).
using PerformanceLevels = std::array<std::uint32_t,7>;
constexpr PerformanceLevels kShopPackageCounts{3,4,3,4,4,3,3}; // 007A4D40 jump table

template<class Unlocked>
PerformanceLevels SelectCareerPerformance(const PerformanceLevels& maximum,Unlocked unlocked) {
    PerformanceLevels chosen{};
    for(unsigned type=0;type<7;++type) {
        // Vehicle-local physics levels and career shop package levels differ.
        // Native customization mapping: maxPackages + localLevel - maxLevel.
        if(maximum[type]>6) continue;
        for(unsigned level=1;level<=maximum[type];++level) {
            const int shop=int(kShopPackageCounts[type])+int(level)-int(maximum[type]);
            if(shop>=1 && shop<=int(kShopPackageCounts[type]) && unlocked(type,shop)) chosen[type]=level;
        }
    }
    return chosen;
}

bool PerformanceSurfaceAvailable() noexcept {
    struct Guard { unsigned address; std::array<std::uint8_t,8> bytes; };
    const Guard guards[]={
        {0x00672E80,{0x6A,0xFF,0x68,0x58,0xA9,0x87,0,0x64}},
        {0x00672D30,{0x6A,0xFF,0x68,0x38,0xA9,0x87,0,0x64}},
        {0x0058A960,{0x51,0xA0,0x24,0x61,0x92,0,0x84,0xC0}},
        {0x0058A5C0,{0x8B,0x44,0x24,0x10,0x8B,0x4C,0x24,0x0C}},
        {0x00576670,{0xA1,0x90,0xCF,0x91,0,0x8B,0x48,0x10}},
        {0x005769C0,{0x8B,0x44,0x24,0x04,0x83,0xF8,0x06,0x77}},
        {0x0056F210,{0x8B,0x44,0x24,0x04,0x81,0xC1,0x18,0x01}},
    };
    for(const auto& guard:guards) {
        std::array<std::uint8_t,8> actual{};
        if(!SafeRead(reinterpret_cast<void*>(Address(guard.address)),&actual)||actual!=guard.bytes) {
            Log(LogLevel::Warning,"PERFORMANCE_SURFACE_MISMATCH address=%08X",guard.address);return false;
        }
    }
    return true;
}

enum class PerformanceEligibility { Deferred, Ready, NoNitrous };
struct CareerPerformancePlan { PerformanceLevels maximum{},chosen{}; };

template<class Unlocked>
PerformanceEligibility PrepareCareerPerformance(const PerformanceLevels& maximum,
                                               CareerPerformancePlan* plan,Unlocked unlocked) {
    if(!plan) return PerformanceEligibility::Deferred;
    for(const auto level:maximum) if(level>6) return PerformanceEligibility::Deferred;
    const auto chosen=SelectCareerPerformance(maximum,unlocked);
    if(!maximum[6]&&unlocked(6,1)) return PerformanceEligibility::NoNitrous;
    plan->maximum=maximum;plan->chosen=chosen;return PerformanceEligibility::Ready;
}

PerformanceEligibility ReadCareerPerformancePlan(std::uint32_t key,CareerPerformancePlan* plan) noexcept {
    using namespace NFSPluginSDK::MW05;
    static_assert(offsetof(FECustomizationRecord,mInstalledPhysics)==0x118);
    static_assert(sizeof(Physics::Upgrades::Package)==32);
    Attrib::Gen::pvehicle instance{};
    bool initialized=false,success=false;
    PerformanceLevels maximum{};
    __try {
        __try {
            cFrontEndDatabase* db=nullptr;void* profile=nullptr;bool loaded=false;std::uint32_t mode=0;
            if(!plan || !PerformanceSurfaceAvailable() ||
                !SafeRead(reinterpret_cast<void*>(Address(0x0091CF90)),&db)||!db ||
                !SafeRead(&db->bProfileLoaded,&loaded)||!loaded ||
                !SafeRead(&db->CurrentUserProfiles[0],&profile)||!profile ||
                !SafeRead(reinterpret_cast<unsigned char*>(db)+0x12C,&mode)||!(mode&1)) {
                Log(LogLevel::Info,"PERFORMANCE_DEFER key=%08X reason=career-profile-or-surface-not-ready",key);return PerformanceEligibility::Deferred;
            }
            reinterpret_cast<void(__thiscall*)(void*,std::uint32_t,int,int)>(Address(0x004E4EA0))(&instance,key,0,0);
            initialized=true;
            if(instance.mCollection && instance.mLayoutPtr) {
                for(unsigned type=0;type<7;++type)
                    maximum[type]=reinterpret_cast<unsigned(__cdecl*)(void*,unsigned)>(Address(0x00672E80))(&instance,type);
                for(const auto level:maximum) if(level>6) {
                    Log(LogLevel::Warning,"PERFORMANCE_SKIP key=%08X reason=unsupported-level value=%u",key,level);return PerformanceEligibility::Deferred;
                }
                const auto eligibility=PrepareCareerPerformance(maximum,plan,[](unsigned type,int shop) {
                    // Career filter 2, player 0, regular shop (no backroom).
                    return reinterpret_cast<bool(__cdecl*)(int,unsigned,int,int,bool)>(Address(0x0058A960))(2,type,shop,0,false);
                });
                if(eligibility!=PerformanceEligibility::Ready) return eligibility;
                success=true;
            }
        } __finally {
            if(initialized) reinterpret_cast<void(__thiscall*)(void*)>(Address(0x0045A430))(&instance);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log(LogLevel::Warning,"PERFORMANCE_DEFER key=%08X exception=%08X",key,GetExceptionCode());return PerformanceEligibility::Deferred;
    }
    return success?PerformanceEligibility::Ready:PerformanceEligibility::Deferred;
}

void WriteCareerPerformancePlan(std::uint32_t key,const CareerPerformancePlan& plan,
                               NFSPluginSDK::MW05::FECustomizationRecord* record) noexcept {
    using namespace NFSPluginSDK::MW05;
    const auto& chosen=plan.chosen;const auto& maximum=plan.maximum;
    // Private spawn record only. No SDK default ctor (mNOS is uninitialized).
    std::memcpy(&record->mInstalledPhysics,chosen.data(),sizeof(chosen));
    record->mInstalledPhysics.mJunkman=JunkmanParts::None;
    Log(LogLevel::Info,"PERFORMANCE_READY key=%08X career=1 tires=%u brakes=%u chassis=%u transmission=%u engine=%u induction=%u nos=%u junkman=0 max=%u,%u,%u,%u,%u,%u,%u",
        key,chosen[0],chosen[1],chosen[2],chosen[3],chosen[4],chosen[5],chosen[6],
        maximum[0],maximum[1],maximum[2],maximum[3],maximum[4],maximum[5],maximum[6]);
}

#include "SpawnEligibility.inl"

bool VerifySpawnPerformance(NFSPluginSDK::MW05::PVehicle* created,
                           const NFSPluginSDK::MW05::FECustomizationRecord& requested) noexcept {
    using namespace NFSPluginSDK::MW05;
    static_assert(offsetof(PVehicle,mAttributes)==0xD0);
    PerformanceLevels expected{},actual{};
    std::memcpy(expected.data(),&requested.mInstalledPhysics,sizeof(expected));
    __try {
        if(!created||!created->mAttributes.mCollection||!created->mAttributes.mLayoutPtr) return false;
        for(unsigned type=0;type<7;++type)
            actual[type]=reinterpret_cast<unsigned(__cdecl*)(const void*,unsigned)>(Address(0x00672D30))(&created->mAttributes,type);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
    const bool equal=actual==expected;
    Log(equal?LogLevel::Info:LogLevel::Warning,"PERFORMANCE_APPLIED vehicle=%p match=%u tires=%u brakes=%u chassis=%u transmission=%u engine=%u induction=%u nos=%u",
        created,unsigned(equal),actual[0],actual[1],actual[2],actual[3],actual[4],actual[5],actual[6]);
    return equal;
}
