#include "openu5/presentation.h"
#include "openu5/outdoor.h"
#include "openu5/world_commands.h"
#include "openu5/world_terrain.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace openu5;
namespace {
void check(bool ok,const char *what){if(!ok){std::cerr<<"presentation regression: "<<what<<"\n";std::exit(1);}}
struct Objects{std::vector<QuestObject> values;};
size_t count(void*p){return static_cast<Objects*>(p)->values.size();}
QuestObject read(void*p,size_t i){return static_cast<Objects*>(p)->values[i];}
bool reserve(void*p,size_t n){static_cast<Objects*>(p)->values.reserve(n);return true;}
void append(void*p,const QuestObject&o){static_cast<Objects*>(p)->values.push_back(o);}
void erase(void*p,size_t i){auto&v=static_cast<Objects*>(p)->values;v.erase(v.begin()+ptrdiff_t(i));}
void write(void*p,size_t i,const QuestObject&o){static_cast<Objects*>(p)->values[i]=o;}
}

int main(){
    std::vector<uint8_t> tiles(32*32,5);tiles[5*32+6]=184;
    MapData small{{13,0},tiles.data(),tiles.size()};WorldData world{};world.small_maps=&small;world.small_map_count=1;
    GameState game{};game.position={{5,5},{13,0}};game.time.hour=12;game.rng.seed(0x4567);game.turns_since_start=99;
    TurnState turn{};TravelState travel{};CommandState commands{};CommandContext context{game,turn,travel,commands,world};
    Objects objects{};QuestWorldServices quest{};quest.context=&objects;quest.count=count;quest.read=read;quest.reserve=reserve;quest.append=append;quest.erase=erase;quest.write=write;context.quest_world=&quest;
    WorldTerrain terrain{};context.terrain=&terrain;
    auto active=get_active_map(world,{13,0});check(active.error==Error::None,"small map setup");

    auto snap=compose_world_presentation(context,active.value,{5,5},0x11c);
    check(snap.tiles[5*11+6]==184,"closed door is visible before Open");
    Command open{};open.kind=CommandKind::Open;open.direction=Direction::East;
    auto opened=world_interaction(context,open,active.value,{},rng_source(game.rng));
    check(opened.status==CommandStatus::Success,"Open command succeeds");
    snap=compose_world_presentation(context,active.value,{5,5},0x11c);
    check(snap.tiles[5*11+6]==68,"opened door uses live door owner");

    // Combat teardown is a UI/base-mode concern; the authoritative world
    // chest identity remains in QuestWorldServices and Open resolves the
    // commanded adjacent coordinate, never an invented nearest chest.
    QuestObject prop{};prop.location=13;prop.floor=0;prop.x=5;prop.y=4;prop.tile=5;prop.prop=true;objects.values.push_back(prop);
    QuestObject chest{};chest.location=13;chest.floor=0;chest.x=5;chest.y=4;chest.tile=1;chest.chest=true;chest.contents=0;objects.values.push_back(chest);
    Command open_chest{};open_chest.kind=CommandKind::Open;open_chest.direction=Direction::North;open_chest.has_direction=true;
    const auto chest_result=world_interaction(context,open_chest,active.value,{},rng_source(game.rng));
    check(chest_result.status==CommandStatus::Success,"post-combat-style Open routes to visible chest coordinate");
    check(objects.values.size()==1&&!objects.values[0].chest,"exact chest lookup is independent of object-layer ordering");
    objects.values.clear();

    // Outdoor victory promotes combat-only chest metadata into the exact
    // defeated actor coordinate, preserving the trapped bit for world Open.
    {
        GameState battle_game{};battle_game.position={{10,10},{0,0}};
        battle_game.party.character_count=battle_game.party.party_size=1;
        auto &avatar=battle_game.party.characters[0];avatar.party_status=0;avatar.status='G';
        avatar.current_hp=avatar.max_hp=100;avatar.dexterity=20;avatar.weapon=255;
        battle_game.rng.seed(0x13579);
        TurnState battle_turn{};TravelState battle_travel{};CommandState battle_commands{};
        std::vector<uint8_t> outdoor_tiles(256*256,5);
        WorldData outdoor_world{outdoor_tiles.data(),outdoor_tiles.data(),outdoor_tiles.size(),outdoor_tiles.size()};
        CommandContext battle_context{battle_game,battle_turn,battle_travel,battle_commands,outdoor_world};
        Objects battle_objects{};QuestWorldServices battle_quest{};
        battle_quest.context=&battle_objects;battle_quest.count=count;battle_quest.read=read;
        battle_quest.reserve=reserve;battle_quest.append=append;battle_quest.erase=erase;battle_quest.write=write;
        battle_context.quest_world=&battle_quest;
        CombatState battle_state{};CombatContext battle_owner{battle_game,battle_turn,battle_state};
        CombatMap arena{};for(auto &tile:arena.tiles)tile=5;
        arena.start_count[int(CombatDirection::South)]=1;
        arena.starts[int(CombatDirection::South)][0]={5,6};arena.unit_count=1;arena.units[0]={5,4};
        CombatEnemy troll{};troll.index=0;troll.name="Troll";troll.hp=10;troll.max_per_map=1;troll.tile=400;
        const CombatMap *maps[]={&arena};const CombatEnemy *enemies[]={&troll};
        CombatResources combat_resources{maps,1,enemies,1,{}};
        OutdoorServices outdoor{};outdoor.combat=&battle_owner;outdoor.resources=&combat_resources;
        outdoor.enemies.push_back({-1,0,400,11,10});battle_context.outdoor=&outdoor;
        auto outdoor_map=get_active_map(outdoor_world,battle_game.position.map);
        check(outdoor_map.error==Error::None&&
                  outdoor_start(battle_context,0,outdoor_map.value,{},false)==CommandStatus::Success,
              "outdoor combat starts with authoritative enemy coordinate");
        battle_state.loot[60]=129;battle_state.chest_contents[60]=7;battle_state.victory=true;
        battle_state.victory_latch(battle_state.victory_context);
        check(battle_objects.values.size()==1&&battle_objects.values[0].chest&&
                  battle_objects.values[0].trapped&&battle_objects.values[0].contents==(128|7)&&
                  battle_objects.values[0].x==11&&battle_objects.values[0].y==10&&
                  battle_state.loot[60]==0,
              "combat chest becomes one authoritative trapped world object");
        battle_context.combat=false;Command exact_open{};exact_open.kind=CommandKind::Open;
        exact_open.direction=Direction::East;exact_open.has_direction=true;
        check(world_interaction(battle_context,exact_open,outdoor_map.value,{},rng_source(battle_game.rng)).status==CommandStatus::Success&&
                  !battle_objects.values.empty()&&!battle_objects.values[0].chest,
              "exact adjacent Open consumes promoted chest and leaves generated loot");
    }

    // Troll toll and other direct encounter starts do not pass through
    // OutdoorServices::victory_latch. Teardown must still promote the exact
    // rendered combat chest into world state at the blocked Troll coordinate.
    {
        GameState direct_game{};direct_game.position={{40,41},{0,0}};
        direct_game.party.character_count=direct_game.party.party_size=1;
        direct_game.party.characters[0].party_status=0;direct_game.party.characters[0].status='G';
        direct_game.party.characters[0].current_hp=direct_game.party.characters[0].max_hp=100;
        TurnState direct_turn{};TravelState direct_travel{};CommandState direct_commands{};
        std::vector<uint8_t> direct_tiles(256*256,5);WorldData direct_world{direct_tiles.data(),direct_tiles.data(),direct_tiles.size(),direct_tiles.size()};
        CommandContext direct_context{direct_game,direct_turn,direct_travel,direct_commands,direct_world};
        Objects direct_objects{};QuestWorldServices direct_quest{};direct_quest.context=&direct_objects;direct_quest.count=count;direct_quest.read=read;direct_quest.reserve=reserve;direct_quest.append=append;direct_quest.erase=erase;direct_quest.write=write;direct_context.quest_world=&direct_quest;
        CombatState direct_state{};direct_state.initialized=true;direct_state.victory=true;direct_state.ended=true;direct_state.loot[60]=129;direct_state.chest_contents[60]=9;direct_state.encounter_location=0;direct_state.encounter_floor=0;direct_state.loot_x=41;direct_state.loot_y=41;direct_state.arena_origin_x=5;direct_state.arena_origin_y=5;direct_state.arena_entry=CombatDirection::South;direct_state.has_world_loot_origin=true;direct_context.combat=true;
        check(finish_encounter_combat(direct_context,direct_state)==CombatResult::Ok,"direct Troll-style encounter tears down");
        check(direct_objects.values.size()==1&&direct_objects.values[0].chest&&direct_objects.values[0].trapped&&direct_objects.values[0].contents==(128|9)&&direct_objects.values[0].location==0&&direct_objects.values[0].floor==0&&direct_objects.values[0].x==41&&direct_objects.values[0].y==41,"direct encounter chest survives teardown with exact Troll identity");
        direct_game.position.xy={40,41};auto direct_map=get_active_map(direct_world,direct_game.position.map);Command exact{};exact.kind=CommandKind::Open;exact.direction=Direction::East;exact.has_direction=true;
        check(world_interaction(direct_context,exact,direct_map.value,{},rng_source(direct_game.rng)).status==CommandStatus::Success,"exact adjacent Open finds direct-encounter chest");
    }

    terrain.set({13,0},4,5,185);snap=compose_world_presentation(context,active.value,{5,5},0x11c);
    check(snap.tiles[5*11+4]==185,"volatile secret-door override is visible");
    QuestObject torch{13,0,5,4,0x101};torch.torch=true;objects.values.push_back(torch);snap=compose_world_presentation(context,active.value,{5,5},0x11c);
    check(snap.tiles[4*11+5]==0x101,"live object overlays terrain");
    QuestObject loose_gem{13,0,6,4,8};loose_gem.loot=true;loose_gem.item_id=8;loose_gem.quality=1;objects.values.push_back(loose_gem);
    snap=compose_world_presentation(context,active.value,{5,5},0x11c);
    check(snap.tiles[4*11+6]==0x108,"loose loot renders from the authored object bank, not as a terrain glyph");
    objects.values.pop_back();
    const auto picked_up=get_quest_object(context,Direction::North,{});check(picked_up.status==CommandStatus::Success&&objects.values.empty(),"Get removes the live object");
    snap=compose_world_presentation(context,active.value,{5,5},0x11c);check(snap.tiles[4*11+5]==5,"picked-up object reveals terrain");

    std::vector<uint8_t> passage_tiles(32*32,5);passage_tiles[13*32+17]=79;MapData passage{{17,2},passage_tiles.data(),passage_tiles.size()};
    world.small_maps=&passage;game.position.map={17,2};game.position.xy={17,13};quest.passage_open=true;active=get_active_map(world,{17,2});
    snap=compose_world_presentation(context,active.value,{17,13},0x11c);check(snap.tiles[5*11+5]==0x11c,"party remains topmost");
    check(snap.tiles[5*11+4]==5,"passage fixture neighbor remains base terrain");
    game.position.xy={16,13};snap=compose_world_presentation(context,active.value,{16,13},0x11c);check(snap.tiles[5*11+6]==68,"opened passage uses quest presentation owner");

    game.position.map={13,0};game.position.xy={0,0};world.small_maps=&small;active=get_active_map(world,{13,0});
    snap=compose_world_presentation(context,active.value,{0,0},0x11c);check(snap.tiles[0]==5,"valid small-map boundary uses edge fill, not fallback tile");
    game.time.hour=0;game.position.xy={16,16};snap=compose_world_presentation(context,active.value,{16,16},0x11c);
    check(snap.tiles[0]==kPresentationHidden,"night visibility/fog censors distant cells");

    const auto seed=game.rng.get_seed();const auto turns=game.turns_since_start;
    check(tile_animation_kind(1)==TileAnimationKind::WaterScroll,"water is presentation animated");
    check(tile_animation_kind(0xb0)==TileAnimationKind::FireNoise,"torch is presentation animated");
    check(animated_tile_frame(212,0,turns)==212&&animated_tile_frame(212,2,turns)==213&&animated_tile_frame(212,8,turns)==212,"110 ms four-frame cadence");
    check(animated_tile_frame(250,0,turns)==250&&animated_tile_frame(250,4,turns)==251,"220 ms toggle cadence");
    check(game.rng.get_seed()==seed&&game.turns_since_start==turns,"animation selection does not touch RNG or turns");

    // High-bank actors use the reference per-actor presentation program at the
    // ~110 ms clock; the party leader remains outside this idle animator.
    NpcActors npcs{};npcs.count=1;npcs.actors[0].location=13;npcs.actors[0].z=0;
    npcs.actors[0].x=17;npcs.actors[0].y=16;npcs.actors[0].schedule.slot=3;
    npcs.actors[0].schedule.type=0x4c;npcs.actors[0].schedule.dialog=1;context.actors=&npcs;
    game.time.hour=12;game.position.map={13,0};game.position.xy={16,16};active=get_active_map(world,{13,0});
    snap=compose_world_presentation(context,active.value,game.position.xy,0x11c);
    check(snap.tiles[5*11+6]==0x14c&&snap.actor_ids[5*11+6]!=0,"snapshot retains stable NPC animation identity");
    ActorAnimationClock actor_clock;actor_clock.render(snap,0);const auto initial_actor=snap.tiles[5*11+6];bool actor_changed=false;
    for(uint32_t phase=2;phase<80&&!actor_changed;phase+=2){snap=compose_world_presentation(context,active.value,game.position.xy,0x11c);actor_clock.render(snap,phase);actor_changed=snap.tiles[5*11+6]!=initial_actor;}
    check(actor_changed,"NPC idle program advances on presentation clock");
    PresentationSnapshot mimic{};mimic.tiles[0]=0x1a8;mimic.actor_ids[0]=99;mimic.actor_seeds[0]=0xa8;ActorAnimationClock mimic_clock;
    for(uint32_t phase=0;phase<80;phase+=2)mimic_clock.render(mimic,phase);
    check(mimic.tiles[0]==0x1a8,"mimic HALT program remains camouflaged");
    check(game.rng.get_seed()==seed&&game.turns_since_start==turns,"actor view PRNG is isolated from gameplay state");

    CombatState combat{};for(auto&t:combat.map.tiles)t=77;CombatEnemy enemy{};enemy.tile=400;combat.count=1;combat.current=0;combat.actors[0].enemy=&enemy;combat.actors[0].position={5,5};combat.actors[0].status=CombatStatus::Active;
    auto battle=compose_combat_presentation(combat,game);check(battle.tiles[0]==77,"combat uses live arena terrain");check(battle.tiles[60]==400,"combat overlays live actor");
    check(battle.active_x==5&&battle.active_y==5&&battle.active_enemy,"enemy active-turn marker comes from CombatState.current");
    ActorAnimationClock combat_clock;combat_clock.render(battle,0);const auto first_combat_frame=battle.tiles[60];bool combat_changed=false;
    for(uint32_t phase=2;phase<80&&!combat_changed;phase+=2){battle=compose_combat_presentation(combat,game);combat_clock.render(battle,phase);combat_changed=battle.tiles[60]!=first_combat_frame;}
    check(combat_changed,"combatant actor program advances on presentation clock");
    // The reference battle remains open after victory so the player can Get
    // loot before exiting. Dead actor cells retain their corpse/loot tile in
    // the combat presentation without inventing a world-level corpse object.
    combat.actors[0].status=CombatStatus::Dead;combat.loot[60]=30;
    battle=compose_combat_presentation(combat,game);
    check(battle.tiles[60]==0x11e&&battle.actor_ids[60]==0,
          "dead enemy corpse remains visible and is not a live actor");
    combat.loot[60]=31;battle=compose_combat_presentation(combat,game);
    check(battle.tiles[60]==0x11f,"generic monster splat maps to the authored remains tile");
    combat.loot[60]=1;battle=compose_combat_presentation(combat,game);
    check(battle.tiles[60]==0x101,"ordinary chest maps to the authored chest tile");
    combat.loot[60]=129;battle=compose_combat_presentation(combat,game);
    check(battle.tiles[60]==0x101&&combat_loot_render_tile(129)==0x101,
          "trapped-chest metadata is masked before resource lookup");
    check(combat_loot_render_tile(30)==0x11e&&combat_loot_render_tile(31)==0x11f,
          "body and splat remains mappings are unchanged");
    check(battle.active_x<0&&battle.active_y<0,"dead current actor is never highlighted");
    combat.actors[0].status=CombatStatus::Active;combat.actors[0].enemy=nullptr;combat.actors[0].member=0;combat.actors[0].charmed=false;
    battle=compose_combat_presentation(combat,game);check(!battle.active_enemy,"live party turn uses player marker");
    combat.actors[0].charmed=true;battle=compose_combat_presentation(combat,game);check(battle.active_enemy,"charmed party turn uses enemy marker");
    ActiveMap resolved{};resolved.geometry={11,11,false};resolved.resolve_context=&combat;resolved.resolve_tile=[](void*p,MapId,int32_t x,int32_t y,int32_t)->int32_t{return static_cast<CombatState*>(p)->map.tiles[y*11+x];};
    check(resolved.tile_at(0,0)==77,"resolver-only maps do not collapse to off-map");
    std::cout<<"presentation dynamic-state, visibility, combat, and animation regressions passed; snapshot="
             <<sizeof(PresentationSnapshot)<<" actor-clock="<<sizeof(ActorAnimationClock)<<"\n";
}
