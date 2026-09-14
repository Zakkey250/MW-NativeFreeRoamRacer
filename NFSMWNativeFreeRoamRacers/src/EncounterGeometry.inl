// Keep the accepted population / minimap coordinate convention unchanged.
// At the encounter boundary convert the legacy SDK field-labelled snapshot to
// physical native XYZ (raw offsets 0/4/8, vertical at +4).
static_assert(offsetof(NFSPluginSDK::MW05::UMath::Vector3,y)==0);
static_assert(offsetof(NFSPluginSDK::MW05::UMath::Vector3,z)==4);
static_assert(offsetof(NFSPluginSDK::MW05::UMath::Vector3,x)==8);
constexpr std::size_t kEncounterComputeHeadingSlot=46;
constexpr std::uintptr_t kEncounterComputeHeading=0x00670700;
using EncounterHeadingFn=void(__thiscall*)(void*,Vec3*);

Vec3 EncounterPhysicalPosition(const Vec3& legacy) noexcept {
    return {legacy.y,legacy.z,legacy.x};
}
bool ReadEncounterHeading(void* vehicle,Vec3& heading) noexcept {
    heading={};
    __try {
        if(!IsExpectedVehicle(vehicle)) return false;
        auto** table=*static_cast<void***>(vehicle);
        // Slot 45 / 00688250 is GetLocalVelocity, NOT a world heading getter.
        if(reinterpret_cast<std::uintptr_t>(table[kEncounterComputeHeadingSlot])!=Address(kEncounterComputeHeading)) return false;
        reinterpret_cast<EncounterHeadingFn>(table[kEncounterComputeHeadingSlot])(vehicle,&heading);
        return std::isfinite(heading.x)&&std::isfinite(heading.y)&&std::isfinite(heading.z)&&
            std::hypot(heading.x,heading.z)>0.001f;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
template<class HeadingReader>
bool MakeEncounterGeometry(const VehicleSnapshot& legacy,VehicleSnapshot& physical,HeadingReader readHeading,bool headingRequired=true) noexcept {
    physical=legacy;physical.position=EncounterPhysicalPosition(legacy.position);physical.heading={};
    if(!std::isfinite(physical.position.x)||!std::isfinite(physical.position.y)||!std::isfinite(physical.position.z)) return false;
    const bool hasHeading=readHeading(legacy.pointer,physical.heading);
    if(!hasHeading) physical.heading={};
    return hasHeading||!headingRequired;
}
bool ReadEncounterGeometry(const VehicleSnapshot& legacy,VehicleSnapshot& physical,bool headingRequired=true) noexcept {
    return MakeEncounterGeometry(legacy,physical,ReadEncounterHeading,headingRequired);
}
bool ReadEncounterVehicle(void* vehicle,VehicleSnapshot* physical) noexcept {
    VehicleSnapshot legacy{};
    return physical&&ReadVehicle(vehicle,&legacy)&&ReadEncounterGeometry(legacy,*physical);
}
