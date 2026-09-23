// H-169: CMDS.OVL 0x05b4-0x05d4 calls NPC.OVL 0x0db4 up to 16 times,
// redraws after each pass, and returns before sleep on action marker 'a'.
#include "../main/alpha_runtime.h"
#include "openu5/actors.h"
#include <cstdio>
#include <cstring>
#include <memory>

using namespace openu5;
namespace {
int checks=0, failures=0;
void check(bool ok,const char *id,const char *why){++checks;if(!ok)++failures;std::printf("%s %s %s\n",ok?"GREEN":"RED",id,why);}
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    int64_t us=0;
    explicit Run(const tdeck::AlphaResourceOwners &pack,uint8_t location,int hour=12) {
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world=pack.world;f.npc_locations=pack.npc_locations;f.pack=&pack;
        f.location_x=pack.location_x;f.location_y=pack.location_y;f.location_count=pack.location_count;
        rt->attach_host_test_fixture(f);
        auto &g=rt->game();g.position.map={0,0};
        g.position.xy={pack.location_x[location-1],pack.location_y[location-1]};
        g.time.hour=hour;g.time.minute=0;g.food=100;
        g.party.party_size=g.party.character_count=1;
        g.party.characters[0].status='G';g.party.characters[0].current_hp=100;
        g.party.characters[0].max_hp=100;key('e');
    }
    void key(uint8_t code){tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.transition=tdeck::KeyTransition::Pressed;e.code=code;e.timestamp_us=(us+=100000);rt->handle(e);}
    void hole(){key('h');key('1');key('\r');}
    NpcActor *npc(uint8_t slot){auto &a=rt->actors();for(size_t i=0;i<a.count;++i)
        if(a.actors[i].schedule.slot==slot)return &a.actors[i];return nullptr;}
};
struct Observer {
    Run *run;EventSink downstream;int maps_before_sleep=0;bool slept=false;int watched_slot=0,first_x=-1,first_y=-1;
    int sleep_hour=-1,sleep_minute=-1,sleep_turns=-1,sleep_status=0;
    int draws_before_sleep=0,first_lo=-1,first_hi=-1;
    static void emit(void *p,const GameEvent &e){auto &o=*static_cast<Observer*>(p);
        if(e.kind==GameEventKind::Message&&e.text&&std::strstr(e.text,"Zzzzzzz")){
            o.slept=true;const auto &g=o.run->rt->game();o.sleep_hour=g.time.hour;
            o.sleep_minute=g.time.minute;o.sleep_turns=int(g.turns_since_start);
            o.sleep_status=g.party.characters[0].status;
        }
        if(e.kind==GameEventKind::MapChanged&&!o.slept){
            ++o.maps_before_sleep;
            if(o.maps_before_sleep==1&&o.watched_slot){
                if(auto *n=o.run->npc(uint8_t(o.watched_slot))){o.first_x=n->x;o.first_y=n->y;}
            }
        }
        if(o.downstream.emit)o.downstream.emit(o.downstream.context,e);
    }
    void attach(){auto &c=run->rt->command_context_for_test();downstream=c.events;c.events={this,emit};
        c.rng_trace={this,[](void*p,const char*,int32_t lo,int32_t hi,int32_t){
            auto &o=*static_cast<Observer*>(p);if(o.slept)return;
            if(o.draws_before_sleep++==0){o.first_lo=lo;o.first_hi=hi;}
        }};
    }
};
}
int main(int argc,char **argv){if(argc<2)return 2;
    tdeck::AlphaResourcePack p;tdeck::AlphaResourceReport report{};
    if(p.open(argv[1],report)!=ESP_OK)return 2;static tdeck::AlphaResourceOwners pack{};
    if(p.load(pack,report)!=ESP_OK)return 2;
    const auto map=get_active_map(pack.world,{13,0});
    check(map.error==Error::None&&map.value.tile_at(12,14)==171,"A1","Iolo's Hut authored LeftBed");
    const auto &slot=pack.npc_locations[12].slots[1];
    check(slot.slot==1&&slot.ai[0]==6&&slot.ai[1]==6&&slot.ai[2]==6,"A2","authored hostile on ground floor");
    Run h(pack,13);auto &g=h.rt->game();auto *npc=h.npc(1);
    check(g.position.map.location==13&&npc&&npc->z==0,"A3","real Enter loads local map and NPC");
    if(!npc)return 1;
    // The authored hostile has walked next to the bed. No schedule/type/AI is changed.
    npc->x=12;npc->y=13;g.position.xy={12,14};
    Observer o{&h};o.attach();const auto turns=g.turns_since_start;
    h.hole();
    check(!o.slept&&o.maps_before_sleep==1,"C1","first NPC pass detects adjacent hostile and cancels before Zzz");
    check(g.time.hour==12&&g.time.minute==0&&g.turns_since_start==turns,"C2","cancelled sleep runs no ten-minute tick or housekeeping");
    check(g.party.characters[0].status=='G'&&g.position.xy.x==12&&g.position.xy.y==14,"C3","cancelled sleep never settles or ejects party");
    Run approach(pack,13);auto &ag=approach.rt->game();auto *hunter=approach.npc(1);
    if(hunter){
        hunter->x=13;hunter->y=15;ag.position.xy={12,14};
        Observer next{&approach};next.watched_slot=1;next.attach();
        approach.hole();
        check(!next.slept&&next.maps_before_sleep==2,"C4",
              "hostile moving into adjacency cancels on the following NPC pass");
        check((next.first_x==13&&next.first_y==14)||(next.first_x==12&&next.first_y==15),"C5",
              "first pass moves the authored hostile toward the bed");
        check(ag.time.hour==12&&ag.time.minute==0&&ag.party.characters[0].status=='G',"C6",
              "two-pass cancellation still precedes the first sleep tick");
    }
    // Synthetic marker-precedence control using two shipped Yew records:
    // the basement hostile is placed on floor 0 beside a higher-index talker.
    // This arrangement tests the shared marker, not a reachable schedule route.
    Run overwrite(pack,4,8);auto &og=overwrite.rt->game();auto *attack=overwrite.npc(15);
    auto *talk=overwrite.npc(20);
    check(og.position.map.location==4&&attack&&talk&&attack->schedule.ai[0]==6&&
          talk->schedule.ai[1]==4&&talk->schedule.dialog!=0,"O1","authored Yew hostile precedes a talker in slot order");
    if(attack&&talk){
        og.position.xy={29,19};attack->x=28;attack->y=19;attack->z=0;
        talk->x=29;talk->y=18;talk->z=0;
        Observer last{&overwrite};last.attach();overwrite.hole();
        check(last.slept&&last.maps_before_sleep==16,"O2","later talk marker overwrites earlier attack in every pass");
    }
    Run no_talker(pack,4,8);auto &ng=no_talker.rt->game();auto *only_attack=no_talker.npc(15);
    auto *distant_talk=no_talker.npc(20);
    if(only_attack&&distant_talk){
        ng.position.xy={29,19};only_attack->x=28;only_attack->y=19;only_attack->z=0;
        distant_talk->x=0;distant_talk->y=0;distant_talk->z=0;
        Observer first{&no_talker};first.attach();no_talker.hole();
        check(!first.slept&&first.maps_before_sleep==1,"O3","without later talk overwrite, same hostile cancels on pass one");
    }
    // A separate authored castle NPC starts a saved, in-progress corridor walk.
    // Its first east step must be visible in the first pre-sleep redraw, before
    // Zzz or any clock/housekeeping tick. The later sleep snap may move it back.
    Run quiet(pack,17);auto &q=quiet.rt->game();auto *walker=quiet.npc(5);
    check(q.position.map.location==17&&walker&&walker->schedule.ai[1]==2,"W1","authored castle walker loaded");
    if(walker){
        q.position.xy={9,7};walker->x=13;walker->y=4;walker->z=0;
        walker->state=2;walker->path_index=0;walker->path[0]=1;walker->path[1]=1;
        q.rng.seed(1);Observer watch{&quiet};watch.watched_slot=5;watch.attach();const auto start=q.turns_since_start;
        quiet.hole();
        check(watch.first_x==14&&watch.first_y==4,"W2","first NPC pass executes the authored walk before sleep");
        check(watch.maps_before_sleep==16&&watch.slept,"W3","exactly 16 redraws precede Zzz on an uninterrupted bed sleep");
        check(watch.sleep_hour==12&&watch.sleep_minute==0&&watch.sleep_turns==int(start),"W4","pre-sleep passes do not advance clock or run housekeeping");
        check(watch.sleep_status=='S',"W5","party is in sleeping state before Zzz, after NPC passes");
        check(q.time.hour==13&&q.turns_since_start==start+6&&q.position.xy.x==10,"W6","six restored bed ticks and common east-step epilogue remain");
        check(watch.draws_before_sleep==74&&watch.first_lo==0&&watch.first_hi==255,"W7",
              "seeded original-roster passes consume movement RNG before sleep in slot order");
    }
    // The 16-pass loop belongs only to the accepted local-bed command.
    Run camp(pack,17);camp.rt->game().position.map={0,0};camp.rt->game().position.xy={80,80};
    Observer outdoors{&camp};outdoors.attach();camp.hole();
    check(outdoors.maps_before_sleep==0,"S1","Camp does not receive the bed pre-sleep NPC passes");
    Run cancel(pack,17);cancel.rt->game().position.xy={9,7};
    Observer aborted{&cancel};aborted.attach();cancel.key('h');cancel.key('0');
    check(aborted.maps_before_sleep==0&&cancel.rt->game().time.hour==12,"S2","zero-hour cancel has no NPC pass or clock step");
    std::printf("batch34_pre_sleep_npc: %d/%d GREEN, %d RED\n",checks-failures,checks,failures);
    return failures?1:0;
}
