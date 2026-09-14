struct SpawnIdentity { void* simable=nullptr; std::uint32_t key=0; };

bool CaptureSpawnIdentity(void* vehicle,SpawnIdentity* output) noexcept {
    if(!output) return false;
    *output={};
    if(!IsExpectedVehicle(vehicle)) return false;
    __try {
        void** table=nullptr;
        if(!SafeRead(vehicle,&table)||!table) return false;
        if(reinterpret_cast<unsigned(__thiscall*)(void*)>(table[kSlotDriverClass])(vehicle)!=kDriverRacer) return false;
        SpawnIdentity captured{};
        // GetVehicleKey (006880A0 -> Attrib::Instance::GetKey 00452430)
        // returns the tuned instance key, NOT the catalog's stock model key.
        captured.key=reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(table[kSlotVehicleKey])(vehicle);
        captured.simable=GetSimablePointer(vehicle);
        if(!captured.key||!captured.simable) return false;
        *output=captured;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
