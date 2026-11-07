#pragma once
#include "PhaseOscillator.h"
#include "WavetableOscillator.h"
namespace sgk::research {
struct AnalogSettings {
    unsigned modeA{},modeB{7},extraWave{7};
    WavetableView table;double tablePosition{};
    double tuneA{},tuneB{},levelA{1},levelB{},extraLevel{},width{.5},detune{.08};
};
// Initial independent approximation of the observed analog modes. Sync/Pitch
// modes currently retain their base saw/pulse; their dynamic behavior is not
// claimed as matched. Ordinary wavetable assets use the same bounded mip reader.
class AnalogVoice {
    AnalogOscillator oscillators_[2][7],extra_;
    unsigned count_[2]{};bool tables_[2]{},tableExtra_{};WavetableVoice tableVoices_[3];
    double levels_[2]{},extraLevel_{};
public:
    bool prepare(const AnalogSettings& s,double frequency,double rate,std::uint32_t seed=1) noexcept {
        if(s.modeA>7||s.modeB>7||
           (s.extraWave>7||s.extraWave==3)||
           !std::isfinite(s.levelA)||!std::isfinite(s.levelB)||!std::isfinite(s.extraLevel)||
           s.levelA<0||s.levelA>1.01||s.levelB<0||s.levelB>1.01||s.extraLevel<0||s.extraLevel>2||
           !std::isfinite(s.detune)||s.detune<0||s.detune>1)return false;
        const unsigned modes[2]={s.modeA,s.modeB};const double tunes[2]={s.tuneA,s.tuneB};
        levels_[0]=s.levelA;levels_[1]=s.levelB;
        for(unsigned bank=0;bank<2;++bank){
            tables_[bank]=modes[bank]==6;count_[bank]=(modes[bank]==7||tables_[bank])?0:(modes[bank]==5?7:1);
            if(tables_[bank]&&levels_[bank]>0){WavetableSettings t;t.table=s.table;t.position=s.tablePosition;
                if(!tableVoices_[bank].prepare(t,frequency*std::pow(2.0,tunes[bank]/12),rate))return false;}
            const auto wave=(modes[bank]==1||modes[bank]==2||modes[bank]==4)?AnalogWave::pulse:AnalogWave::saw;
            for(unsigned j=0;j<count_[bank];++j){
                const double detune=count_[bank]>1?(double(j)-3)/3*s.detune:0;
                if(!oscillators_[bank][j].prepare(wave,frequency*std::pow(2.0,(tunes[bank]+detune)/12),rate,s.width,
                    count_[bank]>1?double(j)/7:0,seed+bank*17+j))return false;
            }
        }
        extraLevel_=s.extraWave==7?0:s.extraLevel;tableExtra_=s.extraWave==5||s.extraWave==6;
        if(tableExtra_&&extraLevel_>0){WavetableSettings t;t.table=s.table;t.position=s.tablePosition;if(!tableVoices_[2].prepare(t,frequency,rate))return false;}
        const auto extraWave=(s.extraWave==4||s.extraWave==5)?AnalogWave::noise:(s.extraWave==1?AnalogWave::pulse:((s.extraWave==2||s.extraWave==6)?AnalogWave::triangle:AnalogWave::saw));
        return extra_.prepare(extraWave,frequency,rate,.5,0,seed+137);
    }
    double next() noexcept {
        double value=0;
        for(unsigned b=0;b<2;++b){double sum=0;for(unsigned j=0;j<count_[b];++j)sum+=oscillators_[b][j].next();
            if(tables_[b]&&levels_[b]>0)value+=tableVoices_[b].next()*levels_[b];
            if(count_[b])value+=sum*levels_[b]/count_[b];}
        if(extraLevel_>0)value+=extra_.next()*extraLevel_*(tableExtra_?tableVoices_[2].next():1);
        return value;
    }
};
}
