namespace native_freeroam { namespace {
unsigned char* alpha38GPS=nullptr;
unsigned alpha38Engages=0,alpha38Stops=0;
bool alpha38Success=true;
bool __cdecl Alpha38Engage(const Vec3* target,float deviation) {
    ++alpha38Engages;
    std::memcpy(alpha38GPS+0x48,target,sizeof(*target));
    const unsigned state=alpha38Success?2u:0u;
    std::memcpy(alpha38GPS+0x74,&state,4);
    std::memcpy(alpha38GPS+0x370,&deviation,4);
    return alpha38Success;
}
void __cdecl Alpha38Stop() {
    ++alpha38Stops;const unsigned state=0;std::memcpy(alpha38GPS+0x74,&state,4);
}
} }
