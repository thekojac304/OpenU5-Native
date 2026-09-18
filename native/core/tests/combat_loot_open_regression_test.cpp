#include "openu5/combat.h"
#include "openu5/inventory.h"
#include "openu5/loot.h"
#include "openu5/presentation.h"
#include "openu5/world_commands.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace openu5;
namespace {
void check(bool ok,const char *what){if(!ok){std::cerr<<"combat loot Open regression: "<<what<<"\n";std::exit(1);}}
struct Objects{std::vector<QuestObject> values;};
size_t count(void*p){return static_cast<Objects*>(p)->values.size();}
QuestObject read(void*p,size_t i){return static_cast<Objects*>(p)->values[i];}
bool reserve(void*p,size_t n){static_cast<Objects*>(p)->values.reserve(n);return true;}
void append(void*p,const QuestObject&o){static_cast<Objects*>(p)->values.push_back(o);}
}

int main(){
    // The stored pair is category/payload, never a universal item id.  These
    // representatives cover every authoritative inventory destination.
    struct DecodeCase{LootGrant grant;LootCategory category;int index;int quantity;};
    const DecodeCase decode_cases[]={{{2,21},LootCategory::Gold,-1,21},{{15,3},LootCategory::Food,-1,3},{{7,129},LootCategory::Keys,-1,1},{{13,2},LootCategory::Torches,-1,2},{{8,1},LootCategory::Gems,-1,1},{{3,5},LootCategory::Potion,5,1},{{4,6},LootCategory::Scroll,6,1},{{5,17},LootCategory::Equipment,17,1}};
    for(const auto &v:decode_cases){const auto d=decode_loot(v.grant);check(d.category==v.category&&d.item_index==v.index&&d.quantity==v.quantity,"canonical loot category/index/quantity decode");}
    GameState game{};game.party.party_size=game.party.character_count=1;
    auto &member=game.party.characters[0];member.party_status=0;member.status='G';
    member.current_hp=member.max_hp=100;member.dexterity=20;
    TurnState turn{};CombatState battle{};battle.initialized=true;battle.victory=true;
    battle.count=1;battle.current=0;battle.actors[0].id=1;battle.actors[0].member=0;
    battle.actors[0].status=CombatStatus::Active;battle.actors[0].position={5,4};
    constexpr int chest_x=6,chest_y=4,chest_key=chest_y*kCombatGrid+chest_x;
    battle.loot[chest_key]=1;battle.chest_contents[chest_key]=15;
    battle.chest_state[chest_key]=CombatChestState::Unopened;
    CombatContext arena{game,turn,battle};

    check(combat_action(arena,CombatAction::OpenAt,chest_x,chest_y)==CombatResult::Ok,
          "the resolved exact-cell Open is accepted in the lingering victory arena");
    check(battle.victory&&!battle.ended,"victory remains active after combat-local Open");
    check(battle.chest_state[chest_key]==CombatChestState::Consumed&&battle.loot[chest_key]<0,
          "Open consumes the exact combat chest rather than only acknowledging the command");
    check(battle.pile_count>0,"authoritative chest logic places its contents in the combat loot pile");

    // Reference storage is an insertion-ordered object stack.  Its last
    // record is both visible and selected by Get (LIFO), and each Get removes
    // exactly that one record.  This covers gems, gold, food, and equipment.
    battle.pile_count=0;
    battle.piles[battle.pile_count++]={{6,4},15,2}; // bottom: food
    battle.piles[battle.pile_count++]={{6,4},8,2};  // gems
    battle.piles[battle.pile_count++]={{6,4},2,20}; // gold
    battle.piles[battle.pile_count++]={{6,4},5,1};  // top: equipment
    const int food_before=game.food,gems_before=game.gems,gold_before=game.gold,equipment_before=game.equipment_quantities[1];
    auto snapshot=compose_combat_presentation(battle,game);
    check(snapshot.tiles[chest_key]==combat_loot_render_tile(5),"top loose item is the visible stack representative");
    check(combat_action(arena,CombatAction::Get,0,-1)==CombatResult::Ok&&battle.pile_count==3&&game.equipment_quantities[1]==equipment_before+1,
          "first Get takes only the LIFO equipment item");
    snapshot=compose_combat_presentation(battle,game);
    check(snapshot.tiles[chest_key]==combat_loot_render_tile(2),"removing the top recomputes gold as the visible representative");
    check(combat_action(arena,CombatAction::Get,0,-1)==CombatResult::Ok&&battle.pile_count==2&&game.gold==gold_before+20,
          "second Get takes only gold");
    check(combat_action(arena,CombatAction::Get,0,-1)==CombatResult::Ok&&battle.pile_count==1&&game.gems==gems_before+2,
          "third Get takes only gems");
    check(combat_action(arena,CombatAction::Get,0,-1)==CombatResult::Ok&&battle.pile_count==0&&game.food==food_before+2,
          "fourth Get takes only food and empties the stack");
    snapshot=compose_combat_presentation(battle,game);
    check(snapshot.tiles[chest_key]!=combat_loot_render_tile(15),"empty stack no longer paints a loose-loot representative");

    // A full destination leaves the chosen top object in place, visibly and
    // authoritatively, for a later retry.
    battle.piles[battle.pile_count++]={{6,4},2,1};
    game.gold=9999;
    check(combat_action(arena,CombatAction::Get,0,-1)==CombatResult::Ok&&battle.pile_count==1&&game.gold==9999,
          "full inventory retains the selected loose object");
    snapshot=compose_combat_presentation(battle,game);
    check(snapshot.tiles[chest_key]==combat_loot_render_tile(2),"full-inventory object remains renderable");
    game.gold=gold_before+20;
    check(combat_action(arena,CombatAction::Get,0,-1)==CombatResult::Ok&&battle.pile_count==0&&game.gold==gold_before+21,
          "retained object is collectible after capacity is available");

    // Directional victory Get is the physical keyboard route; it reaches the
    // same authoritative inventory mutation and removes only its selected item.
    battle.piles[battle.pile_count++]={{6,4},8,1};
    std::vector<uint8_t> route_tiles(256*256,5);WorldData route_world{route_tiles.data(),route_tiles.data(),route_tiles.size(),route_tiles.size()};TravelState route_travel{};CommandState route_commands{};CommandContext route{game,turn,route_travel,route_commands,route_world};route.combat=true;route.combat_context=&arena;
    Command directional_get{};directional_get.kind=CommandKind::CombatGet;directional_get.has_direction=true;directional_get.direction=Direction::East;
    check(dispatch_world_command(route,directional_get).status==CommandStatus::Success&&game.gems==gems_before+3,"directional combat Get reaches the same gem inventory field");

    std::vector<uint8_t> world_tiles(256*256,5);WorldData world{world_tiles.data(),world_tiles.data(),world_tiles.size(),world_tiles.size()};
    TravelState travel{};CommandState commands{};CommandContext context{game,turn,travel,commands,world};
    context.combat=true;context.combat_context=&arena;Objects objects{};QuestWorldServices quest{};
    quest.context=&objects;quest.count=count;quest.read=read;quest.reserve=reserve;quest.append=append;context.quest_world=&quest;
    check(combat_action(arena,CombatAction::EscapeQuick)==CombatResult::Ok&&battle.ended,
          "Back/Cancel remains the explicit post-victory exit");
    check(finish_encounter_combat(context,battle)==CombatResult::Ok,"normal combat teardown succeeds");
    check(objects.values.empty(),"a consumed combat chest is never promoted or duplicated in the world");

    // A gem reaches its real inventory field only through Get, and View consumes
    // that same field.  Walking/rendering is deliberately absent from this path.
    context.combat=false;context.combat_context=nullptr;
    Command view{};view.kind=CommandKind::ViewGem;
    check(execute_command(context,view).status==CommandStatus::Success&&game.gems==gems_before+2,
          "View a gem consumes the gem previously granted by explicit Get");

    // Ready is a single authoritative slot.  Selecting an occupied slot cannot
    // duplicate state; Unready returns the prior item before the replacement is readied.
    GameState ready{};ready.party.party_size=ready.party.character_count=1;
    auto &ready_member=ready.party.characters[0];ready_member.strength=99;ready_member.helmet=1;
    ready_member.armor=ready_member.weapon=ready_member.shield=ready_member.ring=ready_member.amulet=kEquipmentNothing;
    ready.equipment_quantities[0]=1;
    check(!equip_item(ready,0,0).ok&&ready.party.characters[0].helmet==1&&ready.equipment_quantities[0]==1,
          "Ready rejects an occupied equipment slot without duplicating either item");
    check(unequip_slot(ready,0,EquipSlot::Helmet).ok&&ready.equipment_quantities[1]==1,
          "Unready returns the replaced item to authoritative inventory");
    check(equip_item(ready,0,0).ok&&ready.party.characters[0].helmet==0&&ready.equipment_quantities[0]==0,
          "Ready applies the chosen replacement exactly once after Unready");
    std::cout<<"victory-arena directional Open, consume, and no-promotion passed\n";
}
