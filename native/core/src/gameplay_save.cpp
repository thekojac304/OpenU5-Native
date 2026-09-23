#include "openu5/gameplay_save.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
namespace openu5::save {
namespace {
bool finite_int(const Json &v,double lo,double hi){return v.kind==Json::Number&&std::isfinite(v.number)&&std::floor(v.number)==v.number&&v.number>=lo&&v.number<=hi;}
}
void preserve_new_journey_template_bytes(Gam &gam,const uint8_t *base,size_t length){
    constexpr size_t begin=0x6b4,end=0x6bc;
    if(!base||length<kGamSize)return;
    std::copy(base+begin,base+end,gam.begin()+begin);
}
void capture_terrain(const WorldTerrain &t,Json &s){
    auto &v=s["mapOverrides"];v=Json::object();for(auto &c:t.persistent){char key[64];std::snprintf(key,sizeof(key),"%u:%d:%d:%d",unsigned(c.map.location),int(c.map.floor),int(c.x),int(c.y));v[key]=Json(c.tile);}
}
Error restore_terrain(const Json &s,WorldTerrain &t){
    auto &v=s["mapOverrides"];if(v.kind!=Json::Null&&v.kind!=Json::Object)return Error::NativeDomain;std::vector<TerrainCell> cells;
    for(size_t i=0;i<v.keys.size();++i){auto &k=v.keys[i];std::string key(k.begin(),k.end());int loc=0,floor=0,x=0,y=0,n=0;if(std::sscanf(key.c_str(),"%d:%d:%d:%d%n",&loc,&floor,&x,&y,&n)!=4||size_t(n)!=key.size()||loc<0||loc>255||floor<-1||floor>255||x<0||x>255||y<0||y>255)return Error::NativeDomain;auto &tile=v.values[i];if(tile.kind!=Json::Number||!std::isfinite(tile.number)||std::floor(tile.number)!=tile.number||tile.number<0||tile.number>511)return Error::NativeDomain;cells.push_back({{uint8_t(loc),int16_t(floor)},x,y,int32_t(tile.integer())});}
    t.persistent=std::move(cells);t.clear_residence();return Error::None;
}
void capture_gameplay(const CommandState &c,const OutdoorServices &o,Json &s){
    s["overworldEnemies"]=Json::array();
    for(auto &e:o.enemies){Json v=Json::object();if(e.slot>=0)v["slot"]=Json(e.slot);v["defIndex"]=Json(e.definition);v["tile"]=Json(e.tile);v["water"]=Json(e.water);v["x"]=Json(e.x);v["y"]=Json(e.y);if(e.hull>=0)v["hull"]=Json(e.hull);if(e.phase>=0)v["phase"]=Json(e.phase);if(e.wind_counter>=0)v["windCtr"]=Json(e.wind_counter);s["overworldEnemies"].values.push_back(std::move(v));}
    s["openDoors"]=Json::array();auto &d=c.door;if(d.turns>0){Json v=Json::object();v["location"]=Json(d.map.location);v["floor"]=Json(d.map.floor);v["x"]=Json(d.x);v["y"]=Json(d.y);v["tile"]=Json(d.tile);v["turnsLeft"]=Json(d.turns);s["openDoors"].values.push_back(std::move(v));}
    if(o.has_chunk_origin){s["chunkOrigin"]=Json::object();s["chunkOrigin"]["x"]=Json(o.chunk_x);s["chunkOrigin"]["y"]=Json(o.chunk_y);}else s.erase("chunkOrigin");
}
Error restore_gameplay(const Json &s,CommandState &c,OutdoorServices &o){
    auto valid=[](const Json &v,int lo,int hi){return v.kind==Json::Number&&std::isfinite(v.number)&&std::floor(v.number)==v.number&&v.number>=lo&&v.number<=hi;};
    std::vector<OutdoorEnemy> enemies;const auto &list=s["overworldEnemies"];
    if(list.kind!=Json::Null&&list.kind!=Json::Array)return Error::NativeDomain;
    if(list.values.size()>31)return Error::NativeDomain;
    for(auto &v:list.values){if(v.kind!=Json::Object)return Error::NativeDomain;
        for(auto key:{"defIndex","tile","x","y"})if(!valid(v[key],0,std::string(key)=="tile"?511:255))return Error::NativeDomain;
        for(auto key:{"slot","hull","phase","windCtr"})if(v.has(key)&&!valid(v[key],0,std::string(key)=="slot"?31:2147483647))return Error::NativeDomain;
        if(v["water"].kind!=Json::Bool)return Error::NativeDomain;
        OutdoorEnemy e;e.definition=int(v["defIndex"].integer());e.tile=int(v["tile"].integer());e.x=int(v["x"].integer());e.y=int(v["y"].integer());e.slot=int(v["slot"].integer(-1));e.hull=int(v["hull"].integer(-1));e.phase=int(v["phase"].integer(-1));e.wind_counter=int(v["windCtr"].integer(-1));e.water=v["water"].truth();enemies.push_back(e);
    }
    CommandState::Door door;const auto &doors=s["openDoors"];if(doors.kind!=Json::Null&&doors.kind!=Json::Array)return Error::NativeDomain;
    if(!doors.values.empty()){auto &d=doors.at(0);for(auto key:{"location","x","y"})if(!valid(d[key],0,255))return Error::NativeDomain;if(!valid(d["floor"],-1,255)||!valid(d["turnsLeft"],0,2147483647)||(d.has("tile")&&!valid(d["tile"],0,511)))return Error::NativeDomain;door.map={uint8_t(d["location"].integer()),int16_t(d["floor"].integer())};door.x=int(d["x"].integer());door.y=int(d["y"].integer());door.tile=int(d["tile"].integer(184));door.turns=int(d["turnsLeft"].integer());}
    bool origin=s.has("chunkOrigin");int x=0,y=0;if(origin){auto &v=s["chunkOrigin"];if(!valid(v["x"],0,255)||!valid(v["y"],0,255))return Error::NativeDomain;x=int(v["x"].integer());y=int(v["y"].integer());}
    c.door=door;o.enemies=std::move(enemies);o.has_chunk_origin=origin;o.chunk_x=x;o.chunk_y=y;return Error::None;
}
void capture_world_objects(const QuestWorldServices &s,Json &out){
    auto &arr=out["worldObjects"];arr=Json::array();
    if(!s.count||!s.read)return;
    for(size_t i=0,n=s.count(s.context);i<n;++i){
        const auto o=s.read(s.context,i);Json v=Json::object();
        v["location"]=Json(o.location);v["floor"]=Json(o.floor);v["x"]=Json(o.x);v["y"]=Json(o.y);v["tile"]=Json(o.tile);v["plotZ"]=Json(o.plot_z);
        v["item"]=Json(int(o.item));v["plot"]=Json(o.plot);v["shadowlord"]=Json(o.shadowlord);v["search"]=Json(o.search);v["loot"]=Json(o.loot);
        v["itemId"]=Json(o.item_id);v["quality"]=Json(o.quality);v["chest"]=Json(o.chest);v["prop"]=Json(o.prop);v["contents"]=Json(o.contents);v["trapped"]=Json(o.trapped);
        v["slot"]=Json(o.slot);v["hull"]=Json(o.hull);v["skiffs"]=Json(o.skiffs);v["ship"]=Json(o.ship);v["torch"]=Json(o.torch);
        arr.values.push_back(std::move(v));
    }
}
namespace {
// One "worldObjects" entry, all or nothing.
bool decode_world_object(const Json &v,QuestObject &o){
    if(v.kind!=Json::Object)return false;
    for(auto key:{"location","x","y"})if(!finite_int(v[key],0,255))return false;
    if(!finite_int(v["floor"],-1,255)||!finite_int(v["tile"],0,2047)||!finite_int(v["plotZ"],-1,255))return false;
    if(!finite_int(v["item"],0,8))return false;
    for(auto key:{"plot","shadowlord","search","loot","chest","prop","trapped","ship","torch"})if(v[key].kind!=Json::Bool)return false;
    if(!finite_int(v["itemId"],0,1023)||!finite_int(v["quality"],0,1023)||!finite_int(v["contents"],0,1023))return false;
    if(!finite_int(v["slot"],-1,255)||!finite_int(v["hull"],0,255)||!finite_int(v["skiffs"],0,255))return false;
    o=QuestObject{};
    o.location=int32_t(v["location"].integer());o.floor=int32_t(v["floor"].integer());o.x=int32_t(v["x"].integer());o.y=int32_t(v["y"].integer());
    o.tile=int32_t(v["tile"].integer());o.plot_z=int32_t(v["plotZ"].integer());o.item=PlotItem(v["item"].integer());
    o.plot=v["plot"].truth();o.shadowlord=v["shadowlord"].truth();o.search=v["search"].truth();o.loot=v["loot"].truth();
    o.item_id=int32_t(v["itemId"].integer());o.quality=int32_t(v["quality"].integer());
    o.chest=v["chest"].truth();o.prop=v["prop"].truth();o.contents=int32_t(v["contents"].integer());o.trapped=v["trapped"].truth();
    o.slot=int32_t(v["slot"].integer());o.hull=int32_t(v["hull"].integer());o.skiffs=int32_t(v["skiffs"].integer());
    o.ship=v["ship"].truth();o.torch=v["torch"].truth();
    return true;
}
}
Error validate_world_objects(const Json &state){
    const auto &arr=state["worldObjects"];
    if(arr.kind==Json::Null)return Error::None;
    if(arr.kind!=Json::Array)return Error::NativeDomain;
    QuestObject o;
    for(auto &v:arr.values)if(!decode_world_object(v,o))return Error::NativeDomain;
    return Error::None;
}
// The reference (`game/src/core/state.ts` WorldObject doc, `deserialize`) restores
// this array verbatim on load with no re-hydration pass: interior/underworld
// entries that would normally be regenerated by a map-entry transition are
// preserved as-is because a load is not a transition. Whole-vector capture is
// therefore correct, not an ephemeral-state overreach.
Error restore_world_objects(const Json &state,QuestWorldServices &s){
    const auto &arr=state["worldObjects"];
    if(arr.kind==Json::Null)return Error::None; // absent (old save): caller already cleared the pool
    if(arr.kind!=Json::Array)return Error::NativeDomain;
    if(!s.reserve||!s.append)return Error::None;
    std::vector<QuestObject> objects;objects.reserve(arr.values.size());
    for(auto &v:arr.values){QuestObject o;if(!decode_world_object(v,o))return Error::NativeDomain;objects.push_back(o);}
    if(!s.reserve(s.context,objects.size()))return Error::NativeDomain;
    for(auto &o:objects)s.append(s.context,o);
    return Error::None;
}
void capture_dungeon(const DungeonState &d,Json &out){
    if(!d.active){out.erase("dungeon");return;}
    Json v=Json::object();
    v["dungeon"]=Json(int(d.pos.dungeon));v["floor"]=Json(int(d.pos.floor));v["x"]=Json(int(d.pos.x));v["y"]=Json(int(d.pos.y));
    v["facing"]=Json(int(d.pos.facing));
    auto &cells=v["cells"];cells=Json::array();cells.values.reserve(512);for(auto c:d.cells)cells.values.emplace_back(int(c));
    auto &revealed=v["revealed"];revealed=Json::array();revealed.values.reserve(64);for(auto b:d.revealed)revealed.values.emplace_back(int(b));
    Json w=Json::object();
    w["bank"]=Json(int(d.wanderer.bank));w["type"]=Json(int(d.wanderer.type));w["x"]=Json(int(d.wanderer.x));w["y"]=Json(int(d.wanderer.y));
    w["floor"]=Json(int(d.wanderer.floor));w["attr"]=Json(int(d.wanderer.attr));w["hidden"]=Json(d.wanderer.hidden);
    w["prevX"]=Json(int(d.wanderer.prev_x));w["prevY"]=Json(int(d.wanderer.prev_y));
    v["wanderer"]=std::move(w);out["dungeon"]=std::move(v);
}
// pos.floor/x/y and facing are validated to the tight ranges the engine
// actually indexes with (dungeon_cell's 8x8x8 grid; the 4-entry facing delta
// tables) rather than their uint8 storage width: those two are unguarded
// array indices downstream (dungeon.cpp `offset()`, quest_world.cpp's
// facing-delta lookup), so an out-of-range value would be an OOB access, not
// just a domain oddity like the other fields below.
// pos.dungeon is the session's location, 33-40 (0x21-0x28, Deceit..Doom): the
// only values 1988 runs the dungeon loop for with a map behind it, and the
// only ones dungeon_load() is ever given. It keys the room maps
// (dungeon_room_map), the rooms-cleared bits, the exit position
// (exit_dungeon: locations[dungeon-1]) and the wall variant. Below 33 a room
// cell yields a negative map index, which dungeon_encounter runs as a corridor
// fight; above 40 it names no arena. The exit lands at (0,0) or at a town's
// entrance (Batch 28, H-166).
// The Rel Tym every-other-turn toggle is not saved: in 1988 it is the local
// [bp-4] of the dungeon loop, zeroed at DUNGEON.OVL:0x0E40 whenever the session
// is (re)entered, a load included (Batch 26).
Error restore_dungeon(const Json &state,DungeonState &d){
    const auto &v=state["dungeon"];
    if(v.kind==Json::Null){d=DungeonState{};return Error::None;}
    if(v.kind!=Json::Object)return Error::NativeDomain;
    if(!finite_int(v["dungeon"],33,40)||!finite_int(v["floor"],0,7)||!finite_int(v["x"],0,7)||!finite_int(v["y"],0,7)||
       !finite_int(v["facing"],0,3))return Error::NativeDomain;
    if(v["cells"].kind!=Json::Array||v["cells"].values.size()!=512)return Error::NativeDomain;
    for(auto &c:v["cells"].values)if(!finite_int(c,0,255))return Error::NativeDomain;
    if(v["revealed"].kind!=Json::Array||v["revealed"].values.size()!=64)return Error::NativeDomain;
    for(auto &b:v["revealed"].values)if(!finite_int(b,0,255))return Error::NativeDomain;
    const auto &w=v["wanderer"];if(w.kind!=Json::Object)return Error::NativeDomain;
    for(auto key:{"bank","type","x","y","floor","attr","prevX","prevY"})if(!finite_int(w[key],0,255))return Error::NativeDomain;
    if(w["hidden"].kind!=Json::Bool)return Error::NativeDomain;
    DungeonState out;
    out.pos.dungeon=uint8_t(v["dungeon"].integer());out.pos.floor=uint8_t(v["floor"].integer());out.pos.x=uint8_t(v["x"].integer());out.pos.y=uint8_t(v["y"].integer());
    out.pos.facing=DungeonFacing(v["facing"].integer());
    for(size_t i=0;i<512;++i)out.cells[i]=uint8_t(v["cells"].at(i).integer());
    for(size_t i=0;i<64;++i)out.revealed[i]=uint8_t(v["revealed"].at(i).integer());
    out.wanderer.bank=uint8_t(w["bank"].integer());out.wanderer.type=uint8_t(w["type"].integer());out.wanderer.x=uint8_t(w["x"].integer());out.wanderer.y=uint8_t(w["y"].integer());
    out.wanderer.floor=uint8_t(w["floor"].integer());out.wanderer.attr=uint8_t(w["attr"].integer());out.wanderer.hidden=w["hidden"].truth();
    out.wanderer.prev_x=uint8_t(w["prevX"].integer());out.wanderer.prev_y=uint8_t(w["prevY"].integer());
    out.active=true;d=out;return Error::None;
}
}

