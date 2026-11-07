#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#if defined(SGK_HYPERION_FAST_SINE)
#include "FastSine.h"
#endif

namespace sgk::research {
struct DrawbarSettings {std::array<double,9> levels{};bool percussion{},third{},soft{},fast{};unsigned mode{},waveFlavor{};double drive{},click{},clickSeconds{.003};};
// Independent additive tonewheel model. It does not decrypt/read the wrapped
// reference Drawbar waveforms; their individual flavor/timbre is unverified.
class DrawbarVoice {
    double phases_[10]{},steps_[10]{},levels_[9]{},percussion_{},decay_{},drive_{1},click_{};unsigned clickLeft_{};std::uint32_t noise_{1};
#if defined(SGK_HYPERION_FAST_SINE)
    const double* sine_{};
#endif
public:
    bool prepare(const DrawbarSettings& s,double frequency,double rate,unsigned velocity=100,bool first=true)noexcept {
        if(!std::isfinite(frequency)||frequency<=0||!std::isfinite(rate)||rate<8000||rate>192000||velocity>127||s.mode>3||s.waveFlavor>127||
           !std::isfinite(s.drive)||s.drive<0||s.drive>1||!std::isfinite(s.click)||s.click<0||s.click>1||
           !std::isfinite(s.clickSeconds)||s.clickSeconds<0||s.clickSeconds>.1)return false;
        constexpr double ratios[]={.5,1.5,1,2,3,4,5,6,8};*this=DrawbarVoice{};
#if defined(SGK_HYPERION_FAST_SINE)
        sine_=FastSine::prepare();
#endif
        for(unsigned j=0;j<9;++j){if(!std::isfinite(s.levels[j])||s.levels[j]<0||s.levels[j]>1)return false;levels_[j]=s.levels[j];steps_[j]=frequency*ratios[j]/rate;}
        steps_[9]=frequency*(s.third?3:2)/rate;
        percussion_=s.percussion&&(s.mode!=2||first)?(s.soft?.15:.3):0;
        if(s.mode==1)percussion_*=double(velocity)/127;
        decay_=std::exp(-1/(rate*(s.fast?.12:.6)));drive_=1+5*s.drive*s.drive;
        clickLeft_=unsigned(s.clickSeconds*rate);click_=s.click*.035;noise_=velocity+1;
        return true;
    }
    double next()noexcept {
        double value=0;
        for(unsigned j=0;j<10;++j){if(steps_[j]>=.5)continue;const double level=j==9?percussion_:levels_[j]*.15;if(level!=0){
#if defined(SGK_HYPERION_FAST_SINE)
            value+=FastSine::at(sine_,phases_[j])*level;
#else
            value+=std::sin(6.28318530717958647692*phases_[j])*level;
#endif
            }
            phases_[j]+=steps_[j];if(phases_[j]>=1)phases_[j]-=1;}
        percussion_*=decay_;if(clickLeft_){noise_^=noise_<<13;noise_^=noise_>>17;noise_^=noise_<<5;value+=(double(noise_)/2147483648.-1)*click_;--clickLeft_;}
        return drive_>1.0001?std::tanh(value*drive_)/std::sqrt(drive_):value;
    }
};
}
