#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace sgk::research {
struct FxSettings {unsigned id{128},output{};double gain{1},pan{.5};std::array<double,12> p{};std::array<double,4> sends{};};
inline bool supportedFx(unsigned id)noexcept {
    switch(id){case 0:case 1:case 2:case 3:case 4:case 5:case 6:case 7:case 8:
    case 10:case 11:case 12:case 13:case 14:case 15:case 16:case 19:
    case 20:case 21:case 22:case 23:case 30:case 31:case 40:case 41:case 42:
    case 50:case 51:case 52:case 54:case 55:case 56:case 60:case 61:case 70:case 71:case 72:
    case 80:case 81:case 85:case 90:case 91:case 92:case 93:case 95:
    case 100:case 101:case 102:case 103:case 104:case 105:case 110:case 128:return true;default:return false;}
}
class FxBiquad {
    double b0_{1},b1_{},b2_{},a1_{},a2_{},z1_[2]{},z2_[2]{};
public:
    void shelf(double rate,double frequency,double db,bool high)noexcept {
        const double w=6.283185307179586*std::clamp(frequency,10.,rate*.45)/rate,c=std::cos(w),A=std::pow(10.,db/40),t=std::sqrt(2*A)*std::sin(w);
        double d;if(high){d=(A+1)-(A-1)*c+t;b0_=A*((A+1)+(A-1)*c+t)/d;b1_=-2*A*((A-1)+(A+1)*c)/d;b2_=A*((A+1)+(A-1)*c-t)/d;a1_=2*((A-1)-(A+1)*c)/d;a2_=((A+1)-(A-1)*c-t)/d;}
        else{d=(A+1)+(A-1)*c+t;b0_=A*((A+1)-(A-1)*c+t)/d;b1_=2*A*((A-1)-(A+1)*c)/d;b2_=A*((A+1)-(A-1)*c-t)/d;a1_=-2*((A-1)+(A+1)*c)/d;a2_=((A+1)+(A-1)*c-t)/d;}
    }
    void peak(double rate,double hz,double db,double q)noexcept {
        const double w=6.283185307179586*std::clamp(hz,10.,rate*.45)/rate,c=std::cos(w),a=std::sin(w)/(2*std::clamp(q,.1,10.)),A=std::pow(10.,db/40),d=1+a/A;
        b0_=(1+a*A)/d;b1_=-2*c/d;b2_=(1-a*A)/d;a1_=-2*c/d;a2_=(1-a/A)/d;
    }
    double next(unsigned channel,double x)noexcept {const double y=b0_*x+z1_[channel];z1_[channel]=b1_*x-a1_*y+z2_[channel];z2_[channel]=b2_*x-a2_*y;return y;}
    void reset()noexcept{for(auto& x:z1_)x=0;for(auto& x:z2_)x=0;}
};
// Independent effect models. Normalized preset controls are retained, while
// unmeasured response curves/subtype behavior are prototype approximations.
// All storage is allocated by prepare(), on the non-audio loader thread.
class FxProcessor {
    struct Line {std::vector<float> data;std::size_t at{},filled{};
        void prepare(std::size_t n){data.assign(n,0);at=filled=0;}
        double read(double distance)const noexcept {if(data.empty()||distance>filled)return 0;distance=std::clamp(distance,1.,double(data.size()-2));
            double p=double(at)-distance;while(p<0)p+=data.size();const auto a=std::size_t(p),b=(a+1)%data.size();return data[a]+(p-a)*(data[b]-data[a]);}
        void push(double value)noexcept{if(data.empty())return;data[at]=float(value);at=(at+1)%data.size();filled=std::min(filled+1,data.size());}
        void reset()noexcept{at=filled=0;}
    } delay_[2],verb_[8];
    FxSettings settings_;double rate_{44100},phase_{},pitchPhase_{},low_[2]{},high_[2]{},envelope_{},hold_[2]{},allpass_[2][6]{};
    double damp_[8]{},delaySamples_{},outputGain_{1},slowEnvelope_{},bandEnv_[3]{},split_[2][2]{};unsigned counter_{};FxBiquad eq_[3];
    std::array<double,12> original_{},target_{};double mixNow_{},gainNow_{1},smoothing_{};bool controlsDirty_{};
    static double unit(double x)noexcept{return std::clamp(x,0.,1.);}
    static double hz(double x)noexcept{return 20*std::pow(1000.,unit(x));}
    void configureControls()noexcept{
        const auto id=settings_.id;const auto& p=settings_.p;outputGain_=1;
        if(id==80){eq_[0].shelf(rate_,100,(p[2]-.5)*24,false);eq_[1].shelf(rate_,5000,(p[3]-.5)*24,true);outputGain_=std::exp(p[0]*2.772588722239781-1.3862943611198906);}
        if(id==81){eq_[0].peak(rate_,hz(p[3]),(p[2]-.5)*24,.2+6*p[4]*p[4]);eq_[1].peak(rate_,hz(p[6]),(p[5]-.5)*24,.2+6*p[7]*p[7]);outputGain_=std::exp(p[0]*5.545177444479562-2.772588722239781);}
        if(id==72){eq_[0].peak(rate_,450+750*p[2],18,3);eq_[1].peak(rate_,1200+1500*(1-p[2]),15,4);}
        controlsDirty_=false;
    }
public:
    bool prepare(const FxSettings& s,double rate){
        if(!supportedFx(s.id)||!std::isfinite(rate)||rate<8000||rate>192000||!std::isfinite(s.gain)||s.gain<0||s.gain>4||
           !std::isfinite(s.pan)||s.pan<0||s.pan>1)return false;
        for(double x:s.p)if(!std::isfinite(x)||x<0||x>1)return false;
        for(double x:s.sends)if(!std::isfinite(x)||x<0||x>4)return false;
        settings_=s;rate_=rate;const auto id=s.id;original_=target_=s.p;smoothing_=-std::expm1(-1/(rate*.01));
        if(id==4||(id>=10&&id<=31)||id==54||id==55||id==56||id==60||id==61)
            for(auto& d:delay_)d.prepare(std::size_t(rate*(id<=19?4:.25))+8);
        if(id<=8||id==19){const double times[]={.0297,.0371,.0411,.0437,.0307,.0323,.0393,.0451};
            for(unsigned j=0;j<8;++j){const double resonator=110*(1+j*.5);verb_[j].prepare(std::size_t(id==7?rate/resonator:rate*times[j])+8);}}
        configureControls();
        reset();return true;
    }
    unsigned id()const noexcept{return settings_.id;}
    bool setControl(unsigned parameter,double value,bool immediate=false)noexcept{
        if(parameter>=12||!std::isfinite(value)||value<0||value>1)return false;target_[parameter]=value;
        if(immediate){settings_.p[parameter]=value;configureControls();mixNow_=settings_.p[0];gainNow_=outputGain_;}return true;
    }
    void resetControls(bool immediate=false)noexcept{target_=original_;if(immediate){settings_.p=original_;configureControls();mixNow_=settings_.p[0];gainNow_=outputGain_;}}
    void reset()noexcept {phase_=pitchPhase_=envelope_=0;counter_=0;delaySamples_=0;mixNow_=settings_.p[0];gainNow_=outputGain_;for(auto& l:delay_)l.reset();for(auto& l:verb_)l.reset();
        for(auto& e:eq_)e.reset();slowEnvelope_=0;for(auto& e:bandEnv_)e=0;for(unsigned c=0;c<2;++c){low_[c]=high_[c]=hold_[c]=0;split_[c][0]=split_[c][1]=0;for(double& x:allpass_[c])x=0;}for(double& x:damp_)x=0;}
    bool process(double* audio,std::size_t frames,double bpm=120)noexcept {
        if(!audio&&frames)return false;const auto id=settings_.id;if(id==128)return true;
        const double alpha=-std::expm1(-double(frames)/(rate_*.015));bool moving=false;
        for(unsigned j=0;j<12;++j)if(settings_.p[j]!=target_[j]){const double difference=target_[j]-settings_.p[j];settings_.p[j]=std::abs(difference)<1e-7?target_[j]:settings_.p[j]+alpha*difference;moving=true;}
        if(moving&&(id==80||id==81||id==72))controlsDirty_=true;if(controlsDirty_)configureControls();const auto& p=settings_.p;
        if(!std::isfinite(bpm)||bpm<20||bpm>400)bpm=120;
        double seconds=.001+3.999*p[2]*p[2];
        if(p[1]>.5){static constexpr double beats[]={.125,.25,.5,.75,1,1.5,2,3,4,6,8};seconds=60/bpm*beats[std::min(10U,unsigned((1-p[2])*10.999))];}
        seconds=std::clamp(seconds,.001,3.9);const double targetDelay=seconds*rate_,feedback=std::min(.92,p[3]*.94);
        if(delaySamples_==0)delaySamples_=targetDelay;
        const double speed=id==110?20*std::pow(100.,p[2]):.05+7*p[2]*p[2],lp=1-std::exp(-6.283185307179586*std::min(hz(p[5]),rate_*.4)/rate_);
        double resonatorDistance[8]{};if(id<=8||id==19){const double root=110*std::pow(4.,p[2]);for(unsigned j=0;j<8;++j)resonatorDistance[j]=id==7?std::floor(rate_/(root*(1+j*.5)))+5:double(verb_[j].data.size()-3);}
        // Block-invariant coefficients: preserve the existing algorithms while avoiding
        // repeated pow/exp/sqrt calls for every sample and channel.
        const double pitchWindow=rate_*.06;
        const double pitchStep=(id==60||id==61)?(1-std::pow(2.,(id==60?std::round(p[2]*48)-24:(p[2]-.5)*.5)/12))/pitchWindow:0;
        const double threshold=(id>=90&&id<=95)?std::pow(10.,(-48+48*p[2])/20):1,ratio=1+19*p[3];
        const double splitA=id==92?1-std::exp(-6.283185307179586*300/rate_):0,splitB=id==92?1-std::exp(-6.283185307179586*3000/rate_):0;
        const double driveScale=1+24*p[2]*p[2],driveComp=(id>=100&&id<=105)?1/std::sqrt(driveScale):1;
        const unsigned holdSamples=1+unsigned(63*p[3]*p[3]);const double bitSteps=id==102?std::pow(2.,2+20*p[2]):1;
        const bool needsLfo=(id>=40&&id<=42)||id==50||id==71;
        for(std::size_t i=0;i<frames;++i){
            const double input[2]={audio[2*i],audio[2*i+1]};double wet[2]={input[0],input[1]};
            phase_+=speed/rate_;phase_-=std::floor(phase_);const double lfo=needsLfo?std::sin(phase_*6.283185307179586):0;
            envelope_+=(std::max(std::abs(input[0]),std::abs(input[1]))-envelope_)*(envelope_<std::max(std::abs(input[0]),std::abs(input[1]))?.02:.0002);
            slowEnvelope_+=(std::max(std::abs(input[0]),std::abs(input[1]))-slowEnvelope_)*.001;
            if(id<=8||id==19){
                double source[2]={input[0],input[1]};
                if(id==19)for(unsigned c=0;c<2;++c){const double tap=delay_[c].read(rate_*(.012+.003*(1+std::sin((phase_+c*.25)*6.283185307179586))));delay_[c].push(input[c]);source[c]=.7*input[c]+.3*tap;}
                wet[0]=wet[1]=0;for(unsigned j=0;j<8;++j){const double tap=verb_[j].read(resonatorDistance[j]);
                    damp_[j]+=(tap-damp_[j])*(.03+.8*(1-p[3]));const double drive=(source[0]+source[1])*.15;
                    verb_[j].push(drive+damp_[j]*(id==6?0:(id==7?.85:.4+.55*p[2])));wet[j&1]+=tap*.25;}
                if(id==4){const double window=rate_*.08,phase=std::fmod(double(counter_),window)/window,other=phase<.5?phase+.5:phase-.5,w=.5-.5*std::cos(phase*6.283185307179586);
                    for(unsigned c=0;c<2;++c){delay_[c].push(wet[c]);wet[c]=delay_[c].read(window*(1+2*phase))*w+delay_[c].read(window*(1+2*other))*(1-w);}}
                if(id==5){const double gate=envelope_>.005?1:0;wet[0]*=gate;wet[1]*=gate;}
            }
            else if(id>=10&&id<=16){
                delaySamples_+=.001*(targetDelay-delaySamples_);const double dist=std::max(1.,delaySamples_);
                for(unsigned c=0;c<2;++c){wet[c]=delay_[c].read(dist*(c&&id==11?.5+p[4]:1));low_[c]+=(wet[c]-low_[c])*lp;wet[c]=low_[c];}
                for(unsigned c=0;c<2;++c){double x=input[c]+wet[id==11?1-c:c]*feedback;if(id==13)x=std::tanh(x);delay_[c].push(x);}
                if(id==14){const double duck=1/(1+envelope_*8);wet[0]*=duck;wet[1]*=duck;}
                if(id==15){const double window=rate_*(.02+.08*p[4]),phase=std::fmod(double(counter_),window)/window,other=phase<.5?phase+.5:phase-.5,w=.5-.5*std::cos(phase*6.283185307179586);
                    for(unsigned c=0;c<2;++c)wet[c]=delay_[c].read(dist+window*phase)*w+delay_[c].read(dist+window*other)*(1-w);}
                if(id==16){for(unsigned c=0;c<2;++c)for(unsigned j=0;j<4;++j){const double x=wet[c],a=.25+.4*p[4];wet[c]=-a*x+allpass_[c][j];allpass_[c][j]=x+a*wet[c];}}
            }
            else if((id>=20&&id<=31)||id==54||id==55||id==56){
                for(unsigned c=0;c<2;++c){const double motion=std::sin((phase_+c*.25)*6.283185307179586);
                    const double base=(id>=30&&id<=31)?.002:.012;
                    wet[c]=delay_[c].read(rate_*(base+.003*p[3]*(1+motion)));
                    delay_[c].push(input[c]+wet[c]*((id>=30&&id<=31)?p[4]*.75:0));
                    if(id==21||id==22||id==23)wet[c]=(wet[c]+delay_[c].read(rate_*(base+.006*(1-motion))))*.5;
                    if(id==55||id==56)wet[c]*=.65+.35*motion;
                }
            }
            else if(id>=40&&id<=42){
                const double a=.1+.8*(.5+.5*lfo);for(unsigned c=0;c<2;++c){double x=input[c];for(unsigned j=0;j<(id==41?6U:4U);++j){const double y=-a*x+allpass_[c][j];allpass_[c][j]=x+a*y;x=y;}wet[c]=x;}
            }
            else if(id==50){wet[0]=input[0]*(1-p[3]*(.5+.5*lfo));wet[1]=input[1]*(1-p[3]*(.5-.5*lfo));}
            else if(id==51||id==52){const double mid=(input[0]+input[1])*.5,side=(input[0]-input[1])*(id==52?.5+p[2]:p[2]);wet[0]=mid+side;wet[1]=mid-side;}
            else if(id==60||id==61){
                const double window=pitchWindow;
                pitchPhase_+=pitchStep;pitchPhase_-=std::floor(pitchPhase_);
                const double b=pitchPhase_<.5?pitchPhase_+.5:pitchPhase_-.5,w=.5-.5*std::cos(6.283185307179586*pitchPhase_);
                for(unsigned c=0;c<2;++c){delay_[c].push(input[c]);wet[c]=delay_[c].read(1+pitchPhase_*window)*w+delay_[c].read(1+b*window)*(1-w);}
            }
            else if(id>=70&&id<=72){const double cutoff=std::min(rate_*.4,hz(unit(p[2]+(id==71?lfo:envelope_)*p[3]*.5))),a=1-std::exp(-6.283185307179586*cutoff/rate_);
                for(unsigned c=0;c<2;++c){low_[c]+=(input[c]-low_[c])*a;wet[c]=id==72?eq_[1].next(c,eq_[0].next(c,input[c]-low_[c])):low_[c];}}
            else if(id==80||id==81){for(unsigned c=0;c<2;++c)wet[c]=eq_[1].next(c,eq_[0].next(c,input[c]))*gainNow_;}
            else if(id==85){for(unsigned c=0;c<2;++c){low_[c]+=(input[c]-low_[c])*.2;wet[c]=input[c]+std::tanh((input[c]-low_[c])*3)*p[2]*.4;}}
            else if(id==92){
                const double a=splitA,b=splitB;double bands[2][3]{};
                for(unsigned c=0;c<2;++c){split_[c][0]+=(input[c]-split_[c][0])*a;split_[c][1]+=(input[c]-split_[c][1])*b;
                    bands[c][0]=split_[c][0];bands[c][1]=split_[c][1]-split_[c][0];bands[c][2]=input[c]-split_[c][1];wet[c]=0;}
                for(unsigned band=0;band<3;++band){const double power=std::max(std::abs(bands[0][band]),std::abs(bands[1][band]));bandEnv_[band]+=(power-bandEnv_[band])*(power>bandEnv_[band]?.02:.0002);
                    const double gain=bandEnv_[band]>threshold?std::pow(threshold/bandEnv_[band],1-1/ratio):1;
                    for(unsigned c=0;c<2;++c)wet[c]+=bands[c][band]*gain;}
            }
            else if(id==93){const double transient=(envelope_-slowEnvelope_)/(slowEnvelope_+.001),gain=std::clamp(1+p[2]*transient,.25,4.);wet[0]=input[0]*gain;wet[1]=input[1]*gain;}
            else if(id>=90&&id<=95){
                double gain=1;if(id==95)gain=envelope_>=threshold?1:0;else if(envelope_>threshold)gain=std::pow(threshold/envelope_,id==90?1:1-1/ratio);
                for(unsigned c=0;c<2;++c)wet[c]=input[c]*gain;}
            else if(id>=100&&id<=105){
                for(unsigned c=0;c<2;++c){
                    if(id==102){if(counter_%holdSamples==0){hold_[c]=std::round(input[c]*bitSteps)/bitSteps;}wet[c]=hold_[c];}
                    else {double x=input[c]*driveScale;
                        if(id==103){low_[c]+=(input[c]-low_[c])*.02;x=(input[c]-low_[c])*driveScale;high_[c]+=(std::tanh(x)-high_[c])*.3;wet[c]=high_[c];}
                        else if(id==104)wet[c]=std::tanh(x+.15)-std::tanh(.15);
                        else if(id==105)wet[c]=std::asin(std::sin(x*1.5707963267948966))*.6366197723675813;
                        else wet[c]=id==101?x/(1+std::abs(x)):std::tanh(x);wet[c]*=driveComp;}}
            }
            else if(id==110){for(unsigned c=0;c<2;++c)wet[c]=input[c]*std::sin((phase_+c*p[3]*.5)*6.283185307179586);}
            mixNow_+=(unit(p[0])-mixNow_)*smoothing_;gainNow_+=(outputGain_-gainNow_)*smoothing_;
            for(unsigned c=0;c<2;++c){const double m=(id==80||id==81)?1:mixNow_;const double x=input[c]*(1-m)+wet[c]*m;
                if(!std::isfinite(x))return false;audio[2*i+c]=std::abs(x)<1e-25?0:x;}++counter_;
        }
        return true;
    }
};
}

