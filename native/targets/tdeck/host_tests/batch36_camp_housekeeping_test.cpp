// H-171: CMDS.OVL 0x0204/07/0a, 0x0314/1b. The redraw's wind roll
// precedes ring regeneration and the five-minute advance; 0x2900 is a
// status redraw and 0x20fa is a delay, not kernel 0x2ae8 housekeeping.
#include "../main/alpha_runtime.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using namespace openu5;
namespace {
const tdeck::AlphaResourceOwners *pack;
int checks=0, failures=0;
void check(bool ok,const char *id,const char *why){++checks;if(!ok)++failures;std::printf("%s %s %s\n",ok?"GREEN":"RED",id,why);}
struct Run {
    struct Draw {int lo,hi,value,hour,minute,hp,food,turns;};
    std::vector<Draw> draws;
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    int64_t us=0;
    Run(int members=2) {
        tdeck::AlphaRuntime::HostTestFixture f;
        f.world=pack->world;f.location_x=pack->location_x;f.location_y=pack->location_y;
        f.location_count=pack->location_count;f.npc_locations=pack->npc_locations;f.pack=pack;
        rt->attach_host_test_fixture(f);
        auto &s=g();s.party.character_count=s.party.party_size=members;
        for(int i=0;i<members;++i){auto &m=s.party.characters[i];
            std::snprintf(m.name,sizeof(m.name),"Member%d",i);
            m.status='G';m.current_hp=100;m.max_hp=900;m.strength=30;m.dexterity=30;}
        s.party.active_character=255;s.time.hour=5;s.time.minute=0;s.food=80;
        s.turns_since_start=7;s.position.map={0,0};s.position.xy={80,80};s.rng.seed(1);
        ctx().rng_trace={this,[](void*p,const char*,int32_t lo,int32_t hi,int32_t value){
            auto &h=*static_cast<Run*>(p);const auto &s=h.g();
            h.draws.push_back({lo,hi,value,int(s.time.hour),int(s.time.minute),
                               int(s.party.characters[0].current_hp),int(s.food),
                               int(s.turns_since_start)});}};
    }
    GameState &g(){return rt->game();}
    CommandContext &ctx(){return rt->command_context_for_test();}
    const UiSession &ui(){return *rt->ui();}
    void key(uint8_t c){tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.code=c;e.transition=tdeck::KeyTransition::Pressed;e.timestamp_us=(us+=100000);rt->handle(e);}
    void hours(int count=1){key('h');key(uint8_t('0'+count));key('\r');}
    void finish_camp(){for(int i=0;i<16&&ui().mode()==UiMode::KeyWait;++i)key(' ');}
};
void watched_cadence(){
    Run h;h.g().party.characters[0].status='P';h.g().party.characters[1].ring=44;
    h.hours();check(h.ui().request()==UiRequestId::CampWatch&&h.draws.empty(),
                    "C0","accepted hours reach watch question without a step or RNG");
    h.key('y');check(h.draws.empty(),"C1","watch picker adds no step or RNG");h.key('2');h.finish_camp();
    check(h.ui().mode()==UiMode::Exploration&&h.g().time.hour==6&&h.g().time.minute==0,
          "C2","watched hour completes at 06:00 and returns to command state");
    std::vector<size_t> wind,ring;
    for(size_t i=0;i<h.draws.size();++i){const auto &d=h.draws[i];
        if(d.lo==0&&d.hi==63)wind.push_back(i);
        if(d.lo==0&&d.hi==7)ring.push_back(i);}
    check(wind.size()==12,"C3","one redraw wind roll per five-minute step, no start or final pass");
    check(ring.size()==12,"C4","one ring roll per step, including the guard's ring");
    bool order=wind.size()==12&&ring.size()==12, survival=ring.size()==12;
    for(const auto index:ring){const auto &d=h.draws[index];survival &= d.hp==100&&d.food==80&&d.turns==7;}
    for(size_t step=0;step<wind.size()&&step<12;++step){
        const auto &d=h.draws[wind[step]];
        order &= ring.size()==12&&wind[step]<ring[step]&&
                 d.hour==5&&d.minute==int(step*5)&&
                 h.draws[ring[step]].hour==5&&h.draws[ring[step]].minute==int(step*5);
        bool watch_after=false;
        if(ring.size()==12)for(size_t k=ring[step]+1;k<h.draws.size();++k){
            const auto &w=h.draws[k];
            if(w.lo==0&&w.hi==3&&w.hour==(step==11?6:5)&&
               w.minute==int((step+1)*5%60)){watch_after=true;break;}
            if(k+1<h.draws.size()&&h.draws[k+1].lo==0&&h.draws[k+1].hi==63)break;
        }
        order &= watch_after;
    }
    check(order,"C5","each step orders wind then ring before clock, guard roll after clock");
    check(survival,"C6","poison, meal and turn counter stay idle throughout camp steps");
    check(h.g().food==80&&h.g().turns_since_start==7,
          "C7","06:00 completion adds no meal or final housekeeping pass");
    int heal_rolls=0;for(const auto &d:h.draws)if(d.lo==1&&d.hi==63)++heal_rolls;
    check(heal_rolls==1,"C8","watcher is excluded from the partial-heal roll");
}
void hourly_encounter_order(){
    int seed=0;
    for(int i=1;i<65536;++i){OriginalRng oracle(i);bool quiet=true;
        for(int step=0;step<12;++step){quiet &= oracle.next(0,63).value!=0;oracle.next(0,7);}
        quiet &= oracle.next(0,63).value!=0;oracle.next(0,7);
        quiet &= oracle.next(0,63).value!=0;
        if(quiet){seed=i;break;}
    }
    check(seed!=0,"E0","fixed original stream avoids the first hourly encounter");
    if(!seed)return;
    Run h(1);h.g().rng.seed(seed);h.g().party.characters[0].ring=44;h.hours(2);
    std::vector<Run::Draw> boundary;int wind_or_encounter=0,ring=0;
    for(const auto &d:h.draws){if(d.lo==0&&d.hi==63)++wind_or_encounter;
        if(d.lo==0&&d.hi==7)++ring;
        if(d.hour==6&&d.minute==0)boundary.push_back(d);}
    check(h.g().time.hour==7&&h.g().time.minute==0&&wind_or_encounter==25&&ring==24,
          "E1","two-hour Camp has 24 wind/ring steps and one hourly encounter roll");
    check(boundary.size()>=3&&boundary[0].lo==0&&boundary[0].hi==63&&
          boundary[1].lo==0&&boundary[1].hi==7&&
          boundary[2].lo==0&&boundary[2].hi==63,
          "E2","at 06:00 redraw wind then ring precede the hourly encounter roll");
}
void wind_change(){
    int seed=0,chosen=0;
    for(int i=1;i<65536;++i){OriginalRng oracle(i);
        if(oracle.next(0,63).value!=0)continue;
        const int candidate=oracle.next(0,4).value;if(candidate==0)continue;
        bool quiet=true;for(int step=1;step<12;++step)quiet &= oracle.next(0,63).value!=0;
        if(quiet){seed=i;chosen=candidate;break;}
    }
    check(seed!=0,"W0","fixed original RNG stream can trigger one Camp wind change");
    if(!seed)return;
    Run h(1);h.g().rng.seed(seed);h.ctx().turn.wind=0;h.ctx().turn.wind_drift_counter=13;h.hours();
    check(h.draws.size()>=2&&h.draws[0].lo==0&&h.draws[0].hi==63&&h.draws[0].value==0&&
          h.draws[1].lo==0&&h.draws[1].hi==4&&h.draws[1].value==chosen,
          "W1","first Camp redraw takes the original wind-change branch");
    check(h.ctx().turn.wind==chosen&&h.ctx().turn.wind_drift_counter==0,
          "W2","Camp redraw changes world wind and resets drift");
}
void entry_clears_time_spell(char spell,const char *id){
    Run h(1);h.ctx().turn.time_spell=spell;h.ctx().turn.spell_turns=2;h.hours();
    int wind=0;for(const auto &d:h.draws)if(d.lo==0&&d.hi==63)++wind;
    check(h.ctx().turn.time_spell==0&&h.ctx().turn.spell_turns==0,id,
          "accepted Camp clears Q/T and its finite counter before the loop");
    check(h.g().time.hour==6&&h.g().time.minute==0&&wind==12,id,
          "cleared Q/T gives twelve ordinary five-minute wind/clock steps");
    check(h.g().turns_since_start==7&&h.g().food==80,id,
          "spell clear is independent of turn housekeeping and meals");
}
void unwatched_cadence(){
    Run h(1);h.hours();
    int wind=0,watch=0;bool when=true;
    for(const auto &d:h.draws){if(d.lo==0&&d.hi==63){when&=d.hour==5&&d.minute==wind*5;++wind;}
        if(d.lo==0&&d.hi==3)++watch;}
    check(wind==12&&when,"U0","unwatched camp also redraws before every five-minute step");
    check(watch==0,"U1","unwatched camp adds no guard movement rolls");
    check(h.g().time.hour==6&&h.g().time.minute==0&&h.g().turns_since_start==7,
          "U2","unwatched completion has no housekeeping turn");
}
}
int main(int argc,char **argv){if(argc<2)return 2;
    tdeck::AlphaResourcePack p;tdeck::AlphaResourceReport report{};
    if(p.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners owners{};
    if(p.load(owners,report)!=ESP_OK)return 2;pack=&owners;
    watched_cadence();unwatched_cadence();wind_change();hourly_encounter_order();
    entry_clears_time_spell('Q',"Q");entry_clears_time_spell('T',"T");
    std::printf("H-171: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
