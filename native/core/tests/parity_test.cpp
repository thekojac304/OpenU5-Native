#include "openu5/movement.h"
#include <stdio.h>
#include "../fixtures/foundation.inc"
using namespace openu5;

static int failure(const char *group, int row, int line) {
    fprintf(stderr, "%s fixture %d failed at line %d\n", group, row, line);
    return row + 1;
}
#define CHECK(condition) do { if (!(condition)) return failure(group, row, __LINE__); } while (false)
static bool same_time(GameTime a, GameTime b) {
    return a.year==b.year && a.month==b.month && a.day==b.day && a.hour==b.hour && a.minute==b.minute;
}
static bool same_text(const char *a, const char *b) {
    if (!a || !b) return a==b;
    while (*a && *a==*b) { ++a; ++b; }
    return *a==*b;
}
static int test_rng() {
    const char *group="rng"; int row=0;
    for (const auto &v:f_seed) {
        OriginalRng r{int32_t(v[0])}; CHECK(r.get_seed()==v[1]);
        r.seed(int32_t(v[0])); CHECK(r.get_seed()==v[1]); ++row;
    }
    OriginalRng chain;
    for (const auto &v:f_raw) { CHECK(chain.next_raw16()==v[0]); ++row; }
    uint32_t digest=2166136261U;
    for (int32_t seed=0;seed<65536;++seed) digest=(digest^OriginalRng(seed).next_raw16())*16777619U;
    CHECK(digest==f_rawHash[0][0]);
    for (const auto &v:f_rng) {
        OriginalRng r{int32_t(v[0])}; auto result=r.next(int32_t(v[1]),int32_t(v[2]));
        CHECK((result.error==Error::InvalidRange)==bool(v[4]));
        CHECK(result.value==v[3]); CHECK(r.get_seed()==v[5]); ++row;
    }
    for (const auto &v:f_hash) { CHECK(time_hash_seed(int32_t(v[0]),int32_t(v[1]),int32_t(v[2]),int32_t(v[3]))==v[4]); ++row; }
    return 0;
}
static int test_time() {
    const char *group="time"; int row=0;
    for (const auto &v:f_time) {
        const GameTime before{int32_t(v[0]),int32_t(v[1]),int32_t(v[2]),int32_t(v[3]),int32_t(v[4])};
        const auto out=advance_minutes(before,int32_t(v[5]));
        CHECK(same_time(out,{int32_t(v[6]),int32_t(v[7]),int32_t(v[8]),int32_t(v[9]),int32_t(v[10])}));
        CHECK(int(day_phase(out))==v[11]); ++row;
    }
    for (const auto &v:f_phase) { GameTime t{}; t.hour=int32_t(v[0]); CHECK(int(day_phase(t))==v[1]); ++row; }
    for (const auto &v:f_schedule) {
        const uint8_t times[4]={uint8_t(v[0]),uint8_t(v[1]),uint8_t(v[2]),uint8_t(v[3])};
        CHECK(schedule_index(times,uint8_t(v[4]))==v[5]); ++row;
    }
    return 0;
}
static int test_world() {
    const char *group="world"; int row=0;
    for (const auto &v:f_tiles) {
        const auto result=is_passable(int32_t(v[0]),TransportMode(v[1]));
        CHECK((result.error==Error::UnknownTile)==bool(v[3]));
        CHECK(result.value==bool(v[2])); CHECK(terrain_speed_class(int32_t(v[0]))==v[4]);
        if (v[0]>=0 && v[0]<256 && v[1]==0) CHECK(is_walkable_tile(uint8_t(v[0]))==result.value);
        ++row;
    }
    for (const auto &v:f_wrap) { CHECK(wrap_coord(int32_t(v[0]))==v[1]); ++row; }
    for (const auto &v:f_direction) {
        const auto delta=direction_delta(Direction(v[0])); CHECK(delta.dx==v[1] && delta.dy==v[2]); ++row;
    }
    CHECK(same_text(direction_name(Direction::North),"north"));
    CHECK(same_text(direction_name(Direction::South),"south"));
    CHECK(same_text(direction_name(Direction::East),"east"));
    CHECK(same_text(direction_name(Direction::West),"west"));
    static uint8_t over[65536], under[65536], small[1024];
    for (int y=0;y<256;y++) for (int x=0;x<256;x++) { over[y*256+x]=uint8_t(x+y*3); under[y*256+x]=uint8_t(x*7+y); }
    for (int y=0;y<32;y++) for (int x=0;x<32;x++) small[y*32+x]=uint8_t(x^y);
    small[1023]=255;
    const MapData local{{13,-1},small,sizeof(small)};
    WorldData world{over,under,sizeof(over),sizeof(under),&local,1};
    for (const auto &v:f_map) {
        const auto result=get_active_map(world,{uint8_t(v[0]),int16_t(v[1])});
        CHECK((result.error!=Error::None)==bool(v[4]));
        if (!v[4]) {
            const auto &m=result.value;
            CHECK(m.id.location==v[0] && m.id.floor==v[1]);
            CHECK(int(m.kind)==v[5] && m.geometry.width==v[6] && m.geometry.height==v[7]);
            CHECK(m.geometry.wraps==bool(v[8]) && m.edge_fill_tile==v[9]);
            CHECK(m.tile_at(int32_t(v[2]),int32_t(v[3]))==v[10]);
        }
        ++row;
    }
    world.overworld_size=1; CHECK(get_active_map(world,{0,0}).error==Error::InvalidMap);
    Position p{}; CHECK(!target_for_step({0,0},{0,32,false},Direction::North,p));
    CHECK(!target_for_step({0,0},{257,32,true},Direction::North,p));
    return 0;
}
static InitialState synthetic_initial() {
    InitialState initial{};
    initial.party.character_count=16; initial.party.party_size=3; initial.party.active_character=255;
    initial.food=321; initial.gold=432; initial.torch_turns=17; initial.karma=77; initial.turns_since_start=12345;
    initial.time={139,13,28,23,59}; initial.position={{15,15},{13,-1}};
    for (uint8_t i=0;i<16;i++) {
        auto &c=initial.party.characters[i];
        c.name[0]='T'; c.name[1]='e'; c.name[2]='s'; c.name[3]='t';
        if (i<10) c.name[4]=char('0'+i);
        else { c.name[4]='1'; c.name[5]=char('0'+i-10); }
        c.gender=11; c.character_class=i==0?'A':'B'; c.status=i%2?'P':'G';
        c.strength=uint8_t(i+10); c.dexterity=uint8_t(i+11); c.intelligence=uint8_t(i+12); c.current_mp=i;
        c.current_hp=uint16_t(100+i); c.max_hp=uint16_t(200+i); c.exp=uint16_t(1000+i); c.level=3; c.months_at_inn=2;
        c.helmet=1; c.armor=2; c.weapon=3; c.shield=4; c.ring=5; c.amulet=6; c.party_status=i<3?0:255;
    }
    return initial;
}
static bool reference_roster(const PartyState &p) {
    if (p.character_count!=16 || p.party_size!=3 || p.active_character!=255) return false;
    for (int i=0;i<16;i++) {
        const auto &c=p.characters[i];
        const int64_t values[]={c.gender,c.character_class,c.status,c.strength,c.dexterity,c.intelligence,c.current_mp,c.current_hp,c.max_hp,c.exp,c.level,c.months_at_inn,c.helmet,c.armor,c.weapon,c.shield,c.ring,c.amulet,c.party_status};
        for (size_t j=0;j<sizeof(values)/sizeof(values[0]);j++) if (values[j]!=f_characters[i][j]) return false;
        for (int j=0;j<9;j++) if (c.name[j]!=f_names[i][j]) return false;
    }
    return true;
}
static int test_state() {
    const char *group="state"; int row=0;
    for (const auto &v:f_party) {
        PartyState p{}; p.character_count=uint8_t(v[0]); p.party_size=int32_t(v[1]);
        constexpr char statuses[]={'D','S','P','G','C'};
        for (uint8_t i=0;i<p.character_count;i++) { p.characters[i].party_status=i%3==1?255:0; p.characters[i].status=statuses[i%5]; }
        CHECK(first_conscious_index(p)==v[2]);
        const auto members=party_members(p); CHECK(members.count==v[3]);
        for (uint8_t i=0;i<members.count;i++) CHECK(members.indices[i]==v[4+i]);
        ++row;
    }
    auto initial=synthetic_initial();
    auto s=create_foundation_state(initial);
    const int64_t actual[]={s.version,s.party.party_size,s.party.active_character,s.food,s.gold,s.torch_turns,s.karma,s.turns_since_start,s.time.year,s.time.month,s.time.day,s.time.hour,s.time.minute,s.position.map.location,s.position.map.floor,s.position.xy.x,s.position.xy.y,int(s.transport)};
    for (size_t i=0;i<sizeof(actual)/sizeof(actual[0]);i++) CHECK(actual[i]==f_initial[0][i]);
    CHECK(s.rng.get_seed()==0);
    for (int i=0;i<16;i++) {
        const auto &c=s.party.characters[i];
        const int64_t actual_char[]={c.gender,c.character_class,c.status,c.strength,c.dexterity,c.intelligence,c.current_mp,c.current_hp,c.max_hp,c.exp,c.level,c.months_at_inn,c.helmet,c.armor,c.weapon,c.shield,c.ring,c.amulet,c.party_status};
        for (size_t j=0;j<sizeof(actual_char)/sizeof(actual_char[0]);j++) CHECK(actual_char[j]==f_characters[i][j]);
        for (int j=0;j<9;j++) CHECK(c.name[j]==f_names[i][j]);
    }
    initial.party.characters[0].name[0]='X'; initial.party.characters[0].current_hp=0; initial.position.xy.x=99;
    CHECK(s.party.characters[0].name[0]=='T' && s.party.characters[0].current_hp==100 && s.position.xy.x==15);
    return 0;
}
static int test_movement() {
    const char *group="movement"; int row=0;
    static uint8_t tiles[65536];
    const char *messages[]={nullptr,"Blocked!","Slow progress!","Very slow!"};
    for (const auto &v:f_movement) {
        for (auto &tile:tiles) tile=uint8_t(v[1]);
        GameState state{}; state.position={{uint8_t(v[3]),uint8_t(v[4])},{uint8_t(v[0]?0:13),0}};
        state.time={139,13,28,23,59}; state.turns_since_start=123; state.rng.seed(0x1234);
        const GameTime before=state.time;
        ActiveMap map{}; map.geometry={uint16_t(v[0]?256:32),uint16_t(v[0]?256:32),bool(v[0])}; map.tiles=tiles; map.edge_fill_tile=255;
        const auto result=resolve_unoccupied_foot_step(state,map,Direction(v[2])); CHECK(result.error==Error::None);
        const auto &r=result.value;
        CHECK(state.position.xy.x==v[5] && state.position.xy.y==v[6]);
        CHECK(r.moved==bool(v[7]) && r.blocked==bool(v[8]) && r.exited_map==bool(v[9]));
        CHECK(int(r.message)==v[10] && same_text(step_message_text(r.message),messages[v[10]]));
        CHECK(r.minutes==v[11] && r.speed_class==v[12] && r.on_bridge==bool(v[13]) && r.on_swamp==bool(v[14]) && r.on_cactus==bool(v[15]));
        CHECK(same_time(state.time,before) && state.turns_since_start==123 && state.rng.get_seed()==0x1234);
        // Preserved slice is the position-only projection: no exit dispatch or turn costs.
        state.position.xy={uint8_t(v[3]),uint8_t(v[4])};
        const auto command=apply_movement_slice(state,map.geometry,{Direction(v[2])},uint8_t(v[1]));
        CHECK(command.movement.moved==r.moved && command.event_count==v[16]);
        CHECK(state.position.xy.x==v[5] && state.position.xy.y==v[6]);
        CHECK(same_time(state.time,before) && state.rng.get_seed()==0x1234 && state.turns_since_start==123);
        CHECK(state.position.map.location==(v[0]?0:13) && state.position.map.floor==0);
        ++row;
    }
    return 0;
}
static int test_sequence() {
    const char *group="sequence"; int row=0;
    auto state=create_foundation_state(synthetic_initial());
    state.rng.seed(0x1234);
    static uint8_t tiles[1024]; const uint8_t choices[]={5,5,5,0x94,4,0x6a,0x2f};
    for (int y=0;y<32;y++) for (int x=0;x<32;x++) tiles[y*32+x]=choices[(x+3*y)%7];
    ActiveMap map{}; map.geometry={32,32,false}; map.tiles=tiles; map.edge_fill_tile=255;
    for (const auto &v:f_sequence) {
        int32_t value=0; StepGeometry move{};
        if (v[0]==0) { auto r=state.rng.next(int32_t(v[1]),int32_t(v[2])); CHECK(r.error==Error::None); value=r.value; }
        else if (v[0]==1) state.time=advance_minutes(state.time,int32_t(v[1]));
        else { auto r=resolve_unoccupied_foot_step(state,map,Direction(v[1])); CHECK(r.error==Error::None); move=r.value; }
        const int64_t actual[]={value,state.position.xy.x,state.position.xy.y,state.time.year,state.time.month,state.time.day,state.time.hour,state.time.minute,state.rng.get_seed(),state.party.party_size,state.party.characters[0].current_hp,state.turns_since_start,state.position.map.location,state.position.map.floor,move.moved,move.blocked,move.exited_map,int(move.message),move.minutes,move.speed_class,move.on_bridge,move.on_swamp,move.on_cactus,0};
        for (size_t j=0;j<sizeof(actual)/sizeof(actual[0]);j++) CHECK(actual[j]==v[j+3]);
        CHECK(reference_roster(state.party));
        CHECK(state.version==1 && state.transport==TransportMode::Foot && state.food==321 && state.gold==432 && state.torch_turns==17 && state.karma==77);
        ++row;
    }
    return 0;
}
int main() {
    const int results[]={test_rng(),test_time(),test_world(),test_state(),test_movement(),test_sequence()};
    // Keep failures nonzero even on hosts that truncate process status to 8 bits.
    for (int i=0;i<6;i++) if (results[i]) return 1;
    printf("All foundation parity fixtures passed. State: %zu bytes; character: %zu bytes.\n",sizeof(GameState),sizeof(CharacterState));
    return 0;
}
