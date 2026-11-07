#pragma once
#include "BankLoader.h"

namespace sgk::research {
// Basic serial/output-bus graph. Storage is prepared off-thread. Auxiliary
// sends and external multichannel outputs require additional routing work.
class GraphPlayer {
    PcmInstrument voice_;const PreparedPatch* patch_{};std::vector<FxProcessor> effects_;std::vector<int> effectIndex_;
    double bpm_{120};std::string error_;FxProcessor returns_[2];bool explicitSend_[2]{},panicPending_{},dry_{};
    struct Stage{double level{1},target{1},blend{1};bool enabled{true},cleared{};};
    std::array<Stage,66> stages_{};double smoothing_{.002};
    bool stage(FxProcessor& effect,double* audio,std::size_t frames,std::size_t index,bool auxiliary)noexcept{
        auto& s=stages_[index];const double target=s.enabled?1:0;
        if(!s.enabled&&s.blend==0){if(!s.cleared){effect.reset();s.cleared=true;}if(auxiliary)std::fill(audio,audio+frames*2,0.);return true;}
        s.cleared=false;
        if(s.enabled&&s.blend==1&&s.level==1&&s.target==1)return effect.process(audio,frames,bpm_);
        double original[512];if(!auxiliary)std::copy(audio,audio+frames*2,original);
        if(!effect.process(audio,frames,bpm_))return false;
        for(std::size_t i=0;i<frames;++i){s.blend+=(target-s.blend)*smoothing_;s.level+=(s.target-s.level)*smoothing_;
            if(std::abs(target-s.blend)<1e-7)s.blend=target;if(std::abs(s.target-s.level)<1e-7)s.level=s.target;
            for(unsigned c=0;c<2;++c){const auto j=i*2+c;audio[j]=audio[j]*s.level*s.blend+(auxiliary?0:original[j]*(1-s.blend));}}
        return true;
    }
public:
    bool prepare(const PreparedPatch& patch,double rate){
        error_.clear();if(!voice_.prepare(patch.layers.data(),patch.layers.size(),rate)){error_="Unsupported source dimensions or synthesis settings";return false;}
        patch_=&patch;effects_.clear();effectIndex_.assign(patch.nodes.size(),-1);effects_.reserve(patch.nodes.size());
        explicitSend_[0]=explicitSend_[1]=false;panicPending_=false;dry_=false;stages_.fill(Stage{});smoothing_=-std::expm1(-1/(rate*.01));
        for(std::size_t i=0;i<patch.nodes.size();++i)if(patch.nodes[i].effect){
            // Do not silently drop saved sends or pretend an external output
            // is a normal stereo route.
            const auto& fx=patch.nodes[i].fx;if(fx.output>2){error_="External multichannel FX output not implemented";return false;}
            if(fx.sends[2]!=0||fx.sends[3]!=0){error_="Global combi FX sends not implemented";return false;}
            for(unsigned j=0;j<2;++j)if(fx.sends[j]>0){if(!patch.returnEnabled[j]){error_="Missing local FX return";return false;}explicitSend_[j]=true;}
            if(effects_.size()>=64){error_="Too many insert FX";return false;}effectIndex_[i]=int(effects_.size());effects_.emplace_back();if(!effects_.back().prepare(fx,rate)){error_="Unsupported FX processor settings";return false;}
        }
        for(unsigned j=0;j<2;++j)if(patch.returnEnabled[j]&&!returns_[j].prepare(patch.returns[j],rate)){error_="Unsupported local return settings";return false;}
        return true;
    }
    void bypassFx(bool dry)noexcept{dry_=dry;}
    // Audio-thread-only edits. Storage and routing remain unchanged.
    bool setEffectControl(unsigned target,unsigned parameter,double value,bool immediate=false)noexcept{
        if(!patch_||target>=effects_.size()+2||!std::isfinite(value)||value<0||value>1)return false;
        const bool auxiliary=target>=effects_.size();const auto a=target-effects_.size();if(auxiliary&&!patch_->returnEnabled[a])return false;
        auto& effect=auxiliary?returns_[a]:effects_[target];auto& s=stages_[target];
        if(parameter<12)return effect.setControl(parameter,value,immediate);
        if(parameter==12){s.target=value*2;if(immediate)s.level=s.target;return true;}
        if(parameter==13){s.enabled=value>=.5;if(immediate)s.blend=s.enabled?1:0;return true;}
        if(parameter==255){effect.resetControls(immediate);s.target=1;s.enabled=true;if(immediate){s.level=s.blend=1;}return true;}return false;
    }
    const std::string& error()const noexcept{return error_;}
    double tail()const noexcept {double seconds=0;if(patch_)for(const auto& n:patch_->nodes)if(n.effect){
        if(n.fx.id<=8||n.fx.id==19)seconds=std::max(seconds,12.);
        else if(n.fx.id>=10&&n.fx.id<=16)seconds=std::max(seconds,60.);
        else if((n.fx.id>=20&&n.fx.id<=31)||n.fx.id==60||n.fx.id==61)seconds=std::max(seconds,.5);}
        if(patch_&&(patch_->returnEnabled[0]||patch_->returnEnabled[1]))seconds=std::max(seconds,12.);return seconds;}
    void tempo(double bpm)noexcept{if(std::isfinite(bpm)&&bpm>=20&&bpm<=400)bpm_=bpm;}
    void reset()noexcept{voice_.reset();for(auto& effect:effects_)effect.reset();for(auto& effect:returns_)effect.reset();panicPending_=false;}
    bool send(unsigned status,unsigned key,unsigned value)noexcept{
        if((status&0xf0)==0xb0&&key==120)panicPending_=true;return voice_.send(status,key,value);
    }
    unsigned activePartials()const noexcept{return voice_.activePartials();}
    unsigned peakPartials()const noexcept{return voice_.peakPartials();}
    bool process(double* output,std::size_t frames)noexcept {
        if(!patch_||(!output&&frames))return false;
        if(dry_||patch_->nodes.empty())return voice_.process(output,frames);
        double bus[512]{},aux[2][512]{};std::size_t done=0;
        while(done<frames){const auto count=std::min<std::size_t>(256,frames-done);for(std::size_t j=0;j<count*2;++j){bus[j]=aux[0][j]=aux[1][j]=0;output[2*done+j]=0;}
            bool routed=false;
            for(std::size_t i=0;i<patch_->nodes.size();++i){const auto& node=patch_->nodes[i];
                if(!node.effect){if(!voice_.process(bus,count,node.source,false))return false;continue;}
                const auto index=std::size_t(effectIndex_[i]);if(!stage(effects_[index],bus,count,index,false))return false;
                for(unsigned a=0;a<2;++a)if(node.fx.sends[a]>0)for(std::size_t j=0;j<count*2;++j)aux[a][j]+=bus[j]*node.fx.sends[a]*patch_->returnSend[a];
                if(node.fx.output){routed=true;for(std::size_t j=0;j<count;++j){output[2*(done+j)]+=bus[2*j]*node.fx.gain*2*(1-node.fx.pan);output[2*(done+j)+1]+=bus[2*j+1]*node.fx.gain*2*node.fx.pan;}
                    if(node.fx.output>1)for(std::size_t j=0;j<count*2;++j)bus[j]=0;}
            }
            if(!routed)for(std::size_t j=0;j<count*2;++j)output[2*done+j]=bus[j];
            for(std::size_t j=0;j<count*2;++j)bus[j]=output[2*done+j];
            for(unsigned a=0;a<2;++a)if(patch_->returnEnabled[a]){
                if(!explicitSend_[a])for(std::size_t j=0;j<count*2;++j)aux[a][j]+=bus[j]*patch_->returnSend[a];
                if(!stage(returns_[a],aux[a],count,effects_.size()+a,true))return false;
                for(std::size_t j=0;j<count;++j){output[2*(done+j)]+=aux[a][2*j]*patch_->returns[a].gain*2*(1-patch_->returns[a].pan);output[2*(done+j)+1]+=aux[a][2*j+1]*patch_->returns[a].gain*2*patch_->returns[a].pan;}
            }
            if(panicPending_&&voice_.activePartials()==0){for(auto& e:effects_)e.reset();for(auto& e:returns_)e.reset();panicPending_=false;}
            for(std::size_t j=0;j<count*2;++j)if(!std::isfinite(output[2*done+j]))return false;
            done+=count;
        }
        return true;
    }
};
}
