#include "alpha_save.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "openu5/frontend_settings.h"
#include "sd_diagnostic_logger.h"

namespace tdeck {
namespace {
constexpr char kTag[]="AlphaSave";
constexpr uint32_t kPsram=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
constexpr uint32_t kInternal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
constexpr size_t kDmaHeadroomBytes=8192;
void *g_dma_headroom=nullptr;
void heap_diagnostic(const char *operation,const char *state,size_t requested=0){
    ESP_LOGI(kTag,"SD_HEAP operation=%s state=%s free_internal=%zu free_dma=%zu largest_dma=%zu largest_internal=%zu requested_dma=%zu",
             operation,state,heap_caps_get_free_size(kInternal),heap_caps_get_free_size(MALLOC_CAP_DMA),
             heap_caps_get_largest_free_block(MALLOC_CAP_DMA),heap_caps_get_largest_free_block(kInternal),requested);
}
struct SdHeadroomGuard{
    const char *state;
    bool locked=false;
    explicit SdHeadroomGuard(const char*s):state(s){
        locked=sdlog::begin_storage_transaction();
        heap_diagnostic("release-reserved-dma",state,kDmaHeadroomBytes);
        if(g_dma_headroom){heap_caps_free(g_dma_headroom);g_dma_headroom=nullptr;}
        heap_diagnostic("sd-window-open",state,512);
    }
    ~SdHeadroomGuard(){
        g_dma_headroom=heap_caps_malloc(kDmaHeadroomBytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
        heap_diagnostic(g_dma_headroom?"dma-reserve-restored":"dma-reserve-restore-failed",state,kDmaHeadroomBytes);
        if(locked)sdlog::end_storage_transaction();
    }
};
struct Commit { uint32_t magic=0x31533555,version=1;uint64_t sequence=0;uint32_t gam=0,ool=0,json=0; };
std::string path(int slot,const char *ext,bool temp=false){char out[96];std::snprintf(out,sizeof(out),"%s/alpha1-g%d.%s%s",AlphaSaveService::kDirectory,slot,ext,temp?".tmp":"");return out;}
bool write_file(const std::string&p,const void*data,size_t n){FILE*f=std::fopen(p.c_str(),"wb");if(!f)return false;bool ok=std::fwrite(data,1,n,f)==n&&std::fflush(f)==0&&fsync(fileno(f))==0;const bool closed=std::fclose(f)==0;return ok&&closed;}
bool read_file(const std::string&p,std::vector<uint8_t>&out){FILE*f=std::fopen(p.c_str(),"rb");if(!f)return false;if(std::fseek(f,0,SEEK_END)||std::ftell(f)<0){std::fclose(f);return false;}auto n=size_t(std::ftell(f));out.resize(n);std::rewind(f);const bool read=std::fread(out.data(),1,n,f)==n;const bool closed=std::fclose(f)==0;return read&&closed;}
bool validate_temp(int slot,const char*ext,size_t size,uint32_t crc){std::vector<uint8_t>b;return read_file(path(slot,ext,true),b)&&b.size()==size&&openu5::save::save_crc32(b.data(),b.size())==crc;}
bool read_commit(int slot,Commit&c){std::vector<uint8_t>b;if(!read_file(path(slot,"commit"),b)||b.size()!=sizeof(c))return false;std::memcpy(&c,b.data(),sizeof(c));return c.magic==0x31533555&&c.version==1;}
bool move_temp(int slot,const char*ext){const auto from=path(slot,ext,true),to=path(slot,ext);unlink(to.c_str());return rename(from.c_str(),to.c_str())==0;}
struct Candidate{Commit commit{};std::vector<uint8_t>gam,ool,json;std::string side;openu5::save::Generation generation{};};
}

// One main-task-owned workspace holds every large persistence temporary. The
// save adapter is intentionally non-reentrant, and the application calls it
// only from app_main, so sharing this PSRAM workspace is both bounded and safe.
struct AlphaSaveScratch {
    Candidate candidates[2]{};
    Candidate verified{};
    openu5::GameState game{};
    openu5::TurnState turn{};
    openu5::CommandState commands{};
    openu5::OutdoorServices outdoor{};
    openu5::WorldTerrain terrain{};
    openu5::NpcActors actors{};
    openu5::save::Json document{};
    openu5::save::Gam gam{};
    openu5::save::Ool ool{};
    openu5::save::Json side{};
    std::string json{};
    Commit commits[2]{};
};

namespace {
bool candidate(int slot,Candidate&v,AlphaSaveScratch&scratch){
    v=Candidate{};
    if(!read_commit(slot,v.commit)||!read_file(path(slot,"gam"),v.gam)||!read_file(path(slot,"ool"),v.ool)||!read_file(path(slot,"json"),v.json))return false;
    v.side.assign(reinterpret_cast<const char*>(v.json.data()),v.json.size());auto &g=v.generation;g.sequence=v.commit.sequence;g.committed=true;g.identity_matches=true;g.gam=v.gam.data();g.gam_size=v.gam.size();g.ool=v.ool.data();g.ool_size=v.ool.size();g.sidecar=&v.side;g.requires_ool=g.requires_sidecar=true;g.gam_crc=v.commit.gam;g.ool_crc=v.commit.ool;g.sidecar_crc=v.commit.json;if(!openu5::save::complete_generation(g))return false;
    scratch.game={};scratch.turn={};scratch.document={};scratch.commands={};scratch.outdoor={};scratch.terrain={};scratch.actors={};
    openu5::save::SidecarSource source;
    if(openu5::save::load_native_state(v.gam.data(),v.gam.size(),&v.side,scratch.game,scratch.turn,scratch.document,source,true)!=openu5::save::Error::None)return false;
    return openu5::save::restore_gameplay(scratch.document,scratch.commands,scratch.outdoor)==openu5::save::Error::None&&
           openu5::save::restore_terrain(scratch.document,scratch.terrain)==openu5::save::Error::None&&
           openu5::save::restore_npc_walk(scratch.document,scratch.game.position.map.location,true,scratch.actors)==openu5::save::Error::None;
}
bool restore_candidate(Candidate&pick,openu5::CommandContext&c,openu5::OutdoorServices&o,openu5::WorldTerrain&t,openu5::NpcActors&a,openu5::save::Json&retained,AlphaSaveScratch&scratch){
    // Validate and restore transactionally. A newer generation with a valid
    // commit/CRC but incompatible semantic payload must not partially replace
    // live state or prevent recovery from the older generation.
    scratch.game=c.game;scratch.turn=c.turn;scratch.commands=c.commands;scratch.outdoor=o;scratch.terrain=t;scratch.actors=a;scratch.document={};
    openu5::save::SidecarSource source;
    auto err=openu5::save::load_native_state(pick.gam.data(),pick.gam.size(),&pick.side,scratch.game,scratch.turn,scratch.document,source,true);
    if(err==openu5::save::Error::None)err=openu5::save::restore_gameplay(scratch.document,scratch.commands,scratch.outdoor);
    if(err==openu5::save::Error::None)err=openu5::save::restore_terrain(scratch.document,scratch.terrain);
    if(err==openu5::save::Error::None)err=openu5::save::restore_npc_walk(scratch.document,scratch.game.position.map.location,true,scratch.actors);
    if(err!=openu5::save::Error::None)return false;
    c.game=std::move(scratch.game);c.turn=std::move(scratch.turn);c.commands=std::move(scratch.commands);
    o=std::move(scratch.outdoor);t=std::move(scratch.terrain);a=std::move(scratch.actors);retained=std::move(scratch.document);return true;
}
}

AlphaSaveScratch *AlphaSaveService::scratch(){
    if(scratch_)return scratch_;
    void *memory=heap_caps_calloc(1,sizeof(AlphaSaveScratch),kPsram);
    if(memory){scratch_=new(memory)AlphaSaveScratch;ESP_LOGI(kTag,"SAVE_SCRATCH psram_bytes=%zu",sizeof(AlphaSaveScratch));}
    else ESP_LOGE(kTag,"SAVE_SCRATCH allocation failed psram_bytes=%zu",sizeof(AlphaSaveScratch));
    return scratch_;
}

bool AlphaSaveService::reserve_dma_headroom(){
    if(!g_dma_headroom)g_dma_headroom=heap_caps_malloc(kDmaHeadroomBytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
    heap_diagnostic(g_dma_headroom?"dma-reserve-created":"dma-reserve-failed","startup",kDmaHeadroomBytes);
    return g_dma_headroom!=nullptr;
}

bool AlphaSaveService::save(openu5::CommandContext &c,openu5::OutdoorServices&o,
                            openu5::WorldTerrain&t,openu5::NpcActors&a,openu5::save::Json&retained,
                            const uint8_t*base,size_t base_size,const uint8_t*base_ool,size_t base_ool_size,uint32_t &ms,bool new_journey){
    const int64_t start=esp_timer_get_time();last_failure_[0]=0;
    SdHeadroomGuard sd_window(new_journey?"frontend-new-journey":"gameplay-save");
    auto *workspace=scratch();if(!workspace){std::snprintf(last_failure_,sizeof(last_failure_),"PSRAM save workspace allocation failed");ms=0;return false;}auto &s=*workspace;
    auto stage=[&](const char*s,bool ok,const char*p=""){const int e=ok?0:errno;const auto margin=uxTaskGetStackHighWaterMark(nullptr)*sizeof(StackType_t);ESP_LOGI(kTag,"NEWGAME_SAVE stage=%s result=%s esp_err=%s errno=%d path=%s stack_margin=%u",s,ok?"ok":"fail",esp_err_to_name(ok?ESP_OK:ESP_FAIL),e,p,unsigned(margin));if(!ok)std::snprintf(last_failure_,sizeof(last_failure_),"%s failed (errno %d)",s,e);return ok;};
    auto directory=[&](const char*p){errno=0;if(mkdir(p,0777)==0)return true;if(errno!=EEXIST)return false;struct stat st{};return stat(p,&st)==0&&S_ISDIR(st.st_mode);};
    if(!stage("create-root-directory",directory("/sd/ultima5"),"/sd/ultima5")||!stage("create-save-directory",directory(kDirectory),kDirectory)){ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}
    const char *storage_state=new_journey?"frontend-new-journey":"gameplay-save";
    constexpr char kFatMountPath[]="/sd";
    heap_diagnostic("free-space-before",storage_state,512);
    uint64_t total_bytes=0,free_bytes=0;errno=0;
    heap_diagnostic("free-space-call-enter",storage_state,512);
    const esp_err_t space_result=esp_vfs_fat_info(kFatMountPath,&total_bytes,&free_bytes);
    const int query_errno=errno;
    heap_diagnostic("free-space-call-exit",storage_state,512);
    heap_diagnostic("free-space-after",storage_state,512);
    ESP_LOGI(kTag,"SD_SPACE path=%s api=esp_vfs_fat_info/f_getfree return=%s(0x%lx) errno=%d total_bytes=%llu free_bytes=%llu total_sectors=%llu free_sectors=%llu sector_bytes=512",
             kFatMountPath,esp_err_to_name(space_result),(unsigned long)space_result,query_errno,
             (unsigned long long)total_bytes,(unsigned long long)free_bytes,
             (unsigned long long)(total_bytes/512U),(unsigned long long)(free_bytes/512U));
    const bool query_ok=space_result==ESP_OK;
    errno=query_errno;
    if(!stage("free-space-query",query_ok,kFatMountPath)){
        std::snprintf(last_failure_,sizeof(last_failure_),
            "SD free-space query I/O failed (errno %d); space not established",query_errno);
        ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;
    }
    const bool space_ok=free_bytes>32768U;
    if(!stage("free-space",space_ok,kFatMountPath)){std::snprintf(last_failure_,sizeof(last_failure_),"Insufficient SD free space");ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}
    openu5::save::capture_gameplay(c.commands,o,retained);openu5::save::capture_terrain(t,retained);
    openu5::save::capture_npc_walk(a,c.game.position.map.location,retained);
    if(c.quest_world)openu5::save::capture_world_objects(*c.quest_world,retained);
    if(c.dungeon_context)openu5::save::capture_dungeon(c.dungeon_context->state,retained);
    s.gam={};s.side={};s.json.clear();
    if(!stage("gam-encode",openu5::save::export_native_state(c.game,c.turn,retained,base,base_size,s.gam,s.side,true)==openu5::save::Error::None,"INIT.GAM")){ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}
    if(new_journey)openu5::save::preserve_new_journey_template_bytes(s.gam,base,base_size);
    s.ool=openu5::save::build_ool(retained,base_ool,base_ool_size);
    if(!stage("ool-encode",s.ool.size()==openu5::save::kOolSize,"INIT.OOL")||!stage("json-encode",openu5::save::encode_json(s.side,s.json)==openu5::save::JsonError::None,"sidecar")){ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}
    s.commits[0]={};s.commits[1]={};uint64_t newest=0;for(int i=0;i<2;++i)if(read_commit(i,s.commits[i]))newest=std::max(newest,s.commits[i].sequence);
    const int slot=int((newest+1)&1);Commit next;next.sequence=newest+1;
    next.gam=openu5::save::save_crc32(s.gam.data(),s.gam.size());next.ool=openu5::save::save_crc32(s.ool.data(),s.ool.size());
    next.json=openu5::save::save_crc32(reinterpret_cast<const uint8_t*>(s.json.data()),s.json.size());
    heap_diagnostic("write-gam-temp",new_journey?"frontend-new-journey":"gameplay-save",512);
    bool ok=stage("write-gam-temp",write_file(path(slot,"gam",true),s.gam.data(),s.gam.size()),path(slot,"gam",true).c_str());
    if(ok)heap_diagnostic("write-ool-temp",new_journey?"frontend-new-journey":"gameplay-save",512);
    if(ok)ok=stage("write-ool-temp",write_file(path(slot,"ool",true),s.ool.data(),s.ool.size()),path(slot,"ool",true).c_str());
    if(ok)heap_diagnostic("write-json-temp",new_journey?"frontend-new-journey":"gameplay-save",512);
    if(ok)ok=stage("write-json-temp",write_file(path(slot,"json",true),s.json.data(),s.json.size()),path(slot,"json",true).c_str());
    if(ok)heap_diagnostic("crc-readback",new_journey?"frontend-new-journey":"gameplay-save",512);
    if(ok)ok=stage("crc-readback",validate_temp(slot,"gam",s.gam.size(),next.gam)&&validate_temp(slot,"ool",s.ool.size(),next.ool)&&validate_temp(slot,"json",s.json.size(),next.json),kDirectory);
    if(ok)ok=stage("rename-gam",move_temp(slot,"gam"),path(slot,"gam").c_str());
    if(ok)ok=stage("rename-ool",move_temp(slot,"ool"),path(slot,"ool").c_str());
    if(ok)ok=stage("rename-json",move_temp(slot,"json"),path(slot,"json").c_str());
    if(ok)ok=stage("write-commit-temp",write_file(path(slot,"commit",true),&next,sizeof(next)),path(slot,"commit",true).c_str());
    if(ok)ok=stage("rename-commit",move_temp(slot,"commit"),path(slot,"commit").c_str());
    if(ok)ok=stage("semantic-validation",candidate(slot,s.verified,s),path(slot,"commit").c_str());
    if(ok)stage("generation-final",true,path(slot,"commit").c_str());
    if(!ok){ESP_LOGE(kTag,"generation %d save failed (errno=%d); previous generation retained",slot,errno);}
    ms=uint32_t((esp_timer_get_time()-start+999)/1000);ESP_LOGI(kTag,"save generation=%llu slot=%d time=%lu ms json=%zu",(unsigned long long)next.sequence,slot,(unsigned long)ms,s.json.size());return ok;
}

bool AlphaSaveService::load(openu5::CommandContext &c,openu5::OutdoorServices&o,
                            openu5::WorldTerrain&t,openu5::NpcActors&a,openu5::save::Json&retained,
                            uint32_t &ms){
    const int64_t start=esp_timer_get_time();SdHeadroomGuard sd_window("load-latest");auto *workspace=scratch();if(!workspace){ms=0;return false;}auto &s=*workspace;for(int i=0;i<2;++i)candidate(i,s.candidates[i],s);
    openu5::save::Generation list[2]={s.candidates[0].generation,s.candidates[1].generation};
    for(int pass=0;pass<2;++pass){const int selected=openu5::save::select_generation(list,2);if(selected<0)break;
        auto &pick=s.candidates[selected];if(restore_candidate(pick,c,o,t,a,retained,s)){ms=uint32_t((esp_timer_get_time()-start+999)/1000);ESP_LOGI(kTag,"load generation=%llu slot=%d time=%lu ms status=0",(unsigned long long)pick.commit.sequence,selected,(unsigned long)ms);return true;}
        ESP_LOGW(kTag,"generation=%llu slot=%d failed semantic restore; trying fallback",(unsigned long long)pick.commit.sequence,selected);list[selected]={};
    }
    ms=uint32_t((esp_timer_get_time()-start+999)/1000);ESP_LOGW(kTag,"no restorable save generation");return false;
}

bool AlphaSaveService::load_slot(int slot,openu5::CommandContext&c,openu5::OutdoorServices&o,openu5::WorldTerrain&t,openu5::NpcActors&a,openu5::save::Json&retained,uint32_t&ms){const int64_t start=esp_timer_get_time();SdHeadroomGuard sd_window("load-slot");auto *workspace=scratch();if(!workspace||slot<0||slot>1){ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}auto&s=*workspace;auto&pick=s.candidates[slot];if(!candidate(slot,pick,s)){ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}const bool ok=restore_candidate(pick,c,o,t,a,retained,s);ms=uint32_t((esp_timer_get_time()-start+999)/1000);return ok;}
void AlphaSaveService::inspect(openu5::FrontendSaveSlot(&slots)[2]){SdHeadroomGuard sd_window("save-inspect");auto*workspace=scratch();for(auto&slot:slots)slot={};if(!workspace)return;auto&s=*workspace;for(int i=0;i<2;++i){auto&v=s.candidates[i];if(!candidate(i,v,s)){Commit c{};slots[i].present=read_commit(i,c);continue;}slots[i].present=slots[i].valid=true;slots[i].sequence=v.commit.sequence;if(s.game.party.character_count)std::snprintf(slots[i].name,sizeof(slots[i].name),"%.9s",s.game.party.characters[0].name);}}

bool AlphaSettingsService::load(openu5::FrontendSettings&s)const{SdHeadroomGuard sd_window("settings-load");std::vector<uint8_t>b;if(!read_file(kPath,b))return false;return openu5::decode_settings(std::string(reinterpret_cast<const char*>(b.data()),b.size()),s);}
bool AlphaSettingsService::save(const openu5::FrontendSettings&s)const{SdHeadroomGuard sd_window("settings-save");mkdir("/sd/ultima5",0777);std::string text;if(!openu5::encode_settings(s,text))return false;const std::string tmp=std::string(kPath)+".tmp";if(!write_file(tmp,text.data(),text.size()))return false;unlink(kPath);return rename(tmp.c_str(),kPath)==0;}
} // namespace tdeck
