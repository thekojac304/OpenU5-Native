#include "openu5/outdoor.h"
#include "openu5/transport.h"
#include <algorithm>
#include <cstdlib>
namespace openu5 {
namespace {
#include "outdoor_tiles.inc"
int delta(int a,int b,int size){int d=a-b;if(d>size/2)d-=size;if(d< -size/2)d+=size;return d;}
bool land(int t){return t>=0&&size_t(t)<sizeof(outdoor_tile_flags)&&(outdoor_tile_flags[t]&1);}
void event(EventSink sink,GameEventKind kind,const char *text=nullptr){GameEvent e;e.kind=kind;e.text=text;if(sink.emit)sink.emit(sink.context,e);}
int pick(const int *ids,const int *weights,size_t n,Rand rand){int roll=rand(0,255);size_t i=0;while(i<n&&weights[i]<=roll){roll-=weights[i];++i;}return i<n?ids[i]:0;}
ActorPool pool(CommandContext &c){
    auto &o=*c.outdoor;o.enemy_view.clear();o.object_view.clear();
    for(const auto &e:o.enemies)o.enemy_view.push_back({e.slot,e.tile,e.x,e.y,c.game.position.map.floor,0});
    auto *q=c.quest_world;
    if(q&&q->count&&q->read)for(size_t i=0;i<q->count(q->context);++i){auto p=q->read(q->context,i);o.object_view.push_back({p.slot,p.tile,p.x,p.y,p.floor,p.location});}
    auto view=compose_world_pool(0,c.game.position.map.floor,o.enemy_view.data(),o.enemy_view.size(),o.object_view.data(),o.object_view.size());
    for(size_t i=0;i<o.enemies.size();++i)o.enemies[i].slot=o.enemy_view[i].slot;
    if(q&&q->write)for(size_t i=0;i<o.object_view.size();++i){auto p=q->read(q->context,i);if(p.slot!=o.object_view[i].slot){p.slot=o.object_view[i].slot;q->write(q->context,i,p);}}
    return view;
}
bool can_enter(const OutdoorServices &o,const OutdoorEnemy &e,const ActiveMap &map,int x,int y){
    for(const auto &other:o.enemies)if(other.x==x&&other.y==y)return false;
    const auto t=map.tile_at(x,y);return t>=0&&size_t(t)<sizeof(outdoor_tile_flags)&&(outdoor_tile_flags[t]&(e.water?2:1));
}
void drift(OutdoorServices &o,OutdoorEnemy &e,const ActiveMap &m,Rand rand){
    static constexpr int dx[]={0,1,0,-1},dy[]={-1,0,1,0};const auto d=rand(0,3);
    int x=(e.x+dx[d]+m.geometry.width)%m.geometry.width,y=(e.y+dy[d]+m.geometry.height)%m.geometry.height;
    if(can_enter(o,e,m,x,y)){e.x=x;e.y=y;}
}
bool chase(OutdoorServices &o,OutdoorEnemy &e,const ActiveMap &m,int px,int py,Rand rand){
    const auto dx=delta(px,e.x,m.geometry.width),dy=delta(py,e.y,m.geometry.height);
    const bool xfirst=rand(0,1)==1;
    for(int i=0;i<2;++i){bool xaxis=(i==0)==xfirst;int sx=xaxis?((dx>0)-(dx<0)):0,sy=xaxis?0:((dy>0)-(dy<0));if(!sx&&!sy)continue;
        int x=(e.x+sx+m.geometry.width)%m.geometry.width,y=(e.y+sy+m.geometry.height)%m.geometry.height;
        if(x==px&&y==py){return true;}if(can_enter(o,e,m,x,y)){e.x=x;e.y=y;return false;}}
    drift(o,e,m,rand);return false;
}
void ranged(CommandContext &c,Rand rand,EventSink sink){
    if(c.game.transport!=TransportMode::Ship){return;}const int damage=rand(1,30);
    if(damage<c.game.ship_hull)c.game.ship_hull-=damage;else sink_player_ship(c.game,c.turn,rand,sink);
}
}
int32_t spawn_monster_tile(int32_t terrain,int32_t floor,Rand rand){
    static constexpr int ws[]={140,132,136,128,44},wsw[]={72,72,40,38,34},wu[]={132,136},wuw[]={128,128};
    static constexpr int ls[]={192,200,144,152,188,196,208,228,204,212,220,216},lsw[]={60,50,40,30,20,15,15,10,10,3,2,1};
    static constexpr int lu[]={148,144,152,240,244,216,220},luw[]={64,56,56,32,32,8,8};
    int t=terrain&255,id=0;bool under=floor>=128;
    if(t<4||(t>=96&&t<=111)||(t>=212&&t<=215)||(t>=228&&t<=231)){
        if(rand(0,64)>=16)return 0;
        if(under)id=pick(wu,wuw,2,rand);
        else if(t==1&&rand(0,7)==7)id=236;
        else id=pick(ws,wsw,5,rand);
    }else if(t==7)id=rand(0,3)==0?224:0;
    else if(t==4&&floor==255)id=248;
    else if(t==12||t==13||(t>=16&&(t&252)!=48))return 0;
    else id=under?pick(lu,luw,7,rand):pick(ls,lsw,12,rand);
    return id?256+id:0;
}
int32_t outdoor_actor_tile(const CommandContext &c,int32_t x,int32_t y){
    if(!c.outdoor||c.game.position.map.location)return 0;
    for(const auto &e:c.outdoor->enemies)if(e.x==x&&e.y==y)return e.tile;
    return 0;
}
CommandStatus outdoor_start(CommandContext &c,size_t index,const ActiveMap &map,EventSink sink,bool intro){
    auto &o=*c.outdoor;if(!o.combat||!o.resources||index>=o.enemies.size()||&o.combat->game!=&c.game||&o.combat->turn!=&c.turn)return CommandStatus::InvalidContext;
    const auto enemy=o.enemies[index];
    if(c.quest_world&&(!c.quest_world->reserve||!c.quest_world->append||!c.quest_world->reserve(c.quest_world->context,27)))return CommandStatus::NeedsStorage;
    auto resources=*o.resources;resources.remove_enemy=nullptr;
    const auto previous=c.events;c.events=sink;
    const auto status=start_encounter_combat(c,o.combat->combat,resources,enemy.definition,map.tile_at(c.game.position.xy.x,c.game.position.xy.y),-1,CombatDirection::South,intro);
    c.events=previous;
    if(status!=CombatResult::Ok)return status==CombatResult::NeedsActorStorage?CommandStatus::NeedsStorage:CommandStatus::CoreError;
    c.combat_context=o.combat;o.enemies.erase(o.enemies.begin()+ptrdiff_t(index));
    o.encounter_location=c.game.position.map.location;
    o.encounter_floor=c.game.position.map.floor;
    o.encounter_x=enemy.x;o.encounter_y=enemy.y;o.pending_prize=enemy.definition==8;
    o.prize_owner=c.quest_world;
    if(enemy.definition==8&&c.quest_world){
        o.prize={};o.prize.location=c.game.position.map.location;o.prize.floor=c.game.position.map.floor;o.prize.x=enemy.x;o.prize.y=enemy.y;
        o.prize.ship=true;o.prize.tile=292;o.prize.hull=99;o.prize.skiffs=2;
    }
    auto &battle=o.combat->combat;
    battle.victory_context=&o;
    battle.victory_latch=[](void *p){
        auto &s=*static_cast<OutdoorServices*>(p);
        if(!s.prize_owner||!s.prize_owner->append)return;
        if(s.pending_prize)s.prize_owner->append(s.prize_owner->context,s.prize);
        // R-03 (Batch 2): the reference never promotes unclaimed arena
        // treasure to a world object on exit. An unopened chest is simply
        // lost when the party leaves the encounter (game.ts collectSpoils()
        // is a stats-only counter, never a worldObjects.push). The pirate
        // ship prize above is unrelated and unaffected.
        s.pending_prize=false;
    };
    if(battle.victory)battle.victory_latch(battle.victory_context);
    return CommandStatus::Success;
}
CommandStatus outdoor_tick(CommandContext &c,const ActiveMap &m,bool spawn,Rand rand,EventSink sink){
    if(!c.outdoor||!c.outdoor->resources||c.combat||c.turn.time_spell=='T')return CommandStatus::Success;
    auto &o=*c.outdoor;const auto &g=c.game;int px=g.position.xy.x,py=g.position.xy.y;
    if(!o.combat)return CommandStatus::InvalidContext;
    if(o.resources->enemy_count && !o.resources->enemies)return CommandStatus::InvalidContext;
    if(c.quest_world&&c.quest_world->count&&c.quest_world->count(c.quest_world->context)&&!c.quest_world->write)return CommandStatus::InvalidContext;
    o.enemies.erase(std::remove_if(o.enemies.begin(),o.enemies.end(),[&](const auto &e){return std::max(std::abs(delta(e.x,px,m.geometry.width)),std::abs(delta(e.y,py,m.geometry.height)))>22;}),o.enemies.end());
    if(o.enemies.size()<8&&spawn){
        auto origin=[](int p){int v=p&240;return ((p&15)<8?v-16:v)&255;};
        const bool fresh=o.has_chunk_origin&&((px-o.chunk_x)&255)<32&&((py-o.chunk_y)&255)<32;
        const auto ox=fresh?o.chunk_x:origin(px),oy=fresh?o.chunk_y:origin(py);
        int x=-1,y=-1;
        for(int i=0;i<1000;++i){int nx=(rand(0,31)+ox)&255,ny=(rand(0,31)+oy)&255;int dx=(nx-px)&255,dy=(ny-py)&255;if(dx<=6||dx>=250||dy<=6||dy>=250)continue;x=nx%m.geometry.width;y=ny%m.geometry.height;break;}
        if(x>=0){const int t=m.tile_at(x,y);if(t>=0){int sprite=spawn_monster_tile(t,g.position.map.floor,rand);const CombatEnemy *def=nullptr;
            for(size_t i=0;sprite&&i<o.resources->enemy_count;++i)if(o.resources->enemies[i]&&o.resources->enemies[i]->tile==sprite){def=o.resources->enemies[i];break;}
            if(def){OutdoorEnemy next;next.definition=def->index;next.tile=def->tile;next.water=!land(t);next.x=x;next.y=y;if(def->index==8)next.hull=100;
                if(can_enter(o,next,m,x,y)){const auto view=pool(c);auto slot=acquire_actor_slot(view,px,py);if(slot){const auto &victim=view[slot];
                    if(victim.kind==PoolOwnerKind::Enemy)o.enemies.erase(o.enemies.begin()+ptrdiff_t(victim.owner_index));
                    else if(victim.kind==PoolOwnerKind::Object){if(!c.quest_world->erase)return CommandStatus::InvalidContext;c.quest_world->erase(c.quest_world->context,victim.owner_index);}
                    next.slot=slot;o.enemies.push_back(next);
                }}
            }
        }}
    }
    (void)pool(c);
    std::vector<size_t> order;order.reserve(o.enemies.size());for(size_t i=0;i<o.enemies.size();++i){if(o.enemies[i].slot<0)o.enemies[i].slot=0;order.push_back(i);}
    std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return o.enemies[a].slot>o.enemies[b].slot;});
    for(auto i:order){auto &e=o.enemies[i];int dx=delta(px,e.x,m.geometry.width),dy=delta(py,e.y,m.geometry.height);
        bool attacker=(std::abs(dx)==1&&!dy)||(!dx&&std::abs(dy)==1);
        if(!attacker){
            if((e.definition==18||e.definition==39)&&std::abs(dx)<=3&&std::abs(dy)<=3&&rand(0,7)==0){ranged(c,rand,sink);continue;}
            if((e.tile&252)==236){e.phase=(e.phase<0?0:e.phase)^1;if(!e.phase)continue;if(rand(0,1)==0){drift(o,e,m,rand);continue;}}
            else if(e.hull>=0){if(!c.turn.wind)continue;e.wind_counter=((e.wind_counter<0?0:e.wind_counter)+1)&255;}
            attacker=chase(o,e,m,px,py,rand);
        }
        if(!attacker)continue;
        if((e.tile&252)==236){
            if(c.turn.transport_tile==28)return CommandStatus::Success;
            o.enemies.erase(o.enemies.begin()+ptrdiff_t(i));event(sink,GameEventKind::Message,"\nWHIRLPOOL!\n");
            if((c.turn.transport_tile&248)==32)ranged(c,rand,sink);
            c.game.position.map.floor=255;c.game.position.xy={34,18};event(sink,GameEventKind::MapChanged);return CommandStatus::Success;
        }
        return outdoor_start(c,i,m,sink);
    }
    return CommandStatus::Success;
}
}
