#include "openu5/world_terrain.h"
#include "openu5/quest_world.h"
#include "openu5/world_commands.h"
#include <algorithm>
namespace openu5 {
namespace {
bool same(MapId a,MapId b){return a.location==b.location&&a.floor==b.floor;}
const TerrainCell *find(const std::vector<TerrainCell> &v,MapId m,int x,int y){for(auto &c:v)if(same(c.map,m)&&c.x==x&&c.y==y)return &c;return nullptr;}
int layered(const WorldTerrain &t,MapId m,int x,int y,int base){if(t.wiped&&same(m,t.wipe_map))return t.wipe_tile;if(auto *c=find(t.transient,m,x,y))return c->tile;if(auto *c=find(t.persistent,m,x,y))return c->tile;return base;}
bool at(const QuestObject &o,WorldPosition p){return o.location==p.map.location&&o.floor==p.map.floor&&o.x==p.xy.x&&o.y==p.xy.y;}
}
int32_t WorldTerrain::raw(const WorldData &w,MapId m,int32_t x,int32_t y)const{return layered(*this,m,x,y,get_active_map(w,m).value.tile_at(x,y));}
int32_t WorldTerrain::effective(const WorldData &w,MapId m,int32_t x,int32_t y)const{auto base=get_active_map(w,m).value.tile_at(x,y);if(auto *c=find(hourly,m,x,y))base=c->tile;return layered(*this,m,x,y,base);}
TerrainSample WorldTerrain::inspect(const WorldData &w,MapId m,int32_t x,int32_t y)const{
    TerrainSample s{};const auto map=get_active_map(w,m);if(map.error!=Error::None)return s;
    s.base=map.value.tile_at(x,y);if(auto *c=find(hourly,m,x,y)){s.hourly=true;s.hourly_tile=c->tile;}
    if(auto *c=find(persistent,m,x,y)){s.persistent=true;s.persistent_tile=c->tile;}
    if(auto *c=find(transient,m,x,y)){s.transient=true;s.transient_tile=c->tile;}
    s.wiped=wiped&&same(m,wipe_map);s.effective=effective(w,m,x,y);return s;
}
void WorldTerrain::set(MapId m,int32_t x,int32_t y,int32_t tile,bool permanent,const char *caller){
    auto &v=permanent?persistent:transient;int old=kOffMap;
    for(auto &c:v)if(same(c.map,m)&&c.x==x&&c.y==y){old=c.tile;if(writes.emit)writes.emit(writes.context,{m,x,y,old,tile,permanent,caller});c.tile=tile;return;}
    if(writes.emit)writes.emit(writes.context,{m,x,y,old,tile,permanent,caller});
    v.push_back({m,x,y,tile});
}
void WorldTerrain::remove_boarded(MapId m,int32_t x,int32_t y,int32_t tile){persistent.erase(std::remove_if(persistent.begin(),persistent.end(),[&](auto &c){return same(c.map,m)&&c.x==x&&c.y==y&&c.tile==tile;}),persistent.end());}
void WorldTerrain::clear_residence(){transient.clear();hourly.clear();wiped=false;}
void WorldTerrain::refresh(const WorldData &w,const GameState &g){
    hourly.clear();auto m=g.position.map;if(!m.location||(g.time.hour>=5&&g.time.hour<20))return;
    auto base=get_active_map(w,m);if(base.error!=Error::None)return;
    for(int y=0;y<31;++y)for(int x=0;x<32;++x)if(base.value.tile_at(x,y)==135)hourly.push_back({m,x,y+1,base.value.tile_at(x,y+1)^221});
    if((base.value.tile_at(g.position.xy.x,g.position.xy.y)&254)==72)return;
    for(int y=0;y<32;++y)for(int x=0;x<32;++x)if((base.value.tile_at(x,y)&254)==72)hourly.push_back({m,x,y,3});
}
TransportServices world_transport_services(CommandContext &ctx){
    if(!ctx.terrain||!ctx.quest_world||!ctx.quest_world->count||!ctx.quest_world->read||!ctx.quest_world->erase)return {};
    TransportServices s;s.context=&ctx;
    s.tile_at=[](void *p,int32_t x,int32_t y){auto &c=*static_cast<CommandContext*>(p);auto m=c.game.position.map;auto tile=get_active_map(c.world,m).value.tile_at(x,y);return open_door_tile(c,m,x,y,quest_world_tile(c,m,x,y,tile));};
    s.horse_owned=[](void *p,WorldPosition pos){auto &c=*static_cast<CommandContext*>(p);if(!pos.map.location||!c.actors)return false;for(size_t i=0;i<c.actors->count;++i){auto &a=c.actors->actors[i];if(a.location==pos.map.location&&a.z==pos.map.floor&&a.x==pos.xy.x&&a.y==pos.xy.y&&a.schedule.dialog)return true;}return false;};
    s.ship_at=[](void *p,WorldPosition pos,int32_t &hull,int32_t &skiffs){auto *q=static_cast<CommandContext*>(p)->quest_world;if(!q||!q->count||!q->read)return false;for(size_t i=0;i<q->count(q->context);++i){auto o=q->read(q->context,i);if(at(o,pos)){if(!o.ship)return false;hull=o.hull;skiffs=o.skiffs;return true;}}return false;};
    s.reserve=[](void *p,bool ship){auto &c=*static_cast<CommandContext*>(p);auto *q=c.quest_world;if(!c.terrain||!q||!q->count||!q->read||!q->erase||(ship&&(!q->append||!q->reserve)))return false;c.terrain->persistent.reserve(c.terrain->persistent.size()+1);return !ship||q->reserve(q->context,1);};
    s.remove_boarded=[](void *p,WorldPosition pos,int32_t tile){auto &c=*static_cast<CommandContext*>(p);auto &q=*c.quest_world;for(size_t i=0;i<q.count(q.context);++i)if(at(q.read(q.context,i),pos)){q.erase(q.context,i);return;}c.terrain->remove_boarded(pos.map,pos.xy.x,pos.xy.y,tile);};
    s.drop=[](void *p,WorldPosition pos,int32_t tile){static_cast<CommandContext*>(p)->terrain->set(pos.map,pos.xy.x,pos.xy.y,tile,true,"transport.drop");};
    s.park_ship=[](void *p,WorldPosition pos,int32_t tile,int32_t hull,int32_t skiffs){auto &q=*static_cast<CommandContext*>(p)->quest_world;QuestObject o;o.location=pos.map.location;o.floor=pos.map.floor;o.x=pos.xy.x;o.y=pos.xy.y;o.tile=tile;o.hull=hull;o.skiffs=skiffs;o.ship=true;q.append(q.context,o);};
    return s;
}
}
