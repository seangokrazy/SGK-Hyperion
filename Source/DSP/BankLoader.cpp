#include "BankLoader.h"
#include <cstring>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace sgk::research {
namespace {
using Params=std::array<double,256>;
void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
bool licenseWrapped(const unsigned char* data,std::size_t size)noexcept{
    constexpr char signature[]="SyncrosoftContentHeader";
    return data&&size>=sizeof(signature)-1&&std::memcmp(data,signature,sizeof(signature)-1)==0;
}
struct Bytes {
    const unsigned char* p{};std::size_t n{};
    Bytes slice(std::size_t at,std::size_t count)const{
        require(at<=n&&count<=n-at,"record outside file");return {p+at,count};
    }
    unsigned u32(std::size_t at)const{
        auto s=slice(at,4);return unsigned(s.p[0])|(unsigned(s.p[1])<<8)|(unsigned(s.p[2])<<16)|(unsigned(s.p[3])<<24);
    }
    double f32(std::size_t at)const{
        auto bits=u32(at);float value;std::memcpy(&value,&bits,4);
        require(std::isfinite(value),"nonfinite parameter");return value;
    }
    std::string text(std::size_t at,std::size_t cap)const{
        auto s=slice(at,cap);std::string out;
        for(std::size_t i=0;i<s.n&&s.p[i];++i){require(s.p[i]>=32&&s.p[i]<127,"unsupported name encoding");out.push_back(char(s.p[i]));}
        return out;
    }
};
double normalized(double x){require(std::isfinite(x)&&x>=0&&x<=1,"control outside normalized range");return x;}
double clamp(double x){return std::max(0.0,std::min(1.0,x));}
double seconds(double x){normalized(x);return x<=.5?4*x*x:32*std::pow(x,5);}
// Observed FM frequency control: semitone-independent ratios in half steps.
double fmRatio(double x){normalized(x);return .5*std::floor(x<=.5?24*x:std::exp(3.378*x+.796));}
EnvelopeSettings envelope(const Params& p,unsigned start){
    return {seconds(p[start]),seconds(p[start+1]),std::max(1e-8,std::min(.99,p[start+2])),seconds(p[start+4]),std::exp(p[start+5]*9-7)};
}
Params parameters(Bytes b,std::size_t at){Params p{};for(unsigned i=0;i<256;++i)p[i]=b.f32(at+4*i);return p;}
Params macros(Params p,const Params& g,bool legacy){
    double depths[15]{},offsets[15]{};
    for(unsigned i=0;i<10;++i)depths[i]=2*normalized(g[0x7e + i])-1;
    if(legacy)depths[3]=2*(g[0x81]*.5+.25)-1;
    const unsigned routes[6][4]={{0xe,0x71,0x72,0x70},{0xf,0x74,0x75,0x73},{0x67,0x77,0x78,0x76},
        {0x69,0x8b,0x8c,0x8a},{0x6b,0x8e,0x8f,0x8d},{0x6d,0x7c,0x7d,0x7b}};
    for(const auto& r:routes){const int target=int(normalized(g[r[3]])*15.899999618530273)-1;
        if(target>=0&&target<15)offsets[target]=(normalized(g[r[0]])-normalized(g[r[1]]))*normalized(g[r[2]]);}
    const unsigned targets[][2]={{0x68,3},{0x6a,2},{0x6e,4},{0x6f,5},{0x70,6},{0x71,14},{0x73,6},{0x7e,7},{0x7f,8},{0x80,13},{0x82,9}};
    if(normalized(p[2])>.5)for(const auto& t:targets){
        const double d=depths[t[1]],positive=std::max(0.0,d);
        p[t[0]]=positive*positive+clamp(normalized(p[t[0]])+offsets[t[1]])*(1-d*d);
    }
    return p;
}
}

struct BankLoader::Bank {
    std::vector<unsigned char> data;unsigned kind{},count{};std::size_t metadataEnd{};
    Bytes bytes()const{return {data.data(),data.size()};}
    Bank(const std::filesystem::path& path,unsigned expected,LoadLimits limits){
        std::ifstream file(path,std::ios::binary|std::ios::ate);require(bool(file),"cannot open bank file");
        auto end=file.tellg();require(end>=104&&std::uint64_t(end)<=limits.fileBytes,"bank exceeds configured file limits or is truncated");
        data.resize(std::size_t(end));file.seekg(0);file.read(reinterpret_cast<char*>(data.data()),std::streamsize(data.size()));require(bool(file),"short bank read");
        auto b=bytes();require(std::memcmp(b.p,"Milan Bank file ",15)==0,"not an ordinary Milan bank");
        kind=b.u32(48);require(kind==expected,"unsupported bank kind or opaque/wrapped variant");
        const auto meta=b.u32(84);require(meta<=b.n-88,"metadata outside bank");metadataEnd=88+meta;
        count=b.u32(88);require(count<=65536&&metadataEnd>=104&&count<=(metadataEnd-104)/48,"invalid bank directory");
    }
    Bytes record(unsigned index)const{
        require(index<count,"invalid bank record index");auto b=bytes();const auto entry=104+std::size_t(index)*48;
        return b.slice(b.u32(entry+16),b.u32(entry+20));
    }
    Bytes sampleRecord(unsigned index)const{
        auto b=bytes();const auto countSamples=b.u32(100);const auto base=std::uint64_t(88)+b.u32(96);
        require(countSamples<=65536&&base<=metadataEnd&&std::uint64_t(countSamples)*32<=metadataEnd-base,"invalid sample descriptor table");
        require(index>0&&index<countSamples,"invalid sample index");const auto entry=std::size_t(base)+std::size_t(index)*32;
        return b.slice(b.u32(entry+8),b.u32(entry+12));
    }
};

std::shared_ptr<BankLoader::Bank> BankLoader::sampleBank(unsigned id){
    auto found=banks_.find(id);if(found!=banks_.end())return found->second;
    require(id<128,"invalid sample bank ID");char prefix[8];std::snprintf(prefix,sizeof(prefix),"%03u ",id);
    std::filesystem::path selected;
    for(const auto& e:std::filesystem::directory_iterator(root_/"Samples"))if(e.is_regular_file()){
        auto name=e.path().filename().string();if(name.compare(0,4,prefix)==0){require(selected.empty(),"ambiguous duplicate sample bank ID");selected=e.path();}
    }
    require(!selected.empty(),"required sample bank not present");auto bank=std::make_shared<Bank>(selected,2,limits_);banks_[id]=bank;return bank;
}

std::shared_ptr<const DecodedPcm> BankLoader::sample(unsigned bank,unsigned index){
    auto key=std::make_pair(bank,index);auto found=decoded_.find(key);if(found!=decoded_.end())return found->second;
    auto source=sampleBank(bank)->sampleRecord(index);SampleInfo info;
    require(!licenseWrapped(source.p,source.n),"unsupported protected sample (Syncrosoft-wrapped); supply an authorized unwrapped export");
    require(inspectSample(source.p,source.n,info)==DecodeResult::ok,"unsupported or malformed sample encoding");
    require(decodedBytes_<=limits_.decodedBytes&&info.sampleCount<=(limits_.decodedBytes-decodedBytes_)/sizeof(float),"configured decoded PCM budget exceeded");
    auto result=std::make_shared<DecodedPcm>();result->pcm.resize(info.sampleCount);
    require(decodeSample(source.p,source.n,result->pcm.data(),result->pcm.size(),result->info)==DecodeResult::ok,"sample decode failed");
    decodedBytes_+=info.sampleCount*sizeof(float);decoded_[key]=result;return result;
}

std::shared_ptr<const WavetableAsset> BankLoader::wavetable(unsigned id,unsigned index){
    const auto key=std::make_pair(id,index);auto found=waves_.find(key);if(found!=waves_.end())return found->second;
    require(id<128,"invalid wavetable bank ID");char prefix[8];std::snprintf(prefix,sizeof(prefix),"%03u ",id);
    std::filesystem::path selected;
    for(const auto& e:std::filesystem::directory_iterator(root_/"Wavetables"))if(e.is_regular_file()&&e.path().filename().string().compare(0,4,prefix)==0){
        require(selected.empty(),"ambiguous duplicate wavetable bank ID");selected=e.path();}
    require(!selected.empty(),"required wavetable bank not present");Bank source(selected,3,limits_);const auto b=source.record(index);
    require(b.n==66312&&b.u32(0)==0x01000064&&b.u32(4)==0x00400000,"unsupported wavetable representation");
    constexpr std::size_t count=33152;
    require(decodedBytes_<=limits_.decodedBytes&&count<=(limits_.decodedBytes-decodedBytes_)/sizeof(float),"configured decoded PCM budget exceeded");
    auto wave=std::make_shared<WavetableAsset>();wave->pcm.resize(count);
    for(std::size_t i=0;i<count;++i){const auto at=8+2*i;const unsigned raw=unsigned(b.p[at])|(unsigned(b.p[at+1])<<8);
        const int value=raw>32767?int(raw)-65536:int(raw);wave->pcm[i]=float(value)/32768;}
    std::size_t at=0;for(unsigned points=256;points>=2;points/=2)for(unsigned frame=0;frame<64;++frame){
        require(wave->pcm[at]==wave->pcm[at+points],"wavetable loop guard mismatch");at+=points+1;}
    require(at==count,"invalid wavetable dimensions");decodedBytes_+=count*sizeof(float);waves_[key]=wave;return wave;
}

// Browser-only metadata check: never decode or preload the sample library.
bool BankLoader::browserMapAvailable(unsigned id,unsigned index)const{
    if(id==128||index==128)return true;
    const auto key=std::make_pair(id,index);const auto cached=browserMaps_.find(key);
    if(cached!=browserMaps_.end())return cached->second;
    bool available=false;
    try{
        auto found=browserSamplePaths_.find(id);
        if(found==browserSamplePaths_.end()){
            char prefix[8];std::snprintf(prefix,sizeof(prefix),"%03u ",id);std::filesystem::path selected;bool duplicate=false;
            for(const auto& file:std::filesystem::directory_iterator(root_/"Samples"))if(file.is_regular_file()&&file.path().filename().string().compare(0,4,prefix)==0){
                if(!selected.empty())duplicate=true;selected=file.path();}
            found=browserSamplePaths_.emplace(id,duplicate?std::filesystem::path{}:selected).first;
        }
        if(!found->second.empty()){
            std::ifstream file(found->second,std::ios::binary|std::ios::ate);const auto length=file.tellg();
            if(file&&length>=104&&std::uint64_t(length)<=limits_.fileBytes){
                unsigned char header[104]{};file.seekg(0);file.read(reinterpret_cast<char*>(header),104);Bytes h{header,104};
                const auto count=h.u32(88),meta=h.u32(84);const auto directoryAt=104ULL+std::uint64_t(index)*48;
                if(file&&std::memcmp(header,"Milan Bank file ",15)==0&&h.u32(48)==2&&index<count&&count<=65536&&
                   meta<=std::uint64_t(length)-88&&directoryAt+48<=88ULL+meta){
                    unsigned char entry[48]{};file.seekg(std::streamoff(directoryAt));file.read(reinterpret_cast<char*>(entry),48);Bytes e{entry,48};
                    const auto offset=e.u32(16),size=e.u32(20);
                    if(file&&offset<=std::uint64_t(length)&&size<=std::uint64_t(length)-offset){
                        unsigned char signature[sizeof("SyncrosoftContentHeader")-1]{};const auto bytes=std::min<std::size_t>(sizeof(signature),size);
                        file.seekg(offset);file.read(reinterpret_cast<char*>(signature),std::streamsize(bytes));
                        available=bool(file)&&!licenseWrapped(signature,bytes);
                    }
                }
            }
        }
    }catch(const std::filesystem::filesystem_error&){available=false;}
    browserMaps_[key]=available;return available;
}

std::vector<PatchEntry> BankLoader::catalog(const std::filesystem::path& path,bool hideUnavailableBanks)const{
    Bank bank(path,1,limits_);std::vector<PatchEntry> entries;entries.reserve(bank.count);
    for(unsigned i=0;i<bank.count;++i){PatchEntry entry;entry.index=i;bool unavailable=false;
        auto b=bank.record(i);
        if(b.n>=64&&std::memcmp(b.p,"StSS",4)==0){entry.name=b.text(32,32);entry.readable=true;
            if(b.n>=0x620){const auto elements=b.u32(20);entry.pcmOnly=entry.supportedEngines=elements>0&&elements<=64&&std::size_t(elements)*0x420<=b.n-0x620;
                unsigned sounds=0;
                for(unsigned j=0;j<elements&&entry.supportedEngines;++j){const auto at=0x620+std::size_t(j)*0x420+32;
                    if(normalized(b.f32(at))>.5)continue;
                    const int engine=int(normalized(b.f32(at+4))*15.899999618530273);
                    if(engine==0){const unsigned id=unsigned(normalized(b.f32(at+0x13*4))*128.89999389648438);if(!supportedFx(id)){entry.supportedEngines=false;break;}continue;}
                    if(engine!=2)entry.pcmOnly=false;
                    if(engine<1||engine>5){entry.supportedEngines=false;break;}++sounds;
                    if(engine==2){const unsigned id=unsigned(normalized(b.f32(at+0x12*4))*128.89999389648438);
                        if(hideUnavailableBanks&&!browserMapAvailable(id,unsigned(normalized(b.f32(at+0x13*4))*128.89999389648438)))unavailable=true;
                        if(std::find(entry.requiredBanks.begin(),entry.requiredBanks.end(),id)==entry.requiredBanks.end())entry.requiredBanks.push_back(id);}
                }
                entry.supportedEngines=entry.supportedEngines&&sounds>0;
            }
        }
        if(!unavailable)entries.push_back(std::move(entry));
    }
    return entries;
}

std::shared_ptr<const PreparedPatch> BankLoader::load(const std::filesystem::path& path,unsigned index,bool legacy){
    Bank bank(path,1,limits_);auto b=bank.record(index);
    return loadRecord(b.p,b.n,legacy);
}
std::shared_ptr<const PreparedPatch> BankLoader::loadRecord(const unsigned char* data,std::size_t bytes,bool legacy){
    require(data&&bytes<=limits_.fileBytes,"invalid or oversized preset record");Bytes b{data,bytes};
    require(!licenseWrapped(data,bytes),"unsupported protected preset record (Syncrosoft-wrapped); supply an authorized unwrapped export");
    require(b.n>=0x620&&std::memcmp(b.p,"StSS",4)==0,"unsupported protected, empty or non-StSS patch record");
    require(b.u32(4)<220000000,"unsupported preset version");const auto count=b.u32(20);
    require(count>0&&count<=64&&std::size_t(count)*0x420<=b.n-0x620,"invalid element table");
    auto globals=parameters(b,0x220);auto result=std::make_shared<PreparedPatch>();result->name=b.text(32,32);
    result->layers.resize(count);result->zones.resize(count);unsigned active=0;
    for(unsigned i=0;i<count;++i){const auto at=0x620+std::size_t(i)*0x420;auto p=parameters(b,at+32);
        const unsigned engine=unsigned(normalized(p[1])*15.899999618530273);
        if(engine==0){PatchNode node;node.effect=true;auto& fx=node.fx;
            fx.id=normalized(p[0])>.5?128:unsigned(normalized(p[0x13])*128.89999389648438);
            require(supportedFx(fx.id),"unsupported FX type");fx.output=unsigned(normalized(p[0x14])*15.899999618530273);
            fx.gain=4*normalized(p[0x8b])*p[0x8b];fx.pan=normalized(p[0x8c]);
            for(unsigned j=0;j<12;++j)fx.p[j]=normalized(p[0x19+j]);
            for(unsigned j=0;j<4;++j)fx.sends[j]=4*normalized(p[0x15+j])*p[0x15+j];
            result->nodes.push_back(node);continue;
        }
        if(normalized(p[0])>.5)continue;
        require(engine>=1&&engine<=5,"preset requires an unsupported synthesis element/FX");
        p=macros(p,globals,legacy);if(p[0]>.5||p[0x8b]==0)continue;
        auto& l=result->layers[active];l.engine=engine;l.node=i;result->nodes.push_back(PatchNode{false,i,FxSettings{}});
        l.lowKey=unsigned(normalized(p[0x31])*127.9000015258789);l.highKey=unsigned(normalized(p[0x32])*127.9000015258789);
        l.lowVelocity=unsigned(normalized(p[0x34])*127.9000015258789);l.highVelocity=unsigned(normalized(p[0x35])*127.9000015258789);
        l.keyFade=std::floor(normalized(p[0x33])*p[0x33]*127.9)+1;l.velocityFade=std::floor(normalized(p[0x36])*p[0x36]*127.9)+1;
        l.trigger=unsigned(normalized(p[0x37])*15.899999618530273);l.choke=unsigned(normalized(p[0x38])*15.899999618530273);
        l.gain=4*normalized(p[0x8b])*p[0x8b];l.pan=normalized(p[0x8c]);
        l.octave=(int(normalized(p[0x46])*8+.5)*12-48);l.transpose=int(normalized(p[0x47])*24+.5)-12;
        const double fine=normalized(p[0x48])-.5;l.fine=2*fine*std::abs(fine);l.keyTracking=normalized(p[0x49])*4-1;
        l.amplitude=envelope(p,0x7e);l.filterEnvelope=envelope(p,0x6f);
        l.filterMode=unsigned(normalized(p[0x67])*15.899999618530273);l.cutoff=normalized(p[0x6a]);l.resonance=normalized(p[0x68]);
        l.filterDepth=(2*normalized(p[0x6e])-1)*6.931471824645996;l.filterKeyFollow=normalized(p[0x6b])*4-1;
        if(engine==1){
            auto& s=l.drawbar;s.waveFlavor=unsigned(normalized(p[0x13])*128.89999389648438);s.mode=unsigned(normalized(p[0x14])*3.9);
            s.percussion=normalized(p[0x15])>.5;s.soft=normalized(p[0x16])<=.5;s.fast=normalized(p[0x17])>.5;s.third=normalized(p[0x18])>.5;
            for(unsigned j=0;j<9;++j){const double step=std::floor(normalized(p[0x19+j])*8.9);s.levels[j]=step*step/64;}
            s.drive=normalized(p[0x22]);s.clickSeconds=.001+.009*normalized(p[0x23]);s.click=normalized(p[0x24]);l.gain*=.25;
            result->zones[active].push_back(ZoneView{});l.zones=result->zones[active].data();l.zoneCount=1;++active;continue;
        }
        if(engine==3){
            auto asset=wavetable(unsigned(normalized(p[0x12])*128.89999389648438),unsigned(normalized(p[0x13])*128.89999389648438));
            auto& s=l.wavetable;s.table={asset->pcm.data(),asset->pcm.size(),256,64};s.position=normalized(p[0x62]);s.blend=normalized(p[0x1d]);
            s.virtualWave=unsigned(normalized(p[0x1c])*7.9);l.gain*=.25;
            l.transpose+=std::floor(normalized(p[0x16])*48+.5)-24;const double fineWave=normalized(p[0x17])-.5;l.fine+=2*fineWave*std::abs(fineWave);
            result->wavetables.push_back(std::move(asset));result->zones[active].push_back(ZoneView{});
            l.zones=result->zones[active].data();l.zoneCount=1;++active;continue;
        }
        if(engine==4){
            auto& s=l.fm;l.gain*=.25;
            s.ratioA=fmRatio(p[0x11]);s.fineA=.5*normalized(p[0x12])*p[0x12];
            s.directA=3*normalized(p[0x13]);s.depthA=3*normalized(p[0x14])*p[0x14];s.velocityA=normalized(p[0x15]);
            s.lowerA=3-4*normalized(p[0x16]);s.pivotA=127.9*normalized(p[0x17]);
            s.upperA=4*normalized(p[0x18])*(2-p[0x18])-2;
            s.ratioB=fmRatio(p[0x19]);s.fineB=.5*normalized(p[0x1a])*p[0x1a];s.keyTrackB=normalized(p[0x1b]);
            s.depthB=3*normalized(p[0x1c])*p[0x1c];s.velocityB=normalized(p[0x1d]);
            s.lowerB=3-4*normalized(p[0x1e]);s.pivotB=127.9*normalized(p[0x1f]);
            s.upperB=4*normalized(p[0x20])*(2-p[0x20])-2;
            s.transitionSeconds=seconds(p[0x21]);s.transition=2*normalized(p[0x22]);
            s.fixedHz=normalized(p[0x23])<=.05?0:50*std::pow(p[0x23],4);
            result->zones[active].push_back(ZoneView{});l.zones=result->zones[active].data();l.zoneCount=1;++active;continue;
        }
        if(engine==5){
            // Independent prototype headroom: oscillator/filter level matching
            // against the original engine is still unverified.
            l.gain*=.25;
            auto& s=l.analog;
            s.modeA=unsigned(normalized(p[0x13])*7.9);s.modeB=unsigned(normalized(p[0x1b])*7.9);
            s.extraWave=unsigned(normalized(p[0x1c])*7.9);
            const double a=normalized(p[0x17])-.5,bFine=normalized(p[0x1a])-.5;
            s.tuneA=std::floor(normalized(p[0x16])*48+.5)-24+2*a*std::abs(a);
            s.tuneB=std::floor(normalized(p[0x19])*48+.5)-24+2*bFine*std::abs(bFine);
            s.levelA=normalized(p[0x15])*p[0x15];s.levelB=normalized(p[0x21]);
            s.extraLevel=normalized(p[0x1d])+.7079458*normalized(p[0x1e])*p[0x1e];
            s.width=std::clamp(.5+normalized(p[0x18])*.1,.01,.99);
            // Unison spacing is an independent conservative prototype, not
            // a recovered vendor oscillator transfer function.
            s.detune=.01+.68*normalized(p[0x20])*p[0x20];
            if((s.modeA==6&&s.levelA>0)||(s.modeB==6&&s.levelB>0)||((s.extraWave==5||s.extraWave==6)&&s.extraLevel>0)){
                auto asset=wavetable(0,unsigned(normalized(p[0x22])*128.89999389648438));
                s.table={asset->pcm.data(),asset->pcm.size(),256,64};s.tablePosition=normalized(p[0x62]);result->wavetables.push_back(std::move(asset));
            }
            result->zones[active].push_back(ZoneView{});
            l.zones=result->zones[active].data();l.zoneCount=1;++active;continue;
        }
        const unsigned bankId=unsigned(normalized(p[0x12])*128.89999389648438),mapId=unsigned(normalized(p[0x13])*128.89999389648438);
        if(bankId==128||mapId==128)continue;
        auto payload=sampleBank(bankId)->record(mapId);
        if(payload.n==0)continue;
        if(licenseWrapped(payload.p,payload.n))throw std::runtime_error("unsupported protected sample map (Syncrosoft-wrapped), bank "+std::to_string(bankId)+", map "+std::to_string(mapId)+"; supply an authorized unwrapped export");
        require(payload.n>0&&payload.n%64==0&&payload.n/64<=4096,"invalid zone table");auto& zones=result->zones[active];zones.reserve(payload.n/64);
        for(std::size_t pos=0;pos<payload.n;pos+=64){auto z=payload.slice(pos,64);auto flags=z.u32(4);
            if(z.u32(8)==0)continue;auto pcm=sample(bankId,z.u32(8));
            ZoneView zone;zone.lowKey=z.p[1];zone.highKey=z.p[3];zone.lowVelocity=z.p[0];zone.highVelocity=z.p[2];zone.root=flags&127;
            zone.endpoint=z.u32(16);zone.loopSpan=z.u32(20);zone.fine=z.f32(24);
            zone.transpose=(flags&0x200)==0;
            zone.sample={pcm->pcm.data(),pcm->info.frames,pcm->info.channels,double(pcm->info.sampleRate)};
            zones.push_back(zone);result->samples.push_back(std::move(pcm));
        }
        if(zones.empty())continue;l.zones=zones.data();l.zoneCount=zones.size();++active;
    }
    require(active>0,"no active PCM elements");result->layers.resize(active);result->zones.resize(active);
    auto returnsAt=0x620+std::size_t(count)*0x420;
    const auto returnFlags=b.u32(24);
    for(unsigned j=0;j<2;++j){
        const unsigned flag=(returnFlags>>(8*j))&255;
        require(flag<=2,"unsupported local FX return flag");if(!flag)continue;
        require(returnsAt<=b.n&&0x420<=b.n-returnsAt,"local FX return outside record");
        const auto p=parameters(b,returnsAt+32);returnsAt+=0x420;
        require(unsigned(normalized(p[1])*15.899999618530273)==0,"unsupported local FX return format");
        auto& fx=result->returns[j];fx.id=normalized(p[0])>.5?128:unsigned(normalized(p[0x13])*128.89999389648438);
        require(supportedFx(fx.id),"unsupported local FX return type");fx.gain=4*normalized(p[0x8b])*p[0x8b];fx.pan=normalized(p[0x8c]);
        for(unsigned k=0;k<12;++k)fx.p[k]=normalized(p[0x19+k]);
        result->returnEnabled[j]=fx.id!=128;result->returnSend[j]=normalized(globals[0x3f+j])*globals[0x3f+j];
    }
    PcmInstrument validator;require(validator.prepare(result->layers.data(),result->layers.size(),44100),"prepared preset dimensions/DSP not supported");
    return result;
}
}
