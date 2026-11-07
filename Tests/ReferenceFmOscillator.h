// Pre-lookup independent FM implementation retained for numerical regression only.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sgk::research::reference {
struct FmSettings {
    double ratioA{1},fineA{},directA{},depthA{},velocityA{},lowerA{},pivotA{60},upperA{};
    double ratioB{1},fineB{},keyTrackB{1},depthB{},velocityB{},lowerB{},pivotB{60},upperB{};
    double transitionSeconds{.1},transition{},fixedHz{};
};
// Independent three-oscillator FM prototype: two modulators affect carrier
// frequency, and modulator A also has a direct audible path. Exact sines and
// a new oversampling filter deliberately differ from the reference renderer.
class FmVoice {
    static constexpr unsigned oversampling=4,taps=48;
    inline static constexpr double coefficients_[taps]={
        0.00087468833872244764,
        0.0011871607678555275,
        0.0011513788150162830,
        0.00055774016115678695,
        -0.00072582609514509476,
        -0.0024859580188194112,
        -0.0039964600942413676,
        -0.0041598898220734032,
        -0.0020194023816772493,
        0.0025359440044729143,
        0.0082480066552516849,
        0.012550233424100905,
        0.012408419951249864,
        0.0057624585772394678,
        -0.0069865571481098499,
        -0.022176314042213683,
        -0.033348225393792376,
        -0.033085485194954675,
        -0.015723455672578265,
        0.020048126992211045,
        0.069781787889239874,
        0.12380457569227614,
        0.16971421873065418,
        0.19608283386415831,
        0.19608283386415831,
        0.16971421873065418,
        0.12380457569227613,
        0.069781787889239874,
        0.020048126992211049,
        -0.015723455672578265,
        -0.033085485194954675,
        -0.033348225393792383,
        -0.022176314042213683,
        -0.0069865571481098508,
        0.0057624585772394678,
        0.012408419951249864,
        0.012550233424100906,
        0.0082480066552516849,
        0.0025359440044729160,
        -0.0020194023816772514,
        -0.0041598898220734067,
        -0.0039964600942413719,
        -0.0024859580188194095,
        -0.00072582609514509531,
        0.00055774016115678728,
        0.0011513788150162836,
        0.0011871607678555281,
        0.00087468833872244764
    };
    double carrier_{},a_{},b_{},carrierStep_{},aStep_{},bStep_{};
    double directA_{},depthA_{},depthB_{},targetB_{},coefficient_{};
    double history_[taps]{};unsigned cursor_{};
    static double wrapped(double x) noexcept{return x-std::floor(x);}
    static double response(double velocity,double sensitivity,double key,double pivot,double lower,double upper) noexcept {
        const double distance=key-pivot;
        const double scale=std::exp(std::min(24.0,distance*(distance<=0?lower:upper))*std::log(2.0)/12);
        return (.5+(velocity*velocity-.5)*sensitivity)*scale;
    }
public:
    bool prepare(const FmSettings& s,double frequency,double rate,unsigned key=60,unsigned velocity=100) noexcept {
        const double fields[]={s.ratioA,s.fineA,s.directA,s.depthA,s.velocityA,s.lowerA,s.pivotA,s.upperA,
            s.ratioB,s.fineB,s.keyTrackB,s.depthB,s.velocityB,s.lowerB,s.pivotB,s.upperB,
            s.transitionSeconds,s.transition,s.fixedHz};
        for(double x:fields)if(!std::isfinite(x))return false;
        if(!std::isfinite(frequency)||frequency<=0||!std::isfinite(rate)||rate<8000||rate>192000||key>127||velocity>127||
           s.ratioA<0||s.ratioA>33||s.ratioB<0||s.ratioB>33||s.fineA<0||s.fineA>.5||s.fineB<0||s.fineB>.5||
           s.directA<0||s.directA>3||s.depthA<0||s.depthA>3||s.depthB<0||s.depthB>3||
           s.velocityA<0||s.velocityA>1||s.velocityB<0||s.velocityB>1||s.keyTrackB<0||s.keyTrackB>1||
           s.pivotA<0||s.pivotA>128||s.pivotB<0||s.pivotB>128||
           s.lowerA<-2||s.lowerA>3||s.upperA<-2||s.upperA>3||s.lowerB<-2||s.lowerB>3||s.upperB<-2||s.upperB>3||
           s.transitionSeconds<0||s.transitionSeconds>32||s.transition<0||s.transition>2||s.fixedHz<0||s.fixedHz>50)return false;
        *this=FmVoice{};
        const double root=s.fixedHz>0?s.fixedHz:frequency,internalRate=rate*oversampling;
        carrierStep_=root/internalRate;
        const double keyCorrection=std::pow(2.0,(60.0-key)/12);
        const double ratioA=s.ratioA+s.fineA*((s.ratioA<2.1&&s.fineA<.02)?keyCorrection:1);
        const double ratioB=(s.ratioB+s.fineB*((s.ratioB<2.1&&s.fineB<.02)?keyCorrection:1))*
            std::pow(2.0,(1-s.keyTrackB)*(60.0-key)/12);
        aStep_=root*ratioA/internalRate;bStep_=root*ratioB/internalRate;
        // Suppress modulators outside the internal Nyquist range.
        if(aStep_>=.5)aStep_=0;if(bStep_>=.5)bStep_=0;
        const double vel=double(velocity)/127;
        const double responseA=response(vel,s.velocityA,key,s.pivotA,s.lowerA,s.upperA);
        const double responseB=response(vel,s.velocityB,key,s.pivotB,s.lowerB,s.upperB);
        directA_=s.directA*responseA;depthA_=s.depthA*responseA;
        const double fullB=s.depthB*responseB;
        depthB_=fullB*(s.transition>1?2-s.transition:1);
        targetB_=fullB*(s.transition<=1?s.transition*s.transition:1);
        coefficient_=s.transitionSeconds<1e-6?1:-std::expm1(-1/(s.transitionSeconds*internalRate));
        return std::isfinite(aStep_)&&std::isfinite(bStep_);
    }
    double next() noexcept {
        constexpr double tau=6.28318530717958647692,modulatorPeak=.3849001794597505;
        for(unsigned i=0;i<oversampling;++i){
            const double a=modulatorPeak*std::sin(tau*a_),b=modulatorPeak*std::sin(tau*b_);
            depthB_+=(targetB_-depthB_)*coefficient_;
            const double value=carrierStep_>=.5?0:std::sin(tau*carrier_)+directA_*a;
            history_[cursor_]=value;cursor_=(cursor_+1)%taps;
            a_=wrapped(a_+aStep_);b_=wrapped(b_+bStep_);
            carrier_=wrapped(carrier_+carrierStep_*(1+a*depthA_+b*depthB_));
        }
        double output=0;unsigned at=cursor_;
        for(unsigned i=0;i<taps;++i){at=at?at-1:taps-1;output+=coefficients_[i]*history_[at];}
        return output;
    }
};
}
