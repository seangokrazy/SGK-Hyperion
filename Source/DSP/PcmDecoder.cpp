#include "PcmDecoder.h"
#include <cmath>
#include <cstring>
#include <limits>

namespace sgk::research {
namespace {
std::uint32_t read32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
std::int64_t signedWord(std::uint32_t value) noexcept {
    return value <= 0x7fffffffU ? std::int64_t(value) : std::int64_t(value) - 0x100000000LL;
}
}

DecodeResult inspectSample(const std::uint8_t* data, std::size_t bytes,
                           SampleInfo& info) noexcept {
    info = {};
    if (!data || bytes < 16) return DecodeResult::invalidHeader;
    const auto channels = std::uint32_t(data[12]);
    if ((data[13] != 1 && data[13] != 2) || (channels != 1 && channels != 2)) return DecodeResult::unsupportedMode;
    if(data[13]==2){
        const auto frames=read32(data+4),rate=read32(data+8);const auto words=std::uint64_t(frames)*channels;
        if(!frames||rate<8000||rate>192000||words>std::numeric_limits<std::size_t>::max()/sizeof(float)||words*2!=bytes-16)
            return DecodeResult::invalidDimensions;
        info={rate,frames,channels,std::size_t(words),16};return DecodeResult::ok;
    }
    const auto a = signedWord(read32(data + 4));
    const auto b = signedWord(read32(data + 8));
    const auto frames = a + b;
    const auto rate = 2 * a + b;
    if (frames < 1 || frames > 0xffffffffLL || rate < 8000 || rate > 192000)
        return DecodeResult::invalidDimensions;
    const auto start = std::size_t(16 + 256 * channels);
    const auto words = std::uint64_t(frames) * channels;
    if (words > std::numeric_limits<std::size_t>::max() / sizeof(float) ||
        bytes < start || words != bytes - start) return DecodeResult::invalidDimensions;
    info = {std::uint32_t(rate), std::uint32_t(frames), channels, std::size_t(words), start};
    return DecodeResult::ok;
}

DecodeResult decodeSample(const std::uint8_t* data, std::size_t bytes,
                          float* destination, std::size_t capacity,
                          SampleInfo& info) noexcept {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                  "The inspected sample representation requires IEEE binary32");
    const auto inspected = inspectSample(data, bytes, info);
    if (inspected != DecodeResult::ok) return inspected;
    if (!destination || capacity < info.sampleCount) return DecodeResult::outputTooSmall;
    if(data[13]==2){for(std::size_t i=0;i<info.sampleCount;++i){const auto at=16+2*i;const unsigned raw=unsigned(data[at])|(unsigned(data[at+1])<<8);
        const int value=raw>32767?int(raw)-65536:int(raw);destination[i]=float(value)/32768;}
        return DecodeResult::ok;}
    float predictors[2] = {0, 0};
    unsigned shifts[2] = {data[14], data[15]};
    for (std::size_t i = 0; i < info.sampleCount; ++i) {
        const auto channel = i % info.channels;
        const unsigned code = data[info.payloadOffset + i];
        unsigned shift = shifts[channel];
        const bool up = code >= 128;
        if (up) ++shift;
        if (shift > 31) return DecodeResult::invalidCode;
        const auto delta = std::int32_t(code) - (up ? 192 : 64);
        // Unsigned wrap is defined, including negative delta bit patterns.
        const std::uint32_t bits = (std::uint32_t(delta) << shift) + 0x40c00000U;
        float encoded;
        std::memcpy(&encoded, &bits, sizeof(encoded));
        const double step = double(encoded) - 6.0;
        const float predictor = float(double(predictors[channel]) + step);
        if (!std::isfinite(predictor)) return DecodeResult::invalidCode;
        predictors[channel] = predictor;
        if (!up && ((code + 32) & 64) && shift) --shift;
        shifts[channel] = shift;
        destination[i] = predictor;
    }
    return DecodeResult::ok;
}
}
