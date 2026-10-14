#pragma once
#include <cstring>
#include <cmath>
// MW05 WRoadNav reference position and direction; physical
// checkpoint ownership and native obstacle/braking state are independent.
inline bool SynchronizeRaceGuidanceTarget(void* nav,const float* xyz,const float* position) {
 float forward[3],square=0;
 for(unsigned k=0;k<3;++k){if(!std::isfinite(xyz[k])||!std::isfinite(position[k]))return false;forward[k]=xyz[k]-position[k];square+=forward[k]*forward[k];}
 if(!std::isfinite(square)||square<4)return false;
 const float norm=std::sqrt(square);for(auto& value:forward)value/=norm;
 std::memcpy(static_cast<unsigned char*>(nav)+0x98,xyz,3*sizeof(float));
 // AIActionRace::CalculateSpeed normalizes fForwardVector (0xBC) and uses
 // alignment with the car to reduce speed. A released native query leaves
 // this independent field pointing along the old road, even after fPosition
 // is updated. Supply the direction of the same issued reference command.
 std::memcpy(static_cast<unsigned char*>(nav)+0xBC,forward,sizeof(forward));
 return true;
}
// Curvature of the arc from the car's heading to the actual issued aim.
// Route knot angles and a second native road spline cannot add braking to
// a different steering command. Height does not create lateral curvature.
inline bool RaceGuidanceCurvature(const float* position,const float* heading,const float* aim,float& curve){
 for(unsigned k=0;k<3;++k)if(!std::isfinite(position[k])||!std::isfinite(heading[k])||!std::isfinite(aim[k]))return false;
 const float dx=aim[0]-position[0],dz=aim[2]-position[2];
 const float square=dx*dx+dz*dz,norm=std::hypot(heading[0],heading[2]);
 if(square<4||norm<.01f||(dx*heading[0]+dz*heading[2])<=0)return false;
 curve=2*std::fabs(heading[0]*dz-heading[2]*dx)/(norm*square);
 return std::isfinite(curve)&&curve<=1;
}
