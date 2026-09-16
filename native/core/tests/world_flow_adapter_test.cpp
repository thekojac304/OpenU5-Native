#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include "openu5/transport.h"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace openu5;
static int checks=0;
static void check(bool yes){++checks;if(!yes){std::cerr<<"adapter check "<<checks<<" failed\n";std::exit(1);}}
int main(){GameState g;TurnState t;TravelState travel;CommandState commands;std::vector<uint8_t> terrain(65536,5);WorldData world;world.overworld=terrain.data();world.overworld_size=terrain.size();CommandContext c{g,t,travel,commands,world};g.position.xy={20,20};g.party.party_size=g.party.character_count=1;auto &r=g.party.characters[0];r.status='G';r.current_hp=r.max_hp=100;r.dexterity=30;r.party_status=0;r.weapon=r.shield=r.helmet=r.armor=r.ring=r.amulet=255;g.food=100;
 struct Objects{int tile=0x124,hull=7,skiffs=2,drops=0,parks=0;bool capacity=true,parked=true;}objects;
 TransportServices s;s.context=&objects;s.tile_at=[](void *p,int x,int y){return x==20&&y==20?static_cast<Objects*>(p)->tile:5;};s.ship_at=[](void *p,WorldPosition,int32_t &h,int32_t &s){auto &o=*static_cast<Objects*>(p);if(!o.parked)return false;h=o.hull;s=o.skiffs;return true;};s.reserve=[](void *p,bool){return static_cast<Objects*>(p)->capacity;};s.remove_boarded=[](void *p,WorldPosition,int32_t){auto &o=*static_cast<Objects*>(p);o.tile=5;o.parked=false;};s.drop=[](void *p,WorldPosition,int tile){auto &o=*static_cast<Objects*>(p);o.tile=tile;++o.drops;};s.park_ship=[](void *p,WorldPosition,int tile,int hull,int skiffs){auto &o=*static_cast<Objects*>(p);o.tile=tile;o.hull=hull;o.skiffs=skiffs;o.parked=true;++o.parks;};c.transport_services=&s;
 Command board;board.kind=CommandKind::Board;Command exit;exit.kind=CommandKind::Disembark;
 t.transport_tile=0x12;g.transport=TransportMode::Horse;auto result=execute_command(c,board);check(result.status==CommandStatus::Rejected&&g.ship_hull==7&&g.ship_skiffs==2&&g.turns_since_start==0);
 t.transport_tile=0x1c;g.transport=TransportMode::Foot;result=execute_command(c,board);check(result.status==CommandStatus::Success&&t.transport_tile==0x24&&g.transport==TransportMode::Ship&&g.turns_since_start==1&&!objects.parked);
 objects.capacity=false;auto before=g.rng.get_seed();result=execute_command(c,exit);check(result.status==CommandStatus::NeedsStorage&&g.rng.get_seed()==before&&g.turns_since_start==1);
 objects.capacity=true;result=execute_command(c,exit);check(result.status==CommandStatus::Success&&g.transport==TransportMode::Foot&&objects.parks==1&&objects.hull==7&&objects.skiffs==2&&g.turns_since_start==2);
 objects.tile=0x110;objects.parked=false;result=execute_command(c,board);check(result.status==CommandStatus::Success&&t.transport_tile==0x12);
 result=execute_command(c,exit);check(result.status==CommandStatus::Success&&objects.tile==0x110&&objects.drops==1);
 objects.tile=5;result=execute_command(c,board);check(result.status==CommandStatus::Rejected);
 DungeonState d;DungeonScratch scratch;DungeonData data;data.location=33;data.cells[9]=0x10;DungeonContext dc{d,scratch};dc.data=&data;dc.count=1;c.dungeon_context=&dc;
 Command enter;enter.kind=CommandKind::EnterDungeon;enter.member=33;result=execute_command(c,enter);check(result.status==CommandStatus::Success&&d.active&&d.pos.x==1&&d.pos.y==1&&d.pos.floor==0&&d.pos.facing==DungeonFacing::South&&g.position.xy.x==20);
 enter.hours=255;execute_command(c,enter);check(d.pos.floor==7&&d.pos.x==7&&d.pos.y==7&&d.pos.facing==DungeonFacing::West);
 CombatMap map;for(auto &v:map.tiles)v=5;map.start_count[2]=1;map.starts[2][0]={5,5};uint8_t sprites[16]{};const CombatEnemy *defs[48]{};CombatState combat;CombatContext cc{g,t,combat};cc.enemy_defs=defs;cc.enemy_def_count=48;FixedCombatSetup fixed{sprites,7};
 d.cells[511]=0xf0;check(start_fixed_combat(c,cc,map,fixed,CombatDirection::South,true)==CombatResult::Ok&&combat.victory&&dungeon_room_cleared(g,33,0)&&d.cells[511]==0xa0);
 check(finish_encounter_combat(c,combat)==CombatResult::Ok&&!c.combat&&d.active&&d.pos.floor==7);
 map.unit_count=1;map.units[0]={5,5};sprites[0]=0xe8;before=g.rng.get_seed();check(initialize_combat(cc,map,CombatDirection::South,nullptr,0,true,&fixed)==CombatResult::NeedsActorStorage&&g.rng.get_seed()==before);
 std::cout<<checks<<" world flow adapter checks passed\n";
}
