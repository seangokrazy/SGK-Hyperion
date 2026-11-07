#include "PcmInstrument.h"
#include <cstdio>
using namespace sgk::research;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(){
    float pcm[1001];for(unsigned i=0;i<1001;++i)pcm[i]=float(.3*std::sin(i*2*3.141592653589793/100));
    ZoneView zone;zone.sample={pcm,1001,1,44100};zone.endpoint=1000;zone.loopSpan=800;
    LayerView layer;layer.zones=&zone;layer.zoneCount=1;layer.amplitude={0,0,.8,.04,.01};
    PcmInstrument engine;double out[512]{};
    CHECK(engine.prepare(&layer,1,44100));CHECK(engine.send(0x90,60,100));CHECK(engine.send(0x90,67,100));
    CHECK(engine.activePartials()==2);CHECK(engine.send(0x80,60,0));
    for(int i=0;i<40;++i)CHECK(engine.process(out,256));
    CHECK(engine.activePartials()==1);CHECK(engine.send(0x80,67,0));
    for(int i=0;i<40;++i)CHECK(engine.process(out,256));
    CHECK(engine.activePartials()==0);CHECK(engine.process(out,256));for(double v:out)CHECK(v==0);
    CHECK(engine.send(0x90,60,100));CHECK(engine.send(0xb0,64,127));CHECK(engine.send(0x80,60,0));
    for(int i=0;i<40;++i)CHECK(engine.process(out,256));CHECK(engine.activePartials()==1);
    CHECK(engine.send(0xb0,64,0));for(int i=0;i<40;++i)CHECK(engine.process(out,256));CHECK(engine.activePartials()==0);
    CHECK(engine.send(0x90,60,100));CHECK(engine.send(0x90,60,80));CHECK(engine.send(0x90,60,0));
    for(int i=0;i<40;++i)CHECK(engine.process(out,256));CHECK(engine.activePartials()==1);
    CHECK(engine.send(0x80,60,0));for(int i=0;i<40;++i)CHECK(engine.process(out,256));CHECK(engine.activePartials()==0);
    LayerView layers[3]={layer,layer,layer};CHECK(engine.prepare(layers,3,44100));
    for(int i=0;i<42;++i)CHECK(engine.send(0x90,60,100));
    CHECK(engine.activePartials()==126);CHECK(!engine.send(0x90,60,100));CHECK(engine.activePartials()==126);
    CHECK(engine.send(0xb0,123,0));for(int i=0;i<10;++i)CHECK(engine.process(out,256));CHECK(engine.activePartials()==0);
    CHECK(!engine.send(0x90,128,100));CHECK(engine.activePartials()==0);
    ZoneView bad=zone;bad.endpoint=1002;LayerView invalid=layer;invalid.zones=&bad;
    CHECK(!engine.prepare(&invalid,1,44100));CHECK(engine.send(0x90,60,100));CHECK(engine.activePartials()==3);
    engine.reset();CHECK(engine.activePartials()==0);
    float stereo[2002];for(unsigned i=0;i<1001;++i){stereo[2*i]=pcm[i];stereo[2*i+1]=-pcm[i];}
    ZoneView zs=zone;zs.sample={stereo,1001,2,44100};LayerView ls=layer;ls.zones=&zs;
    CHECK(engine.prepare(&ls,1,44100));CHECK(engine.send(0x90,60,100));CHECK(engine.process(out,256));
    for(unsigned i=0;i<256;++i)CHECK(out[2*i]==-out[2*i+1]);
    CHECK(!engine.process(nullptr,1));
    std::puts("Native instrument checks passed");return 0;
}
