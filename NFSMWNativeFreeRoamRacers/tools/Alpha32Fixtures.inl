namespace native_freeroam { namespace {
NFSPluginSDK::MW05::UMath::Vector3 alpha32Vector{};
const NFSPluginSDK::MW05::UMath::Vector3* __fastcall Alpha32Vector(void*,void*) {return &alpha32Vector;}
float __fastcall Alpha32Float(void*,void*) {return 0;}
unsigned alpha32PathCalls=0;
void* alpha32PathNav=nullptr;
const Vec3 *alpha32PathPosition=nullptr,*alpha32PathHeading=nullptr;
bool alpha32PathFlag=false;
bool __fastcall Alpha32Path(void* nav,void*,const Vec3* pos,const Vec3* heading,bool flag) {
    ++alpha32PathCalls;alpha32PathNav=nav;alpha32PathPosition=pos;alpha32PathHeading=heading;alpha32PathFlag=flag;return flag;
}
} }
