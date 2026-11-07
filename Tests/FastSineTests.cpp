#define SGK_HYPERION_FAST_SINE 1
#include "FmOscillator.h"
#include "ReferenceFmOscillator.h"
#include <cstring>
#include <cstdio>
using namespace sgk::research;
int main(){const auto* table=FastSine::prepare();double lookupError=0,error=0;
    for(unsigned n=0;n<65536;++n){const double phase=(n+.371)/65536;lookupError=std::max(lookupError,std::abs(FastSine::at(table,phase)-std::sin(6.28318530717958647692*phase)));}
    if(lookupError>7.4e-8)return 1;
    for(double rate:{44100.,48000.,96000.})for(unsigned key:{36U,60U,84U}){
        FmSettings s;s.ratioA=2;s.depthA=2.1;s.directA=.4;s.ratioB=3.5;s.depthB=1.7;s.transition=.4;s.transitionSeconds=.17;s.velocityA=.6;s.velocityB=.3;
        reference::FmSettings old;static_assert(sizeof old==sizeof s);std::memcpy(&old,&s,sizeof s);
        FmVoice fast;reference::FmVoice original;const double frequency=440*std::pow(2.,(double(key)-69)/12);
        if(!fast.prepare(s,frequency,rate,key,96)||!original.prepare(old,frequency,rate,key,96))return 2;
        for(unsigned n=0;n<unsigned(rate*2);++n)error=std::max(error,std::abs(fast.next()-original.next()));
    }
    std::printf("Lookup max error %.12g; two-second FM render max error %.12g\n",lookupError,error);
    if(error>1e-4)return 3;
    std::puts("PASS: prepared sine table bounds and FM numerical regression across keys/rates");return 0;
}
