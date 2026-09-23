// H-167: the 1988 kernel's hours -> optional watch -> one guard -> camp path.
// Runs the shipped arena and the production AlphaRuntime raw-key dispatcher.
#include "../main/alpha_runtime.h"
#include "openu5/rest.h"
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
    struct Draw {int lo,hi,value;};
    std::vector<Draw> draws;
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    int64_t us=0; size_t mark=0;
    Run(int members=2) {
        tdeck::AlphaRuntime::HostTestFixture f;
        f.world=pack->world;f.location_x=pack->location_x;f.location_y=pack->location_y;
        f.location_count=pack->location_count;f.npc_locations=pack->npc_locations;f.pack=pack;
        rt->attach_host_test_fixture(f);
        auto &g=rt->game();g.party.character_count=g.party.party_size=members;
        for(int i=0;i<members;++i){auto &m=g.party.characters[i];
            std::snprintf(m.name,sizeof(m.name),"Member%d",i);
            m.status='G';m.current_hp=100;m.max_hp=900;m.strength=30;m.dexterity=30;}
        g.party.active_character=255;g.time.hour=12;g.time.minute=0;g.food=80;
        g.position.map={0,0};g.position.xy={80,80};g.rng.seed(1);
        ctx().rng_trace={this,[](void*p,const char*,int32_t lo,int32_t hi,int32_t value){
            static_cast<Run*>(p)->draws.push_back({lo,hi,value});}};
    }
    GameState &g(){return rt->game();}
    const UiSession &ui(){return *rt->ui();}
    CommandContext &ctx(){return rt->command_context_for_test();}
    void key(uint8_t c){tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.code=c;e.transition=tdeck::KeyTransition::Pressed;e.timestamp_us=(us+=100000);rt->handle(e);}
    bool saw(const char *s){for(size_t i=mark;i<ui().transcript_size();++i)
        if(auto *b=ui().transcript_at(i);b&&std::strstr(b->text,s))return true;return false;}
    void start(){mark=ui().transcript_size();key('h');key('1');key('\r');}
};
void watched(){
    Run h;h.mark=h.ui().transcript_size();h.key('h');
    check(std::strstr(h.ui().prompt(),"For how many hours? (1-9)")!=nullptr&&
          h.saw("Hole up & camp!"),"W0","original camp heading and hours text");
    h.key('1');h.key('\r');
    check(h.ui().mode()==UiMode::YesNo&&h.ui().request()==UiRequestId::CampWatch,
          "W1","two ready members reach watch question after hours");
    check(std::strstr(h.ui().prompt(),"Wilt thou set a watch?")!=nullptr,
          "W2","original watch text");
    check(h.g().time.hour==12&&h.g().time.minute==0,"W3","prompt consumes no time or RNG");
    check(h.g().rng.get_seed()==1&&h.draws.empty(),"W3a","hours prompt has no RNG draw");
    h.key('y');
    check(h.ui().mode()==UiMode::PartySelection&&std::strstr(h.ui().prompt(),"Who will stand guard?")!=nullptr,
          "W4","yes enters member picker");
    check(h.g().rng.get_seed()==1&&h.draws.empty(),"W4a","watch choice has no RNG draw");
    h.key('2');
    check(h.g().time.hour==13&&h.g().time.minute==0,"W5","one hour camp completes");
    check(h.ui().mode()==UiMode::Exploration,"W6","returns to world command state");
    check(h.g().party.characters[1].current_hp==100,"W7","watcher excluded from partial heal");
    check(h.saw("Party rested!"),"W8","camp wake result appears");
    int watch_draws=0, heal_at=-1, apparition_at=-1, apparition_gates=0;
    OriginalRng oracle(1); bool replay=true;
    for(size_t i=0;i<h.draws.size();++i){const auto d=h.draws[i];
        if(d.lo==0&&d.hi==3)++watch_draws;
        if(d.lo==1&&d.hi==63&&heal_at<0)heal_at=int(i);
        if(d.lo==0&&d.hi==99){apparition_at=int(i);++apparition_gates;}
        replay &= oracle.next(d.lo,d.hi).value==d.value;}
    check(watch_draws>=12&&watch_draws<=24&&heal_at>=watch_draws&&apparition_at>heal_at,
          "W9","per-step watch rolls precede partial heal and apparition gate");
    check(replay&&oracle.get_seed()==h.g().rng.get_seed(),"W10","draw trace replays original seeded RNG exactly");
    check(apparition_gates==1,"W11","camp completion and apparition gate run once");
}
void invalid_and_cancel(){
    Run h;h.g().party.characters[1].status='P';h.start();
    check(h.ui().mode()==UiMode::YesNo,"I1","G plus P still offers watch");
    h.key('y');h.key('2');
    check(h.saw("None posted!"),"I2","poisoned watcher is declined once");
    check(h.g().time.hour==13&&h.ui().mode()==UiMode::Exploration,"I3","invalid choice still camps without guard");
    check(h.g().party.characters[1].current_hp>100,"I4","invalid watcher receives ordinary partial heal");
    Run c;c.start();c.key('y');c.key(27);
    check(c.saw("None posted!"),"C1","picker cancel posts nobody");
    check(c.g().time.hour==13&&c.ui().mode()==UiMode::Exploration,"C2","picker cancel still camps");
    Run n;n.start();n.key('n');
    check(n.g().time.hour==13&&n.ui().mode()==UiMode::Exploration,"N1","No camps without picker");
    check(!n.saw("None posted!"),"N2","No does not report a failed guard");
    int n_watch=0;for(const auto d:n.draws)if(d.lo==0&&d.hi==3)++n_watch;
    check(n_watch==0,"N3","unwatched camp consumes no watch RNG");
}
void no_choice_and_bed(){
    Run h(1);h.start();
    check(h.ui().mode()==UiMode::Exploration&&h.g().time.hour==13,"S1","one ready member bypasses watch question");
    check(!h.saw("Wilt thou set a watch?"),"S2","no optional watch prompt for one member");
}
void hours_cancel(){
    for(const auto code:{uint8_t('0'),uint8_t(' '),uint8_t(27)}){
        Run h;h.key('h');h.key(code);
        check(h.ui().mode()==UiMode::Exploration&&h.g().time.hour==12&&
              h.g().rng.get_seed()==1&&h.draws.empty(),"C3",
              "zero, Space and Escape cancel before watch without time or RNG");
    }
}
void arena_services(){
    Run h;
    const auto *s=h.ctx().rest_services;
    const auto *arena=pack->combat_map_views[0];
    check(s&&s->guard_start&&s->cell_free&&arena&&arena->start_count[2]>=2,
          "A1","production services bind shipped CampFire south starts");
    if(!s||!s->guard_start||!s->cell_free||!arena)return;
    const auto guard=s->guard_start(s->context,1);
    const auto p=arena->starts[2][1], sleeper=arena->starts[2][0];
    check(guard.present&&guard.col==p.x&&guard.row==p.y,"A2","guard starts at authored south[1]");
    check(!s->cell_free(s->context,1,sleeper.x,sleeper.y),"A3","live sleeper blocks guard");
    check(s->cell_free(s->context,1,p.x,p.y),"A4","guard does not block own post");
    check(!s->cell_free(s->context,1,5,5)&&arena->tiles[5*11+5]==179,
          "A5","authored campfire is not walkable");
    int rolls[]={0,2,1};
    Rand into_fire{rolls,[](void*p,int32_t,int32_t){auto*a=static_cast<int*>(p);return a[++a[0]];}};
    const auto stopped=camp_guard_walk({5,4,true},into_fire,*s,1);
    check(stopped.col==5&&stopped.row==4,"A5a","actual guard step cannot enter the fire");
    h.g().party.characters[0].status='D';
    check(s->cell_free(s->context,1,sleeper.x,sleeper.y),"A6","dead member leaves no sleeper actor");
}
}
int main(int argc,char **argv){
    if(argc<2)return 2;
    tdeck::AlphaResourcePack p;tdeck::AlphaResourceReport report{};
    if(p.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners owners{};
    if(p.load(owners,report)!=ESP_OK)return 2;pack=&owners;
    watched();invalid_and_cancel();no_choice_and_bed();hours_cancel();arena_services();
    std::printf("H-167: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
