#include "openu5/combat.h"
#include "openu5/gameplay_save.h"
#include "openu5/outdoor.h"
#include "openu5/world_commands.h"
#include "openu5/world_terrain.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace openu5;
namespace {
void check(bool ok,const char *what){if(!ok){std::cerr<<"direct Troll handoff: "<<what<<"\n";std::exit(1);}}
struct Objects { std::vector<QuestObject> values; };
size_t count(void *p){return static_cast<Objects*>(p)->values.size();}
QuestObject read(void *p,size_t i){return static_cast<Objects*>(p)->values[i];}
bool reserve(void *p,size_t n){static_cast<Objects*>(p)->values.reserve(n);return true;}
void append(void *p,const QuestObject&o){static_cast<Objects*>(p)->values.push_back(o);}
void erase(void *p,size_t i){auto &v=static_cast<Objects*>(p)->values;v.erase(v.begin()+ptrdiff_t(i));}
void write(void *p,size_t i,const QuestObject&o){static_cast<Objects*>(p)->values[i]=o;}
}

int main(){
    constexpr int player_x=101,player_y=102,bridge_x=101,bridge_y=102,bridge_tile=106;
    std::vector<uint8_t> tiles(256*256,5);tiles[bridge_y*256+bridge_x]=bridge_tile;
    WorldData world{tiles.data(),tiles.data(),tiles.size(),tiles.size()};
    GameState game{};game.position={{uint8_t(player_x),uint8_t(player_y)},{0,0}};
    game.party.party_size=game.party.character_count=1;auto &avatar=game.party.characters[0];
    avatar.party_status=0;avatar.status='G';avatar.current_hp=avatar.max_hp=100;avatar.dexterity=20;game.rng.seed(0x2600);
    TurnState turn{};TravelState travel{};CommandState commands{};
    CommandContext context{game,turn,travel,commands,world};
    Objects objects{};QuestWorldServices quest{};quest.context=&objects;quest.count=count;quest.read=read;
    quest.reserve=reserve;quest.append=append;quest.erase=erase;quest.write=write;context.quest_world=&quest;
    WorldTerrain terrain{};context.terrain=&terrain;terrain.refresh(world,game);

    CombatMap arena{};for(auto &tile:arena.tiles)tile=5;arena.start_count[int(CombatDirection::South)]=1;
    arena.starts[int(CombatDirection::South)][0]={5,6};arena.unit_count=1;arena.units[0]={6,4};
    CombatEnemy troll{};troll.index=41;troll.name="Troll";troll.hp=10;troll.max_per_map=1;troll.tile=400;
    const CombatMap *maps[]={&arena};const CombatEnemy *enemies[42]{};enemies[41]=&troll;
    CombatState battle{};CombatContext battle_owner{game,turn,battle};
    CombatResources resources{maps,1,enemies,42,{}};OutdoorServices outdoor{};
    outdoor.combat=&battle_owner;outdoor.resources=&resources;context.outdoor=&outdoor;

    // These are the exact pending values captured by Runner::turn after the
    // production Move onto bridge tile 106; the refusal below is the real
    // CommandKind::TrollToll entry path, not a hand-built CombatState.
    commands.awaiting_troll=true;commands.troll_toll=99;commands.troll_x=bridge_x;commands.troll_y=bridge_y;
    Command refuse{};refuse.kind=CommandKind::TrollToll;refuse.member=0;
    check(execute_command(context,refuse).status==CommandStatus::Success,"production toll refusal enters combat");
    check(context.combat&&battle.initialized&&battle.count==2,"one Troll direct encounter initialized");
    check(battle.encounter_location==0&&battle.encounter_floor==0&&battle.loot_x==bridge_x&&battle.loot_y==bridge_y&&battle.has_world_loot_origin,
          "production entry captures the blocked bridge coordinate");
    check(battle.arena_origin_x==5&&battle.arena_origin_y==6&&battle.arena_entry==CombatDirection::South,
          "production entry captures the combat arena origin and orientation");

    int troll_actor=-1;for(int i=0;i<battle.count;++i)if(battle.actors[i].enemy){troll_actor=i;break;}
    check(troll_actor>=0,"direct encounter contains exactly one Troll");
    const auto combat_chest=battle.actors[troll_actor].position;
    // Simulate the combat engine's already-completed one-Troll victory state;
    // return coordinates and teardown remain entirely production-owned.
    battle.actors[troll_actor].status=CombatStatus::Dead;
    const int chest_cell=combat_chest.y*kCombatGrid+combat_chest.x;
    battle.loot[chest_cell]=129;battle.chest_contents[chest_cell]=9;battle.victory=true;battle.ended=true;
    check(finish_encounter_combat(context,battle)==CombatResult::Ok,"normal finished-combat teardown runs");
    // STALE EXPECTATION CORRECTED (Batch 2, R-03): this block previously
    // asserted the pre-fix native behavior -- "teardown promotes one chest
    // through the arena-relative world transform" -- and then exercised
    // world Open against that invented object. The reference never promotes
    // an unopened arena chest to a world object on exit; it is simply lost
    // with the rest of the arena. combat_cell_to_world's arena-relative
    // rotation math has no remaining production caller once the promotion
    // that used it is removed. With nothing promoted, there is nothing left
    // in the world for Open to find, so the trailing "exact-direction Open
    // finds the promoted chest" sub-case is removed along with it.
    check(objects.values.empty(),
          "R-03: reference-faithful teardown must not promote the defeated Troll's unopened chest to a world object");
    check(terrain.effective(world,{0,0},bridge_x,bridge_y)==bridge_tile,"bridge terrain survives teardown");

    save::Json snapshot=save::Json::object();save::capture_terrain(terrain,snapshot);std::string encoded;
    check(save::encode_json(snapshot,encoded)==save::JsonError::None,"terrain save encodes");save::Json decoded;WorldTerrain restored;
    check(save::parse_json(encoded,decoded)==save::JsonError::None&&save::restore_terrain(decoded,restored)==save::Error::None,
          "terrain save reloads");
    restored.refresh(world,game);check(restored.effective(world,{0,0},bridge_x,bridge_y)==bridge_tile,"bridge remains authored after save/reload");

    // FLOW A: take the same production TrollToll entry, consume the visible
    // arena chest while victory is still lingering, then leave with Back.
    game.position.xy={uint8_t(player_x),uint8_t(player_y)};CombatState opened{};CombatContext opened_owner{game,turn,opened};
    outdoor.combat=&opened_owner;commands.awaiting_troll=true;commands.troll_toll=99;commands.troll_x=bridge_x;commands.troll_y=bridge_y;
    check(execute_command(context,refuse).status==CommandStatus::Success&&context.combat&&opened.initialized,
          "second production Troll refusal enters a fresh victory arena");
    int opened_troll=-1,opened_player=-1;for(int i=0;i<opened.count;++i){if(opened.actors[i].enemy)opened_troll=i;else opened_player=i;}
    check(opened_troll>=0&&opened_player>=0,"fresh Troll arena has an active player and Troll");
    opened.actors[opened_troll].status=CombatStatus::Dead;opened.actors[opened_player].position={5,4};opened.current=opened_player;
    const int opened_cell=4*kCombatGrid+6;opened.loot[opened_cell]=1;opened.chest_contents[opened_cell]=15;opened.chest_state[opened_cell]=CombatChestState::Unopened;opened.victory=true;
    check(combat_action(opened_owner,CombatAction::Open,int(CombatDirection::East))==CombatResult::Ok&&opened.victory&&!opened.ended,
          "directional Open works in the production direct-Troll victory arena");
    check(opened.chest_state[opened_cell]==CombatChestState::Consumed&&opened.loot[opened_cell]<0,
          "combat Open consumes the direct-Troll chest");
    const size_t objects_before_opened_exit=objects.values.size();
    check(combat_action(opened_owner,CombatAction::EscapeQuick)==CombatResult::Ok&&opened.ended,
          "Back explicitly exits after combat-local Troll loot");
    check(finish_encounter_combat(context,opened)==CombatResult::Ok&&objects.values.size()==objects_before_opened_exit,
          "opened direct-Troll chest is not promoted or duplicated");
    std::cout<<"production direct Troll entry, one-Troll victory handoff, exact Open, and terrain persistence passed\n";
}
