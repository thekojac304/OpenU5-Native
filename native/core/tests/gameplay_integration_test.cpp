#include "openu5/gameplay_save.h"
#include <cstdlib>
#include <iostream>
#include <memory>
using namespace openu5;
using save::Json;
static int checks=0;
static void check(bool ok){++checks;if(!ok){std::cerr<<"integration assertion "<<checks<<"\n";std::exit(1);}}
int main(){
    auto game=std::make_unique<GameState>();auto &g=*game;TurnState turn;TravelState travel;CommandState commands;OutdoorServices outdoor;
    std::vector<uint8_t> tiles(1024,68);tiles[32+2]=135;tiles[32*3+3]=72;tiles[32*3+4]=73;WorldData world;MapData map{{1,0},tiles.data(),tiles.size()};world.small_maps=&map;world.small_map_count=1;
    g.position={{10,10},{1,0}};g.time.hour=20;WorldTerrain terrain;terrain.refresh(world,g);
    check(terrain.raw(world,{1,0},2,2)==68&&terrain.effective(world,{1,0},2,2)==153);
    check(terrain.effective(world,{1,0},3,3)==3&&terrain.effective(world,{1,0},4,3)==3);
    g.position.xy={3,3};terrain.refresh(world,g);check(terrain.effective(world,{1,0},3,3)==72&&terrain.effective(world,{1,0},4,3)==73&&terrain.effective(world,{1,0},2,2)==153);
    terrain.set({1,0},2,2,292,true);terrain.set({1,0},2,2,186);check(terrain.raw(world,{1,0},2,2)==186&&terrain.effective(world,{1,0},2,2)==186);
    terrain.wiped=true;terrain.wipe_map={1,0};terrain.wipe_tile=143;check(terrain.effective(world,{1,0},2,2)==143);
    terrain.clear_residence();terrain.refresh(world,g);check(terrain.effective(world,{1,0},2,2)==292);
    terrain.remove_boarded({1,0},2,2,293);check(terrain.effective(world,{1,0},2,2)==292);
    terrain.remove_boarded({1,0},2,2,292);check(terrain.effective(world,{1,0},2,2)==153);
    g.time.hour=5;terrain.refresh(world,g);check(terrain.effective(world,{1,0},2,2)==68);
    terrain.set({1,-1},31,31,511,true);Json state=Json::object();save::capture_terrain(terrain,state);std::string buffer;check(save::encode_json(state,buffer)==save::JsonError::None);Json decoded;check(save::parse_json(buffer,decoded)==save::JsonError::None);WorldTerrain restored;check(save::restore_terrain(decoded,restored)==save::Error::None&&restored.persistent.size()==1&&restored.persistent[0].map.floor==-1);
    decoded["mapOverrides"]["1:0:256:1"]=Json(5);check(save::restore_terrain(decoded,restored)==save::Error::NativeDomain&&restored.persistent.size()==1);
    outdoor.enemies.push_back({31,8,300,255,0,true});outdoor.has_chunk_origin=true;outdoor.chunk_x=240;commands.door={{1,-1},31,30,186,4};save::capture_gameplay(commands,outdoor,state);
    CommandState c2;OutdoorServices o2;check(save::restore_gameplay(state,c2,o2)==save::Error::None&&c2.door.tile==186&&o2.enemies.size()==1&&o2.enemies[0].slot==31&&o2.chunk_x==240);
    state["overworldEnemies"].values[0]["x"]=Json(256);check(save::restore_gameplay(state,c2,o2)==save::Error::NativeDomain&&o2.enemies[0].x==255&&c2.door.turns==4);
    state["overworldEnemies"]=Json::array();state["openDoors"].values[0]["turnsLeft"]=Json(-1);check(save::restore_gameplay(state,c2,o2)==save::Error::NativeDomain&&o2.enemies.size()==1);
    CommandContext ctx{g,turn,travel,commands,world};check(world_transport_services(ctx).tile_at==nullptr);
    auto battle=std::make_unique<CombatState>();CombatEnemy enemy;enemy.name="Test";enemy.group_name="TEST";enemy.max_per_map=1;
    const CombatEnemy *defs[]{&enemy};CombatResources resources;resources.enemies=defs;resources.enemy_count=1;
    const auto seed=g.rng.get_seed();commands.door.turns=4;
    check(preflight_encounter_combat(ctx,*battle,resources,0,5)==CombatResult::MissingMap&&g.rng.get_seed()==seed&&commands.door.turns==4);
    check(start_encounter_combat(ctx,*battle,resources,0,5)==CombatResult::MissingMap&&!ctx.combat&&g.rng.get_seed()==seed&&commands.door.turns==4);
    auto arena=std::make_unique<CombatMap>();for(auto &tile:arena->tiles)tile=5;const CombatMap *maps[]{arena.get()};resources.maps=maps;resources.map_count=1;
    check(preflight_encounter_combat(ctx,*battle,resources,0,5)==CombatResult::Ok&&g.rng.get_seed()==seed&&commands.door.turns==4);
    arena->tiles[0]=-1;check(preflight_encounter_combat(ctx,*battle,resources,0,5)==CombatResult::Invalid&&g.rng.get_seed()==seed);
    std::cout<<checks<<" gameplay owner/domain assertions\n";
}
