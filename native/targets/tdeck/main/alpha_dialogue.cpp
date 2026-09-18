#include "alpha_dialogue.h"

#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"

namespace tdeck {
namespace {
constexpr char kTag[] = "AlphaDialogue";
uint16_t u16(const uint8_t *p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t u32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
struct Reader {
    const uint8_t *p = nullptr, *end = nullptr;
    bool ok = true;
    uint8_t byte(){if(p==end){ok=false;return 0;}return *p++;}
    uint16_t word(){if(size_t(end-p)<2){ok=false;return 0;}auto v=u16(p);p+=2;return v;}
    int32_t integer(){if(size_t(end-p)<4){ok=false;return 0;}auto v=int32_t(u32(p));p+=4;return v;}
};
}

AlphaDialogueCache::~AlphaDialogueCache(){clear();}
void AlphaDialogueCache::clear(){if(arena_)heap_caps_free(arena_);arena_=nullptr;capacity_=used_=0;script_={};master_=openu5::TalkMaster::None;dialog_=0;}
void *AlphaDialogueCache::allocate(size_t bytes,size_t alignment){const size_t at=(used_+alignment-1)&~(alignment-1);if(at>capacity_||bytes>capacity_-at)return nullptr;void*out=arena_+at;std::memset(out,0,bytes);used_=at+bytes;return out;}

const openu5::TalkScript *AlphaDialogueCache::lookup(void *p,openu5::TalkMaster master,int32_t dialog){return p?static_cast<AlphaDialogueCache*>(p)->load(master,dialog):nullptr;}
const openu5::TalkScript *AlphaDialogueCache::load(openu5::TalkMaster master,int32_t dialog){
    if(master==master_&&dialog==dialog_&&arena_)return &script_;
    clear();
    if(!data_||size_<4)return nullptr;
    const uint32_t count=u32(data_);
    if(count>1024||size_<4+size_t(count)*16)return nullptr;
    const uint8_t *blob=nullptr;size_t length=0;
    for(uint32_t i=0;i<count;++i){const uint8_t*e=data_+4+i*16;if(e[0]==uint8_t(master)&&int32_t(u32(e+4))==dialog){const size_t at=u32(e+8);length=u32(e+12);if(at<=size_&&length<=size_-at)blob=data_+at;break;}}
    if(!blob)return nullptr;
    capacity_=length*8+4096;
    arena_=static_cast<uint8_t*>(heap_caps_calloc(1,capacity_,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!arena_)return nullptr;
    Reader r{blob,blob+length};
    auto text=[&]()->openu5::TalkText{const size_t n=r.word();if(!r.ok||size_t(r.end-r.p)<n*2){r.ok=false;return{};}auto*out=static_cast<char16_t*>(allocate((n+1)*sizeof(char16_t),alignof(char16_t)));if(!out){r.ok=false;return{};}for(size_t i=0;i<n;++i)out[i]=char16_t(u16(r.p+i*2));r.p+=n*2;return{out,n};};
    auto line=[&](auto&& self)->openu5::TalkLine{const size_t n=r.word();auto*out=static_cast<openu5::TalkItem*>(allocate(n*sizeof(openu5::TalkItem),alignof(openu5::TalkItem)));if(n&&!out){r.ok=false;return{};}for(size_t i=0;i<n&&r.ok;++i){out[i].op=openu5::TalkOp(r.byte());out[i].value=r.integer();out[i].text=text();}return{out,n};};
    auto qas=[&](auto&& self)->openu5::TalkView<openu5::TalkQA>{const size_t n=r.word();auto*out=static_cast<openu5::TalkQA*>(allocate(n*sizeof(openu5::TalkQA),alignof(openu5::TalkQA)));if(n&&!out){r.ok=false;return{};}for(size_t i=0;i<n&&r.ok;++i){const size_t kn=r.word();auto*keys=static_cast<openu5::TalkText*>(allocate(kn*sizeof(openu5::TalkText),alignof(openu5::TalkText)));if(kn&&!keys){r.ok=false;break;}for(size_t k=0;k<kn&&r.ok;++k)keys[k]=text();const size_t an=r.word();auto*answers=static_cast<openu5::TalkLine*>(allocate(an*sizeof(openu5::TalkLine),alignof(openu5::TalkLine)));if(an&&!answers){r.ok=false;break;}for(size_t a=0;a<an&&r.ok;++a)answers[a]=line(line);out[i]={{keys,kn},{answers,an}};}return{out,n};};
    script_.npc_index=r.integer();script_.name=line(line);script_.description=line(line);script_.greeting=line(line);script_.job=line(line);script_.bye=line(line);script_.qa=qas(qas);
    const size_t ln=r.word();auto*labels=static_cast<openu5::TalkLabel*>(allocate(ln*sizeof(openu5::TalkLabel),alignof(openu5::TalkLabel)));if(ln&&!labels)r.ok=false;
    for(size_t i=0;i<ln&&r.ok;++i){labels[i].label=r.integer();labels[i].initial=line(line);const size_t dn=r.word();auto*defaults=static_cast<openu5::TalkLine*>(allocate(dn*sizeof(openu5::TalkLine),alignof(openu5::TalkLine)));if(dn&&!defaults){r.ok=false;break;}for(size_t d=0;d<dn&&r.ok;++d)defaults[d]=line(line);labels[i].defaults={defaults,dn};labels[i].qa=qas(qas);}script_.labels={labels,ln};
    if(!r.ok||r.p!=r.end){ESP_LOGE(kTag,"dialogue record parse failed master=%u dialog=%ld",unsigned(master),long(dialog));clear();return nullptr;}
    master_=master;dialog_=dialog;ESP_LOGI(kTag,"loaded master=%u dialog=%ld PSRAM=%zu",unsigned(master),long(dialog),capacity_);return &script_;
}
} // namespace tdeck
