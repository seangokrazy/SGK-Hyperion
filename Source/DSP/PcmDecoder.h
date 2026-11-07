#pragma once
#include <cstddef>
#include <cstdint>

namespace sgk::research {
enum class DecodeResult { ok, invalidHeader, unsupportedMode, invalidDimensions,
                          outputTooSmall, invalidCode };
struct SampleInfo {
    std::uint32_t sampleRate{};
    std::uint32_t frames{};
    std::uint32_t channels{};
    std::size_t sampleCount{};
    std::size_t payloadOffset{};
};
// Ordinary unwrapped format-1 adaptive blocks and format-2 signed PCM16. No heap,
// I/O, locks or global state in either function. Decode is intended for loading,
// not for repeatedly decoding entire sources in an audio callback.
// On any non-ok result, discard output (an invalid stream may have produced a
// partial decode). Capacity/dimension failures are checked before output writes.
DecodeResult inspectSample(const std::uint8_t* data, std::size_t bytes,
                           SampleInfo& info) noexcept;
DecodeResult decodeSample(const std::uint8_t* data, std::size_t bytes,
                          float* destination, std::size_t capacity,
                          SampleInfo& info) noexcept;
}
