#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sgk::research {
// Independent diagnostic DSP ports. No heap, I/O, locks or shared mutable state.
// These reproduce our Python experiments, not verified native Hypersonic output.
struct EnvelopeSettings {
    double attack{}, decay{}, sustain{.8}, release{.1}, shape{.01};
};
class Envelope {
    enum class Stage { attack, decay, sustain, release };
    Stage stage_{Stage::attack};
    double rate_{44100}, level_{}, attack_{}, decay_{}, release_{}, sustain_{}, target_{};
    static double coefficient(double seconds, double k, double rate) noexcept {
        return seconds < 1e-6 ? 1.0 : -std::expm1(-k / (rate * seconds));
    }
public:
    bool prepare(const EnvelopeSettings& s, double rate) noexcept {
        if (!std::isfinite(rate) || rate < 8000 || rate > 192000 ||
            !std::isfinite(s.attack) || !std::isfinite(s.decay) || !std::isfinite(s.sustain) ||
            !std::isfinite(s.release) || !std::isfinite(s.shape) || s.attack < 0 || s.decay < 0 ||
            s.release < 0 || s.sustain < 0 || s.sustain > 1 || s.shape <= 0) return false;
        rate_=rate;level_=0;stage_=Stage::attack;sustain_=s.sustain;
        attack_=coefficient(s.attack,.345,rate);
        decay_=coefficient(s.decay,-1.035*std::log((s.shape+.1)/(s.shape+1)),rate);
        release_=coefficient(s.release,3.45,rate);
        target_=s.sustain-(1-s.sustain)*s.shape;
        return true;
    }
    void noteOff(bool forced=false) noexcept {
        stage_=Stage::release;
        if (forced) release_=std::max(release_,coefficient(.005,3.45,rate_));
    }
    double next() noexcept {
        switch(stage_) {
        case Stage::attack:
            level_+=(3.4-level_)*attack_;
            if(level_>=1){level_=1;stage_=Stage::decay;} break;
        case Stage::decay:
            level_+=(target_-level_)*decay_;
            if(level_<=sustain_){level_=sustain_;stage_=Stage::sustain;} break;
        case Stage::release:level_+=(1e-10-level_)*release_;break;
        case Stage::sustain:break;
        }
        return std::max(0.0,level_);
    }
    bool finished()const noexcept{return stage_==Stage::release && level_<=1e-6;}
};

class Filter {
    double rate_{44100}, resonance_{}, requested_{}, low_{}, band_{}, previous_{}, extraLow_{}, extraBand_{};
    double energy_{}, feedback_{}, frequency_{}, damping_{}, extraFrequency_{};
    unsigned mode_{},remaining_{};
    void tick() noexcept {
        double feedback=.95-resonance_;
        if(feedback<.15) {
            feedback=std::max(feedback,requested_<=1 ? .5*(requested_-1) : 0.0);
            const double energy=(low_*low_+band_*band_)*.005;
            energy_+=(energy-energy_)*.5;
            feedback+=energy_;
        }
        feedback=std::min(1.0,feedback);
        double ceiling=1.92-.85*feedback;
        double frequency=requested_;
        if(frequency>ceiling) {
            const double guard=mode_==5 ? .3 : (mode_==6 || mode_==7) ? .5 : .77;
            feedback+=std::min(frequency-ceiling,1.0)*(guard-feedback);
            ceiling=1.92-.85*feedback;
            frequency=std::min(frequency,ceiling);
        } else if(mode_!=8) ceiling=frequency;
        feedback_=feedback;frequency_=frequency;damping_=ceiling;
        extraFrequency_=std::min(.63*frequency,1.0);
    }
public:
    bool atControlTick()const noexcept{return remaining_==0;}
    bool configure(double cutoff,double resonance,double logModulation=0) noexcept {
        if(!std::isfinite(cutoff)||!std::isfinite(resonance)||!std::isfinite(logModulation)||
           cutoff<0||cutoff>1||resonance<0||resonance>1||logModulation< -32||logModulation>32) return false;
        resonance_=resonance;
        requested_=261.6255798339844/rate_*std::exp(cutoff*6.931471824645996+logModulation);
        return true;
    }
    bool prepare(double rate,unsigned mode,double cutoff,double resonance) noexcept {
        if(!std::isfinite(rate)||rate<8000||rate>192000||mode>8)return false;
        *this=Filter{};rate_=rate;mode_=mode;
        return configure(cutoff,resonance);
    }
    bool process(double input,double& output) noexcept {
        if(!std::isfinite(input))return false;
        if(mode_==0){output=input;return true;}
        if(remaining_==0){tick();remaining_=32;}
        const double error=low_+band_*feedback_-input-previous_;
        band_-=damping_*error;
        low_+=frequency_*band_;previous_=input;
        extraBand_-=extraFrequency_*(extraBand_-low_+extraLow_);
        extraLow_+=extraFrequency_*extraBand_;
        double result=0;
        switch(mode_) {
        case 1:result=extraLow_;break;
        case 2:case 8:result=low_;break;
        case 3:result=extraBand_;break;
        case 4:result=band_;break;
        case 5:result=error;break;
        case 6:result=2*input-band_;break;
        case 7:result=2*input-error;break;
        }
        --remaining_;
        if(!std::isfinite(result))return false;
        output=result*.5;return true;
    }
};
}
