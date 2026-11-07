#include "PcmDecoder.h"
#include <cstdio>
#include <cstring>
using namespace sgk::research;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x); return 1; } } while (0)
static void word(unsigned char* p, std::uint32_t v) {
    for (unsigned i=0;i<4;++i) p[i]=static_cast<unsigned char>(v>>(8*i));
}
int main() {
    unsigned char data[276]{};
    word(data+4,44096); word(data+8,std::uint32_t(-44092));
    data[12]=1;data[13]=1;data[14]=8;data[272]=127;data[273]=1;data[274]=64;data[275]=64;
    SampleInfo info;float out[5]={0,0,0,0,123};
    CHECK(inspectSample(data,sizeof(data),info)==DecodeResult::ok);
    CHECK(info.sampleRate==44100 && info.frames==4 && info.sampleCount==4);
    CHECK(decodeSample(data,sizeof(data),out,4,info)==DecodeResult::ok);
    CHECK(out[0]==63.0f*256/2097152 && out[1]==0 && out[2]==0 && out[4]==123);
    out[0]=42;
    CHECK(decodeSample(data,sizeof(data),out,3,info)==DecodeResult::outputTooSmall && out[0]==42);
    CHECK(inspectSample(data,sizeof(data)-1,info)==DecodeResult::invalidDimensions);
    CHECK(inspectSample(nullptr,0,info)==DecodeResult::invalidHeader);
    data[13]=99;CHECK(inspectSample(data,sizeof(data),info)==DecodeResult::unsupportedMode);data[13]=1;
    data[14]=32;CHECK(decodeSample(data,sizeof(data),out,4,info)==DecodeResult::invalidCode);
    unsigned char stereo[532]{};word(stereo+4,44098);word(stereo+8,std::uint32_t(-44096));
    stereo[12]=2;stereo[13]=1;stereo[14]=8;stereo[15]=8;
    stereo[528]=127;stereo[529]=64;stereo[530]=1;stereo[531]=64;
    CHECK(decodeSample(stereo,sizeof(stereo),out,4,info)==DecodeResult::ok);
    CHECK(info.frames==2 && info.channels==2 && out[0]>0 && out[1]==0 && out[2]==0 && out[3]==0);
    unsigned char raw[24]{};word(raw+4,2);word(raw+8,48000);raw[12]=2;raw[13]=2;
    raw[17]=128;raw[18]=255;raw[19]=127;raw[20]=1;raw[22]=255;raw[23]=255;
    CHECK(decodeSample(raw,sizeof(raw),out,4,info)==DecodeResult::ok);
    CHECK(info.frames==2&&info.sampleRate==48000&&info.payloadOffset==16);
    CHECK(out[0]==-1&&out[1]==32767.f/32768&&out[2]==1.f/32768&&out[3]==-1.f/32768);
    CHECK(inspectSample(raw,sizeof(raw)-1,info)==DecodeResult::invalidDimensions);
    std::puts("Native decoder checks passed");return 0;
}
