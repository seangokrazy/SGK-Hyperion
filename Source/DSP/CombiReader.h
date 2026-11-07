#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace sgk::research {
struct CombiPart {unsigned slot{},metadata{};std::string name;std::vector<unsigned char> patch;};
struct CombiDefinition {std::string name;double master{.5};std::vector<CombiPart> parts;std::array<std::array<double,256>,4> globalFx{};std::array<unsigned char,16> linkFlags{};bool hasLinkFlags{true};};
CombiDefinition readCombi(const std::filesystem::path&,std::size_t maximumBytes=512ULL*1024*1024);
}
