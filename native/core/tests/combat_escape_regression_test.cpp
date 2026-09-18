#include "openu5/combat.h"
#include "openu5/world_commands.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace openu5;

namespace {
void check(bool ok,const char *what){
    if(!ok){std::cerr<<"combat escape regression: "<<what<<"\n";std::exit(1);}
}
struct Objects { std::vector<QuestObject> values; };
size_t count(void *p){return static_cast<Objects *>(p)->values.size();}
QuestObject read(void *p,size_t i){return static_cast<Objects *>(p)->values[i];}
bool reserve(void *p,size_t n){static_cast<Objects *>(p)->values.reserve(n);return true;}
void append(void *p,const QuestObject &o){static_cast<Objects *>(p)->values.push_back(o);}
void erase(void *p,size_t i){auto &v=static_cast<Objects *>(p)->values;v.erase(v.begin()+ptrdiff_t(i));}
void write(void *p,size_t i,const QuestObject &o){static_cast<Objects *>(p)->values[i]=o;}
}

int main(){
    // This is the Troll-toll shape: Troll A is already defeated and has left
    // a combat chest, while Troll B reaches the arena edge through a real AI
    // EnemyStep.  It deliberately does not invoke teardown until the normal
    // combat engine has retired the fleeing actor and ended the fight.
    GameState game{};game.position={{40,41},{0,0}};
    game.party.character_count=game.party.party_size=1;
    auto &member=game.party.characters[0];member.party_status=0;member.status='G';
    member.current_hp=member.max_hp=100;member.dexterity=20;
    TurnState turn{};TravelState travel{};CommandState commands{};
    std::vector<uint8_t> world_tiles(256*256,5);
    constexpr int bridge_x=41,bridge_y=41,bridge_tile=77;
    world_tiles[bridge_y*256+bridge_x]=bridge_tile;
    WorldData world{world_tiles.data(),world_tiles.data(),world_tiles.size(),world_tiles.size()};
    CommandContext context{game,turn,travel,commands,world};
    Objects objects{};QuestWorldServices quest{};
    quest.context=&objects;quest.count=count;quest.read=read;quest.reserve=reserve;
    quest.append=append;quest.erase=erase;quest.write=write;context.quest_world=&quest;

    CombatEnemy troll{};troll.index=41;troll.name="Troll";troll.hp=10;troll.max_per_map=2;
    CombatState combat{};combat.initialized=true;combat.count=3;combat.map={};
    for(auto &tile:combat.map.tiles)tile=5;
    combat.encounter_location=0;combat.encounter_floor=0;
    combat.loot_x=bridge_x;combat.loot_y=bridge_y;combat.has_world_loot_origin=true;
    combat.arena_origin_x=6;combat.arena_origin_y=4;combat.arena_entry=CombatDirection::South;
    auto &avatar=combat.actors[0];avatar.member=0;avatar.status=CombatStatus::Active;
    avatar.position={5,5};avatar.speed=1;avatar.counter=255;
    auto &defeated=combat.actors[1];defeated.enemy=&troll;defeated.id=2;
    defeated.status=CombatStatus::Dead;defeated.position={6,4};
    combat.loot[4*kCombatGrid+6]=129;combat.chest_contents[4*kCombatGrid+6]=9;
    auto &fleeing=combat.actors[2];fleeing.enemy=&troll;fleeing.id=3;
    fleeing.status=CombatStatus::Active;fleeing.fleeing=true;fleeing.position={10,5};
    // Keep it below the quarter-health flee threshold even if its AI turn
    // receives the possible one-point recovery roll.
    fleeing.speed=35;fleeing.counter=1;fleeing.hp=1;fleeing.max_hp=20;
    CombatContext combat_context{game,turn,combat};context.combat=true;context.combat_context=&combat_context;

    CombatResult stepped=CombatResult::Ok;
    for(int step=0;step<16&&fleeing.status!=CombatStatus::Fled;++step)
        stepped=combat_action(combat_context,CombatAction::EnemyStep);
    check(stepped==CombatResult::Ok,"normal EnemyStep accepts boundary exit");
    check(fleeing.status==CombatStatus::Fled&&fleeing.hp==0,"off-map Troll is explicitly escaped, not left alive");
    check(combat.current!=2,"escaped Troll is removed from turn scheduling");
    check(combat.victory&&!combat.ended&&!combat_over(combat),
          "escaped Troll does not block the canonical post-victory loot stage");
    check(combat.loot[4*kCombatGrid+6]==129,"defeated Troll chest remains until common teardown");

    // U5 keeps the party in the cleared arena to Get/Open remains.  The normal
    // Back/Cancel route then executes EscapeQuick and closes the encounter.
    check(combat_action(combat_context,CombatAction::EscapeQuick)==CombatResult::Ok&&combat.ended,
          "normal post-victory escape ends combat after enemy departure");

    check(finish_encounter_combat(context,combat)==CombatResult::Ok,"direct Troll encounter uses common teardown");
    check(!context.combat&&!combat.initialized,"combat context returns to exploration exactly once");
    check(objects.values.size()==1&&objects.values[0].chest&&objects.values[0].trapped&&
              objects.values[0].contents==(128|9),"defeated Troll chest is promoted exactly once");
    check(objects.values[0].location==0&&objects.values[0].floor==0&&
              objects.values[0].x==bridge_x&&objects.values[0].y==bridge_y,
          "promoted chest preserves exact world identity");
    const auto active=get_active_map(world,game.position.map);
    check(active.error==Error::None&&active.value.tile_at(bridge_x,bridge_y)==bridge_tile,
          "authoritative bridge tile survives combat teardown");
    Command open{};open.kind=CommandKind::Open;open.direction=Direction::East;open.has_direction=true;
    check(world_interaction(context,open,active.value,{},rng_source(game.rng)).status==CommandStatus::Success,
          "exact-direction Open finds promoted chest");
    bool chest_left=false;for(const auto &object:objects.values)chest_left|=object.chest;
    check(!chest_left,"Open consumes the one promoted chest without duplicates");
    std::cout<<"Troll escape, teardown, exact chest promotion, and bridge restoration passed\n";
}
