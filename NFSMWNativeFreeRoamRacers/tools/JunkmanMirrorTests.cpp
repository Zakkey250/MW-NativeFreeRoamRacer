#include "../include/JunkmanMirrorPolicy.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace native_freeroam::junkman;
    assert(Count(0)==0 && Count(0x7f)==7 && Mask(0xffffffffu)==0x7f);
    assert(Plan(0,0,[](unsigned){return true;})==0);
    assert(Plan(0x15,0,[](unsigned){return true;})==0x15); // exact 3 parts
    assert(Plan(0x15,0x5,[](unsigned){return true;})==0x15); // idempotent
    assert(Count(Plan(0x15,0,[](unsigned type){return type!=2;}))==3); // fallback
    assert(Plan(0x7f,0,[](unsigned type){return type<4;})==0xf); // limited model
    assert(Plan(0,0x4,[](unsigned){return true;})==0x4); // preserve existing
    std::puts("PASS Junkman count, exact mirroring, fallback and no duplicates");
}
