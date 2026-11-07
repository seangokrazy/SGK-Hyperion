#include "CombiReader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace sgk::research {
namespace {
void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
unsigned little(const std::vector<unsigned char>& b,std::size_t at){check(at<=b.size()&&4<=b.size()-at,"combi field outside chunk");return unsigned(b[at])|(unsigned(b[at+1])<<8)|(unsigned(b[at+2])<<16)|(unsigned(b[at+3])<<24);}
unsigned big(const std::vector<unsigned char>& b,std::size_t at){check(at<=b.size()&&4<=b.size()-at,"combi wrapper truncated");return(unsigned(b[at])<<24)|(unsigned(b[at+1])<<16)|(unsigned(b[at+2])<<8)|unsigned(b[at+3]);}
void put(std::vector<unsigned char>& b,std::size_t at,unsigned x){for(unsigned j=0;j<4;++j)b[at+j]=static_cast<unsigned char>(x>>(8*j));}
double number(const std::vector<unsigned char>& b,std::size_t at){const auto bits=little(b,at);float x;std::memcpy(&x,&bits,4);check(std::isfinite(x),"nonfinite combi parameter");return x;}
std::string name(const std::vector<unsigned char>& b,std::size_t at){check(at<=b.size()&&32<=b.size()-at,"combi name outside chunk");std::string s;for(unsigned j=0;j<32&&b[at+j];++j){check(b[at+j]>=32&&b[at+j]<127,"unsupported combi name encoding");s.push_back(char(b[at+j]));}return s;}
}
CombiDefinition readCombi(const std::filesystem::path& path,std::size_t maximumBytes){
    std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f),"cannot open combi");const auto length=f.tellg();
    check(length>=248&&std::uint64_t(length)<=maximumBytes,"combi exceeds configured file limit or is truncated");
    std::vector<unsigned char> wrapper(static_cast<std::size_t>(length));f.seekg(0);f.read(reinterpret_cast<char*>(wrapper.data()),std::streamsize(wrapper.size()));check(bool(f),"short combi read");
    check(std::memcmp(wrapper.data(),"CcnK",4)==0&&std::memcmp(wrapper.data()+8,"FBCh",4)==0&&std::memcmp(wrapper.data()+16,"StSS",4)==0,"unsupported combi FXB wrapper");
    check(big(wrapper,156)==wrapper.size()-160,"inconsistent combi chunk size");
    std::vector<unsigned char> b(wrapper.begin()+160,wrapper.end());check(little(b,0)==0x53537453,"unsupported combi chunk");
    const auto version=little(b,4);check(version<220000000&&little(b,12)==0x142,"unsupported combi version/layout");
    const bool oldMaster=version==10030910;
    check(big(wrapper,4)==wrapper.size()-8||(oldMaster&&big(wrapper,4)==0),"inconsistent combi wrapper size");
    const auto effectsAt=little(b,80),globalAt=little(b,84);
    check(effectsAt>=88&&effectsAt<=b.size()&&globalAt>=effectsAt&&globalAt<=b.size()&&globalAt-effectsAt==4*0x420,"invalid combi global FX directory");
    check(globalAt<=b.size()&&(oldMaster?32U:64U)<=b.size()-globalAt&&little(b,globalAt)==(oldMaster?4U:52U),"unsupported combi master layout");
    CombiDefinition result;result.hasLinkFlags=!oldMaster;
    result.name=oldMaster?path.stem().string():name(b,globalAt+24);if(result.name.empty())result.name=path.stem().string();result.master=number(b,globalAt+4);
    check(result.master>=0&&result.master<=1,"invalid combi master level");
    if(!oldMaster)std::copy_n(b.begin()+globalAt+8,16,result.linkFlags.begin());
    std::vector<unsigned> offsets;
    for(unsigned slot=0;slot<16;++slot){const auto off=little(b,16+slot*4);if(!off)continue;
        check(off>=88&&off<effectsAt&&(offsets.empty()||off>offsets.back()),"overlapping or unordered combi parts");offsets.push_back(off);}
    for(unsigned slot=0;slot<16;++slot){const auto off=little(b,16+slot*4);if(!off)continue;
        const auto next=std::upper_bound(offsets.begin(),offsets.end(),off);const auto end=next==offsets.end()?effectsAt:*next;
        check(end>off&&end-off>=0x610,"truncated combi part");const auto count=little(b,off+4);
        // Four packed return flags describe how many return records follow
        // the main elements; not every saved part has two local returns.
        const auto flags=little(b,off+8);unsigned returnCount=0;
        for(unsigned byte=0;byte<4;++byte){const unsigned flag=(flags>>(8*byte))&255;check(flag<=2,"unsupported combi return flag");if(flag)++returnCount;}
        check(count<=64&&0x610+std::size_t(count+returnCount)*0x420+4<=end-off,"combi part element table outside bounds");
        CombiPart part;part.slot=slot;part.metadata=little(b,off);part.patch.resize(std::size_t(end-off)+16);
        put(part.patch,0,0x53537453);put(part.patch,4,version);put(part.patch,12,0x150);
        put(part.patch,16,part.metadata);put(part.patch,20,count);put(part.patch,24,little(b,off+8));put(part.patch,28,little(b,off+12));
        std::copy(b.begin()+off+16,b.begin()+end,part.patch.begin()+32);part.name=name(part.patch,32);
        result.parts.push_back(std::move(part));
    }
    check(!result.parts.empty(),"combi contains no stored parts");
    for(unsigned node=0;node<4;++node)for(unsigned i=0;i<256;++i)result.globalFx[node][i]=number(b,effectsAt+node*0x420+32+i*4);
    return result;
}
}
