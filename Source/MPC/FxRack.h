#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
namespace hyperionfx {
constexpr unsigned Max=66;
struct Values {std::uint32_t target{},id{};float p[12]{};float level{.5f};std::uint32_t enabled{1};};
struct Snapshot {std::uint32_t count{};std::array<Values,Max> values{};};
inline Snapshot capture(const sgk::research::PreparedPatch& patch){Snapshot s;unsigned target=0;
    auto add=[&](const sgk::research::FxSettings& f,unsigned t){if(f.id==128||s.count>=Max)return;auto& v=s.values[s.count++];v.target=t;v.id=f.id;for(unsigned j=0;j<12;++j)v.p[j]=float(f.p[j]);};
    for(const auto& node:patch.nodes)if(node.effect)add(node.fx,target++);
    for(unsigned j=0;j<2;++j)if(patch.returnEnabled[j])add(patch.returns[j],target+j);return s;
}
inline const char* name(unsigned id){switch(id){
case 0:return "Hall Reverb";case 1:return "Room Reverb";case 2:return "Plate Reverb";case 3:return "Spring Reverb";case 4:return "Reverse Reverb";case 5:return "Gated Reverb";case 6:return "Early Reflections";case 7:return "Tuned Resonator";case 8:return "Reverb";
case 10:return "Delay";case 11:return "Stereo Delay";case 12:return "Cross Delay";case 13:return "Tape Delay";case 14:return "Ducking Delay";case 15:return "Grain Delay";case 16:return "Diffuse Delay";case 19:return "Chorus Reverb";
case 20:return "Chorus";case 21:return "Ensemble";case 22:return "Dimension Chorus";case 23:return "Chorus II";case 30:return "Flanger";case 31:return "Flanger II";
case 40:return "Phaser";case 41:return "Phaser II";case 42:return "Phaser III";case 50:return "Auto Pan";case 51:return "Stereo Width";case 52:return "Stereo Spread";case 54:return "Scanner";case 55:return "Rotary";case 56:return "Rotary II";
case 60:return "Pitch Shift";case 61:return "Detune";case 70:return "Filter";case 71:return "Auto Filter";case 72:return "Talkbox";case 80:return "Shelf EQ";case 81:return "Parametric EQ";case 85:return "Enhancer";
case 90:return "Limiter";case 91:return "Compressor";case 92:return "Multiband Compressor";case 93:return "Punch";case 95:return "Gate";case 100:return "Overdrive";case 101:return "Distortion";case 102:return "Bit Crusher";case 103:return "Tube Drive";case 104:return "Fuzz";case 105:return "Saturation";case 110:return "Ring Modulator";default:return "Effect";}}
struct Control {int raw;const char* label;};
inline Control control(unsigned id,unsigned n){
    static const Control verb[]={{0,"Mix"},{2,"Decay"},{3,"Damping"}};
    static const Control delay[]={{0,"Mix"},{1,"Tempo Sync"},{2,"Time"},{3,"Feedback"},{5,"Tone"},{4,"Stereo / Texture"}};
    static const Control modulation[]={{0,"Mix"},{2,"Rate"},{3,"Depth"},{4,"Feedback"}};
    static const Control eq[]={{0,"Output"},{2,"Low Gain"},{3,"High Gain"}};
    static const Control peq[]={{0,"Output"},{2,"Band 1 Gain"},{3,"Band 1 Frequency"},{4,"Band 1 Q"},{5,"Band 2 Gain"},{6,"Band 2 Frequency"},{7,"Band 2 Q"}};
    if(id==81)return n<7?peq[n]:Control{-1,""};if(id==80)return n<3?eq[n]:Control{-1,""};
    if(id<=8||id==19){auto c=n<3?verb[n]:Control{-1,""};if(id==7&&n==1)c.label="Root Pitch";if(id==19&&n==1)c.label="Decay / Movement";return c;}
    if(id>=10&&id<=16)return n<((id==11||id==15||id==16)?6U:5U)?delay[n]:Control{-1,""};
    if((id>=20&&id<=31)||id==54||id==55||id==56||id==50)return n<((id==30||id==31)?4U:3U)?modulation[n]:Control{-1,""};
    if(id>=40&&id<=42)return n<2?modulation[n]:Control{-1,""};
    if(n==0)return {0,"Mix"};if(n==1)return {2,id==51||id==52?"Width":id==60?"Pitch":id==61?"Detune":id==70||id==71?"Cutoff":id==72?"Vowel":id==90||id==91||id==92||id==95?"Threshold":id==93?"Punch":id==102?"Bit Depth":id==110?"Rate":id>=100?"Drive":"Amount"};
    if(n==2){if(id==70||id==71||id==72)return {3,"Envelope Depth"};if(id==91||id==92)return {3,"Ratio"};if(id==102)return {3,"Sample Hold"};if(id==110)return {3,"Stereo Phase"};}return {-1,""};
}
inline unsigned count(unsigned id){unsigned n=0;while(n<12&&control(id,n).raw>=0)++n;return n;}
inline std::string format(const Values& v,int raw,double bpm){char b[64];const double x=v.p[raw];
    if(raw==1&&v.id>=10&&v.id<=16)return x>=.5?"SYNC":"FREE";
    if(raw==2&&v.id>=10&&v.id<=16){if(v.p[1]>.5){const char* labels[]={"1/32","1/16","1/8","1/8 D","1/4","1/4 D","1/2","1/2 D","1 BAR","1 BAR D","2 BARS"};const double beats[]={.125,.25,.5,.75,1,1.5,2,3,4,6,8};const auto i=std::min(10U,unsigned((1-x)*10.999));if(60/bpm*beats[i]<=3.9)return labels[i];return "3900 ms (max)";}std::snprintf(b,sizeof b,"%.0f ms",1000*std::min(3.9,.001+3.999*x*x));return b;}
    if((raw==5&&v.id>=10&&v.id<=16)||(raw==2&&(v.id==70||v.id==71))||(v.id==81&&(raw==3||raw==6))){std::snprintf(b,sizeof b,"%.0f Hz",20*std::pow(1000.,x));return b;}
    if((raw==0&&(v.id==80||v.id==81))||(v.id==80&&(raw==2||raw==3))||(v.id==81&&(raw==2||raw==5))){const double span=raw==0&&v.id==81?48:24;std::snprintf(b,sizeof b,"%+.1f dB",(x-.5)*span);return b;}
    if(v.id==81&&(raw==4||raw==7)){std::snprintf(b,sizeof b,"Q %.2f",.2+6*x*x);return b;}
    if(raw==2&&v.id==7){std::snprintf(b,sizeof b,"%.0f Hz",110*std::pow(4.,x));return b;}
    if(raw==2&&(v.id==60||v.id==61)){std::snprintf(b,sizeof b,v.id==60?"%+.0f st":"%+.1f cents",v.id==60?std::round(48*x)-24:(x-.5)*50);return b;}
    if(raw==2&&v.id>=90&&v.id<=95&&v.id!=93){std::snprintf(b,sizeof b,"%.1f dB",-48+48*x);return b;}
    if(raw==2&&((v.id>=20&&v.id<=42)||v.id==50||v.id==54||v.id==55||v.id==56||v.id==110)){std::snprintf(b,sizeof b,"%.2f Hz",v.id==110?20*std::pow(100.,x):.05+7*x*x);return b;}
    if(raw==3&&(v.id==91||v.id==92)){std::snprintf(b,sizeof b,"%.1f : 1",1+19*x);return b;}
    std::snprintf(b,sizeof b,"%.0f %%",x*100);return b;
}
}
