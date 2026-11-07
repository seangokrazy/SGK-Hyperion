#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sgk::research {
// Independent phase oscillator. No reference executable code or tables.
// Saw and pulse discontinuities use polynomial BLEP correction.
enum class AnalogWave : unsigned { saw, pulse, triangle, noise };
class AnalogOscillator {
    double phase_{},increment_{},width_{.5};
    std::uint32_t random_{0x6d2b79f5};
    AnalogWave wave_{AnalogWave::saw};
    static double blep(double phase,double step) noexcept {
        if(phase<step){const double x=phase/step;return x+x-x*x-1;}
        if(phase>1-step){const double x=(phase-1)/step;return x*x+x+x+1;}
        return 0;
    }
public:
    bool prepare(AnalogWave wave,double frequency,double rate,double width=.5,double phase=0,std::uint32_t seed=1) noexcept {
        if(unsigned(wave)>3||!std::isfinite(frequency)||frequency<=0||!std::isfinite(rate)||rate<8000||rate>192000||
           !std::isfinite(width)||width<=0||width>=1||!std::isfinite(phase))return false;
        wave_=wave;increment_=frequency/rate;phase_=phase-std::floor(phase);width_=width;random_=seed?seed:1;
        return true;
    }
    double next() noexcept {
        // Frequencies at or beyond Nyquist are silent rather than folded back.
        if(increment_>=.5)return 0;
        const double t=phase_,dt=increment_;double value=0;
        switch(wave_){
        case AnalogWave::saw:value=2*t-1-blep(t,dt);break;
        case AnalogWave::pulse:{
            const double w=std::clamp(width_,dt,1-dt);
            double shifted=t-w;if(shifted<0)shifted+=1;
            value=(t<w?1.0:-1.0)+blep(t,dt)-blep(shifted,dt);break;
        }
        case AnalogWave::triangle:value=1-4*std::abs(t-.5);break;
        case AnalogWave::noise:
            random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
            value=double(random_)/2147483648.0-1;break;
        }
        phase_+=dt;if(phase_>=1)phase_-=1;
        return value;
    }
};
}
