// H-173: CMDS.OVL 0x0066-0x0079 stores a wrapped target hour;
// 0x01ee-0x01f8 checks it before each five-minute Camp iteration.
#include "../main/alpha_runtime.h"
#include <cstdio>
#include <memory>
#include <vector>

using namespace openu5;
namespace {
const tdeck::AlphaResourceOwners *pack;
int checks=0, failures=0;
void check(bool ok,const char *id,const char *why){++checks;if(!ok)++failures;
    std::printf("%s %s %s\n",ok?"GREEN":"RED",id,why);}
struct Run {
    struct Draw {int lo,hi,hour,minute;};
    std::vector<Draw> draws;
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    int64_t us=0;
    Run(int hour,int minute,int members=1) {
        tdeck::AlphaRuntime::HostTestFixture f;
        f.world=pack->world;f.location_x=pack->location_x;f.location_y=pack->location_y;
        f.location_count=pack->location_count;f.npc_locations=pack->npc_locations;f.pack=pack;
        rt->attach_host_test_fixture(f);
        auto &s=g();s.party.character_count=s.party.party_size=members;
        for(int i=0;i<members;++i){auto &m=s.party.characters[i];
            std::snprintf(m.name,sizeof(m.name),"Member%d",i);
            m.status='G';m.current_hp=100;m.max_hp=900;m.strength=30;m.dexterity=30;}
        s.party.active_character=255;s.time.hour=hour;s.time.minute=minute;
        s.food=80;s.turns_since_start=7;s.position.map={0,0};s.position.xy={80,80};
        s.rng.seed(12345);
        // One ring draw is made during each loop iteration, before clock advance.
        s.party.characters[0].ring=44;
        ctx().rng_trace={this,[](void*p,const char*,int32_t lo,int32_t hi,int32_t){
            auto &h=*static_cast<Run*>(p);const auto &s=h.g();
            h.draws.push_back({lo,hi,int(s.time.hour),int(s.time.minute)});}};
    }
    GameState &g(){return rt->game();}
    CommandContext &ctx(){return rt->command_context_for_test();}
    const UiSession &ui(){return *rt->ui();}
    void key(uint8_t c){tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.code=c;e.transition=tdeck::KeyTransition::Pressed;e.timestamp_us=(us+=100000);rt->handle(e);}
    void camp(int hours,bool watched=false){key('h');key(uint8_t('0'+hours));key('\r');
        if(watched){key('y');key('2');}}
    int count(int lo,int hi)const{int n=0;for(const auto &d:draws)if(d.lo==lo&&d.hi==hi)++n;return n;}
};
void case_one(int start_hour,int start_minute,int hours,int end_hour,int steps,const char *id,int end_minute=0){
    Run h(start_hour,start_minute);h.camp(hours);
    check(h.ui().mode()==UiMode::Exploration&&h.g().time.hour==end_hour&&
          h.g().time.minute==end_minute,id,"raw-key Camp stops at the original target hour");
    check(h.count(0,7)==steps,id,"ring rolls prove the exact five-minute step count");
    check(h.count(0,63)==steps+hours-1,id,
          "one wind draw per step and one encounter check per intervening hour");
    check(h.g().food==80&&h.g().turns_since_start==7,id,
          "duration does not introduce ordinary turn housekeeping");
    bool ordered=true;int n=0;
    for(size_t i=0;i<h.draws.size();++i)if(h.draws[i].lo==0&&h.draws[i].hi==7){
        const int total=start_minute+n*5;
        ordered &= h.draws[i].hour==(start_hour+total/60)%24&&
                   h.draws[i].minute==total%60;
        ++n;
    }
    check(ordered&&n==steps,id,"every ring draw precedes its clock step, with no final redraw");
}
void watched_final_step(){
    Run h(12,55,2);h.camp(1,true);
    check(h.g().time.hour==13&&h.g().time.minute==0&&h.count(0,7)==1,
          "W0","watched Camp completes after the single boundary step");
    int guard_at_target=0;for(const auto &d:h.draws)
        if(d.lo==0&&d.hi==3&&d.hour==13&&d.minute==0)++guard_at_target;
    check(guard_at_target>=1,"W1","posted guard moves after the final clock advance");
    check(h.count(1,63)==1&&h.count(0,99)==1,"W2",
          "completion healing and apparition gate run once after the final guard move");
}
}
int main(int argc,char **argv){if(argc<2)return 2;
    tdeck::AlphaResourcePack p;tdeck::AlphaResourceReport report{};
    if(p.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners owners{};
    if(p.load(owners,report)!=ESP_OK)return 2;pack=&owners;
    case_one(12,0,1,13,12,"Z");
    case_one(12,5,1,13,11,"N05");
    case_one(12,1,1,13,12,"N01",1);
    case_one(12,55,1,13,1,"N55");
    case_one(5,50,1,6,2,"N50");
    case_one(23,55,1,0,1,"M1");
    case_one(23,5,2,1,23,"M2");
    case_one(12,5,2,14,23,"N2");
    watched_final_step();
    std::printf("H-173: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
