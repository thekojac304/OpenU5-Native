#include "alpha_save.h"
#include "alpha_save_generation.h"

#include <algorithm>
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
using Commit=AlphaSaveCommit;
// Alpha 4 A4-SAVE2: a generation's files. Logical slot 0 is the pre-SAVE2
// pair (alpha1-g0, alpha1-g1) unchanged, so an older card is Slot 1 byte for
// byte and older firmware still reads it; slots 1 and 2 are alpha1-s2-gN and
// alpha1-s3-gN. No name is a prefix of another slot's.
std::string path(int slot,int gen,const char *ext,bool temp=false){char out[96];
    if(slot==0)std::snprintf(out,sizeof(out),"%s/alpha1-g%d.%s%s",AlphaSaveService::kDirectory,gen,ext,temp?".tmp":"");
    else std::snprintf(out,sizeof(out),"%s/alpha1-s%d-g%d.%s%s",AlphaSaveService::kDirectory,slot+1,gen,ext,temp?".tmp":"");
    return out;}
bool write_file(const std::string&p,const void*data,size_t n){FILE*f=std::fopen(p.c_str(),"wb");if(!f)return false;bool ok=std::fwrite(data,1,n,f)==n&&std::fflush(f)==0&&fsync(fileno(f))==0;const bool closed=std::fclose(f)==0;return ok&&closed;}
bool read_file(const std::string&p,std::vector<uint8_t>&out){FILE*f=std::fopen(p.c_str(),"rb");if(!f)return false;if(std::fseek(f,0,SEEK_END)||std::ftell(f)<0){std::fclose(f);return false;}auto n=size_t(std::ftell(f));out.resize(n);std::rewind(f);const bool read=std::fread(out.data(),1,n,f)==n;const bool closed=std::fclose(f)==0;return read&&closed;}
bool validate_temp(int slot,int gen,const char*ext,size_t size,uint32_t crc){std::vector<uint8_t>b;return read_file(path(slot,gen,ext,true),b)&&b.size()==size&&openu5::save::save_crc32(b.data(),b.size())==crc;}
bool read_commit(int slot,int gen,Commit&c){std::vector<uint8_t>b;if(!read_file(path(slot,gen,"commit"),b)||b.size()!=sizeof(c))return false;std::memcpy(&c,b.data(),sizeof(c));return c.magic==0x31533555&&c.version==1;}
bool move_temp(int slot,int gen,const char*ext){const auto from=path(slot,gen,ext,true),to=path(slot,gen,ext);unlink(to.c_str());return rename(from.c_str(),to.c_str())==0;}
constexpr int kSlots=AlphaSaveService::kSlots;
using Candidate=AlphaSaveCandidate;
}

// One main-task-owned workspace holds every large persistence temporary. The
// save adapter is intentionally non-reentrant, and the application calls it
// only from app_main, so sharing this PSRAM workspace is both bounded and safe.
//
// Alpha 3 A3-04G (ALPHA3_AUDIO.md section 28): the workspace no longer keeps
// anything between calls. A verified generation stages a save document of
// ~190 KB of small allocations (internal RAM first); until A3-04G the last one
// stayed here after every inspect and save, ~150 KB of internal heap for the
// rest of the run. Every public operation now releases it on every exit.
struct AlphaSaveScratch {
    Candidate candidates[2]{};
    Candidate verified{};
    AlphaSaveStage stage{};
    openu5::save::Gam gam{};
    openu5::save::Ool ool{};
    openu5::save::Json side{};
    std::string json{};
    Commit commits[kSlots][2]{};
    // A3-04G. The save list's memory, per slot: the summary of a generation
    // the gate accepted and the commit record it was derived from. A summary
    // is reused only while that commit reads back with the same fields (the
    // sequence and the three files' CRCs; not memcmp: the record's 4 trailing
    // padding bytes are written uninitialised and are not copied). This service is the
    // card's only writer: save() stores the slot it wrote from its own
    // post-write check, or forgets it on any failure. Only accepted
    // generations are kept, so an empty, refused or unreadable slot is looked
    // at again on every open, as before. Load never uses it.
    // A4-SAVE2: one entry per generation of every logical slot.
    struct InspectCache {
        bool known[kSlots][2]{};
        Commit commit[kSlots][2]{};
        openu5::FrontendSaveSlot summary[kSlots][2]{};
        bool hit(int s,int i,const Commit&c)const{const auto&k=commit[s][i];
            return known[s][i]&&k.magic==c.magic&&k.version==c.version&&k.sequence==c.sequence&&k.gam==c.gam&&k.ool==c.ool&&k.json==c.json;}
        void store(int s,int i,const Commit&c,const openu5::FrontendSaveSlot&v){known[s][i]=true;commit[s][i]=c;summary[s][i]=v;}
        void forget(int s,int i){if(s>=0&&s<kSlots&&i>=0&&i<2)known[s][i]=false;}
    } cache{};
};

namespace {
// The three files of a generation whose commit record is already in v.commit.
bool read_files(int slot,int gen,Candidate&v){
    return read_file(path(slot,gen,"gam"),v.gam)&&read_file(path(slot,gen,"ool"),v.ool)&&read_file(path(slot,gen,"json"),v.json);
}
// Read one generation's commit and files, then the shared semantic check.
bool candidate(int slot,int gen,Candidate&v,AlphaSaveScratch&scratch){
    v=Candidate{};
    if(!read_commit(slot,gen,v.commit)||!read_files(slot,gen,v))return false;
    return verify_candidate(v,scratch.stage);
}
// A4-SAVE2: every commit record on the card (s.commits), and the highest
// sequence among them (0: none). Two 32-byte reads per slot; an empty slot
// costs two failed opens.
uint64_t read_all_commits(AlphaSaveScratch&s,bool(&present)[kSlots][2]){
    uint64_t newest=0;
    for(int k=0;k<kSlots;++k)for(int g=0;g<2;++g){s.commits[k][g]={};present[k][g]=read_commit(k,g,s.commits[k][g]);if(present[k][g]&&s.commits[k][g].sequence>newest)newest=s.commits[k][g].sequence;}
    return newest;
}
// The slots in Continue's order: by each slot's newest commit, newest first
// (a tie: the lower slot). A slot with no commit is left out. Returns the count.
int continue_order(const AlphaSaveScratch&s,const bool(&present)[kSlots][2],int(&order)[kSlots]){
    uint64_t newest[kSlots]{};int n=0;
    for(int k=0;k<kSlots;++k){const int g=newest_committed_slot(present[k],s.commits[k]);if(g<0)continue;
        newest[k]=s.commits[k][g].sequence;int at=n++;
        while(at>0&&newest[order[at-1]]<newest[k]){order[at]=order[at-1];--at;}
        order[at]=k;}
    return n;
}
// Restore one logical slot: both generations read and verified (each staging
// released before the other is read), then restore_newest() -- the newest
// accepted generation, falling back to the other. -1 with every live owner
// untouched when neither restores.
// A4-UI3: `older` -- the generation restored is older than the other one's
// commit record (read_commit's acceptance: the record a newest-first load tried first).
int restore_slot(AlphaSaveScratch&s,int slot,openu5::CommandContext&c,openu5::OutdoorServices&o,openu5::WorldTerrain&t,openu5::NpcActors&a,openu5::save::Json&retained,bool&older){
    for(int i=0;i<2;++i){candidate(slot,i,s.candidates[i],s);release_stage(s.stage);}
    const int gen=restore_newest(s.candidates,c,o,t,a,retained,s.stage);
    const auto&other=s.candidates[1-(gen<0?0:gen)].commit;
    older=gen>=0&&other.magic==0x31533555&&other.version==1&&other.sequence>s.candidates[gen].commit.sequence;
    release_candidate(s.candidates[0]);release_candidate(s.candidates[1]);
    return gen;
}
// Alpha 4 A4-SAVE1: whether slot `n`'s generation (commit already in
// s.commits[n]) is one the load gate accepts. Its files are read and their
// CRCs checked every time; the semantic gate runs unless the save list holds
// an accepted summary for this very commit (same sequence and CRCs, so with
// matching CRCs the same bytes). A refused slot is dropped from the list.
// Runs before the save encodes anything and releases what it staged, so the
// save never holds this document and its own at once.
bool generation_accepted(AlphaSaveScratch&s,int slot,int n){
    auto&v=s.candidates[n];v=Candidate{};v.commit=s.commits[slot][n];
    bool ok=read_files(slot,n,v)&&files_match_commit(v);
    if(ok&&!s.cache.hit(slot,n,v.commit)){ok=verify_candidate(v,s.stage);if(ok)s.cache.store(slot,n,v.commit,summarize_candidate(v,s.stage));}
    if(!ok)s.cache.forget(slot,n);
    release_candidate(v);release_stage(s.stage);return ok;
}
// A3-04G: give back every buffer the workspace holds; the fixed parts stay.
void release_workspace(AlphaSaveScratch&s){
    release_candidate(s.candidates[0]);release_candidate(s.candidates[1]);release_candidate(s.verified);
    release_stage(s.stage);s.side=openu5::save::Json{};std::string().swap(s.json);
}
// Declared after the SdHeadroomGuard, so it runs first on the way out: the
// workspace is empty before the DMA headroom is taken back.
struct ReleaseOnExit {
    AlphaSaveScratch &s;
    ~ReleaseOnExit(){release_workspace(s);}
};
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
                            const uint8_t*base,size_t base_size,const uint8_t*base_ool,size_t base_ool_size,uint32_t &ms,bool new_journey,int requested_slot){
    const int64_t start=esp_timer_get_time();last_failure_[0]=0;
    SdHeadroomGuard sd_window(new_journey?"frontend-new-journey":"gameplay-save");
    auto *workspace=scratch();if(!workspace){std::snprintf(last_failure_,sizeof(last_failure_),"PSRAM save workspace allocation failed");ms=0;return false;}auto &s=*workspace;
    ReleaseOnExit release{s};
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
    // Alpha 4 A4-SAVE2: the logical slot. -1 is the slot holding the card's
    // newest commit (a blank card: slot 0), where every pre-SAVE2 save went.
    // Only that slot's files are touched below.
    if(requested_slot<-1||requested_slot>=kSlots){std::snprintf(last_failure_,sizeof(last_failure_),"No such save slot %d",requested_slot);ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}
    bool present_all[kSlots][2]{};
    const uint64_t card_newest=read_all_commits(s,present_all);
    int logical=requested_slot;
    if(logical<0){int order[kSlots];logical=continue_order(s,present_all,order)>0?order[0]:0;}
    // Alpha 4 A4-SAVE1: the target within the slot. Its newest generation by
    // sequence is kept when the gate accepts it; when it is refused it is the
    // one replaced, and the older generation -- what the slot's load restores
    // -- is left untouched. A4-SAVE2: the sequence is above every commit on
    // the card, so the slot saved last is the one Continue restores.
    const auto&present=present_all[logical];
    const int newest=newest_committed_slot(present,s.commits[logical]);
    const bool newest_ok=newest>=0&&generation_accepted(s,logical,newest);
    const AlphaSaveTarget target=choose_save_target(present,s.commits[logical],newest_ok,card_newest);
    ESP_LOGI(kTag,"SAVE_TARGET save_slot=%d newest=%d accepted=%d slot=%d keep=%d sequence=%llu",logical,newest,int(newest_ok),target.slot,target.keep,(unsigned long long)target.sequence);
    capture_save_document(c,o,t,a,retained);
    s.gam={};s.side={};s.json.clear();
    if(!stage("gam-encode",openu5::save::export_native_state(c.game,c.turn,retained,base,base_size,s.gam,s.side,true)==openu5::save::Error::None,"INIT.GAM")){ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}
    if(new_journey)openu5::save::preserve_new_journey_template_bytes(s.gam,base,base_size);
    s.ool=openu5::save::build_ool(retained,base_ool,base_ool_size);
    if(!stage("ool-encode",s.ool.size()==openu5::save::kOolSize,"INIT.OOL")||!stage("json-encode",openu5::save::encode_json(s.side,s.json)==openu5::save::JsonError::None,"sidecar")){ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}
    const int slot=logical,gen=target.slot;Commit next;next.sequence=target.sequence;
    next.gam=openu5::save::save_crc32(s.gam.data(),s.gam.size());next.ool=openu5::save::save_crc32(s.ool.data(),s.ool.size());
    next.json=openu5::save::save_crc32(reinterpret_cast<const uint8_t*>(s.json.data()),s.json.size());
    heap_diagnostic("write-gam-temp",new_journey?"frontend-new-journey":"gameplay-save",512);
    bool ok=stage("write-gam-temp",write_file(path(slot,gen,"gam",true),s.gam.data(),s.gam.size()),path(slot,gen,"gam",true).c_str());
    if(ok)heap_diagnostic("write-ool-temp",new_journey?"frontend-new-journey":"gameplay-save",512);
    if(ok)ok=stage("write-ool-temp",write_file(path(slot,gen,"ool",true),s.ool.data(),s.ool.size()),path(slot,gen,"ool",true).c_str());
    if(ok)heap_diagnostic("write-json-temp",new_journey?"frontend-new-journey":"gameplay-save",512);
    if(ok)ok=stage("write-json-temp",write_file(path(slot,gen,"json",true),s.json.data(),s.json.size()),path(slot,gen,"json",true).c_str());
    if(ok)heap_diagnostic("crc-readback",new_journey?"frontend-new-journey":"gameplay-save",512);
    if(ok)ok=stage("crc-readback",validate_temp(slot,gen,"gam",s.gam.size(),next.gam)&&validate_temp(slot,gen,"ool",s.ool.size(),next.ool)&&validate_temp(slot,gen,"json",s.json.size(),next.json),kDirectory);
    if(ok)ok=stage("rename-gam",move_temp(slot,gen,"gam"),path(slot,gen,"gam").c_str());
    if(ok)ok=stage("rename-ool",move_temp(slot,gen,"ool"),path(slot,gen,"ool").c_str());
    if(ok)ok=stage("rename-json",move_temp(slot,gen,"json"),path(slot,gen,"json").c_str());
    if(ok)ok=stage("write-commit-temp",write_file(path(slot,gen,"commit",true),&next,sizeof(next)),path(slot,gen,"commit",true).c_str());
    if(ok)ok=stage("rename-commit",move_temp(slot,gen,"commit"),path(slot,gen,"commit").c_str());
    if(ok)ok=stage("semantic-validation",candidate(slot,gen,s.verified,s),path(slot,gen,"commit").c_str());
    // A3-04G: the save list. The written slot's summary is the one its own
    // post-write check just derived from the card; after any failure the slot
    // may hold new files under its old commit, so it is forgotten. The other
    // slot's files were not touched.
    if(ok)s.cache.store(slot,gen,s.verified.commit,summarize_candidate(s.verified,s.stage));else s.cache.forget(slot,gen);
    if(ok)last_slot_=slot;
    if(ok)stage("generation-final",true,path(slot,gen,"commit").c_str());
    if(!ok){ESP_LOGE(kTag,"slot %d generation %d save failed (errno=%d); previous generation retained",slot,gen,errno);}
    ms=uint32_t((esp_timer_get_time()-start+999)/1000);ESP_LOGI(kTag,"save generation=%llu save_slot=%d slot=%d time=%lu ms json=%zu",(unsigned long long)next.sequence,slot,gen,(unsigned long)ms,s.json.size());return ok;
}

// Continue. Alpha 4 A4-SAVE2: the slots in the order of their newest commit,
// newest first; each is restored as load_slot() does (its newest accepted
// generation, else the one before it), and the first that restores wins. On a
// pre-SAVE2 card only Slot 1 exists, and this is the pre-SAVE2 load exactly.
bool AlphaSaveService::load(openu5::CommandContext &c,openu5::OutdoorServices&o,
                            openu5::WorldTerrain&t,openu5::NpcActors&a,openu5::save::Json&retained,
                            uint32_t &ms){
    const int64_t start=esp_timer_get_time();SdHeadroomGuard sd_window("load-latest");auto *workspace=scratch();if(!workspace){ms=0;return false;}auto &s=*workspace;
    // A3-04G: each generation's staged document is released before the next
    // one's files are read; restore_newest() stages the chosen one again.
    ReleaseOnExit release{s};
    bool present[kSlots][2]{};read_all_commits(s,present);
    int order[kSlots];const int n=continue_order(s,present,order);
    for(int i=0;i<n;++i){const int slot=order[i];bool older=false;const int gen=restore_slot(s,slot,c,o,t,a,retained,older);
        if(gen<0){ESP_LOGW(kTag,"continue: save slot %d restores nothing; trying the next",slot);continue;}
        ms=uint32_t((esp_timer_get_time()-start+999)/1000);last_slot_=slot;last_load_recovered_=older||i>0;
        ESP_LOGI(kTag,"load generation=%llu save_slot=%d slot=%d time=%lu ms status=0",(unsigned long long)s.commits[slot][gen].sequence,slot,gen,(unsigned long)ms);
        return true;}
    ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;
}

bool AlphaSaveService::load_slot(int slot,openu5::CommandContext&c,openu5::OutdoorServices&o,openu5::WorldTerrain&t,openu5::NpcActors&a,openu5::save::Json&retained,uint32_t&ms){
    const int64_t start=esp_timer_get_time();SdHeadroomGuard sd_window("load-slot");auto *workspace=scratch();
    if(!workspace||slot<0||slot>=kSlots){ms=uint32_t((esp_timer_get_time()-start+999)/1000);return false;}
    auto&s=*workspace;ReleaseOnExit release{s};
    bool older=false;const int gen=restore_slot(s,slot,c,o,t,a,retained,older);ms=uint32_t((esp_timer_get_time()-start+999)/1000);
    if(gen<0)return false;
    last_slot_=slot;last_load_recovered_=older;ESP_LOGI(kTag,"load save_slot=%d slot=%d time=%lu ms status=0",slot,gen,(unsigned long)ms);return true;
}
// The save list for the title screen and the System Menu.
//
// Alpha 3 A3-04G (ALPHA3_AUDIO.md section 28). Until A3-04G every open read all
// four files of both slots over the TFT's 800 kHz SPI bus and staged both
// generations whole (each import is a ~190 KB document), ~0.72 s on the device,
// and kept the last one. Now, per slot: the commit record is read; if it is
// the one an accepted summary was derived from (InspectCache), that summary is
// the answer; otherwise the slot is read and verified exactly as before, and
// its staging released before the next slot is read. The listing a player sees
// is the same one a cold inspection gives (a3_04g_storage_runtime K2-K10).
// A4-SAVE2: one logical slot's two generations, each verified as above.
void AlphaSaveService::inspect_generations(int slot_index,openu5::FrontendSaveSlot(&slots)[2]){
    const int64_t start=esp_timer_get_time();
    SdHeadroomGuard sd_window("save-inspect");
    auto*workspace=scratch();
    for(auto&slot:slots)slot={};
    if(!workspace||slot_index<0||slot_index>=kSlots)return;
    auto&s=*workspace;
    const char*source[2]={"empty","empty"};
    int64_t commit_us=0,read_us=0,verify_us=0;
    size_t bytes=0;
    for(int i=0;i<2;++i){
        Commit commit{};
        int64_t t=esp_timer_get_time();
        const bool present=read_commit(slot_index,i,commit);
        commit_us+=esp_timer_get_time()-t;
        if(!present)continue;
        bytes+=sizeof(commit);
        if(s.cache.hit(slot_index,i,commit)){slots[i]=s.cache.summary[slot_index][i];source[i]="cached";continue;}
        s.cache.forget(slot_index,i);
        auto&v=s.candidates[i];
        v=Candidate{};
        v.commit=commit;
        t=esp_timer_get_time();
        const bool read=read_files(slot_index,i,v);
        read_us+=esp_timer_get_time()-t;
        bytes+=v.gam.size()+v.ool.size()+v.json.size();
        t=esp_timer_get_time();
        const bool valid=read&&verify_candidate(v,s.stage);
        verify_us+=esp_timer_get_time()-t;
        if(valid){slots[i]=summarize_candidate(v,s.stage);s.cache.store(slot_index,i,v.commit,slots[i]);source[i]="verified";}
        else{slots[i].present=true;slots[i].sequence=commit.sequence;source[i]="refused";} // A4-UI2: ordered by age
        release_candidate(v);
        release_stage(s.stage);
    }
    ESP_LOGI(kTag,"SAVE_INSPECT save_slot=%d slot0=%s slot1=%s bytes=%zu commit_us=%lld read_us=%lld verify_us=%lld total_us=%lld",
             slot_index,source[0],source[1],bytes,(long long)commit_us,(long long)read_us,(long long)verify_us,
             (long long)(esp_timer_get_time()-start));
}

// Alpha 4 A4-SAVE2 (ALPHA4_UI.md section 4): the menus' save list. Per slot:
// its two commit records; the newest generation is staged only when the save
// list has no accepted summary for that exact commit, and the one before it
// only when the newest is refused. Every staging is released before the next
// generation is read, so a cold open never holds two documents; a warm open
// of an unchanged card is the commit reads alone. The status is what
// load_slot() would do: restore the newest, restore the one before, or
// nothing.
void AlphaSaveService::inspect_catalog(openu5::FrontendSaveCatalog&out){
    const int64_t start=esp_timer_get_time();
    SdHeadroomGuard sd_window("save-catalog");
    out=openu5::FrontendSaveCatalog{};
    auto*workspace=scratch();
    if(!workspace)return;
    auto&s=*workspace;
    const char*source[kSlots]={};
    int64_t commit_us=0,verify_us=0;
    size_t bytes=0;unsigned staged=0;
    // One generation's summary: 1 from the save list, 2 read and verified,
    // 0 refused.
    auto probe=[&](int k,int g,openu5::FrontendSaveSlot&summary)->int{
        if(s.cache.hit(k,g,s.commits[k][g])){summary=s.cache.summary[k][g];return 1;}
        s.cache.forget(k,g);
        auto&v=s.candidates[g];v=Candidate{};v.commit=s.commits[k][g];
        const int64_t t=esp_timer_get_time();
        const bool read=read_files(k,g,v);
        bytes+=v.gam.size()+v.ool.size()+v.json.size();++staged;
        const bool valid=read&&verify_candidate(v,s.stage);
        if(valid){summary=summarize_candidate(v,s.stage);s.cache.store(k,g,v.commit,summary);}
        release_candidate(v);release_stage(s.stage);
        verify_us+=esp_timer_get_time()-t;
        return valid?2:0;
    };
    for(int k=0;k<kSlots;++k){
        auto&e=out.slots[k];
        bool present[2]{};
        const int64_t t=esp_timer_get_time();
        for(int g=0;g<2;++g){s.commits[k][g]={};present[g]=read_commit(k,g,s.commits[k][g]);if(present[g])bytes+=sizeof(Commit);}
        commit_us+=esp_timer_get_time()-t;
        const int newest=newest_committed_slot(present,s.commits[k]);
        if(newest<0){source[k]="empty";continue;}
        e.sequence=s.commits[k][newest].sequence;
        const int first=probe(k,newest,e.shown);
        if(first){e.status=openu5::SaveSlotStatus::Saved;source[k]=first==1?"cached":"verified";continue;}
        const int older=1-newest;
        if(present[older]&&probe(k,older,e.shown)){e.status=openu5::SaveSlotStatus::Recovered;source[k]="recovered";continue;}
        e.shown=openu5::FrontendSaveSlot{};e.status=openu5::SaveSlotStatus::Damaged;source[k]="damaged";
    }
    ESP_LOGI(kTag,"SAVE_CATALOG slot1=%s slot2=%s slot3=%s staged=%u bytes=%zu commit_us=%lld verify_us=%lld total_us=%lld",
             source[0],source[1],source[2],staged,bytes,(long long)commit_us,(long long)verify_us,(long long)(esp_timer_get_time()-start));
}

// Alpha 4 A4-SAVE3 (ALPHA4_UI.md section 5): original PC/DOS saves.
namespace {
bool make_directory(const char*p){errno=0;if(mkdir(p,0777)==0)return true;if(errno!=EEXIST)return false;struct stat st{};return stat(p,&st)==0&&S_ISDIR(st.st_mode);}
// -1: no such file; else its size.
long file_size(const std::string&p){struct stat st{};if(stat(p.c_str(),&st)!=0||!S_ISREG(st.st_mode))return -1;return long(st.st_size);}
std::string in_dir(const char*dir,const char*name){return std::string(dir)+"/"+name;}
// A whole file written beside its final name, read back, then renamed over it.
bool replace_file(const std::string&p,const void*data,size_t n){
    const std::string tmp=p+".tmp";std::vector<uint8_t>back;
    if(!write_file(tmp,data,n)||!read_file(tmp,back)||back.size()!=n||(n&&std::memcmp(back.data(),data,n)!=0)){unlink(tmp.c_str());return false;}
    unlink(p.c_str());return rename(tmp.c_str(),p.c_str())==0;
}
}

AlphaSaveService::PcRead AlphaSaveService::read_pc_import(PcSource&src){
    SdHeadroomGuard sd_window("pc-import-read");
    src=PcSource{};
    const auto gam=in_dir(kImportDirectory,openu5::save::pc::kGamFile),ool=in_dir(kImportDirectory,openu5::save::pc::kOolFile);
    const long gam_size=file_size(gam),ool_size=file_size(ool);
    ESP_LOGI(kTag,"PC_IMPORT_READ gam=%ld ool=%ld",gam_size,ool_size);
    if(gam_size<0&&ool_size<0)return PcRead::NoFiles;
    if(gam_size<0)return PcRead::NoGam;
    if(ool_size<0)return PcRead::NoOol;
    src.gam_size=size_t(gam_size);src.ool_size=size_t(ool_size);
    // Only a file of the original size is read: a stray large file is never loaded whole.
    if(src.gam_size==openu5::save::kGamSize&&!read_file(gam,src.gam))return PcRead::ReadFailed;
    if(src.ool_size==openu5::save::kOolSize&&!read_file(ool,src.ool))return PcRead::ReadFailed;
    if((!src.gam.empty()&&src.gam.size()!=src.gam_size)||(!src.ool.empty()&&src.ool.size()!=src.ool_size))return PcRead::ReadFailed;
    src.gam_crc=openu5::save::save_crc32(src.gam.data(),src.gam.size());src.ool_crc=openu5::save::save_crc32(src.ool.data(),src.ool.size());
    return PcRead::Ok;
}

int AlphaSaveService::pc_import_marker(uint32_t gam_crc,uint32_t ool_crc){
    SdHeadroomGuard sd_window("pc-import-marker");
    std::vector<uint8_t>b;if(!read_file(kImportMarker,b))return -1;
    const std::string text(b.begin(),b.end());unsigned g=0,o=0;int slot=-1;
    if(std::sscanf(text.c_str(),"gam=%x ool=%x slot=%d",&g,&o,&slot)!=3)return -1;
    return g==gam_crc&&o==ool_crc&&slot>=0&&slot<kSlots?slot:-1;
}

bool AlphaSaveService::record_pc_import(uint32_t gam_crc,uint32_t ool_crc,int slot){
    SdHeadroomGuard sd_window("pc-import-marker");
    char text[64];const int n=std::snprintf(text,sizeof(text),"gam=%08lx ool=%08lx slot=%d\n",(unsigned long)gam_crc,(unsigned long)ool_crc,slot);
    return make_directory("/sd/ultima5")&&replace_file(kImportMarker,text,size_t(n));
}

// The generation exported is the one load_slot() restores (restore_newest):
// the newest the gate accepts, else the one before it. It is staged by the
// same verify_candidate(), one document at a time, and released before any
// file is written. The slot's own files are only read.
AlphaSaveService::PcExport AlphaSaveService::export_pc_save(int slot,PcExportResult&out){
    const int64_t start=esp_timer_get_time();out=PcExportResult{};last_failure_[0]=0;
    SdHeadroomGuard sd_window("pc-export");
    if(slot<0||slot>=kSlots)return PcExport::NoSlot;
    auto*workspace=scratch();if(!workspace)return PcExport::NoWorkspace;
    auto&s=*workspace;ReleaseOnExit release{s};
    bool present[2]{};Commit commits[2]{};
    for(int g=0;g<2;++g)present[g]=read_commit(slot,g,commits[g]);
    const int newest=newest_committed_slot(present,commits);
    auto&v=s.candidates[0];int chosen=-1;
    for(int pass=0;pass<2&&newest>=0&&chosen<0;++pass){
        const int g=pass?1-newest:newest;if(!present[g])continue;
        v=Candidate{};v.commit=commits[g];
        if(read_files(slot,g,v)&&verify_candidate(v,s.stage))chosen=g;
        else{release_candidate(v);release_stage(s.stage);}
    }
    if(chosen<0){std::snprintf(last_failure_,sizeof(last_failure_),"Slot %d has no loadable save",slot+1);return PcExport::NoLoadable;}
    if(openu5::save::pc::exportable(s.stage.document)!=openu5::save::pc::Check::Ok){
        std::snprintf(last_failure_,sizeof(last_failure_),"%s",openu5::save::pc::check_text(openu5::save::pc::Check::Dungeon));return PcExport::Dungeon;}
    openu5::save::Gam gam{};openu5::save::Ool ool{};
    std::copy_n(v.gam.begin(),openu5::save::kGamSize,gam.begin());std::copy_n(v.ool.begin(),openu5::save::kOolSize,ool.begin());
    openu5::save::pc::complete_export(s.stage.document,gam,ool,&out.report);
    out.generation=chosen;out.sequence=v.commit.sequence;
    release_candidate(v);release_stage(s.stage);
    // What the export carries and what it leaves out (section 5.8), for the player.
    const auto&r=out.report;std::string manifest;char line[160];
    auto add=[&](const char*text){manifest+=text;manifest+="\r\n";};
    add("Ultima V Native - PC save export");
    std::snprintf(line,sizeof(line),"Slot %d, save %llu. Copy SAVED.GAM and SAVED.OOL into the Ultima V directory, then Journey Onward.",slot+1,(unsigned long long)out.sequence);add(line);
    std::snprintf(line,sizeof(line),"Vehicles written: %u frigate(s), %u horse(s), %u skiff(s).",unsigned(r.frigates),unsigned(r.horses),unsigned(r.skiffs));add(line);
    add("Not carried to the PC:");
    if(r.town_npcs_left_out)add("- the people and chests of the current town: they return when the party leaves and re-enters it");
    if(r.unplaced){std::snprintf(line,sizeof(line),"- %u vehicle(s): no free object slot",unsigned(r.unplaced));add(line);}
    if(r.carpets){std::snprintf(line,sizeof(line),"- %u magic carpet(s) left standing on the map",unsigned(r.carpets));add(line);}
    if(r.loot){std::snprintf(line,sizeof(line),"- %u item(s) lying on the ground",unsigned(r.loot));add(line);}
    if(r.shadowlords)add("- a summoned Shadowlord");
    add("- Native-only state: the save slot, its backup and commit record, settings, open doors, the HMS Cape toggle");
    char dir[48];std::snprintf(dir,sizeof(dir),"%s/slot%d",kExportDirectory,slot+1);
    const auto gam_path=in_dir(dir,openu5::save::pc::kGamFile),ool_path=in_dir(dir,openu5::save::pc::kOolFile);
    bool ok=make_directory("/sd/ultima5")&&make_directory(kExportDirectory)&&make_directory(dir);
    if(ok)ok=replace_file(gam_path,gam.data(),gam.size());
    if(ok)ok=replace_file(ool_path,ool.data(),ool.size());
    if(ok)ok=replace_file(in_dir(dir,"EXPORT.TXT"),manifest.data(),manifest.size());
    if(!ok){std::snprintf(last_failure_,sizeof(last_failure_),"Export could not be written (errno %d)",errno);
        ESP_LOGE(kTag,"PC_EXPORT slot=%d write failed errno=%d",slot,errno);return PcExport::WriteFailed;}
    // The files on the card, read back: the same bytes, an original save the
    // bridge accepts, and one Native's own codec reads.
    std::vector<uint8_t>back_gam,back_ool;
    ok=read_file(gam_path,back_gam)&&read_file(ool_path,back_ool)&&back_gam.size()==gam.size()&&back_ool.size()==ool.size()&&
       std::equal(gam.begin(),gam.end(),back_gam.begin())&&std::equal(ool.begin(),ool.end(),back_ool.begin())&&
       openu5::save::pc::check_original(back_gam.data(),back_gam.size(),back_ool.data(),back_ool.size())==openu5::save::pc::Check::Ok&&
       openu5::save::pc::import_original(back_gam.data(),back_ool.data(),s.stage.game,s.stage.turn,s.stage.document)==openu5::save::Error::None;
    release_stage(s.stage);
    if(!ok){std::snprintf(last_failure_,sizeof(last_failure_),"%s","Exported files failed their check");return PcExport::VerifyFailed;}
    ESP_LOGI(kTag,"PC_EXPORT slot=%d generation=%d sequence=%llu frigates=%u horses=%u skiffs=%u unplaced=%u town=%d time_us=%lld",
             slot,chosen,(unsigned long long)out.sequence,unsigned(r.frigates),unsigned(r.horses),unsigned(r.skiffs),unsigned(r.unplaced),
             int(r.town_npcs_left_out),(long long)(esp_timer_get_time()-start));
    return PcExport::Ok;
}

bool AlphaSettingsService::load(openu5::FrontendSettings&s)const{SdHeadroomGuard sd_window("settings-load");std::vector<uint8_t>b;if(!read_file(kPath,b))return false;return openu5::decode_settings(std::string(reinterpret_cast<const char*>(b.data()),b.size()),s);}
bool AlphaSettingsService::save(const openu5::FrontendSettings&s)const{SdHeadroomGuard sd_window("settings-save");mkdir("/sd/ultima5",0777);std::string text;if(!openu5::encode_settings(s,text))return false;const std::string tmp=std::string(kPath)+".tmp";if(!write_file(tmp,text.data(),text.size()))return false;unlink(kPath);return rename(tmp.c_str(),kPath)==0;}
} // namespace tdeck
