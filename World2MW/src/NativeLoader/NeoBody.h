#pragma once
// Read-only English 1.3 IVehicle -> IEntity -> IRigidBody surface. Slots 9,
// 13 and 18 are velocity, forward and dimensions in the bundled MW05 SDK.
struct NeoBodyMotion {
 float position[3]{},heading[3]{},velocity[3]{},dimension[3]{};
 float halfWidth=1,halfLength=2.5f;
};
static bool NeoReadBody(BYTE* vehicle,NeoBodyMotion& motion){
 if(!vehicle||*reinterpret_cast<unsigned*>(vehicle)!=0x8AA828)return false;
 auto entity=vehicle-0x80;auto table=*reinterpret_cast<void***>(entity);
 auto body=reinterpret_cast<void*(__thiscall*)(void*)>(table[21])(entity);if(!body)return false;
 auto bt=*reinterpret_cast<void***>(body);
 auto position=reinterpret_cast<const float*(__thiscall*)(void*)>(bt[8])(body);
 auto velocity=reinterpret_cast<const float*(__thiscall*)(void*)>(bt[9])(body);
 if(!position||!velocity)return false;
 memcpy(motion.position,position,12);memcpy(motion.velocity,velocity,12);
 reinterpret_cast<void(__thiscall*)(void*,float*)>(bt[13])(body,motion.heading);
 reinterpret_cast<void(__thiscall*)(void*,float*)>(bt[18])(body,motion.dimension);
 for(unsigned i=0;i<3;++i)if(!std::isfinite(motion.position[i])||!std::isfinite(motion.velocity[i])||!std::isfinite(motion.heading[i]))return false;
 float n=std::sqrt(motion.heading[0]*motion.heading[0]+motion.heading[1]*motion.heading[1]+motion.heading[2]*motion.heading[2]);
 if(n<.9f||n>1.1f||std::hypot(motion.velocity[0],motion.velocity[2])>200)return false;
 for(auto& h:motion.heading)h/=n;
 // MW's rigid-body dimensions are half extents (BMW: .938 / 2.271 m).
 if(std::isfinite(motion.dimension[0])&&std::isfinite(motion.dimension[2])&&motion.dimension[0]>.5f&&motion.dimension[0]<3.5f&&motion.dimension[2]>1&&motion.dimension[2]<12){
  motion.halfWidth=motion.dimension[0];motion.halfLength=motion.dimension[2];
 }
 return true;
}
