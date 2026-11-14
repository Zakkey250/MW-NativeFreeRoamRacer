#pragma once
#include "Types.h"
namespace mwneo {
// Compensate measured chassis response, not vehicle power/grip. A short
// prediction unwinds the wheel before current angular/lateral momentum has
// carried the car beyond a newly selected target. Recovery/flight opt out.
inline float PredictSteeringCurve(const Vehicle& v,Point aim,float yawRate,float fallback){
 if(!Finite(v.velocity)||!std::isfinite(yawRate)||v.speed<20)return fallback;
 const float horizon=.12f;
 auto h=UnitXZ(v.heading),travel=UnitXZ(v.velocity);
 if(LengthXZ(travel)<.9f||DotXZ(h,travel)<.94f)return fallback;
 auto delta=Sub(aim,v.position);
 if(LengthXZ(delta)<std::max(12.f,v.speed*.35f))return fallback;
 float yaw=std::clamp(yawRate*horizon,-.12f,.12f),cs=std::cos(yaw),sn=std::sin(yaw);
 Point heading{h.x*cs-h.z*sn,0,h.z*cs+h.x*sn};
 // Integrate the same angular motion into position. Translating straight
 // while rotating heading invents an outside offset on every steady bend.
 float turn=yaw/horizon;
 if(std::abs(turn)>.001f){
  delta.x-=(v.velocity.x*sn-v.velocity.z*(1-cs))/turn;
  delta.z-=(v.velocity.z*sn+v.velocity.x*(1-cs))/turn;
 }else {delta.x-=v.velocity.x*horizon;delta.z-=v.velocity.z*horizon;}
 float square=DotXZ(delta,delta);
 if(square<64||DotXZ(heading,delta)<4)return fallback;
 return 2*CrossXZ(heading,delta)/square;
}
}
