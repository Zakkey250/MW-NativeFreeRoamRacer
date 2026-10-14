#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace mwel_difficulty {
struct Settings {
 bool enabled=true,catchUp=true,nitrous=true;
 float engineScale=1.75f,catchUpMaxScale=2,startMetres=50,fullMetres=600;
};
struct RaceSample {unsigned rank=0;float playerGap=0;bool eligible=false;};
inline float Number(const char* text,float fallback,float low,float high){
 if(!text||!*text)return fallback;char* end=nullptr;float value=std::strtof(text,&end);
 while(end&&(*end==' '||*end=='\t'))++end;
 return end!=text&&end&&!*end&&std::isfinite(value)?std::clamp(value,low,high):fallback;
}
inline float Multiplier(const Settings& s,const RaceSample& race){
 if(!s.enabled)return 1;
 float extra=1;
 if(s.catchUp&&race.eligible&&race.rank>=2&&race.rank<=16&&std::isfinite(race.playerGap)&&race.playerGap>s.startMetres){
  const float t=std::clamp((race.playerGap-s.startMetres)/std::max(1.f,s.fullMetres-s.startMetres),0.f,1.f);
  // Continuous at both ends, avoiding a power step on every overtake.
  extra=1+(s.catchUpMaxScale-1)*t*t*(3-2*t);
 }
 return std::clamp(s.engineScale*extra,1.f,8.f);
}
inline float Output(float native,float scale){
 return std::isfinite(native)&&native>0&&native<1e8f&&std::isfinite(scale)&&scale>=1&&scale<=8?native*scale:native;
}
inline unsigned FieldMask(unsigned count){return count&&count<=16?(1u<<count)-1:0;}
}
