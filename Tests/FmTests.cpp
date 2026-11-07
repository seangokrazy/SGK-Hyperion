#include "PcmInstrument.h"
#include <cstdio>
#include <limits>
using namespace sgk::research;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Failed %d: %s\n",__LINE__,#x);return 1;}}while(0)
static double magnitude(FmSettings s,double root,double bin){
    FmVoice voice;if(!voice.prepare(s,root,48000))return -1;
    for(unsigned i=0;i<4096;++i)voice.next();
    double real=0,imag=0;
    for(unsigned i=0;i<48000;++i){const double x=voice.next(),angle=6.28318530717958647692*bin*i/48000;
        real+=x*std::cos(angle);imag+=x*std::sin(angle);}
    return 2*std::hypot(real,imag)/48000;
}
int main(){
    FmSettings plain;
    const double fundamental=magnitude(plain,440,440);CHECK(fundamental>.98&&fundamental<1.01);
    CHECK(magnitude(plain,440,880)<1e-5);
    FmSettings mod=plain;mod.depthA=2;CHECK(magnitude(mod,440,880)>.02);
    // 30 kHz modulator A would alias to 18 kHz at 48 kHz without oversampling.
    // With no frequency modulation, it isolates the anti-alias filter's effect.
    FmSettings high=plain;high.ratioA=5;high.directA=1;
    CHECK(magnitude(high,6000,18000)<.01);CHECK(magnitude(high,6000,6000)>.9);
    for(double rate:{44100.,48000.,96000.,192000.})for(unsigned key:{36U,60U,84U}){
        FmSettings s;s.ratioA=3.5;s.ratioB=6;s.depthA=3;s.depthB=3;s.directA=3;s.transitionSeconds=.01;s.transition=2;
        s.velocityA=s.velocityB=1;
        FmVoice v;CHECK(v.prepare(s,440*std::pow(2.0,(double(key)-69)/12),rate,key,127));
        for(unsigned i=0;i<10000;++i){const double x=v.next();CHECK(std::isfinite(x)&&std::abs(x)<5);}
    }
    FmVoice reject;FmSettings bad;bad.ratioA=std::numeric_limits<double>::quiet_NaN();CHECK(!reject.prepare(bad,440,48000));
    bad=FmSettings{};bad.transition=-1;CHECK(!reject.prepare(bad,440,48000));CHECK(!reject.prepare(plain,440,48000,128));
    ZoneView zone;LayerView layer;layer.engine=4;layer.zones=&zone;layer.zoneCount=1;
    layer.amplitude={0,0,.8,.04,.01};layer.fm=mod;PcmInstrument engine;
    CHECK(engine.prepare(&layer,1,48000));CHECK(engine.send(0x90,60,100));CHECK(engine.send(0x90,67,100));
    double out[512]{};CHECK(engine.process(out,256));double peak=0;
    for(double x:out){CHECK(std::isfinite(x));peak=std::max(peak,std::abs(x));}CHECK(peak>.01);
    CHECK(engine.send(0xb0,64,127));CHECK(engine.send(0x80,60,0));CHECK(engine.send(0x80,67,0));
    for(unsigned i=0;i<100;++i)CHECK(engine.process(out,256));CHECK(engine.activePartials()==2);
    CHECK(engine.send(0xb0,64,0));for(unsigned i=0;i<100;++i)CHECK(engine.process(out,256));CHECK(engine.activePartials()==0);
    CHECK(engine.process(out,256));for(double x:out)CHECK(x==0);
    // Different host block sizes must not change FM phase/filter progression.
    FmVoice a,b;CHECK(a.prepare(mod,440,48000));CHECK(b.prepare(mod,440,48000));
    double expected[257];for(double& x:expected)x=a.next();
    unsigned count=0;for(unsigned n:{64U,64U,64U,65U})for(unsigned i=0;i<n;++i)CHECK(expected[count++]==b.next());
    std::puts("PASS: FM pitch/spectrum, sidebands, anti-alias filtering, rates, polyphony, release and block invariance");return 0;
}
