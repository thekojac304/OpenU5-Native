// H-170: CMDS.OVL 0x059e-0x05b0 chooses an hour; 0x063b compares that
// hour before each 0x0647 ten-minute advance. Q is Rel Tym Quickness, not
// a target timestamp. Exercise the shipped castle through raw (H)ole-up.
#include "../main/alpha_runtime.h"
#include "openu5/rest.h"
#include "openu5/world_terrain.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
using namespace openu5;
namespace {
int checks=0,failures=0;
void check(bool ok,const char *id,const char *why){++checks;if(!ok)++failures;std::printf("%s %s %s\n",ok?"GREEN":"RED",id,why);}
const tdeck::AlphaResourceOwners *owners;
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    int64_t us=0; RestServices original{},observer{}; EventSink downstream{};
    struct Tick {int hour,minute,turns,spell_turns;char spell;};
    std::vector<Tick> ticks; int prepasses=0;bool zzz=false,thrown=false,eject_first=false;
    Run(int hour,int minute){
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world=owners->world;f.npc_locations=owners->npc_locations;f.pack=owners;
        f.location_x=owners->location_x;f.location_y=owners->location_y;f.location_count=owners->location_count;
        rt->attach_host_test_fixture(f);
        auto &g=rt->game();g.position.map={0,0};
        g.position.xy={owners->location_x[16],owners->location_y[16]};
        g.time.hour=hour;g.time.minute=minute;g.food=100;
        g.party.party_size=g.party.character_count=1;
        g.party.characters[0].status='G';g.party.characters[0].current_hp=500;
        g.party.characters[0].max_hp=900;key('e');
    }
    void key(uint8_t code){tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.transition=tdeck::KeyTransition::Pressed;e.code=code;e.timestamp_us=(us+=100000);rt->handle(e);}
    bool free_bed(int endhour){
        auto &g=rt->game();auto *s=rt->command_context_for_test().rest_services;
        const int saved=g.time.hour;
        for(int y=0;y<32;++y)for(int x=0;x<31;++x){
            if(rt->command_context_for_test().terrain->raw(owners->world,{17,0},x,y)!=0xab)continue;
            bool free=true;
            for(int hour=saved;;hour=(hour+1)%24){g.time.hour=hour;s->snap_npcs(s->context);
                if(s->occupied(s->context,x,y,0)){free=false;break;}
                if(hour==endhour)break;}
            if(free){g.time.hour=saved;s->snap_npcs(s->context);g.position.xy={uint8_t(x),uint8_t(y)};return true;}
        }
        g.time.hour=saved;s->snap_npcs(s->context);return false;
    }
    void observe(){
        auto &c=rt->command_context_for_test();original=*c.rest_services;observer=original;observer.context=this;
        observer.snap_npcs=[](void *p){auto &r=*static_cast<Run*>(p);
            r.original.snap_npcs(r.original.context);
            auto &g=r.rt->game();auto &t=r.rt->command_context_for_test().turn;
            r.ticks.push_back({g.time.hour,g.time.minute,int(g.turns_since_start),t.spell_turns,t.time_spell});};
        observer.occupied=[](void *p,int x,int y,int z){auto &r=*static_cast<Run*>(p);
            return (r.eject_first&&r.ticks.size()==1) || r.original.occupied(r.original.context,x,y,z);};
        c.rest_services=&observer;downstream=c.events;c.events={this,[](void *p,const GameEvent &e){
            auto &r=*static_cast<Run*>(p);
            if(e.kind==GameEventKind::Message&&e.text&&std::strstr(e.text,"Zzzzzzz"))r.zzz=true;
            if(e.kind==GameEventKind::Message&&e.text&&std::strstr(e.text,"Thrown out of bed"))r.thrown=true;
            if(e.kind==GameEventKind::MapChanged&&!r.zzz)++r.prepasses;
            if(r.downstream.emit)r.downstream.emit(r.downstream.context,e);}};
    }
    void hole(int hours){key('h');key(uint8_t('0'+hours));key('\r');
        auto &c=rt->command_context_for_test();c.rest_services=&original;c.events=downstream;}
};
void scenario(int hour,int minute,int hours,int qturns,int endhour,int endminute,
              int ticks,const char *id){
    Run r(hour,minute);check(r.rt->game().position.map.location==17&&r.free_bed(endhour),id,"authentic castle and unoccupied bed through target hour");
    if(r.rt->game().position.map.location!=17)return;
    auto &g=r.rt->game();auto &t=r.rt->command_context_for_test().turn;
    if(qturns){t.time_spell='Q';t.spell_turns=qturns;}
    const auto turns=g.turns_since_start;r.observe();r.hole(hours);
    check(r.zzz&&r.prepasses==16,id,"16 NPC passes before Zzz and the first tick");
    check(g.time.hour==endhour&&g.time.minute==endminute,id,"sleep stops at original target hour and minute reached by tick");
    check(int(r.ticks.size())==ticks&&g.turns_since_start==turns+ticks,id,"one housekeeping call and NPC snap per tick");
    check(!r.ticks.empty()&&r.ticks.back().hour==endhour&&r.ticks.back().minute==endminute,id,"target tick completes housekeeping and NPC snap before exit");
    if(qturns)check(t.time_spell==0&&t.spell_turns==0,id,"Q expires through shared housekeeping, including target tick");
    if(qturns==2&&r.ticks.size()>=2)
        check(r.ticks[0].spell=='Q'&&r.ticks[0].spell_turns==1&&
              r.ticks[1].spell==0&&r.ticks[1].spell_turns==0,id,
              "Q remains active through first tick and expires in second housekeeping call");
    if(hour==23&&r.ticks.size()>=2)
        check(r.ticks[0].hour==23&&r.ticks[0].minute==55&&
              r.ticks[1].hour==0&&r.ticks[1].minute==0,id,
              "live clock wraps 23 to 00 while target remains 01");
}
}
int main(int argc,char **argv){if(argc<2)return 2;
    tdeck::AlphaResourcePack p;tdeck::AlphaResourceReport report{};
    if(p.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners pack{};if(p.load(pack,report)!=ESP_OK)return 2;owners=&pack;
    scenario(12,0,1,2,13,0,7,"Q-middle");
    scenario(12,50,1,2,13,0,2,"Q-boundary");
    scenario(12,55,1,1,13,0,1,"Q-expiry-at-target");
    scenario(23,50,1,2,1,0,8,"Q-midnight-wrap-23");
    scenario(19,55,1,1,20,0,1,"Q-terrain-hour");
    scenario(12,37,1,0,13,7,3,"minute-not-compared");
    {
        Run r(12,55);check(r.free_bed(13),"Q-eject","free bed before target tick");
        auto &g=r.rt->game();auto &t=r.rt->command_context_for_test().turn;
        t.time_spell='Q';t.spell_turns=1;r.eject_first=true;r.observe();r.hole(1);
        check(r.thrown&&r.ticks.size()==1&&g.time.hour==13&&g.time.minute==0,"Q-eject",
              "target tick can eject after clock, housekeeping and NPC snap");
        check(t.time_spell==0&&t.spell_turns==0,"Q-eject",
              "Q expires on the same tick as bed ejection");
    }
    std::printf("batch35_q_sleep_target: %d/%d GREEN, %d RED\n",checks-failures,checks,failures);
    return failures?1:0;
}
