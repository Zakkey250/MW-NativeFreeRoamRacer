namespace native_freeroam { namespace {
NFSPluginSDK::MW05::UMath::Vector3 alpha32Vector{};
const NFSPluginSDK::MW05::UMath::Vector3* __fastcall Alpha32Vector(void*,void*) {return &alpha32Vector;}
float __fastcall Alpha32Float(void*,void*) {return 0;}
unsigned alpha32PathCalls=0;
void* alpha32PathNav=nullptr;
const Vec3 *alpha32PathPosition=nullptr,*alpha32PathHeading=nullptr;
bool alpha32PathFlag=false;
Vec3 alpha51CopiedPosition{},alpha51CopiedHeading{};
bool alpha51HadHeading=false;
unsigned alpha53DriveCalls=0;
Vec3 alpha53DriveAim{};
const Vec3* alpha53DrivePointer=nullptr;
void* alpha53DriveOwner=nullptr;
unsigned alpha54SpeedCalls=0;
float alpha54SpeedValue=0;
void* alpha54SpeedOwner=nullptr;
void __fastcall Alpha54Speed(void* owner,void*,float speed) {
    ++alpha54SpeedCalls;alpha54SpeedOwner=owner;alpha54SpeedValue=speed;
}
void __fastcall Alpha53Drive(void* owner,void*,const Vec3* aim) {
    ++alpha53DriveCalls;alpha53DriveOwner=owner;alpha53DrivePointer=aim;
    if(aim) alpha53DriveAim=*aim;
}
bool __fastcall Alpha32Path(void* nav,void*,const Vec3* pos,const Vec3* heading,bool flag) {
    ++alpha32PathCalls;alpha32PathNav=nav;alpha32PathPosition=pos;alpha32PathHeading=heading;alpha32PathFlag=flag;
    if(pos) alpha51CopiedPosition=*pos;
    alpha51HadHeading=heading!=nullptr;if(heading) alpha51CopiedHeading=*heading;
    return flag;
}
} }
