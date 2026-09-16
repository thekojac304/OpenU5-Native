#include "openu5/dialogue.h"
#include "openu5/dialogue_orchestration.h"
#include <deque>
#include <fstream>
#include <iostream>
#include <cstring>
#include <new>
#include <cstdlib>
#include <cstddef>
#include <algorithm>
// Host-only allocator probe: fixtures/assets are outside the measured interval.
// Header overhead is excluded; this measures requested live payload bytes.
namespace allocation_probe {
struct alignas(std::max_align_t) Header { size_t size; bool measured; };
bool enabled=false;size_t live=0,peak=0,largest=0;
}
void *operator new(size_t size){using namespace allocation_probe;auto *p=static_cast<Header *>(std::malloc(sizeof(Header)+size));if(!p)throw std::bad_alloc();p->size=size;p->measured=enabled;if(enabled){live+=size;peak=std::max(peak,live);largest=std::max(largest,size);}return p+1;}
void operator delete(void *p) noexcept {if(!p)return;using namespace allocation_probe;auto *h=static_cast<Header *>(p)-1;if(h->measured)live-=h->size;std::free(h);}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete[](void *p) noexcept {::operator delete(p);}
void operator delete(void *p,size_t) noexcept {::operator delete(p);}
void operator delete[](void *p,size_t) noexcept {::operator delete(p);}
using namespace openu5;
struct Reader {
    std::ifstream in;
    explicit Reader(const char *p):in(p,std::ios::binary) {}
    uint32_t n() { unsigned char b[4]{}; in.read(reinterpret_cast<char *>(b),4); if(!in) {std::cerr<<"Bad fixture\n";std::exit(2);} return uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24); }
    std::u16string s() { auto count=n();std::u16string s;while(count--){unsigned char b[2];in.read(reinterpret_cast<char *>(b),2);s+=char16_t(b[0]|(b[1]<<8));}return s; }
};
template<class T> TalkView<T> view(const std::vector<T> &v) {return {v.data(),v.size()};}
struct Assets {
    std::deque<std::u16string> texts;
    std::deque<std::vector<TalkItem>> items;
    std::deque<std::vector<TalkText>> keywords;
    std::deque<std::vector<TalkLine>> lines;
    std::deque<std::vector<TalkQA>> questions;
    std::deque<std::vector<TalkLabel>> labels;
    TalkText text(Reader &r){texts.push_back(r.s());return texts.back();}
    TalkLine line(Reader &r){auto n=r.n();items.emplace_back();auto &v=items.back();while(n--){TalkItem i;i.op=TalkOp(r.n());i.text=text(r);i.value=int32_t(r.n());v.push_back(i);}return view(v);}
    TalkView<TalkLine> multiline(Reader &r){auto n=r.n();lines.emplace_back();auto &v=lines.back();while(n--)v.push_back(line(r));return view(v);}
    TalkView<TalkQA> qa(Reader &r){auto n=r.n();questions.emplace_back();auto &v=questions.back();while(n--){TalkQA q;auto nk=r.n();keywords.emplace_back();auto &k=keywords.back();while(nk--)k.push_back(text(r));q.keywords=view(k);q.answer=multiline(r);v.push_back(q);}return view(v);}
    TalkScript script(Reader &r){TalkScript s;s.npc_index=int32_t(r.n());s.name=line(r);s.description=line(r);s.greeting=line(r);s.job=line(r);s.bye=line(r);s.qa=qa(r);auto n=r.n();labels.emplace_back();auto &v=labels.back();while(n--){TalkLabel l;l.label=int32_t(r.n());l.initial=line(r);l.defaults=multiline(r);l.qa=qa(r);v.push_back(l);}s.labels=view(v);return s;}
};
struct Hash {
    uint32_t h=2166136261u;
    void b(uint8_t x){h=(h^x)*16777619u;}
    void n(uint32_t x){for(int i=0;i<4;i++)b(uint8_t(x>>(8*i)));}
    void s(TalkText s){n(uint32_t(s.size()));for(char16_t c:s){b(uint8_t(c));b(uint8_t(c>>8));}}
};
std::u16string widen(const char *s){std::u16string r;while(*s)r+=char16_t(static_cast<unsigned char>(*s++));return r;}
GameState initial(int v) {
    constexpr int gold[]={0,1,30,9999},karma[]={0,50,98,99},turns[]={0,99,100,255};
    GameState g;g.gold=uint16_t(gold[v%4]);g.food=9999;g.keys=98;g.gems=99;g.skull_keys=99;g.magic_carpets=98;
    g.karma=uint8_t(karma[v%4]);g.turns_since_start=turns[v%4];
    constexpr uint16_t lengths[]={0,48,64,256};g.equipment_count=lengths[v%4];
    for(uint16_t i=0;i<g.equipment_count;i++)g.equipment_quantities[i]=v%4==3?100:98;
    g.party.character_count=16;g.party.party_size=v%4==3?6:2;g.party.active_character=1;
    for(int i=0;i<16;i++){auto &c=g.party.characters[i];std::string name=i==0?"Avatar":i==1?"Mariah":i==8?"Gorn":"MEM"+std::to_string(i);std::memcpy(c.name,name.c_str(),name.size()+1);c.party_status=uint8_t(i<g.party.party_size?0:255);c.weapon=uint8_t(i);c.exp=uint16_t(i*123);}
    return g;
}
void state(Hash &h,const GameState &g) {
    for(auto n:{int(g.gold),int(g.food),g.keys,g.gems,g.torches,g.skull_keys,g.magic_carpets,int(g.karma),int(g.turns_since_start),int(g.grapple),int(g.sextant),int(g.spyglass),int(g.black_badge),g.party.party_size,int(g.party.active_character)})h.n(uint32_t(n));
    for(auto n:g.equipment_quantities)h.n(uint32_t(n));
    h.n(g.equipment_count);
    for(const auto &c:g.party.characters){h.s(widen(c.name));h.n(c.party_status);h.n(c.weapon);h.n(c.exp);}
}
void output(Hash &h,const std::vector<DialogueOutput> &os){h.n(uint32_t(os.size()));for(const auto &o:os){h.n(uint32_t(o.kind));if(o.kind==DialogueOutputKind::Line){h.s(o.text);h.n(o.rune);h.n(uint32_t(o.pause));h.n(uint32_t(o.segments.size()));for(const auto &s:o.segments){h.s(s.text);h.n(s.rune);}}else if(o.kind==DialogueOutputKind::Prompt)h.n(o.question);else{h.n(uint32_t(o.effect.kind));h.n(uint32_t(o.effect.value));}}}
void flow_event(void *p,const GameEvent &e){
    auto &h=*static_cast<Hash *>(p);
    auto message=[&](TalkText text,bool rune=false){h.n(0);h.s(text);h.n(rune);};
    if(e.kind==GameEventKind::Message){message(widen(e.text));return;}
    if(e.kind!=GameEventKind::Dialogue)return;
    const auto &d=*e.dialogue;
    if(d.kind==DialogueEventKind::Handoff){h.n(3);return;}
    if(d.kind==DialogueEventKind::EffectMessage){message(d.message);return;}
    if(d.kind!=DialogueEventKind::Output)return;
    const auto &o=*d.output;
    if(o.kind==DialogueOutputKind::Prompt)message(o.question?u"You respond-":u"Your interest?");
    if(o.kind==DialogueOutputKind::Line&&!talk_trim(o.text).empty()){
        if(o.segments.empty())message(o.text,o.rune);
        else{h.n(1);h.n(uint32_t(o.segments.size()));for(const auto &s:o.segments){h.s(s.text);h.n(s.rune);}}
    }
}
NpcActor actor(uint8_t slot,uint8_t type,uint8_t dialog,bool schedule){NpcActor n;n.schedule.slot=slot;n.schedule.type=type;n.schedule.dialog=dialog;n.location=1;n.x=int16_t(10+slot);n.y=10;for(int i=0;i<3;i++)n.schedule.ai[i]=uint8_t(i+1);for(int i=0;i<4;i++)n.schedule.times[i]=uint8_t(schedule?i+1:0);return n;}
void actor_state(Hash &h,const GameState &g,const NpcActors &a){
    for(auto n:g.npc_met)h.n(n);for(auto n:g.npc_dead)h.n(n);h.n(g.time.hour);h.n(g.time.minute);h.n(uint32_t(a.count));
    for(size_t i=0;i<a.count;i++){const auto &n=a.actors[i];for(auto v:{int(n.schedule.slot),int(n.schedule.type),int(n.schedule.dialog),int(n.location),int(n.x),int(n.y),int(n.z),int(n.state),int(n.served_slot),int(n.path_index),int(n.stuck)})h.n(uint32_t(v));for(auto v:n.schedule.ai)h.n(v);for(auto v:n.schedule.times)h.n(v);for(auto v:n.path)h.n(v);}
}
int main(int argc,char **argv){
    if(argc!=2)return 2;Reader r(argv[1]);if(r.n()!=0x54414c4b)return 2;
    Assets assets;std::vector<TalkScript> scripts;auto ns=r.n();while(ns--)scripts.push_back(assets.script(r));
    const auto cases=r.n();size_t snapshots=0,retained=0,line_chars=0,line_capacity=0,batch=0;int errors=0;
    for(uint32_t ci=0;ci<cases;ci++){
        auto si=r.n(),v=r.n(),known=r.n();int32_t label=int32_t(r.n());auto ni=r.n();std::vector<std::u16string> inputs;while(ni--)inputs.push_back(r.s());
        auto script=scripts[si];TalkItem jump{TalkOp::Label,{},label};
        if(label>=0){script.description={};script.greeting={};script.job={&jump,1};}
        auto g=initial(int(v));OriginalRng rng(int(v*1709+13));Conversation c;
        TalkText names[]={u"Avatar",u"Mariah"};ConversationContext ctx;ctx.avatar_name=u"Avatar";ctx.party_names={names,2};ctx.has_party_names=true;ctx.knows=known!=0;ctx.context=&rng;ctx.self_intro_roll=[](void *p)->int32_t{return static_cast<OriginalRng *>(p)->next(0,1).value;};c.bind(script,ctx);
        if(si==0&&v==3){ctx.translate=[](void *,TalkText t){return t.empty()?std::u16string{}:u"["+std::u16string(t)+u"]";};ctx.see_compose=[](void *,TalkText t){return u"SEE{"+std::u16string(t)+u"}";};ctx.aliases=[](void *,TalkText k)->TalkView<TalkText>{static const TalkText aliases[]={u"occupation",u"workish"};return k==u"job"?TalkView<TalkText>{aliases,2}:TalkView<TalkText>{};};c.bind(script,ctx);}
        std::u16string name;for(const auto &i:script.name)if(i.op==TalkOp::Text)name+=i.text;
        for(int step=-1;step<int(inputs.size());step++){
            allocation_probe::enabled=true;const auto &os=step<0?c.start():c.input(inputs[size_t(step)]);allocation_probe::enabled=false;Hash h;output(h,os);h.n(c.ended());h.n(c.met_avatar());
            retained=std::max(retained,c.retained_bytes());batch=std::max(batch,os.size());for(const auto &o:os){line_chars=std::max(line_chars,o.text.size());line_capacity=std::max(line_capacity,o.text.capacity());}
            for(const auto &o:os)if(o.kind==DialogueOutputKind::Effect&&step>=0){auto res=apply_dialogue_effect(g,o.effect,talk_trim(name),int32_t(0x6c+v));h.n(res.message_count);for(uint8_t i=0;i<res.message_count;i++)h.s(res.messages[i]);h.n(res.ended);h.n(res.despawn_npc);h.n(res.alarm);}
            state(h,g);h.n(rng.get_seed());auto expected=r.n();snapshots++;
            if(expected!=h.h){if(errors++<12){std::cerr<<"case="<<ci<<" script="<<si<<" label="<<label<<" variant="<<v<<" known="<<known<<" step="<<step<<" expected="<<expected<<" actual="<<h.h<<"\n";for(const auto &o:os){std::cerr<<"  "<<int(o.kind)<<" ";for(auto ch:o.text)std::cerr<<(ch<128?char(ch):'?');std::cerr<<"\n";}}}
        }
    }
    const auto flows=r.n();size_t flow_snapshots=0;
    for(uint32_t fi=0;fi<flows;fi++){
        auto si=r.n(),v=r.n();auto g=initial(int(v));g.rng.seed(int(v*1709+13));g.position={{10,10},{1,0}};g.time.hour=12;g.time.minute=34;g.npc_met[0]=(v&1)?2:0;
        NpcActors actors;actors.count=6;actors.actors[0]=actor(1,uint8_t(0x6c+v),uint8_t(scripts[si].npc_index),true);actors.actors[1]=actor(6,0xb4,1,true);actors.actors[2]=actor(5,0x90,0xfe,true);actors.actors[3]=actor(4,0x40,0xfe,false);actors.actors[4]=actor(3,0x71,1,true);actors.actors[5]=actor(2,0xfc,255,true);
        TurnState turn;TravelState travel;CommandState commands;WorldData world;CommandContext ctx{g,turn,travel,commands,world};ctx.actors=&actors;
        DialogueSession session;DialogueServices services{session};services.registry.context=&scripts[si];services.registry.get=[](void *p,TalkMaster m,int32_t id)->const TalkScript *{auto *s=static_cast<TalkScript *>(p);return m==TalkMaster::Towne&&s->npc_index==id?s:nullptr;};ctx.dialogue_services=&services;
        const auto count=r.n();
        for(uint32_t step=0;step<count;step++){
            auto action=r.n();auto text=r.s();Hash h;ctx.events={&h,flow_event};Command cmd;cmd.kind=action==0?CommandKind::BeginConversation:action==1?CommandKind::DialogueText:CommandKind::EndConversation;cmd.member=1;cmd.text=text.data();cmd.text_length=text.size();execute_command(ctx,cmd);
            h.n(99);h.n(session.active);state(h,g);actor_state(h,g,actors);h.n(g.rng.get_seed());auto expected=r.n();snapshots++;flow_snapshots++;
            if(expected!=h.h&&errors++<12)std::cerr<<"flow="<<fi<<" script="<<si<<" variant="<<v<<" step="<<step<<" expected="<<expected<<" actual="<<h.h<<"\n";
        }
    }
    const auto rules=r.n();
    for(uint32_t i=0;i<rules;i++){auto v=r.n();DialogueEffect e;e.kind=DialogueEffectKind(r.n());e.value=int32_t(r.n());auto tile=int32_t(r.n());auto name=r.s();auto g=initial(int(v));auto res=apply_dialogue_effect(g,e,name,tile);Hash h;h.n(res.message_count);for(uint8_t j=0;j<res.message_count;j++)h.s(res.messages[j]);h.n(res.ended);h.n(res.despawn_npc);h.n(res.alarm);state(h,g);auto expected=r.n();if(expected!=h.h&&errors++<12)std::cerr<<"effect="<<i<<" expected="<<expected<<" actual="<<h.h<<"\n";snapshots++;}
    const auto alarms=r.n();constexpr uint8_t tiles[]={0xfc,0xd8,0x70,0x71,0x40,0x73,0x74,0xb4};
    for(uint32_t seed=0;seed<alarms;seed++){auto g=initial(int(seed%4));g.time.hour=12;g.time.minute=34;NpcActors a;a.count=32;for(uint32_t i=0;i<32;i++)a.actors[i]=actor(uint8_t(31-i),tiles[(i+seed)%8],i%3==0?0xfe:1,(i+seed)%3!=0);OriginalRng rng{int32_t(seed)};dialogue_alarm(a,1,rng_source(rng));Hash h;actor_state(h,g,a);h.n(rng.get_seed());auto expected=r.n();if(expected!=h.h&&errors++<12)std::cerr<<"alarm="<<seed<<" expected="<<expected<<" actual="<<h.h<<"\n";snapshots++;}
    std::cout<<snapshots<<" dialogue snapshots ("<<flow_snapshots<<" orchestration), "<<errors<<" mismatches\n";
    std::cout<<"Runtime retained capacity bound observed: "<<retained<<" bytes; largest output text "<<line_chars<<" UTF-16 units; largest output batch "<<batch<<" records\n";
    std::cout<<"Measured interpreter heap peak "<<allocation_probe::peak<<" bytes, largest allocation "<<allocation_probe::largest<<" bytes; output text capacity "<<line_capacity<<" UTF-16 units\n";
    return errors?1:0;
}
