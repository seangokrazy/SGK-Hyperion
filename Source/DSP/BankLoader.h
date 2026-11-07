#pragma once
#include "PcmInstrument.h"
#include "PcmDecoder.h"
#include "FxEngine.h"
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace sgk::research {
struct LoadLimits { std::size_t fileBytes=512ULL*1024*1024,decodedBytes=512ULL*1024*1024; };
struct PatchEntry { unsigned index{};std::string name;bool readable{},pcmOnly{},supportedEngines{};std::vector<unsigned> requiredBanks; };
struct DecodedPcm { SampleInfo info;std::vector<float> pcm; };
struct WavetableAsset {std::vector<float> pcm;};
struct PatchNode {bool effect{};unsigned source{};FxSettings fx;};
struct PreparedPatch {
    std::string name;
    std::vector<LayerView> layers;
    std::vector<std::vector<ZoneView>> zones;
    std::vector<std::shared_ptr<const DecodedPcm>> samples;
    std::vector<std::shared_ptr<const WavetableAsset>> wavetables;
    std::vector<PatchNode> nodes;
    std::array<FxSettings,2> returns;std::array<bool,2> returnEnabled{};std::array<double,2> returnSend{};
    PreparedPatch()=default;
    PreparedPatch(const PreparedPatch&)=delete;
    PreparedPatch& operator=(const PreparedPatch&)=delete;
};
// Allocating loader/cache: call only while stopped or on a non-audio worker.
// Ordinary unwrapped format only. No vendor executable or licensing path.
class BankLoader {
    struct Bank;
    std::filesystem::path root_;
    LoadLimits limits_;
    std::map<unsigned,std::shared_ptr<Bank>> banks_;
    std::map<std::pair<unsigned,unsigned>,std::shared_ptr<const DecodedPcm>> decoded_;
    std::map<std::pair<unsigned,unsigned>,std::shared_ptr<const WavetableAsset>> waves_;
    std::size_t decodedBytes_{};
    mutable std::map<unsigned,std::filesystem::path> browserSamplePaths_;
    mutable std::map<std::pair<unsigned,unsigned>,bool> browserMaps_;
    bool browserMapAvailable(unsigned bank,unsigned map)const;
    std::shared_ptr<Bank> sampleBank(unsigned id);
    std::shared_ptr<const DecodedPcm> sample(unsigned bank,unsigned index);
    std::shared_ptr<const WavetableAsset> wavetable(unsigned bank,unsigned index);
public:
    explicit BankLoader(std::filesystem::path root,LoadLimits limits={}):root_(std::move(root)),limits_(limits){}
    std::vector<PatchEntry> catalog(const std::filesystem::path& patchBank,bool hideUnavailableBanks=false)const;
    std::shared_ptr<const PreparedPatch> load(const std::filesystem::path& patchBank,unsigned index,bool legacy=false);
    std::shared_ptr<const PreparedPatch> loadRecord(const unsigned char* data,std::size_t bytes,bool legacy=false);
    std::size_t decodedSamples()const noexcept{return decoded_.size();}
    std::size_t decodedBytes()const noexcept{return decodedBytes_;}
};
}
