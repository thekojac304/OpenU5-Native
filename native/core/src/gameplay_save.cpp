#include "openu5/gameplay_save.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace openu5::save {
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
}

