// H-178: ULTIMA.EXE 0x69e1-0x6a4b, called by CMDS.OVL 0x005e.
#include "../main/alpha_runtime.h"
#include "openu5/rest.h"
#include <cstdio>
#include <memory>
#include <initializer_list>
#include <vector>
using namespace openu5;
namespace {
const tdeck::AlphaResourceOwners *pack;
int checks=0, failures=0;
void check(bool ok,const char *label){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"GREEN":"RED",label);}
struct Draw {int lo,hi,value,hour,minute;char status[4];uint8_t ring[4];uint16_t hp[4];};
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    std::vector<Draw> draws;
    int64_t us=0;
    explicit Run(int seed,int members=2){
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world=pack->world;f.npc_locations=pack->npc_locations;f.pack=pack;
        f.location_x=pack->location_x;f.location_y=pack->location_y;
        f.location_count=pack->location_count;
        rt->attach_host_test_fixture(f);
        auto &s=g();s.position.map={0,0};s.position.xy={80,80};
        s.time.hour=5;s.time.minute=0;s.food=80;
        s.party.character_count=s.party.party_size=members;
        s.party.active_character=255;
        for(int i=0;i<members;++i){auto &m=s.party.characters[i];
            std::snprintf(m.name,sizeof(m.name),"Member%d",i);
            m.status='G';m.current_hp=m.max_hp=30;m.level=1;m.exp=0;
            m.strength=m.dexterity=m.intelligence=20;m.ring=255;
        }
        s.rng.seed(seed);
        rt->command_context_for_test().rng_trace={this,[](void *p,const char *,int32_t lo,int32_t hi,int32_t value){
            auto &r=*static_cast<Run*>(p);auto &g=r.g();Draw d{};
            d.lo=lo;d.hi=hi;d.value=value;d.hour=g.time.hour;d.minute=g.time.minute;
            for(int i=0;i<4;++i){d.status[i]=g.party.characters[i].status;
                d.ring[i]=g.party.characters[i].ring;d.hp[i]=g.party.characters[i].current_hp;}
            r.draws.push_back(d);
        }};
    }
    GameState &g(){return rt->game();}
    void key(uint8_t code){tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.transition=tdeck::KeyTransition::Pressed;e.code=code;e.timestamp_us=(us+=100000);rt->handle(e);}
    void camp(int guard=-1,int hours=1){key('h');key(uint8_t('0'+hours));key('\r');
        if(rt->ui()->request()==UiRequestId::CampWatch){key(guard<0?'n':'y');if(guard>=0)key(uint8_t('1'+guard));}
        for(int i=0;i<12&&rt->ui()->mode()==UiMode::KeyWait;++i)key(' ');
    }
};
bool sequence(const Run &r,int seed,std::initializer_list<int> bounds){
    if(r.draws.size()<bounds.size())return false;
    OriginalRng oracle(seed);size_t i=0;
    for(const int hi:bounds){const auto &d=r.draws[i++];
        if(d.lo!=0||d.hi!=hi||d.value!=oracle.next(0,hi).value||
           d.hour!=5||d.minute!=0)return false;}
    return true;
}
int count_range(const Run &r,int lo,int hi){int n=0;for(const auto &d:r.draws)if(d.lo==lo&&d.hi==hi)++n;return n;}
}
int main(int argc,char **argv){
    if(argc<2)return 2;
    tdeck::AlphaResourcePack source;tdeck::AlphaResourceReport report{};
    if(source.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners owners{};
    if(source.load(owners,report)!=ESP_OK)return 2;
    pack=&owners;
    Run none(1);none.camp();
    check(sequence(none,1,{63})&&none.draws[0].status[0]=='S'&&count_range(none,0,15)==0,"no ring: first Camp draw is wind");
    Run invis(1);invis.g().party.characters[0].ring=42;invis.camp();
    check(sequence(invis,1,{15,63})&&invis.draws[0].status[0]=='G'&&invis.draws[1].status[0]=='S'&&invis.g().party.characters[0].ring==42,
          "invisibility ring: one entry draw before sleep and wind");
    check(count_range(invis,0,7)==0&&invis.g().party.characters[0].current_hp==30,
          "invisibility ring adds no regeneration draw or entry healing");
    Run regen(1);regen.g().party.characters[0].ring=44;regen.camp();
    check(sequence(regen,1,{15,7,63})&&regen.draws[1].status[0]=='G'&&regen.draws[2].status[0]=='S'&&count_range(regen,0,7)==13,
          "full-HP ring rolls expiry and party regeneration at entry, then each step");
    Run dual(1);dual.g().party.characters[0].ring=44;
    dual.g().party.characters[1].ring=44;
    dual.g().party.characters[1].current_hp=5;dual.camp();
    check(sequence(dual,1,{15,7,7,15,7,7,63})&&
          dual.draws[6].hp[1]==6&&dual.draws[6].hp[0]==30,
          "two wearers interleave expiry with full-party regeneration and heal at entry");
    check(count_range(dual,0,7)==28,
          "two entry regeneration passes add four rolls before 24 Camp-step rolls");
    Run sleeping(1);sleeping.g().party.characters[0].status='S';
    sleeping.g().party.characters[0].ring=44;
    sleeping.g().party.characters[1].ring=44;sleeping.camp();
    check(sequence(sleeping,1,{15,15,7,7,63})&&count_range(sleeping,0,7)==26,
          "already-sleeping wearer skips its own trigger but joins the other party pass");
    Run pair(72);pair.g().party.characters[0].ring=42;
    pair.g().party.characters[1].ring=44;pair.camp();
    check(sequence(pair,72,{15,15,7,63})&&pair.g().party.characters[0].ring==255&&
          pair.g().party.characters[1].ring==44,
          "roster order: slot zero expires on roll 11, slot one retains its ring");
    Run reversed(72);reversed.g().party.characters[0].ring=44;
    reversed.g().party.characters[1].ring=42;reversed.camp();
    check(sequence(reversed,72,{15,15,63})&&reversed.g().party.characters[0].ring==255&&
          reversed.g().party.characters[1].ring==42&&count_range(reversed,0,7)==0,
          "expired regeneration ring has no later five-minute rolls");
    Run dead(1,3);dead.g().party.characters[0].status='D';dead.g().party.characters[0].ring=42;
    dead.g().party.characters[1].ring=44;dead.camp();
    check(sequence(dead,1,{15,7,63})&&dead.g().party.characters[0].ring==42,
          "dead ring skips entry draw but the live wearer still regenerates");
    Run poisoned(1);poisoned.g().party.characters[0].status='P';
    poisoned.g().party.characters[0].ring=42;poisoned.camp();
    check(poisoned.draws.size()>1&&poisoned.draws[0].lo==0&&
          poisoned.draws[0].hi==15&&poisoned.draws[0].status[0]=='P',
          "poisoned member still rolls at scene entry");
    Run guard(1);guard.g().party.characters[1].ring=42;guard.camp(1);
    check(guard.draws.size()>1&&guard.draws[0].lo==0&&
          guard.draws[0].hi==15&&guard.draws[0].status[1]=='G',
          "posted guard still rolls at scene entry");
    Run guard_regen(1);guard_regen.g().party.characters[1].ring=44;
    guard_regen.camp(1);
    check(sequence(guard_regen,1,{15,7,63})&&
          guard_regen.draws[0].status[1]=='G'&&count_range(guard_regen,0,7)==13,
          "posted guard's regeneration ring triggers a party pass at entry");
    Run protection(1);protection.g().party.characters[0].ring=43;protection.camp();
    check(sequence(protection,1,{63})&&count_range(protection,0,15)==0,
          "protection ring has no entry expiry draw");
    Run refused(1);refused.g().party.characters[0].ring=42;
    refused.g().transport=TransportMode::Horse;refused.camp();
    check(count_range(refused,0,15)==0&&refused.g().party.characters[0].ring==42,
          "rejected Camp consumes no entry ring draw");
    Run ambush(87);ambush.g().party.characters[0].ring=42;ambush.camp(-1,2);
    bool encounter=false;
    for(const auto &d:ambush.draws)if(d.hour==6&&d.minute==0&&
        d.lo==0&&d.hi==63&&d.value==0)encounter=true;
    check(sequence(ambush,87,{15,63})&&encounter&&ambush.g().time.hour==6&&
          ambush.g().time.minute==0,
          "accepted Camp consumes entry draw before the later hourly ambush");
    Run gate_plain(2);gate_plain.g().party.characters[0].exp=100;gate_plain.camp();
    Run gate_ring(2);gate_ring.g().party.characters[0].exp=100;
    gate_ring.g().party.characters[0].ring=42;gate_ring.camp();
    check(gate_plain.g().party.characters[0].level==2&&
          gate_ring.g().party.characters[0].level==1,
          "seed 2: entry ring draw changes the later apparition gate");
    Run stat_plain(87);stat_plain.g().party.characters[0].exp=100;stat_plain.camp();
    Run stat_ring(87);stat_ring.g().party.characters[0].exp=100;
    stat_ring.g().party.characters[0].ring=42;stat_ring.camp();
    check(stat_plain.g().party.characters[0].level==2&&
          stat_ring.g().party.characters[0].level==2&&
          stat_plain.g().party.characters[0].dexterity==21&&
          stat_ring.g().party.characters[0].intelligence==21,
          "seed 87: both gates hit but the advancement stat changes");
    check(none.g().rng.get_seed()==5052&&invis.g().rng.get_seed()==1689&&
          regen.g().rng.get_seed()==58813&&pair.g().rng.get_seed()==47350,
          "seed 1/72 final RNG states preserve entry and per-step draw counts");
    check(gate_plain.g().rng.get_seed()==3458&&gate_ring.g().rng.get_seed()==3458&&
          stat_plain.g().rng.get_seed()==40482&&stat_ring.g().rng.get_seed()==54358,
          "seed 2/87 final RNG states preserve gate and stat draw order");
    std::printf("ENTRY seed1 ring42=%d first_wind=%d; seed72 pair=%d,%d first_wind=%d\n",
        invis.draws[0].value,invis.draws[1].value,
        pair.draws[0].value,pair.draws[1].value,pair.draws[3].value);
    std::printf("RNG gate seed2 plain=%u ring=%u; stat seed87 plain=%u ring=%u\n",
        unsigned(gate_plain.g().rng.get_seed()),unsigned(gate_ring.g().rng.get_seed()),
        unsigned(stat_plain.g().rng.get_seed()),unsigned(stat_ring.g().rng.get_seed()));
    std::printf("RNG no-ring=%u invis=%u regen=%u pair=%u\n",
        unsigned(none.g().rng.get_seed()),unsigned(invis.g().rng.get_seed()),
        unsigned(regen.g().rng.get_seed()),unsigned(pair.g().rng.get_seed()));
    std::printf("Batch 44 ring entry: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
