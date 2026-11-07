#pragma once
#include "VST2ABI.h"
#include <cmath>
namespace sgkzen {
// Standard VST2 host time layout, also used by the existing SGK Motion wrapper.
// Request tempo only: accepting tempo does not imply PPQ/bar-phase locking.
struct HostTimeInfo {
 double samplePos,sampleRate,nanoSeconds,ppqPos,tempo,barStartPos,cycleStartPos,cycleEndPos;
 int32_t numerator,denominator,smpteOffset,smpteFrameRate,samplesToNextClock,flags;
};
inline double readHostTempo(AEffect*effect,Host host,double previous){
 if(!std::isfinite(previous)||previous<20||previous>400)previous=120;
 if(!host)return previous;
 auto*time=reinterpret_cast<const HostTimeInfo*>(host(effect,7,0,1<<10,nullptr,0));
 if(!time||!(time->flags&(1<<10))||!std::isfinite(time->tempo)||time->tempo<20||time->tempo>400)return previous;
 return time->tempo;
}
}
