#include "GraphPlayer.h"
#include <cstdio>
#include <cstdlib>
#include <new>
static bool audioThread=false;static unsigned allocations=0;
void* operator new(std::size_t n){if(audioThread)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Failed %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace sgk::research;
int main(){
    double b[512];unsigned tested=0;
    for(unsigned id=0;id<128;++id)if(supportedFx(id)){
        FxSettings settings;settings.id=id;settings.p.fill(.5);FxProcessor fx;CHECK(fx.prepare(settings,48000));
        audioThread=true;
        for(unsigned block=0;block<100;++block){for(unsigned j=0;j<12;++j)CHECK(fx.setControl(j,(block&1)?.1:.9));
            for(unsigned j=0;j<512;++j)b[j]=.1*std::sin((block*512+j)*.1);CHECK(fx.process(b,256,140));for(double x:b)CHECK(std::isfinite(x)&&std::abs(x)<50);}
        fx.resetControls(true);fx.reset();for(auto& x:b)x=0;CHECK(fx.process(b,256));for(double x:b)CHECK(x==0);audioThread=false;++tested;
    }
    // Editing mix preserves a running delay tail, rather than re-preparing it.
    FxSettings settings;settings.id=10;settings.p[0]=1;settings.p[2]=.1;settings.p[3]=.7;settings.p[5]=1;FxProcessor fx;CHECK(fx.prepare(settings,48000));
    for(auto& x:b)x=0;b[0]=b[1]=1;CHECK(fx.process(b,256));CHECK(fx.setControl(0,.8));double tail=0;
    for(unsigned k=0;k<20;++k){for(auto& x:b)x=0;CHECK(fx.process(b,256));for(double x:b)tail+=std::abs(x);}CHECK(tail>.1);
    PreparedPatch patch;patch.layers.resize(1);patch.zones.resize(1);patch.zones[0].push_back(ZoneView{});auto& l=patch.layers[0];l.engine=5;l.zones=patch.zones[0].data();l.zoneCount=1;l.amplitude={0,0,.8,.01,.01};
    patch.nodes.push_back(PatchNode{false,0,FxSettings{}});settings.id=100;settings.p.fill(.5);patch.nodes.push_back(PatchNode{true,0,settings});patch.returnEnabled[0]=true;patch.returnSend[0]=.3;patch.returns[0].id=1;patch.returns[0].p.fill(.5);
    GraphPlayer edited,dry;CHECK(edited.prepare(patch,48000));CHECK(dry.prepare(patch,48000));dry.bypassFx(true);CHECK(edited.send(0x90,60,100));CHECK(dry.send(0x90,60,100));
    CHECK(edited.setEffectControl(0,13,0,true));CHECK(edited.setEffectControl(1,13,0,true));double expected[512];audioThread=true;
    for(unsigned k=0;k<100;++k){CHECK(edited.process(b,256));CHECK(dry.process(expected,256));for(unsigned j=0;j<512;++j)CHECK(b[j]==expected[j]);}
    CHECK(edited.setEffectControl(0,13,1));CHECK(edited.setEffectControl(1,13,1));CHECK(edited.setEffectControl(0,12,.2));
    for(unsigned k=0;k<100;++k){CHECK(edited.process(b,256));for(double x:b)CHECK(std::isfinite(x));}
    CHECK(edited.setEffectControl(0,255,0));CHECK(!edited.setEffectControl(66,0,.5));audioThread=false;CHECK(allocations==0);
    std::printf("PASS: %u live FX types, tail preservation, insert/return bypass, reset and zero audio-thread allocations\n",tested);
}
