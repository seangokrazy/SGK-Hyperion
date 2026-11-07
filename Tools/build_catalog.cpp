#include "GraphPlayer.h"
#include <fstream>
#include <cctype>
#include <iostream>
using namespace sgk::research;
unsigned category(std::string b,std::string n){for(auto& c:b)c=char(std::tolower(static_cast<unsigned char>(c)));for(auto& c:n)c=char(std::tolower(static_cast<unsigned char>(c)));
    const char* words[]={"brass","string","piano","bass","pad","synth","drum","guitar","woodwind","bell","ethnic","vocal","effect"};
    for(unsigned j=0;j<13;++j)if(b.find(words[j])!=std::string::npos||n.find(words[j])!=std::string::npos)return j+1;
    if(b.find("organ")!=std::string::npos||b.find("clav")!=std::string::npos)return 3;
    if(b.find("lead")!=std::string::npos||b.find("arpeggio")!=std::string::npos)return 6;return 14;
}
int main(int argc,char** argv){try{if(argc!=2){std::cerr<<"Usage: hyperion-catalog <content-folder-containing-Patches-and-Samples>\n";return 2;}
    const auto root=std::filesystem::absolute(argv[1]);if(!std::filesystem::is_directory(root/"Patches")||!std::filesystem::is_directory(root/"Samples"))throw std::runtime_error("Expected Patches and Samples folders");
    std::vector<std::filesystem::path> banks;for(const auto& f:std::filesystem::directory_iterator(root/"Patches"))if(f.is_regular_file())banks.push_back(f.path());std::sort(banks.begin(),banks.end());
    std::string rows;unsigned passed=0,rejected=0;for(const auto& bank:banks){const auto name=bank.filename().string();if(name.size()>191||name.find_first_of("\t\r\n")!=std::string::npos)continue;
        BankLoader reader(root);std::vector<PatchEntry> entries;try{entries=reader.catalog(bank);}catch(const std::exception& e){std::cerr<<"SKIP "<<name<<": "<<e.what()<<'\n';continue;}
        for(const auto& entry:entries){if(!entry.readable||!entry.supportedEngines||entry.name.size()>191||entry.name.empty()||entry.name.find_first_of("\t\r\n")!=std::string::npos){++rejected;continue;}
            try{BankLoader loader(root);auto patch=loader.load(bank,entry.index,false);GraphPlayer player;if(!player.prepare(*patch,44100))throw std::runtime_error(player.error());double audio[512]{};
                for(unsigned key:{36U,60U,84U}){player.reset();player.send(0x90,key,100);for(unsigned block=0;block<16;++block)if(!player.process(audio,256))throw std::runtime_error("Non-finite render");}
                rows+=name+'\t'+std::to_string(entry.index)+'\t'+entry.name+'\t'+std::to_string(category(name,entry.name))+'\n';++passed;
            }catch(const std::exception& e){++rejected;std::cerr<<"SKIP "<<name<<" #"<<entry.index<<": "<<e.what()<<'\n';}}
        std::cout<<"Scanned "<<name<<"; playable so far "<<passed<<std::endl;
    }if(!passed)throw std::runtime_error("No compatible presets; catalog not changed");
    const auto temp=root/"catalog.tsv.new",dest=root/"catalog.tsv";{std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<rows;if(!f)throw std::runtime_error("Cannot write catalog");}
    if(std::filesystem::exists(dest)){const auto backup=root/"catalog.tsv.backup";if(std::filesystem::exists(backup))throw std::runtime_error("Remove/rename existing catalog.tsv.backup before replacing catalog");std::filesystem::rename(dest,backup);}
    std::filesystem::rename(temp,dest);std::cout<<"Wrote "<<passed<<" presets; rejected "<<rejected<<" unsupported entries.\n";return 0;
}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
