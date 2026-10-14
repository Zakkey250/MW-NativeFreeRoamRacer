#pragma once
#include "Types.h"
namespace mwneo {
// Physics time, not wall-clock time: pauses and streaming stalls cannot trip
// recovery. Reversing or rocking in place is not forward race progress.
struct JamWatch {
 bool active=false;uint32_t event=0,lap=0,edge=0;float age=0,progress=0,retry=0;
 Point anchor{};
 void Reset(){*this={};}
 bool Update(const Vehicle& v,const Guidance& g,Recovery recovery){
  if(!g.status||!Finite(v.position)||!std::isfinite(g.progress)){Reset();return false;}
  if(!std::isfinite(v.dt)||v.dt<=0)return false;
  float dt=std::min(v.dt,.1f);retry=std::max(0.f,retry-dt);
  if(active&&(event!=g.event||lap!=g.lap||edge!=g.edge))Reset();
  bool aligned=DotXZ(UnitXZ(v.heading),UnitXZ(g.forward))>.7f;
  bool contact=g.leadVehicle==RaceTrafficPush&&g.obstacleDistance<12;
  bool gatePending=(g.predictiveAvoidance&PendingGateReacquire)!=0;
  bool struggling=gatePending||g.status==2||recovery!=Recovery::Normal||contact;
  if(!active){
   // A recovery already requested from measured body motion remains a jam
   // even when wheel spin reports a nonzero native speed.
   if(!struggling||(!gatePending&&v.speed>5&&recovery==Recovery::Normal))return false;
   active=true;event=g.event;lap=g.lap;edge=g.edge;progress=g.progress;anchor=v.position;age=0;
  }
  // Quantized projection jumps and spinning wheels must not cancel a jam.
  // A real forward rejoin does, even if an avoidance flag is still present.
  bool advance=g.progress>=progress+3&&LengthXZ(Sub(v.position,anchor))>=3&&aligned;
  bool running=g.status==1&&recovery==Recovery::Normal&&v.speed>5&&aligned&&!contact&&
     DotXZ(Sub(v.position,anchor),UnitXZ(g.forward))>=5;
  if(!gatePending&&(advance||running)){Reset();return false;}
  age+=dt;return age>=(gatePending?.5f:3.f)&&retry<=0&&(!gatePending||std::abs(v.speed)<5);
 }
 void Attempted(){retry=1.f;}
};
}
