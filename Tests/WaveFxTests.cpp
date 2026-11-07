#include "GraphPlayer.h"
#include <cstdio>
using namespace sgk::research;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Failed %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    std::vector<float> table(33152);std::size_t at=0;
    for(unsigned n=256;n>=2;n/=2)for(unsigned frame=0;frame<64;++frame)for(unsigned j=0;j<=n;++j)
        table[at++]=j==n?0:float(std::sin(6.283185307179586*j/n)*(.25+.75*frame/63));
    CHECK(at==table.size());WavetableSettings ws;ws.table={table.data(),table.size(),256,64};
    WavetableVoice quiet,loud;CHECK(quiet.prepare(ws,440,48000));ws.position=1;CHECK(loud.prepare(ws,440,48000));
    for(unsigned j=0;j<48000;++j){const double a=quiet.next(),b=loud.next();CHECK(std::isfinite(b)&&std::abs(b-4*a)<1e-6);}
    ws.table.samples=3;CHECK(!loud.prepare(ws,440,48000));ws.table.samples=table.size();
    CHECK(loud.prepare(ws,24000,48000));CHECK(loud.next()==0);
    AnalogSettings analog;analog.modeA=6;analog.table=ws.table;analog.tablePosition=.5;
    AnalogVoice combined;CHECK(combined.prepare(analog,440,48000));double analogEnergy=0;
    for(unsigned i=0;i<4800;++i){const auto x=combined.next();CHECK(std::isfinite(x));analogEnergy+=x*x;}CHECK(analogEnergy>1);
    for(unsigned mode:{5U,6U}){analog.modeA=7;analog.extraWave=mode;analog.extraLevel=.5;CHECK(combined.prepare(analog,440,48000));
        double energy=0;for(unsigned i=0;i<4800;++i){const auto x=combined.next();CHECK(std::isfinite(x));energy+=x*x;}CHECK(energy>1);}
    DrawbarSettings organ;organ.levels[2]=1;DrawbarVoice drawbar;CHECK(drawbar.prepare(organ,440,48000));
    double fundamental=0,organEnergy=0;for(unsigned i=0;i<48000;++i){const auto x=drawbar.next();CHECK(std::isfinite(x));organEnergy+=x*x;fundamental+=x*std::sin(6.283185307179586*440*i/48000);}
    CHECK(organEnergy>100&&fundamental>3500);CHECK(drawbar.prepare(organ,24000,48000));for(unsigned i=0;i<100;++i)CHECK(drawbar.next()==0);
    unsigned types=0;for(unsigned id=0;id<=128;++id)if(supportedFx(id)){
        FxSettings s;s.id=id;s.p.fill(.5);FxProcessor effect;CHECK(effect.prepare(s,48000));++types;
        double block[512]{};for(unsigned b=0;b<375;++b){for(unsigned j=0;j<256;++j){const unsigned n=b*256+j;block[2*j]=n<24000?.1*std::sin(n*.13):0;block[2*j+1]=n<24000?.1*std::cos(n*.17):0;}
            CHECK(effect.process(block,256,140));for(double x:block)CHECK(std::isfinite(x)&&std::abs(x)<30);}
        effect.reset();for(double& x:block)x=0;CHECK(effect.process(block,256));for(double x:block)CHECK(x==0);
    }
    for(unsigned id:{80U,81U}){FxSettings s;s.id=id;s.p.fill(.5);FxProcessor eq;CHECK(eq.prepare(s,48000));
        double b[512],expected[512];for(unsigned j=0;j<512;++j)b[j]=expected[j]=std::sin(j*.17)*.1;
        CHECK(eq.process(b,256));for(unsigned j=0;j<512;++j)CHECK(std::abs(b[j]-expected[j])<1e-12);}
    FxSettings delay;delay.id=10;delay.p[0]=1;delay.p[5]=1;FxProcessor echo;CHECK(echo.prepare(delay,48000));
    double b[512]{};b[0]=b[1]=1;CHECK(echo.process(b,256));for(unsigned j=0;j<48;++j)CHECK(b[j*2]==0);CHECK(std::abs(b[96])>.5);
    PreparedPatch patch;patch.layers.resize(1);patch.zones.resize(1);patch.zones[0].push_back(ZoneView{});
    auto& l=patch.layers[0];l.engine=5;l.zones=patch.zones[0].data();l.zoneCount=1;l.amplitude={0,0,.8,.01,.01};
    patch.nodes.push_back(PatchNode{false,0,FxSettings{}});patch.returnEnabled[0]=true;patch.returnSend[0]=.3;
    patch.returns[0].id=1;patch.returns[0].p.fill(.5);patch.returns[0].p[0]=1;
    GraphPlayer player;CHECK(player.prepare(patch,48000));CHECK(player.send(0x90,60,100));for(unsigned i=0;i<30;++i)CHECK(player.process(b,256));
    CHECK(player.send(0x80,60,0));double tail=0;for(unsigned i=0;i<150;++i){CHECK(player.process(b,256));if(i>100)for(double x:b)tail+=std::abs(x);}
    CHECK(player.activePartials()==0&&tail>0);player.reset();CHECK(player.process(b,256));for(double x:b)CHECK(x==0);
    // Sustain- and mod-wheel-gated layers must not become unconditional notes.
    l.trigger=1;patch.returnEnabled[0]=false;CHECK(player.prepare(patch,48000));CHECK(player.send(0x90,60,100));CHECK(player.activePartials()==0);
    CHECK(player.send(0xb0,1,127));CHECK(player.send(0x90,60,100));CHECK(player.activePartials()==1);player.reset();
    l.trigger=4;CHECK(player.prepare(patch,48000));CHECK(player.send(0x90,60,100));CHECK(player.activePartials()==0);
    CHECK(player.send(0xb0,64,127));CHECK(player.send(0x90,60,100));CHECK(player.activePartials()==1);
    std::printf("PASS: waveform morph/mips, %u FX types, flat EQ, delay timing, returns, reset and MIDI conditions\n",types);return 0;
}
