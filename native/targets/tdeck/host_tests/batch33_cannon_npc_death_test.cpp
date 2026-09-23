// H-168: cannon death must clear the live NPC slot on the hit, as
// CMDS.OVL:0x0d47-0x0d82 does through TOWN.OVL:0x0052 and 0x00b0.
#include "../main/alpha_runtime.h"
#include "openu5/actors.h"
#include "openu5/transitions.h"
#include <cstdio>
#include <memory>

using namespace openu5;
namespace {
int checks=0, failures=0;
void check(bool ok,const char *id,const char *why){++checks;if(!ok)++failures;std::printf("%s %s %s\n",ok?"GREEN":"RED",id,why);}
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt=std::make_unique<tdeck::AlphaRuntime>();
    int64_t us=0;
    explicit Run(const tdeck::AlphaResourceOwners &pack){
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world=pack.world;f.npc_locations=pack.npc_locations;f.pack=&pack;
        f.location_x=pack.location_x;f.location_y=pack.location_y;f.location_count=pack.location_count;
        rt->attach_host_test_fixture(f);
        auto &g=rt->game();g.position.map={25,0};g.position.xy={12,12};
        g.time.hour=7;g.time.minute=0;g.karma=50;g.party.party_size=g.party.character_count=1;
        g.party.characters[0].status='G';g.party.characters[0].current_hp=100;
        g.party.characters[0].max_hp=100;g.food=100;
        rt->turn().transport_tile=28;
        const auto &data=pack.npc_locations[24];
        enter_npc_map(rt->actors(),data.slots,data.count,25,7,g.npc_dead[24]);
    }
    void key(uint8_t code){tdeck::RawInputEvent e{};e.kind=tdeck::RawInputKind::Keyboard;
        e.transition=tdeck::KeyTransition::Pressed;e.code=code;e.timestamp_us=(us+=100000);rt->handle(e);}
    void ball(tdeck::RawInputKind kind){tdeck::RawInputEvent e{};e.kind=kind;
        e.transition=tdeck::KeyTransition::Pressed;e.timestamp_us=(us+=100000);rt->handle(e);}
    void fire(){key('f');ball(tdeck::RawInputKind::TrackballUp);key('\r');}
    // This is the same load primitive and reload callback the command runner
    // uses on local-map entry. The callback reads the shipped .NPC table.
    void reload(uint8_t id=25){auto &c=rt->command_context_for_test();
        TransitionServices s{&c,[](void*p,ReloadEffect e,uint8_t id){auto &c=*static_cast<CommandContext*>(p);
            if(c.services.reload)c.services.reload(c.services.context,e,id,c.events);},nullptr};
        load_small_map(rt->game(),rt->turn(),rt->travel(),id,nullptr,s);
    }
};
}
int main(int argc,char **argv){
    if(argc<2)return 2;
    tdeck::AlphaResourcePack p;tdeck::AlphaResourceReport report{};
    if(p.open(argv[1],report)!=ESP_OK)return 2;
    static tdeck::AlphaResourceOwners pack{};
    if(p.load(pack,report)!=ESP_OK)return 2;
    const auto map=get_active_map(pack.world,{25,0});
    check(map.error==Error::None && map.value.tile_at(12,11)==0xb4,"A1","shipped Ararat north cannon at (12,11)");
    check(pack.npc_locations[24].count>1 && pack.npc_locations[24].slots[1].slot==1 &&
          pack.npc_locations[24].slots[1].type==0x40,"A2","authored Ararat NPC slot 1");
    if(map.error!=Error::None || pack.npc_locations[24].count<=1)return 1;
    Run shot(pack);auto &g=shot.rt->game();auto &list=shot.rt->actors();
    check(list.count==1&&list.actors[0].schedule.slot==1,"A3","entry loads authentic live NPC");
    // Place that same actor in the cannon's first impact cell, representing
    // a walk into its line. Its type, AI and schedule remain the shipped data.
    list.actors[0].x=12;list.actors[0].y=10;
    const auto object_count=shot.rt->objects_for_test().size();
    shot.fire();
    check((g.npc_dead[24]&2)!=0&&g.karma==45,"K1","shipped Fire command kills slot and charges karma");
    check(list.count==0,"K2","death clears live slot immediately");
    check(!npc_occupied(list,g.position,25,0,12,10,255),"K3","dead NPC no longer collides");
    shot.fire();
    check(g.karma==45,"K3a","a second cannon ball cannot target the dead NPC");
    check(shot.rt->objects_for_test().size()==object_count,"K3b","death creates no world corpse object");
    shot.key(' '); // normal command turn: guard and NPC movement/update passes
    check(list.count==0,"K4","subsequent world turn cannot update killed NPC");
    // The bed-loop NPC update is a second independent route that previously
    // moved the dead actor back to its authored schedule coordinates.
    snap_npcs_to_schedule(list,25,7);
    if(list.count)std::printf("RED_WITNESS dead slot still moved to (%d,%d) on schedule update\n",
                              int(list.actors[0].x),int(list.actors[0].y));
    check(list.count==0,"K5","schedule update cannot move killed NPC");
    shot.reload();
    check(list.count==0&&(g.npc_dead[24]&2)!=0,"K6","map reload preserves, rather than repairs, death");
    Run miss(pack);auto &mg=miss.rt->game();auto &ml=miss.rt->actors();
    check(ml.count==1,"M1","fresh authored actor available for miss control");
    miss.fire();
    check(ml.count==1&&(mg.npc_dead[24]&2)==0&&mg.karma==50,"M2","empty cannon line does not kill NPC");
    miss.key(' ');
    check(ml.count==1,"M3","ordinary NPC remains in update list");
    // The same original clear-slot routine applies to a guard, but its type
    // 0x70 is excluded by TOWN 0x0052. It vanishes now and may return on entry.
    const auto castle=get_active_map(pack.world,{17,2});
    check(castle.error==Error::None&&castle.value.tile_at(15,26)==0xb6,
          "G1","shipped castle south cannon at (15,26) on floor 2");
    const auto &guard_slot=pack.npc_locations[16].slots[27];
    check(guard_slot.slot==27&&guard_slot.type==0x70,"G2","authored castle guard slot 27");
    Run guard(pack);auto &gg=guard.rt->game();auto &gl=guard.rt->actors();
    gg.position.map={17,2};gg.position.xy={14,26};
    enter_npc_map(gl,&guard_slot,1,17,7,gg.npc_dead[16]);
    check(gl.count==1&&gl.actors[0].schedule.slot==27,"G3","guard occupies a live NPC slot");
    gl.actors[0].x=15;gl.actors[0].y=27;gl.actors[0].z=2;
    guard.fire();
    check(gl.count==0&&gg.karma==45&&!(gg.npc_dead[16]&(uint32_t(1)<<27)),
          "G4","cannon despawns guard immediately without persistent dead bit");
    guard.reload(17);
    bool returned=false;for(size_t i=0;i<gl.count;++i)if(gl.actors[i].schedule.slot==27)returned=true;
    check(returned,"G5","authored guard reappears only after map re-entry");
    std::printf("H-168: %d/%d checks\n",checks-failures,checks);
    return failures?1:0;
}
