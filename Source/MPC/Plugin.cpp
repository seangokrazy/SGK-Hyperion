#include "VST2ABI.h"
#include "HostTempo.h"
#include "AudioFP.h"
#include "../DSP/GraphPlayer.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cstddef>
#include "FxRack.h"
#include <cstdlib>
#include <dlfcn.h>
#ifndef HYPERION_USER
#define HYPERION_USER "/storage/SGK_Hyperion_User"
#endif
// Resolve only on the loader thread; no filesystem work in the callback.
std::filesystem::path libraryRoot(){
    if(const auto* env=std::getenv("SGK_HYPERION_LIBRARY")){if(*env)return std::filesystem::path(env);}
    Dl_info info{};if(dladdr(reinterpret_cast<void*>(&VSTPluginMain),&info)&&info.dli_fname){
        const auto module=std::filesystem::absolute(info.dli_fname).parent_path();
        std::ifstream config(module/"User"/"library-path.txt");std::string path;if(std::getline(config,path)){if(!path.empty()&&path.back()=='\r')path.pop_back();if(!path.empty())return std::filesystem::path(path);}
        return module/"Library";
    }
    throw std::runtime_error("Could not locate the Hyperion module");
}
#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
namespace {
using namespace sgk::research;
constexpr int Count=288,UID=0x53474859;
constexpr const char* categories[]={"All","Brass","Strings","Keys","Bass","Pads","Synth","Drums","Guitar","Winds","Bells","Ethnic","Voices","FX","Other"};
struct Entry{std::string bank,name;unsigned record{},category{};};
struct State{unsigned magic=0x48595001,bytes=sizeof(State),record{};float gain=1,pan=.5f;unsigned fx=1;char bank[192]{};};
struct SavedState{State base;hyperionfx::Snapshot rack;};
struct MidiEvent{int32_t type,size,offset,flags,length,noteOffset;unsigned char data[4];char tail[4];};
struct MidiEvents{int32_t count;intptr_t reserved;MidiEvent* events[1];};
struct Slot{std::atomic<int> state{0};std::shared_ptr<const PreparedPatch> patch;GraphPlayer engine;float rate=44100;int choice=-1;unsigned revision{};hyperionfx::Snapshot factory,initial;};
struct Instance{
    AEffect effect{};Host host{};std::thread worker;std::atomic<bool> stop{false},catalogReady{false},panic{false},favoriteDirty{false},uiDirty{true};
    std::filesystem::path library;std::mutex ui;std::vector<Entry> entries;std::vector<std::string> banks{"All Banks"};std::vector<int> view;std::vector<unsigned char> favorites;
    int bank{},category=1,cursor{},menu{},page{},panel{},categoryPage{};bool favoritesOnly{};std::string message="Reading library",restoreBank,query;unsigned restoreRecord{};
    float values[Count]{};std::chrono::steady_clock::time_point taps[Count]{};float encoderLast[3]{.5f,.5f,.5f};
    std::atomic<unsigned> encoderReads[3]{};unsigned encoderSeen[3]{};std::chrono::steady_clock::time_point encoderTime[3]{};
    hyperionfx::Snapshot rack,factory,restoreRack;unsigned rackRevision{},restoreRevision{},rackInsertCount{};int rackSelected{},rackPage{},controlPage{};bool rackEditing{};
    struct FxCommand{unsigned revision,target,param;float value;};FxCommand commands[256]{};std::atomic<unsigned> commandRead{},commandWrite{},playingRevision{};std::atomic<double> displayBpm{120};
    std::atomic<unsigned> wanted{1};std::atomic<int> requested{-1},pending{-1},audible{-1};std::atomic<float> rate{44100},gain{1},pan{.5f},meter[2]{};std::atomic<bool> fx{true},clip{false};
    Slot slots[3];int active=-1,retiring=-1,fade{},fadeTotal=220;float smoothGain=1;double scratch[512]{},oldScratch[512]{},bpm=120;
    struct Event{unsigned status,key,value;int offset;};Event midi[256]{};unsigned midiCount{};SavedState state;
    ~Instance(){stop=true;if(worker.joinable())worker.join();}
};
void notify(Instance*s){s->uiDirty=true;}
int quickCategory(Instance*s,int slot){const int c=s->categoryPage*6+slot+1;return c==15?0:c>15?-1:c;}
std::string bankName(const std::string& name){return name.size()>4?name.substr(4):name;}
std::string lower(std::string name){for(auto& c:name)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return name;}
std::string nameLine(const std::string& name,bool second){if(name.size()<=23)return second?std::string{}:name;auto cut=name.rfind(' ',23);if(cut==std::string::npos||cut<8)cut=23;return second?name.substr(cut+(name[cut]==' '?1:0)):name.substr(0,cut);}
std::string cardLine(const std::string& name,bool second){auto cut=name.rfind(' ',std::min<std::size_t>(23,name.size()/2+3));if(cut==std::string::npos)return nameLine(name,second);return second?name.substr(cut+1):name.substr(0,cut);}
void filter(Instance*s){s->view.clear();for(int i=0;i<int(s->entries.size());++i){const auto&e=s->entries[std::size_t(i)];
    if((s->bank==0||e.bank==s->banks[std::size_t(s->bank)])&&(s->category==0||e.category==unsigned(s->category))&&(!s->favoritesOnly||s->favorites[std::size_t(i)])&&(s->query.empty()||lower(e.name).find(s->query)!=std::string::npos))s->view.push_back(i);}
    s->cursor=std::clamp(s->cursor,0,std::max(0,int(s->view.size())-1));}
void choose(Instance*s,int index,bool force=false){if(index<0||index>=int(s->entries.size())||(!force&&s->requested==index&&s->audible==index))return;s->requested=index;s->wanted.fetch_add(1);s->message="Loading "+s->entries[std::size_t(index)].name;}
bool editFx(Instance*s,unsigned index,unsigned parameter,float value){
    if(index>=s->rack.count||s->rackRevision!=s->playingRevision||s->requested!=s->audible)return false;
    const auto write=s->commandWrite.load(),read=s->commandRead.load();if(write-read>=256){s->message="FX busy - try again";return false;}
    auto& v=s->rack.values[index];s->commands[write%256]={s->rackRevision,v.target,parameter,value};s->commandWrite.store(write+1);
    if(parameter<12)v.p[parameter]=value;else if(parameter==12)v.level=value;else if(parameter==13)v.enabled=value>=.5f;else if(parameter==255)v=s->factory.values[index];return true;
}
hyperionfx::Control selectedControl(Instance*s,int index){return s->rackSelected<int(s->rack.count)?hyperionfx::control(s->rack.values[s->rackSelected].id,unsigned(s->controlPage*6+index)):hyperionfx::Control{-1,""};}
void advance(Instance*s,int step,bool play){if(s->view.empty())return;const int size=int(s->view.size());s->cursor=(s->cursor+step)%size;if(s->cursor<0)s->cursor+=size;if(play)choose(s,s->view[std::size_t(s->cursor)]);}
int menuCount(Instance*s){return s->menu==1?int(s->banks.size()):s->menu==2?15:int(s->view.size());}
int pageSize(Instance*s){return s->menu==4?6:16;}
std::string menuName(Instance*s,int at){if(at<0||at>=menuCount(s))return {};return s->menu==1?(at?bankName(s->banks[std::size_t(at)]):"All Banks"):s->menu==2?categories[at]:s->entries[std::size_t(s->view[std::size_t(at)])].name;}
void loadCatalog(Instance*s){
    std::ifstream file(s->library/"catalog.tsv");if(!file)throw std::runtime_error("Configure library first");std::string line;
    while(std::getline(file,line)){std::istringstream row(line);std::string bank,record,name,category;
        if(!std::getline(row,bank,'\t')||!std::getline(row,record,'\t')||!std::getline(row,name,'\t')||!std::getline(row,category))throw std::runtime_error("Invalid library catalog");
        if(bank.find('/')!=std::string::npos||bank.find('\\')!=std::string::npos||bank==".."||bank.empty()||name.empty()||bank.size()>191||name.size()>191||s->entries.size()>=65536)throw std::runtime_error("Unsafe catalog entry");
        std::size_t end=0;const auto n=std::stoul(record,&end);if(end!=record.size()||n>65535)throw std::runtime_error("Invalid preset index");
        end=0;const auto c=std::stoul(category,&end);if(end!=category.size()||c>=15)throw std::runtime_error("Invalid category");
        s->entries.push_back({bank,name,unsigned(n),unsigned(c)});if(std::find(s->banks.begin(),s->banks.end(),bank)==s->banks.end())s->banks.push_back(bank);
    }
    if(s->entries.empty())throw std::runtime_error("Library catalog is empty");s->favorites.resize(s->entries.size());
    std::ifstream saved(std::filesystem::path(HYPERION_USER)/"favorites.tsv");
    while(std::getline(saved,line)){const auto tab=line.find('\t');if(tab==std::string::npos)continue;
        for(std::size_t i=0;i<s->entries.size();++i)if(s->entries[i].bank==line.substr(0,tab)&&std::to_string(s->entries[i].record)==line.substr(tab+1))s->favorites[i]=1;}
}
void run(Instance*s){
    try{s->library=libraryRoot();int initial=0;{std::lock_guard<std::mutex> lock(s->ui);loadCatalog(s);
        for(int i=0;i<int(s->entries.size());++i)if((!s->restoreBank.empty()&&s->entries[i].bank==s->restoreBank&&s->entries[i].record==s->restoreRecord)||(s->restoreBank.empty()&&s->entries[i].name=="Solo Trumpet")){initial=i;break;}
        const auto it=std::find(s->banks.begin(),s->banks.end(),s->entries[std::size_t(initial)].bank);s->bank=int(it-s->banks.begin());
        s->category=int(s->entries[std::size_t(initial)].category);filter(s);auto v=std::find(s->view.begin(),s->view.end(),initial);if(v!=s->view.end())s->cursor=int(v-s->view.begin());
        s->requested=initial;s->catalogReady=true;}
        unsigned handled=0,shownRevision=0;int oldMeter[2]{-1,-1},oldAudible=-2;auto refresh=std::chrono::steady_clock::now();
        while(!s->stop){
            for(auto& slot:s->slots){int free=0;if(slot.state.compare_exchange_strong(free,1)){slot.patch.reset();slot.state=0;}}
            if(s->favoriteDirty.exchange(false)){std::string data;{std::lock_guard<std::mutex> lock(s->ui);for(std::size_t i=0;i<s->entries.size();++i)if(s->favorites[i])data+=s->entries[i].bank+'\t'+std::to_string(s->entries[i].record)+'\n';}
                std::filesystem::create_directories(HYPERION_USER);const auto dest=std::filesystem::path(HYPERION_USER)/"favorites.tsv",temp=std::filesystem::path(HYPERION_USER)/"favorites.tmp";
                {std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<data;if(!f)throw std::runtime_error("Could not save favorites");}std::filesystem::rename(temp,dest);}
            const auto revision=s->wanted.load();
            if(revision!=handled){int building=-1;for(int i=0;i<3;++i){int free=0;if(s->slots[i].state.compare_exchange_strong(free,1)){building=i;break;}}
                if(building>=0){auto& slot=s->slots[building];try{const int choice=s->requested;Entry entry;
                    {std::lock_guard<std::mutex> lock(s->ui);if(choice<0||choice>=int(s->entries.size()))throw std::runtime_error("Preset unavailable");entry=s->entries[std::size_t(choice)];}
                    BankLoader loader(s->library);slot.patch=loader.load(s->library/"Patches"/entry.bank,entry.record);
                    slot.rate=s->rate;slot.choice=choice;slot.revision=revision;if(!slot.engine.prepare(*slot.patch,slot.rate))throw std::runtime_error(slot.engine.error());
                    slot.factory=hyperionfx::capture(*slot.patch);slot.initial=slot.factory;
                    {std::lock_guard<std::mutex> lock(s->ui);const auto* saved=s->restoreRevision==revision?&s->restoreRack:(choice==s->audible&&s->rackRevision==s->playingRevision?&s->rack:nullptr);
                        if(saved&&saved->count==slot.initial.count){bool match=true;for(unsigned j=0;j<saved->count;++j)if(saved->values[j].target!=slot.initial.values[j].target||saved->values[j].id!=slot.initial.values[j].id)match=false;if(match)slot.initial=*saved;else s->message="FX recall layout mismatch";}}
                    for(unsigned j=0;j<slot.initial.count;++j){const auto& v=slot.initial.values[j];for(unsigned k=0;k<12;++k)slot.engine.setEffectControl(v.target,k,v.p[k],true);slot.engine.setEffectControl(v.target,12,v.level,true);slot.engine.setEffectControl(v.target,13,float(v.enabled),true);}
                    if(revision==s->wanted&&!s->stop){slot.state=2;const int replaced=s->pending.exchange(building);if(replaced>=0)s->slots[replaced].state=0;}
                    else slot.state=0;
                }catch(const std::exception& e){slot.state=0;std::lock_guard<std::mutex> lock(s->ui);s->message=std::string("Load failed: ")+e.what();}
                handled=revision;notify(s);}
            }
            const auto current=s->playingRevision.load();if(current!=shownRevision){for(const auto& slot:s->slots)if(slot.state==3&&slot.revision==current){std::lock_guard<std::mutex> lock(s->ui);s->rack=slot.initial;s->factory=slot.factory;s->rackInsertCount=0;for(const auto& node:slot.patch->nodes)if(node.effect)++s->rackInsertCount;s->rackRevision=current;s->rackSelected=s->rackPage=s->controlPage=0;s->rackEditing=false;shownRevision=current;notify(s);break;}}
            const auto tick=std::chrono::steady_clock::now();if(tick-refresh>=std::chrono::milliseconds(100)){bool changed=false;
                for(int c=0;c<2;++c){const int v=int(s->meter[c]*12);if(v!=oldMeter[c]){oldMeter[c]=v;changed=true;}}
                const int a=s->audible;if(a!=oldAudible){oldAudible=a;changed=true;if(a>=0&&a==s->requested){std::lock_guard<std::mutex> lock(s->ui);s->message="READY - "+s->entries[std::size_t(a)].name;}}
                changed=s->uiDirty.exchange(false)||changed;if(changed&&s->host)s->host(&s->effect,42,0,0,nullptr,0);refresh=tick;}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }catch(const std::exception& e){{std::lock_guard<std::mutex> lock(s->ui);s->message=e.what();}if(s->host)s->host(&s->effect,42,0,0,nullptr,0);}
}
bool readOnly(int i){return i==14||i==15||i==16||i==17||i==18||i==19||i==20||i==21||i==22||i==23||i==24||i==25||i==26||i==27||i==28||(i>=64&&i<91)||(i>=96&&i<116)||(i>=198&&i<=211)||(i>=220&&i<=232)||(i>=236&&i<=241)||(i>=256&&i<=271);}
float get(AEffect*e,int32_t i){auto*s=static_cast<Instance*>(e->object);if(i<0||i>=Count)return 0;if(i<3){std::lock_guard<std::mutex> lock(s->ui);
    if(s->panel==1)return i==0?s->gain/2:i==1?s->pan.load():s->fx?1.f:0.f;
    if(s->panel==2){auto c=selectedControl(s,i);return c.raw>=0?s->rack.values[s->rackSelected].p[c.raw]:0.f;}
    ++s->encoderReads[i];return .5f;}if(i==5)return s->gain/2;if(i==6)return s->fx?1.f:0.f;if(i==7)return s->pan;
    if(i==14)return s->audible>=0&&s->requested==s->audible?1.f:0.f;if(i==18||i==19)return s->meter[i-18];
    std::lock_guard<std::mutex> lock(s->ui);if(i==23)return s->menu?1.f:0.f;if(i==24)return s->panel/3.f;
    if(i==89)return s->menu/4.f;if(i==90){const int selected=s->view.empty()?-1:s->view[std::size_t(s->cursor)];return selected>=0&&s->favorites[std::size_t(selected)]?1.f:0.f;}
    if(i==88)return s->clip?1.f:0.f;
    if(i==229)return s->rackEditing?1.f:0.f;
    if(i==175)return s->rackSelected<int(s->rack.count)?float(s->rack.values[s->rackSelected].enabled):0.f;
    if(i==178)return s->rackSelected<int(s->rack.count)?s->rack.values[s->rackSelected].level:.5f;
    if(i>=192&&i<198){const auto c=selectedControl(s,i-192);return c.raw>=0?s->rack.values[s->rackSelected].p[c.raw]:0.f;}
    if(i>=236&&i<242)return selectedControl(s,i-236).raw>=0?1.f:0.f;
    if(i>=220&&i<228){const unsigned at=unsigned(s->rackPage*8+i-220);return at<s->rack.count?float(s->rack.values[at].enabled):0.f;}
    if(i>=64&&i<80){const int at=s->page*pageSize(s)+i-64;return at>=menuCount(s)?0.f:.5f;}
    if(i>=80&&i<86){const int at=quickCategory(s,i-80);return at<0?0.f:at==s->category?1.f:.5f;}
    return s->values[i];}
void set(AEffect*e,int32_t i,float value){auto*s=static_cast<Instance*>(e->object);if(i<0||i>=Count||readOnly(i)||!std::isfinite(value))return;value=std::clamp(value,0.f,1.f);
    if(i==5){s->gain=value*2;return;}if(i==6){s->fx=value>=.5f;return;}if(i==7){s->pan=value;return;}if(i==8){s->panic=true;s->clip=false;return;}
    std::lock_guard<std::mutex> lock(s->ui);if(!s->catalogReady)return;
    if((i>=192&&i<198)||i==175||i==178){if(s->rackSelected<int(s->rack.count)){if(i==175)editFx(s,unsigned(s->rackSelected),13,value);else if(i==178)editFx(s,unsigned(s->rackSelected),12,value);else{auto c=selectedControl(s,i-192);if(c.raw>=0)editFx(s,unsigned(s->rackSelected),unsigned(c.raw),value);}}notify(s);return;}
    if(i<3){
        if(s->panel==1){if(i==0)s->gain=value*2;else if(i==1)s->pan=value;else s->fx=value>=.5f;notify(s);return;}
        if(s->panel==2){auto c=selectedControl(s,i);if(c.raw>=0)editFx(s,unsigned(s->rackSelected),unsigned(c.raw),value);notify(s);return;}
        const auto now=std::chrono::steady_clock::now();const auto reads=s->encoderReads[i].load();
        const bool rebased=reads!=s->encoderSeen[i]||now-s->encoderTime[i]>std::chrono::milliseconds(300);
        float delta=rebased?value-.5f:value-s->encoderLast[i];if((value-.5f)*(s->encoderLast[i]-.5f)<0||std::abs(delta)<1e-6f)delta=value-.5f;
        s->encoderLast[i]=value;s->encoderSeen[i]=reads;s->encoderTime[i]=now;if(std::abs(delta)<1e-6f)return;
        const int steps=(delta<0?-1:1)*std::clamp(int(std::lround(std::abs(delta)/.01f)),1,4);
        if(i==0){s->category=std::clamp(s->category+steps,0,14);s->categoryPage=s->category?(s->category-1)/6:2;s->cursor=0;filter(s);}
        else if(i==1)advance(s,steps,false);else{s->bank=std::clamp(s->bank+steps,0,int(s->banks.size())-1);s->category=0;s->categoryPage=0;s->cursor=0;filter(s);}
    }else{const auto now=std::chrono::steady_clock::now();if(s->values[i]==value||now-s->taps[i]<std::chrono::milliseconds(120))return;s->values[i]=value;s->taps[i]=now;
        if(i==170)s->rackEditing=false;
        else if(i==171||i==172)s->rackPage=std::clamp(s->rackPage+(i==171?-1:1),0,std::max(0,(int(s->rack.count)-1)/8));
        else if(i==173||i==174){const auto n=s->rackSelected<int(s->rack.count)?hyperionfx::count(s->rack.values[s->rackSelected].id):0;s->controlPage=std::clamp(s->controlPage+(i==173?-1:1),0,std::max(0,(int(n)-1)/6));}
        else if(i==176)editFx(s,unsigned(s->rackSelected),255,0);
        else if(i==179){for(unsigned j=0;j<s->rack.count;++j)editFx(s,j,255,0);}
        else if(i>=212&&i<220){const int at=s->rackPage*8+i-212;if(at<int(s->rack.count)){s->rackSelected=at;s->controlPage=0;s->rackEditing=true;}}
        else if(i>=242&&i<250){const unsigned at=unsigned(s->rackPage*8+i-242);if(at<s->rack.count)editFx(s,at,13,s->rack.values[at].enabled?0.f:1.f);}
        else if(i==3||i==4)advance(s,i==3?-1:1,true);
        else if(i==9){s->menu=1;s->page=s->bank/16;}
        else if(i==10){s->menu=4;s->page=0;}
        else if(i==11){s->menu=3;s->page=s->cursor/16;}
        else if(i==12){s->favoritesOnly=!s->favoritesOnly;s->cursor=0;filter(s);s->menu=3;s->page=0;}
        else if(i==13||i==29){const int a=i==29?s->audible.load():(s->view.empty()?-1:s->view[std::size_t(s->cursor)]);if(a>=0){s->favorites[std::size_t(a)]^=1;s->favoriteDirty=true;}}
        else if(i==30||i==31)s->page=std::clamp(s->page+(i==30?-1:1),0,std::max(0,(menuCount(s)-1)/pageSize(s)));
        else if(i==32){if(!s->view.empty())choose(s,s->view[std::size_t(s->cursor)]);s->menu=0;}
        else if(i==33)s->menu=0;
        else if(i>=34&&i<=37){s->panel=i-34;s->menu=0;}
        else if(i==38||i==39){s->categoryPage=std::clamp(s->categoryPage+(i==38?-1:1),0,2);}
        else if(i>=40&&i<56){const int at=s->page*pageSize(s)+i-40;if(at<menuCount(s)){
            if(s->menu==1){s->bank=at;s->category=0;s->categoryPage=0;s->cursor=0;filter(s);}
            else if(s->menu==2){s->category=at;s->cursor=0;filter(s);}else{s->cursor=at;choose(s,s->view[std::size_t(at)]);}s->menu=0;}}
        else if(i>=56&&i<62){const int c=quickCategory(s,i-56);if(c>=0){s->category=c;s->cursor=0;filter(s);}}
        else if(i==62)advance(s,-1,false);else if(i==63)advance(s,1,false);
        else if(i>=128&&i<154){if(s->query.size()<32)s->query+=char('a'+i-128);s->cursor=s->page=0;filter(s);}
        else if(i==154){if(s->query.size()<32)s->query+=' ';s->cursor=s->page=0;filter(s);}
        else if(i==155){if(!s->query.empty())s->query.pop_back();s->cursor=s->page=0;filter(s);}
        else if(i==156){s->query.clear();s->cursor=s->page=0;filter(s);}
        else if(i>=160&&i<170){if(s->query.size()<32)s->query+=char('0'+i-160);s->cursor=s->page=0;filter(s);}
    }
    notify(s);
}
void display(Instance*s,int i,char*out){std::string text;std::lock_guard<std::mutex> lock(s->ui);const int a=s->audible;
    const Entry* playing=a>=0&&a<int(s->entries.size())?&s->entries[std::size_t(a)]:nullptr;
    const int selected=s->view.empty()?-1:s->view[std::size_t(s->cursor)];
    if(i==5||(s->panel==1&&i==0)){char b[32];std::snprintf(b,sizeof b,"%.1f dB",20*std::log10(std::max(.00001f,s->gain.load())));text=b;}
    else if(i==6||(s->panel==1&&i==2))text=s->fx?"PRESET FX ON":"PRESET FX OFF";else if(i==7||(s->panel==1&&i==1)){char b[32];std::snprintf(b,sizeof b,"%d",int((s->pan-.5f)*200));text=b;}
    else if(i==14)text=playing?(s->requested==a?"READY":"LOADING"):s->message;
    else if(i==15||i==113)text=selected>=0?nameLine(s->entries[std::size_t(selected)].name,i==113):i==15?"No sounds":"";
    else if(i==16)text=s->bank?bankName(s->banks[std::size_t(s->bank)]):"All Banks";
    else if(i==17)text=selected==a?"PLAYING":"TAP TO PLAY";
    else if(i==20||i==21||i==114||i==115){if(!s->view.empty()){const int size=int(s->view.size());const int c=(s->cursor+size+((i==20||i==114)?-1:1))%size;text=cardLine(s->entries[std::size_t(s->view[std::size_t(c)])].name,i>=114);}}
    else if(i==22)text=std::to_string(s->view.empty()?0:s->cursor+1)+" / "+std::to_string(s->view.size());
    else if(i==25)text=s->menu==1?"BANKS":s->menu==2?"CATEGORIES":s->favoritesOnly?"FAVORITES":"ALL SOUNDS";
    else if(i==26)text=std::to_string(s->page+1)+" / "+std::to_string(std::max(1,(menuCount(s)+pageSize(s)-1)/pageSize(s)));
    else if(i==27)text=playing?playing->name:"No sound loaded";else if(i==28)text=s->message;
    else if(i>=40&&i<56)text=nameLine(menuName(s,s->page*pageSize(s)+i-40),false);
    else if(i>=96&&i<112)text=nameLine(menuName(s,s->page*pageSize(s)+i-96),true);
    else if(i==86)text=s->query.empty()?"Type to search":s->query;
    else if(i>=80&&i<86){const int c=quickCategory(s,i-80);if(c>=0)text=categories[c];}
    else if(i==228)text=s->rackSelected<int(s->rack.count)?hyperionfx::name(s->rack.values[s->rackSelected].id):"No preset FX";
    else if(i==175)text=s->rackSelected<int(s->rack.count)&&s->rack.values[s->rackSelected].enabled?"ON":"BYPASSED";
    else if(i==178){char b[32];const auto level=s->rackSelected<int(s->rack.count)?s->rack.values[s->rackSelected].level:.5f;std::snprintf(b,sizeof b,"%.1f dB",20*std::log10(std::max(.00001f,level*2)));text=b;}
    else if((i>=192&&i<198)||(s->panel==2&&i<3)){auto c=selectedControl(s,i<3?i:i-192);if(c.raw>=0)text=hyperionfx::format(s->rack.values[s->rackSelected],c.raw,s->displayBpm);}
    else if(i>=198&&i<204)text=selectedControl(s,i-198).label;
    else if(i>=204&&i<212){const unsigned at=unsigned(s->rackPage*8+i-204);text=at<s->rack.count?hyperionfx::name(s->rack.values[at].id):"Empty";}
    else if(i>=256&&i<264){const unsigned at=unsigned(s->rackPage*8+i-256);text=at<s->rack.count?(s->rack.values[at].target>=s->rackInsertCount?"RET "+std::to_string(s->rack.values[at].target-s->rackInsertCount+1):"INS "+std::to_string(s->rack.values[at].target+1)):"";}
    else if(i>=264&&i<272){const unsigned at=unsigned(s->rackPage*8+i-264);text=at<s->rack.count?(s->rack.values[at].enabled?"ON":"BYPASSED"):"";}
    else if(i==230)text=std::to_string(s->rackPage+1)+" / "+std::to_string(std::max(1,(int(s->rack.count)+7)/8));
    else if(i==231)text=std::to_string(s->controlPage+1)+" / "+std::to_string(s->rackSelected<int(s->rack.count)?std::max(1,(int(hyperionfx::count(s->rack.values[s->rackSelected].id))+5)/6):1);
    else if(i==232)text=s->rack.count?"Tap an effect to edit":"No FX in this preset";
    else if(i==0)text=categories[s->category];else if(i==1)text=selected>=0?s->entries[std::size_t(selected)].name:"No sounds";else if(i==2)text=s->bank?bankName(s->banks[std::size_t(s->bank)]):"All Banks";
    // Native host parameter displays provide 24 bytes, including termination.
    const auto n=std::min<std::size_t>(23,text.size());std::memcpy(out,text.data(),n);out[n]=0;
}
void segment(Instance*s,float** out,int at,int frames){while(frames>0){const auto count=std::min(frames,256);bool ok=false,old=false;
    if(s->active>=0){auto& slot=s->slots[s->active];slot.engine.bypassFx(!s->fx);slot.engine.tempo(s->bpm);if(slot.rate==s->rate)ok=slot.engine.process(s->scratch,std::size_t(count));}
    if(s->retiring>=0)old=s->slots[s->retiring].engine.process(s->oldScratch,std::size_t(count));
    const float pan=s->pan,gain=s->gain;for(int j=0;j<count;++j){const double fade=s->fade>0?double(--s->fade)/s->fadeTotal:0;s->smoothGain+=(gain-s->smoothGain)*.002f;
        for(int c=0;c<2;++c){double x=ok?s->scratch[2*j+c]:0;if(old)x+=s->oldScratch[2*j+c]*fade;out[c][at+j]=float(x*s->smoothGain*(c?2*pan:2*(1-pan)));}}
    if(s->retiring>=0&&(!s->fade||!old)){s->slots[s->retiring].state=0;s->retiring=-1;}at+=count;frames-=count;
}}
void process(AEffect*e,float**,float**out,int32_t frames){AudioFP fp;auto*s=static_cast<Instance*>(e->object);if(frames<=0||!out||!out[0]||!out[1])return;
    if(s->retiring<0){const int next=s->pending.exchange(-1);if(next>=0){auto& slot=s->slots[next];if(slot.revision==s->wanted&&slot.rate==s->rate){
        s->retiring=s->active;s->active=next;slot.state=3;s->audible=slot.choice;s->playingRevision=slot.revision;s->fade=s->fadeTotal=std::max(32,int(s->rate*.005f));
        if(s->retiring>=0)for(unsigned c=0;c<16;++c)s->slots[s->retiring].engine.send(0xb0|c,120,0);
    }else slot.state=0;}}
    if(s->panic.exchange(false)){if(s->active>=0)s->slots[s->active].engine.reset();if(s->retiring>=0)s->slots[s->retiring].engine.reset();}
    s->bpm=sgkzen::readHostTempo(e,s->host,s->bpm);s->displayBpm=s->bpm;
    {auto read=s->commandRead.load();const auto write=s->commandWrite.load();unsigned processed=0;while(read!=write&&processed++<32){const auto c=s->commands[read%256];if(s->active>=0&&c.revision==s->playingRevision)s->slots[s->active].engine.setEffectControl(c.target,c.param,c.value);++read;}s->commandRead.store(read);}
    for(unsigned i=1;i<s->midiCount;++i){const auto value=s->midi[i];unsigned j=i;while(j&&s->midi[j-1].offset>value.offset){s->midi[j]=s->midi[j-1];--j;}s->midi[j]=value;}
    int at=0;for(unsigned i=0;i<s->midiCount;++i){const auto& m=s->midi[i];const int offset=std::clamp(m.offset,at,frames);segment(s,out,at,offset-at);at=offset;
        if(s->active>=0)s->slots[s->active].engine.send(m.status,m.key,m.value);}
    segment(s,out,at,frames-at);s->midiCount=0;
    for(int c=0;c<2;++c){float peak=0;for(int i=0;i<frames;++i)peak=std::max(peak,std::abs(out[c][i]));if(peak>1)s->clip=true;s->meter[c]=std::max(std::clamp((20*std::log10(std::max(peak,1e-8f))+48)/48,0.f,1.f),s->meter[c]*float(std::exp(-frames/(s->rate*.18f))));}
}
intptr_t dispatch(AEffect*e,int32_t op,int32_t i,intptr_t value,void*ptr,float f){auto*s=static_cast<Instance*>(e->object);switch(op){
    case 0:return 1;case 1:delete s;return 1;
    case 6:if(ptr)static_cast<char*>(ptr)[0]=0;return 1;case 7:if(ptr)display(s,i,static_cast<char*>(ptr));return 1;
    case 8:if(ptr){std::lock_guard<std::mutex> lock(s->ui);const char* name=s->panel==2&&i<3?selectedControl(s,i).label:s->panel==1&&i==0?"Output":s->panel==1&&i==1?"Pan":s->panel==1&&i==2?"Preset FX":i==0?"Category":i==1?"Preset Browse":i==2?"Bank Browse":i==5?"Output":i==6?"Preset FX":i==7?"Pan":"Hyperion";std::snprintf(static_cast<char*>(ptr),24,"%s",name);}return 1;
    case 10:if(std::isfinite(f)&&f>=8000&&f<=192000&&f!=s->rate){s->rate=f;s->wanted.fetch_add(1);}return 1;
    case 12:if(value==0){s->midiCount=0;s->panic=true;}return 1;
    case 23:if(ptr){std::lock_guard<std::mutex> lock(s->ui);s->state=SavedState{};auto& state=s->state.base;state.magic=0x48595002;state.bytes=sizeof(SavedState);state.gain=s->gain;state.pan=s->pan;state.fx=s->fx?1:0;const int a=s->audible;
        if(a>=0&&a<int(s->entries.size())){const auto& entry=s->entries[std::size_t(a)];std::snprintf(state.bank,sizeof state.bank,"%s",entry.bank.c_str());state.record=entry.record;}
        if(s->rackRevision==s->playingRevision)s->state.rack=s->rack;*static_cast<void**>(ptr)=&s->state;return sizeof(SavedState);}return 0;
    case 24:if(ptr&&(value==sizeof(State)||value==sizeof(SavedState))){SavedState saved;std::memcpy(&saved,ptr,std::size_t(value));const auto& state=saved.base;
        if((value==sizeof(State)?state.magic!=0x48595001:state.magic!=0x48595002)||state.bytes!=unsigned(value)||!std::memchr(state.bank,0,sizeof state.bank)||!std::isfinite(state.gain)||state.gain<0||state.gain>2||!std::isfinite(state.pan)||state.pan<0||state.pan>1||state.fx>1||saved.rack.count>hyperionfx::Max)return 0;
        for(unsigned j=0;j<saved.rack.count;++j){const auto& v=saved.rack.values[j];if(v.target>=66||!supportedFx(v.id)||v.enabled>1||!std::isfinite(v.level)||v.level<0||v.level>1)return 0;for(float x:v.p)if(!std::isfinite(x)||x<0||x>1)return 0;for(unsigned k=0;k<j;++k)if(saved.rack.values[k].target==v.target)return 0;}
        std::lock_guard<std::mutex> lock(s->ui);int found=-1;if(s->catalogReady){for(int at=0;at<int(s->entries.size());++at)if(s->entries[std::size_t(at)].bank==state.bank&&s->entries[std::size_t(at)].record==state.record){found=at;break;}if(found<0)return 0;}
        s->gain=state.gain;s->pan=state.pan;s->fx=state.fx!=0;s->restoreBank=state.bank;s->restoreRecord=state.record;s->restoreRack=saved.rack;
        if(found>=0){const auto& entry=s->entries[std::size_t(found)];s->bank=int(std::find(s->banks.begin(),s->banks.end(),entry.bank)-s->banks.begin());s->category=int(entry.category);s->categoryPage=s->category?(s->category-1)/6:2;
            s->query.clear();s->favoritesOnly=false;s->menu=s->panel=s->page=s->cursor=0;filter(s);const auto cursor=std::find(s->view.begin(),s->view.end(),found);if(cursor!=s->view.end())s->cursor=int(cursor-s->view.begin());choose(s,found,true);s->restoreRevision=s->wanted;notify(s);return 1;}
        s->restoreRevision=s->wanted.fetch_add(1)+1;return 1;}return 0;
    case 25:if(ptr){auto* events=static_cast<MidiEvents*>(ptr);if(events->count<0||events->count>256||unsigned(events->count)>256-s->midiCount){s->panic=true;s->midiCount=0;return 1;}
        auto** items=reinterpret_cast<MidiEvent**>(static_cast<char*>(ptr)+offsetof(MidiEvents,events));for(int k=0;k<events->count;++k)if(items[k]&&items[k]->type==1&&items[k]->size>=24){const auto* m=items[k];s->midi[s->midiCount++]={m->data[0],m->data[1],m->data[2],std::max(0,m->offset)};}}return 1;
    case 26:return i>=0&&i<Count&&!readOnly(i);case 35:return 2;
    case 45:case 48:if(ptr)std::strcpy(static_cast<char*>(ptr),"SGK Hyperion");return 1;case 47:if(ptr)std::strcpy(static_cast<char*>(ptr),"SGK");return 1;case 49:return 102;
    case 51:return ptr&&(!std::strcmp(static_cast<char*>(ptr),"receiveVstMidiEvent")||!std::strcmp(static_cast<char*>(ptr),"receiveVstEvents")||!std::strcmp(static_cast<char*>(ptr),"receiveVstTimeInfo"));case 58:return 2400;default:return 0;
}}
}
extern "C" EXPORT AEffect* VSTPluginMain(Host host){try{
    auto*s=new Instance;s->host=host;auto&e=s->effect;e.magic=0x56737450;e.dispatcher=dispatch;e.setParameter=set;e.getParameter=get;e.numPrograms=1;e.numParams=Count;e.numOutputs=2;e.flags=(1<<4)|(1<<5)|(1<<8);e.object=s;e.uniqueID=UID;e.version=102;e.processReplacing=process;
    try{s->worker=std::thread(run,s);}catch(...){delete s;return nullptr;}return&e;
}catch(...){return nullptr;}}
