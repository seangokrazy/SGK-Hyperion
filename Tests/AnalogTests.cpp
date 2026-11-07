#include "PcmInstrument.h"
#include <cstdio>
using namespace sgk::research;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Failed %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    constexpr double pi=3.14159265358979323846;
    for(double rate:{44100.,48000.,96000.})for(unsigned kind=0;kind<4;++kind){
        AnalogOscillator osc;CHECK(osc.prepare(AnalogWave(kind),440,rate));
        double sum=0,energy=0;
        for(int n=0;n<int(rate);++n){const auto x=osc.next();CHECK(std::isfinite(x)&&std::abs(x)<=1.01);sum+=x;energy+=x*x;}
        CHECK(energy>100);CHECK(std::abs(sum/rate)<.02);
    }
    // High-note saw: compare a folded fifth harmonic with an uncorrected saw.
    double cleanReal=0,cleanImag=0,rawReal=0,rawImag=0;
    AnalogOscillator clean;CHECK(clean.prepare(AnalogWave::saw,7000,48000));
    for(unsigned i=0;i<48000;++i){const double angle=2*pi*13000*i/48000;
        const double c=clean.next(),r=2*std::fmod(double(i)*7000/48000,1)-1;
        cleanReal+=c*std::cos(angle);cleanImag+=c*std::sin(angle);
        rawReal+=r*std::cos(angle);rawImag+=r*std::sin(angle);
    }
    CHECK(std::hypot(cleanReal,cleanImag)<std::hypot(rawReal,rawImag)*.2);
    AnalogOscillator silent;CHECK(silent.prepare(AnalogWave::saw,24000,48000));CHECK(silent.next()==0);
    CHECK(!silent.prepare(AnalogWave::saw,-1,48000));CHECK(!silent.prepare(AnalogWave::pulse,440,48000,0));
    AnalogOscillator a,b;CHECK(a.prepare(AnalogWave::noise,440,48000,.5,0,123));CHECK(b.prepare(AnalogWave::noise,440,48000,.5,0,123));
    for(int i=0;i<100;++i)CHECK(a.next()==b.next());
    // Synth voices use the shared note/envelope/filter path with no PCM data.
    ZoneView zone;LayerView layer;layer.engine=5;layer.zones=&zone;layer.zoneCount=1;
    layer.amplitude={0,0,.8,.04,.01};layer.analog.modeB=1;layer.analog.levelB=.3;
    PcmInstrument engine;CHECK(engine.prepare(&layer,1,48000));CHECK(engine.send(0x90,60,100));
    CHECK(engine.send(0x90,67,100));double out[512]{};CHECK(engine.process(out,256));
    double peak=0;for(double v:out){CHECK(std::isfinite(v));peak=std::max(peak,std::abs(v));}CHECK(peak>.01);
    CHECK(engine.send(0xb0,64,127));CHECK(engine.send(0x80,60,0));CHECK(engine.send(0x80,67,0));
    for(int i=0;i<30;++i)CHECK(engine.process(out,256));CHECK(engine.activePartials()==2);
    CHECK(engine.send(0xb0,64,0));for(int i=0;i<100;++i)CHECK(engine.process(out,256));CHECK(engine.activePartials()==0);
    CHECK(engine.process(out,256));for(double v:out)CHECK(v==0);
    layer.analog.modeA=6;CHECK(!engine.prepare(&layer,1,48000));
    std::puts("PASS: analog oscillator rates, bounds, noise determinism and high-note alias reduction");return 0;
}
