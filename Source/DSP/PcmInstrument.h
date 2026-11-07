#pragma once
#include "VoiceDsp.h"
#include "AnalogOscillator.h"
#include "FmOscillator.h"
#include "WavetableOscillator.h"
#include "DrawbarOscillator.h"
#include <cstddef>
#include <limits>
#include <new>

namespace sgk::research {
static_assert(sizeof(FmVoice)<=sizeof(AnalogVoice),"FM must fit existing per-voice oscillator storage");
struct PcmView { const float* data{};std::uint32_t frames{},channels{};double rate{}; };
struct ZoneView {
    unsigned lowKey{},highKey{127},lowVelocity{},highVelocity{127},root{60};
    PcmView sample;unsigned endpoint{},loopSpan{};double fine{};
    bool transpose{true};
};
struct LayerView {
    unsigned engine{2},node{};AnalogSettings analog;FmSettings fm;WavetableSettings wavetable;DrawbarSettings drawbar;
    unsigned lowKey{},highKey{127},lowVelocity{},highVelocity{127};
    double keyFade{1},velocityFade{1};unsigned trigger{},choke{};
    double gain{1},pan{.5},octave{},transpose{},fine{},keyTracking{1};
    EnvelopeSettings amplitude,filterEnvelope;
    unsigned filterMode{};double cutoff{},resonance{},filterDepth{},filterKeyFollow{1};
    const ZoneView* zones{};std::size_t zoneCount{};
};

class PcmInstrument {
public:
    static constexpr unsigned maxLayers=64,maxNotes=64,maxPartials=128;
private:
    struct Note { bool active{},held{},release{},forced{};unsigned channel{},key{},parts{};std::uint64_t order{}; };
    struct Partial {
        bool active{},released{},forced{},choked{};unsigned note{};
        const ZoneView* zone{};const LayerView* layer{};
        union Sources {AnalogVoice analog;FmVoice fm;WavetableVoice wavetable;DrawbarVoice drawbar;Sources():analog(){};} source;
        Envelope amp,filterEnv;Filter filters[2];
        double position{},step{},keyMod{},gain{};
        bool start(unsigned slot,unsigned key,unsigned velocity,const ZoneView& z,const LayerView& l,double rate,bool first=true) noexcept {
            *this=Partial{};note=slot;zone=&z;layer=&l;
            const auto rangeGain=[](unsigned v,unsigned lo,unsigned hi,double width){
                const double left=double((v+128-lo)%128+1),right=double((hi+128-v)%128+1);return std::min(1.,std::min(left,right)/width);};
            const double fade=rangeGain(key,l.lowKey,l.highKey,l.keyFade)*rangeGain(velocity,l.lowVelocity,l.highVelocity,l.velocityFade);
            if(!amp.prepare(l.amplitude,rate)||!filterEnv.prepare(l.filterEnvelope,rate))return false;
            for(unsigned c=0;c<z.sample.channels;++c)if(!filters[c].prepare(rate,l.filterMode,l.cutoff,l.resonance))return false;
            if(l.engine!=2){
                if(!filters[0].prepare(rate,l.filterMode,l.cutoff,l.resonance))return false;
                const double pitch=(double(key)-60)*l.keyTracking+l.octave+l.transpose+l.fine;
                const double frequency=261.6255653005986*std::pow(2.0,pitch/12);
                if(l.engine==5){if(!source.analog.prepare(l.analog,frequency,rate,slot*131+key+1))return false;}
                else if(l.engine==4){new (&source.fm)FmVoice{};if(!source.fm.prepare(l.fm,frequency,rate,key,velocity))return false;}
                else if(l.engine==3){new (&source.wavetable)WavetableVoice{};if(!source.wavetable.prepare(l.wavetable,frequency,rate))return false;}
                else {new (&source.drawbar)DrawbarVoice{};if(!source.drawbar.prepare(l.drawbar,frequency,rate,velocity,first))return false;}
                keyMod=(double(key)-60)*std::log(2.0)/12*l.filterKeyFollow;
                gain=double(velocity)/127*.2*l.gain*fade;active=true;return true;
            }
            const double keyPitch=z.transpose?(double(key)-60)*l.keyTracking+60-z.root:0;
            const double pitch=keyPitch+z.fine+l.octave+l.transpose+l.fine;
            step=z.sample.rate/rate*std::pow(2.0,pitch/12);
            keyMod=(double(key)-60)*std::log(2.0)/12*l.filterKeyFollow;
            gain=double(velocity)/127*.2*l.gain*fade;active=std::isfinite(step)&&step>0;
            return active;
        }
        bool render(const Note& n,double* output,std::size_t frames) noexcept {
            if(n.release&&!released){amp.noteOff();filterEnv.noteOff();released=true;}
            if((n.forced||choked)&&!forced){amp.noteOff(true);filterEnv.noteOff(true);forced=true;}
            if(layer->engine!=2){
                const auto& l=*layer;
                for(std::size_t i=0;i<frames&&active;++i){
                    const double envelope=filterEnv.next();double filtered=0;
                    if(l.filterMode!=0&&filters[0].atControlTick())
                        if(!filters[0].configure(l.cutoff,l.resonance,keyMod+envelope*l.filterDepth))return false;
                    const double oscillator=l.engine==5?source.analog.next():(l.engine==4?source.fm.next():(l.engine==3?source.wavetable.next():source.drawbar.next()));
                    if(!filters[0].process(oscillator,filtered))return false;
                    const double value=filtered*amp.next()*gain;
                    output[2*i]+=value*(2*(1-l.pan));output[2*i+1]+=value*(2*l.pan);
                    if(amp.finished())active=false;
                }
                return true;
            }
            const auto& z=*zone;const auto& l=*layer;const auto& s=z.sample;
            for(std::size_t i=0;i<frames&&active;++i){
                if(position>=z.endpoint){
                    if(!z.loopSpan){active=false;break;}
                    position=z.endpoint-z.loopSpan+std::fmod(position-z.endpoint,double(z.loopSpan));
                }
                const auto at=unsigned(position);const double fraction=position-at;
                unsigned following=at+1;
                if(following>=z.endpoint)following=z.loopSpan?z.endpoint-z.loopSpan:at;
                double amplitude=amp.next()*gain;const double env=filterEnv.next();
                if(!z.loopSpan)amplitude*=std::min(1.0,(z.endpoint-position)/std::max(1.0,std::min(z.endpoint/2.0,s.rate*.005)));
                for(unsigned c=0;c<s.channels;++c){
                    // Match the float32 interpolation used by the reference experiment.
                    const float a=s.data[std::size_t(at)*s.channels+c]*float(1-fraction);
                    const float b=s.data[std::size_t(following)*s.channels+c]*float(fraction);
                    const float value=a+b;double filtered=0;
                    if(l.filterMode!=0&&filters[c].atControlTick())
                        if(!filters[c].configure(l.cutoff,l.resonance,keyMod+env*l.filterDepth))return false;
                    if(!filters[c].process(value,filtered))return false;
                    const double sample=filtered*amplitude;
                    if(s.channels==1){output[2*i]+=sample*(2*(1-l.pan));output[2*i+1]+=sample*(2*l.pan);}
                    else output[2*i+c]+=sample*(c==0?2*(1-l.pan):2*l.pan);
                }
                position+=step;if(amp.finished())active=false;
            }
            return true;
        }
    };
    const LayerView* layers_{};std::size_t layerCount_{};double rate_{44100};
    Note notes_[maxNotes]{};Partial partials_[maxPartials]{};
    unsigned active_[maxPartials]{},count_{},peak_{};bool pedal_[16]{};std::uint64_t order_{1};
    unsigned patchNotes_[16]{},elementNotes_[maxLayers][16]{},modWheel_[16]{};
public:
    // Views must remain alive and immutable until reset/replaced off the audio thread.
    bool prepare(const LayerView* layers,std::size_t count,double rate) noexcept {
        if(!layers||count==0||count>maxLayers||!std::isfinite(rate)||rate<8000||rate>192000)return false;
        for(std::size_t i=0;i<count;++i){
            const auto& l=layers[i];Envelope e;Filter f;
            if((l.engine<1||l.engine>5)||!l.zones||l.zoneCount==0||l.zoneCount>4096||l.lowKey>127||l.highKey>127||
               l.lowVelocity>127||l.highVelocity>127||!std::isfinite(l.gain)||l.gain<0||
               !std::isfinite(l.keyFade)||l.keyFade<1||l.keyFade>128||!std::isfinite(l.velocityFade)||l.velocityFade<1||l.velocityFade>128||l.trigger>9||l.choke>15||
               !std::isfinite(l.pan)||l.pan<0||l.pan>1||!std::isfinite(l.octave)||!std::isfinite(l.transpose)||
               !std::isfinite(l.fine)||!std::isfinite(l.keyTracking)||!std::isfinite(l.filterDepth)||!std::isfinite(l.filterKeyFollow)||
               !e.prepare(l.amplitude,rate)||!e.prepare(l.filterEnvelope,rate)||!f.prepare(rate,l.filterMode,l.cutoff,l.resonance))return false;
            if(l.engine==5){AnalogVoice a;if(!a.prepare(l.analog,440,rate))return false;continue;}
            if(l.engine==4){FmVoice fmValidator;if(!fmValidator.prepare(l.fm,440,rate))return false;continue;}
            if(l.engine==3){WavetableVoice waveValidator;if(!waveValidator.prepare(l.wavetable,440,rate))return false;continue;}
            if(l.engine==1){DrawbarVoice drawbarValidator;if(!drawbarValidator.prepare(l.drawbar,440,rate))return false;continue;}
            for(std::size_t j=0;j<l.zoneCount;++j){const auto& z=l.zones[j];
                if(!z.sample.data||z.sample.channels<1||z.sample.channels>2||z.endpoint==0||z.endpoint>z.sample.frames||
                   z.loopSpan>z.endpoint||!std::isfinite(z.sample.rate)||z.sample.rate<8000||z.sample.rate>192000||
                   z.lowKey>z.highKey||z.highKey>127||z.lowVelocity>z.highVelocity||z.highVelocity>127||
                   z.root>127||!std::isfinite(z.fine))return false;
            }
        }
        reset();layers_=layers;layerCount_=count;rate_=rate;return true;
    }
    void reset() noexcept {
        for(auto& n:notes_)n=Note{};
        for(auto& p:partials_)p.active=false;
        for(auto& p:pedal_)p=false;
        for(unsigned ch=0;ch<16;++ch){patchNotes_[ch]=modWheel_[ch]=0;for(unsigned i=0;i<maxLayers;++i)elementNotes_[i][ch]=0;}
        count_=peak_=0;order_=1;
    }
    unsigned activePartials()const noexcept{return count_;}
    unsigned peakPartials()const noexcept{return peak_;}
    bool send(unsigned status,unsigned key,unsigned value) noexcept {
        if(status>255||key>127||value>127)return false;
        const unsigned channel=status&15,kind=status&0xf0;
        if(kind==0x90&&value){
            ++patchNotes_[channel];
            bool first=true;for(const auto& n:notes_)if(n.active&&n.held&&n.channel==channel)first=false;
            const ZoneView* selected[maxLayers]{};const LayerView* owners[maxLayers]{};unsigned needed=0;
            for(std::size_t i=0;i<layerCount_;++i){const auto& l=layers_[i];
                const auto inside=[](unsigned x,unsigned low,unsigned high){return low<=high?(x>=low&&x<=high):(x>=low||x<=high);};
                if(!inside(key,l.lowKey,l.highKey)||l.gain==0)continue;++elementNotes_[i][channel];
                bool trigger=true;switch(l.trigger){case 1:trigger=modWheel_[channel]>=64;break;case 2:trigger=modWheel_[channel]<64;break;
                    case 3:trigger=!pedal_[channel];break;case 4:trigger=pedal_[channel];break;
                    case 5:trigger=(patchNotes_[channel]&1)!=0;break;case 6:trigger=(patchNotes_[channel]&1)==0;break;
                    case 7:trigger=(elementNotes_[i][channel]&1)!=0;break;case 8:trigger=(elementNotes_[i][channel]&1)==0;break;default:break;}
                if(!trigger||!inside(value,l.lowVelocity,l.highVelocity))continue;
                for(std::size_t j=0;j<l.zoneCount;++j){const auto& z=l.zones[j];
                    if(key>=z.lowKey&&key<=z.highKey&&value>=z.lowVelocity&&value<=z.highVelocity){selected[needed]=&z;owners[needed++]=&l;break;}
                }
            }
            if(!needed)return true;
            if(count_+needed>maxPartials)return false;
            unsigned slot=0;while(slot<maxNotes&&notes_[slot].active)++slot;if(slot==maxNotes)return false;
            for(unsigned i=0;i<count_;++i){auto& old=partials_[active_[i]];const auto& oldNote=notes_[old.note];if(oldNote.channel!=channel)continue;
                for(unsigned j=0;j<needed;++j){const auto* l=owners[j];const auto policy=l->choke;
                    if((policy==1&&oldNote.key==key&&old.layer==l)||(policy==2&&old.layer!=l)||policy==3||(policy>=5&&old.layer->choke==policy))old.choked=true;}
            }
            unsigned added=0;
            for(unsigned i=0;i<maxPartials&&added<needed;++i)if(!partials_[i].active){
                if(!partials_[i].start(slot,key,value,*selected[added],*owners[added],rate_,first)){
                    for(unsigned j=0;j<added;++j)partials_[active_[count_+j]].active=false;
                    return false;
                }
                active_[count_+added]=i;++added;
            }
            notes_[slot]={true,true,false,false,channel,key,needed,order_++};count_+=needed;peak_=std::max(peak_,count_);return true;
        }
        if(kind==0x80||(kind==0x90&&value==0)){
            unsigned chosen=maxNotes;std::uint64_t oldest=~std::uint64_t(0);
            for(unsigned i=0;i<maxNotes;++i)if(notes_[i].active&&notes_[i].held&&notes_[i].channel==channel&&notes_[i].key==key&&notes_[i].order<oldest){oldest=notes_[i].order;chosen=i;}
            if(chosen!=maxNotes){notes_[chosen].held=false;if(!pedal_[channel])notes_[chosen].release=true;}
        } else if(kind==0xb0&&key==1){modWheel_[channel]=value;
        } else if(kind==0xb0&&key==64){
            pedal_[channel]=value>=64;
            if(!pedal_[channel])for(auto& n:notes_)if(n.active&&n.channel==channel&&!n.held)n.release=true;
        } else if(kind==0xb0&&(key==120||key==123)){
            for(auto& n:notes_)if(n.active&&n.channel==channel){n.held=false;n.release=true;n.forced=true;}
        }
        return true;
    }
    bool process(double* output,std::size_t frames,unsigned node=~0U,bool clear=true) noexcept {
        if((frames&&!output)||frames>std::numeric_limits<std::size_t>::max()/2)return false;
        if(clear)for(std::size_t i=0;i<frames*2;++i)output[i]=0;
        unsigned kept=0;bool ok=true;
        for(unsigned i=0;i<count_;++i){auto& p=partials_[active_[i]];
            if((node==~0U||p.layer->node==node)&&!p.render(notes_[p.note],output,frames)){p.active=false;ok=false;}
            if(p.active)active_[kept++]=active_[i];
            else {auto& n=notes_[p.note];if(--n.parts==0)n.active=false;}
        }
        count_=kept;
        for(std::size_t i=0;i<frames*2;++i)if(!std::isfinite(output[i]))return false;
        return ok;
    }
};
}
