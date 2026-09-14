// WRoadNav::InitFromOther (00777660) reads only these six scalar fields
// before rebuilding its own navigation. Do not carry a borrowed traffic-nav
// pointer across ConstructTrafficSeed / SetDriverClass: those recycle AI storage.
struct AnchorRoadSeed {
    alignas(4) std::array<unsigned char,0x2C8> bytes{};
    template<class T> bool Read(void* nav,std::size_t offset) noexcept {
        T value{};
        if(!AudioRead(reinterpret_cast<std::uintptr_t>(nav),offset,&value)) return false;
        std::memcpy(bytes.data()+offset,&value,sizeof(value));return true;
    }
    template<class T> T At(std::size_t offset) const noexcept {
        T value{};std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;
    }
};
bool ValidateAnchorCopySurface() noexcept {
    const std::uint8_t reset[]={0x53,0x56,0x8B,0xF1,0x8B,0x06,0x57,0xFF,0x50,0x48};
    const std::uint8_t copy[]={0x8B,0x44,0x24,0x04,0x56,0x8B,0xF1,0x3B,0xC6};
    const std::uint8_t tail[]={0x5E,0xC2,0x08,0x00};
    return AudioCodeMatches(0x00422690,reset)&&AudioCodeMatches(0x00777660,copy)&&AudioCodeMatches(0x0077776C,tail);
}
bool ValidAnchorRoadScalars(const AnchorRoadSeed& seed) noexcept {
    const auto segment=seed.At<std::int16_t>(0x8E);
    const auto node=seed.At<std::uint8_t>(0x8C);
    const auto time=seed.At<float>(0x90),offset=seed.At<float>(0x2C4);
    const auto lane=seed.At<std::int8_t>(0x2C1);
    return seed.At<std::uint8_t>(0x50)==1 && segment>=0 && node<=1 &&
        std::isfinite(time)&&time>=0&&time<=1 && std::isfinite(offset)&&std::abs(offset)<=100 && lane>=-1&&lane<=15;
}
bool ValidateAnchorRoadNetwork(const AnchorRoadSeed& seed) noexcept {
    if(!ValidAnchorRoadScalars(seed)) return false;
    std::uintptr_t segments=0,nodes=0,profiles=0;
    if(!AudioRead(Address(0x009B38C0),0,&segments)||!AudioRead(Address(0x009B38BC),0,&nodes)||
        !AudioRead(Address(0x009B38B8),0,&profiles)) return false;
    const auto index=seed.At<std::int16_t>(0x8E);
    const std::size_t segmentOffset=static_cast<std::size_t>(index)*0x16;
    for(unsigned end=0;end<2;++end) {
        std::uint16_t nodeIndex=0;std::int16_t profileIndex=0;
        if(!AudioRead(segments,segmentOffset+2*end,&nodeIndex)||
            !AudioRead(nodes,static_cast<std::size_t>(nodeIndex)*0x20+0xE,&profileIndex)) return false;
        if(profileIndex>=0) {
            std::uint8_t count=0;
            if(!AudioRead(profiles,static_cast<std::size_t>(profileIndex)*0x40,&count)||count>32) return false;
        }
    }
    return true;
}
bool CaptureAnchorRoadSeed(void* nav,AnchorRoadSeed& seed) noexcept {
    seed={};
    return seed.Read<std::uint8_t>(nav,0x50)&&seed.Read<std::uint8_t>(nav,0x8C)&&
        seed.Read<std::int16_t>(nav,0x8E)&&seed.Read<float>(nav,0x90)&&
        seed.Read<std::int8_t>(nav,0x2C1)&&seed.Read<float>(nav,0x2C4)&&
        ValidateAnchorRoadNetwork(seed);
}

bool AnchorGenerationMatches(const VehicleSnapshot& expected,void* simable) noexcept {
    VehicleSnapshot live{};
    return simable&&ReadVehicle(expected.pointer,&live)&&live.vehicleKey==expected.vehicleKey&&
        live.driverClass==kDriverTraffic&&GetSimablePointer(expected.pointer)==simable;
}
