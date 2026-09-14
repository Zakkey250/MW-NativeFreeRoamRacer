#pragma once
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <span>
namespace native_freeroam::encounter_wave {
struct Pcm { unsigned rate=0, channels=0, bits=0; std::size_t offset=0, bytes=0; };
inline bool Parse(std::span<const unsigned char> b, Pcm& out) {
    out={};
    auto u16=[&](std::size_t p) {return unsigned(b[p])|(unsigned(b[p+1])<<8);};
    auto u32=[&](std::size_t p) {return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|
        (std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);};
    if(b.size()<44 || b.size()>8*1024*1024 || std::memcmp(b.data(),"RIFF",4) ||
       std::memcmp(b.data()+8,"WAVE",4)) return false;
    const std::uint64_t end=std::uint64_t(u32(4))+8;
    if(end>b.size() || end<12) return false;
    bool format=false,data=false;
    for(std::size_t p=12;p+8<=end;) {
        const auto n=u32(p+4); const auto start=p+8;
        if(n>end-start) return false;
        if(!std::memcmp(b.data()+p,"fmt ",4)) {
            if(format || n<16 || u16(start)!=1) return false;
            out.channels=u16(start+2);out.rate=u32(start+4);out.bits=u16(start+14);
            if((out.channels!=1&&out.channels!=2) || out.bits!=16 || out.rate<8000 || out.rate>96000 ||
                u16(start+12)!=out.channels*2 || u32(start+8)!=out.rate*out.channels*2) return false;
            format=true;
        } else if(!std::memcmp(b.data()+p,"data",4)) {
            if(data || !n) return false;
            out.offset=start;out.bytes=n;data=true;
        }
        p=start+n+(n&1);
        if(p>end) return false;
    }
    return format && data && out.bytes%(out.channels*2)==0 &&
        out.bytes<=out.rate*out.channels*2*60;
}
inline int Speaker(const wchar_t* name) {
    const auto p=std::wcsstr(name,L"speaker");
    if(!p) return -1;
    const auto s=p+7;
    if(s[0]<L'0'||s[0]>L'9'||!s[1]||s[1]<L'0'||s[1]>L'9'||s[2]!=L'_') return -1;
    return (s[0]-L'0')*10+s[1]-L'0';
}
}
