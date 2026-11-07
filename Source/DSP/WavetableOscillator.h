#pragma once
#include "PhaseOscillator.h"
#include <cstddef>

namespace sgk::research {
struct WavetableView {const float* data{};std::size_t samples{};unsigned points{},frames{};};
struct WavetableSettings {WavetableView table;double position{},blend{};unsigned virtualWave{};};
class WavetableVoice {
    WavetableView table_;double phase_{},step_{},frameMix_{},blend_{};
    unsigned points_{},frame_{},nextFrame_{},mode_{};std::size_t offset_{};
    AnalogOscillator virtual_,triangle_;
    double at(unsigned frame,double phase)const noexcept {
        const double pos=phase*points_;const unsigned i=unsigned(pos);const double f=pos-i;
        const auto base=offset_+std::size_t(frame)*(points_+1);
        return table_.data[base+i]*(1-f)+table_.data[base+i+1]*f;
    }
public:
    bool prepare(const WavetableSettings& s,double frequency,double rate)noexcept {
        if(!s.table.data||s.table.points!=256||s.table.frames!=64||s.table.samples!=33152||
           !std::isfinite(s.position)||s.position<0||s.position>1||!std::isfinite(s.blend)||s.blend<0||s.blend>1||
           (s.blend>0&&s.virtualWave>4)||!std::isfinite(frequency)||frequency<=0||
           !std::isfinite(rate)||rate<8000||rate>192000)return false;
        table_=s.table;phase_=0;step_=frequency/rate;points_=table_.points;offset_=0;blend_=s.blend;mode_=s.virtualWave;
        while(points_>2&&step_*points_>1){offset_+=std::size_t(points_+1)*table_.frames;points_/=2;}
        const double frame=s.position*(table_.frames-1);frame_=unsigned(frame);nextFrame_=std::min(frame_+1,table_.frames-1);frameMix_=frame-frame_;
        const auto wave=s.virtualWave==1?AnalogWave::pulse:(s.virtualWave==2?AnalogWave::triangle:(s.virtualWave==4?AnalogWave::noise:AnalogWave::saw));
        return virtual_.prepare(wave,frequency,rate)&&triangle_.prepare(AnalogWave::triangle,frequency,rate);
    }
    double next()noexcept {
        if(step_>=.5)return 0;
        const double table=at(frame_,phase_)*(1-frameMix_)+at(nextFrame_,phase_)*frameMix_;
        double virtualValue=virtual_.next();if(mode_==3)virtualValue=.5*(virtualValue+triangle_.next());
        phase_+=step_;if(phase_>=1)phase_-=1;
        return table*(1-blend_)+virtualValue*blend_;
    }
};
}
