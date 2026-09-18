#include "openu5/world_commands.h"
#include "openu5/outdoor.h"
#include "openu5/loot.h"
#include "openu5/dialogue_orchestration.h"
#include <algorithm>
#include <vector>
namespace openu5 {
namespace {
void event(EventSink sink,GameEventKind k,const char *text=nullptr){GameEvent e;e.kind=k;e.text=text;if(sink.emit)sink.emit(sink.context,e);}
bool here(const QuestObject &o,const GameState &g,int x,int y){return o.location==g.position.map.location&&o.floor==g.position.map.floor&&o.x==x&&o.y==y;}
bool objects(const QuestWorldServices *q){return q&&q->count&&q->read;}
size_t chest_at(QuestWorldServices &q,const GameState &g,int x,int y,bool open){
    size_t count=q.count(q.context),chest=count,pile=count;
    for(size_t i=0;i<count;++i){auto o=q.read(q.context,i);if(!here(o,g,x,y))continue;if(o.chest&&chest==count)chest=i;if(o.loot&&(o.item_id==1||(open&&o.item_id==14)))pile=i;}
    return chest<count?chest:pile;
}
WorldCommandResult open_chest(CommandContext &c,size_t index,EventSink sink,Rand rand){
    auto &q=*c.quest_world;auto &g=c.game;auto chest=q.read(q.context,index);
    if(chest.loot&&chest.item_id==14){event(sink,GameEventKind::Message,"Can't!");return {};}
    if(!q.reserve||!q.append||!q.erase||!q.write)return {CommandStatus::InvalidContext};
    // The actor pool can accept at most 31 grants. Reserve before trap/loot RNG.
    if(!q.reserve(q.context,31))return {CommandStatus::NeedsStorage};
    if(g.position.map.location>=1&&g.position.map.location<=32)g.karma=uint8_t(g.karma>2?g.karma-2:0);
    const auto raw=chest.contents;
    if(raw&128){const auto trap=chest_trap(g,g.position.map.location,g.party.active_character==255?0:g.party.active_character,rand);
        event(sink,GameEventKind::Sfx,"dungeon-trap");event(sink,GameEventKind::Message,"Trapped!");event(sink,GameEventKind::Message,trap.message);
        if(trap.damage_mask){GameEvent e;e.kind=GameEventKind::PoisonTick;for(uint8_t i=0;i<6;++i)if(trap.damage_mask&(1<<i))e.slots[e.slot_count++]=i;if(sink.emit)sink.emit(sink.context,e);}
    }
    std::vector<LootGrant> grants;
    chest_loot(raw&127,rand,{&grants,[](void *p,LootGrant g){static_cast<std::vector<LootGrant>*>(p)->push_back(g);}});
    std::vector<PoolEntity> enemies,items;
    if(c.outdoor)for(auto &e:c.outdoor->enemies)enemies.push_back({e.slot,e.tile,e.x,e.y,g.position.map.floor,0});
    for(size_t i=0;i<q.count(q.context);++i){auto o=q.read(q.context,i);items.push_back({o.slot,o.tile,o.x,o.y,o.floor,o.location});}
    const auto view=compose_world_pool(chest.location,chest.floor,enemies.data(),enemies.size(),items.data(),items.size());
    if(c.outdoor)for(size_t i=0;i<enemies.size();++i)c.outdoor->enemies[i].slot=enemies[i].slot;
    for(size_t i=0;i<items.size();++i){auto o=q.read(q.context,i);if(o.slot!=items[i].slot){o.slot=items[i].slot;q.write(q.context,i,o);}}
    uint32_t used=0;for(size_t i=1;i<view.size();++i)if(view[i].tile||view[i].kind!=PoolOwnerKind::None)used|=uint32_t(1)<<i;
    std::vector<int> placed;
    for(auto grant:grants){auto slot=find_free_actor_slot(used);if(!slot)break;used|=uint32_t(1)<<slot;
        QuestObject o;o.location=chest.location;o.floor=chest.floor;o.x=chest.x;o.y=chest.y;o.slot=slot;o.tile=grant.id&255;o.loot=true;o.item_id=grant.id;o.quality=grant.quantity&255;o.contents=o.quality;q.append(q.context,o);placed.push_back(grant.id);}
    if(placed.empty())event(sink,GameEventKind::Message,"Chest empty!");
    else {event(sink,GameEventKind::Message,"Found:");for(int id:placed)event(sink,GameEventKind::Message,loot_open_line(id));event(sink,GameEventKind::MapChanged);}
    q.erase(q.context,index);event(sink,GameEventKind::MapChanged);return {CommandStatus::Success,true};
}
}
int32_t open_door_tile(const CommandContext &c,MapId map,int32_t x,int32_t y,int32_t tile){
    const auto &d=c.commands.door;if(d.turns<=0||d.map.location!=map.location||d.map.floor!=map.floor||d.x!=x||d.y!=y)return tile;
    auto *q=c.quest_world;if(objects(q))for(size_t i=0;i<q->count(q->context);++i){auto o=q->read(q->context,i);if(o.location==map.location&&o.floor==map.floor&&o.x==x&&o.y==y&&!o.loot&&!o.search)return tile;}
    return 68;
}
WorldCommandResult world_interaction(CommandContext &c,Command cmd,const ActiveMap &map,EventSink sink,Rand rand){
    const auto d=direction_delta(cmd.direction);auto &g=c.game;int x=g.position.xy.x+d.dx,y=g.position.xy.y+d.dy;if(map.geometry.wraps){x&=255;y&=255;}
    auto *q=c.quest_world;const auto tile=map.tile_at(x,y);auto say=[&](const char *text){event(sink,GameEventKind::Message,text);};
    auto write=[&](int tx,int ty,int t){q->volatile_tile(q->context,tx,ty,t);};
    if(cmd.kind==CommandKind::Attack){
        const int loc=g.position.map.location,transport=c.turn.transport_tile,under=map.tile_at(g.position.xy.x,g.position.xy.y);
        if(under<4&&(!loc?((transport&252)==40||(transport&254)==20):transport!=28)){say("On foot!\n");return {};}
        if(!loc){if(c.outdoor)for(size_t i=0;i<c.outdoor->enemies.size();++i){auto &e=c.outdoor->enemies[i];if(e.x==x&&e.y==y)return {outdoor_start(c,i,map,sink,false)};}say("Nothing to attack!\n");return {};}
        int raw=q&&q->tile_at?q->tile_at(q->context,x,y):get_active_map(c.world,g.position.map).value.tile_at(x,y);
        if(raw==157){if(!q||!q->volatile_tile)return {CommandStatus::InvalidContext};write(x,y,159);say("Broken!\n");event(sink,GameEventKind::Sfx,"mirror-break");event(sink,GameEventKind::MapChanged);return {};}
        NpcActor *npc=nullptr;if(c.actors)for(size_t i=0;i<c.actors->count;++i){auto &n=c.actors->actors[i];if(n.location==loc&&n.z==g.position.map.floor&&n.x==x&&n.y==y){npc=&n;break;}}
        int t=npc?npc->schedule.type:0,fam=t&252;if(!npc||t<64||(t>=232&&t<240)||fam==180){say("Nothing to attack!\n");return {};}
        if(t<128){g.karma=uint8_t(g.karma>5?g.karma-5:0);event(sink,GameEventKind::PartyChanged);dialogue_alarm(*c.actors,uint8_t(loc),rand);}else if(fam==216)dialogue_alarm(*c.actors,uint8_t(loc),rand);
        if(raw==132||raw==133||raw==159||raw==171){if(t==120){say("Missed!\n");return {};}say("Murdered!\n");g.karma=uint8_t(g.karma>5?g.karma-5:0);event(sink,GameEventKind::PartyChanged);auto copy=*npc;dialogue_despawn(g,c.actors,copy);return {};}
        return {town_attack_commit(c,*npc,false,sink)};
    }
    if(cmd.kind==CommandKind::Open){
        if(objects(q)){auto index=chest_at(*q,g,x,y,true);if(index<q->count(q->context))return open_chest(c,index,sink,rand);}
        int raw=q&&q->tile_at?q->tile_at(q->context,x,y):get_active_map(c.world,g.position.map).value.tile_at(x,y);
        if(raw==184||raw==186){c.commands.door={g.position.map,x,y,raw,4};say("Opened!");return {CommandStatus::Success,true};}
        say(raw==175?"It's open!":raw==153?"Too heavy!":raw==185||raw==187||raw==151||raw==152?"Locked!":"Nothing to open!");return {};
    }
    if(cmd.kind==CommandKind::Push){
        const bool push=tile==91||(tile>=144&&tile<=147)||(tile>=165&&tile<=166)||(tile>=168&&tile<=169)||(tile>=173&&tile<=175)||(tile>=180&&tile<=183);
        if(!push){say("Won't budge!\n");return {};}
        if(!q||!q->volatile_tile)return {CommandStatus::InvalidContext};
        int fill=(tile&252)==180?69:68,dx=x+d.dx,dy=y+d.dy,dest=map.tile_at(dx,dy),under=map.tile_at(g.position.xy.x,g.position.xy.y);
        bool pull=dest!=fill;if(pull&&under!=fill){say("Won't budge\n");return {};}
        const int facing[]={0,2,1,3};int oriented=((tile&252)==144||(tile&252)==180)?(tile&252)+(facing[int(cmd.direction)]^(pull?2:0)):tile;
        say(pull?"Pulled!\n":"Pushed!\n");write(pull?g.position.xy.x:dx,pull?g.position.xy.y:dy,oriented);write(x,y,pull?under:dest);g.position.xy={uint8_t(x),uint8_t(y)};return {CommandStatus::Success,true,false,true};
    }
    if(cmd.kind!=CommandKind::Jimmy)return {CommandStatus::Unsupported};
    if(g.keys<=0){say("No Keys!");return {};}
    if(!g.party.character_count)return {CommandStatus::InvalidContext};
    int member=g.party.active_character<g.party.character_count?g.party.active_character:0,dex=g.party.characters[member].dexterity;
    auto broke=[&](){say("Key broke!\n");g.keys=std::max<int32_t>(0,g.keys-1);};
    if(tile==151||tile==152){broke();return {};}
    if(tile==185||tile==187){
        if(!q||!q->volatile_tile)return {CommandStatus::InvalidContext};
        if(dex>rand(0,29)){say("Unlocked!\n");write(x,y,tile-1);return {CommandStatus::Success,true,true};}broke();return {};
    }
    if(tile==132||tile==133){
        NpcActor *npc=nullptr;if(c.actors)for(size_t i=0;i<c.actors->count;++i){auto &n=c.actors->actors[i];if(n.location==g.position.map.location&&n.z==g.position.map.floor&&n.x==x&&n.y==y){npc=&n;break;}}
        if(g.position.map.location<128&&!npc){say("No one is there!\n");return {};}
        if(g.position.map.location>=127&&(!q||!q->volatile_tile))return {CommandStatus::InvalidContext};
        if(dex<=rand(0,29)){broke();return {};}
        if(g.position.map.location>=127){say("Unlocked\n");write(x,y,68);return {CommandStatus::Success,true,true};}
        if(!npc){say("Couldn't find this npc\n\n");return {};}
        npc->schedule.dialog=0;const auto loc=g.position.map.location;
        if(loc<1||loc>32)return {CommandStatus::InvalidContext};
        const auto mask=uint32_t(1)<<npc->schedule.slot;
        if(!(g.npc_dead[loc-1]&mask)){say("\n\"I thank thee!\"\n");g.karma=uint8_t(std::min(99,int(g.karma)+2));event(sink,GameEventKind::PartyChanged);}g.npc_dead[loc-1]|=mask;return {};
    }
    if(objects(q)){auto index=chest_at(*q,g,x,y,false);if(index<q->count(q->context)){
        if(!q->write){return {CommandStatus::InvalidContext};}auto chest=q->read(q->context,index);int threshold=(uint16_t((chest.contents&127)-dex+30)>>1)&255;
        if(!(chest.contents&128)||rand(1,30)<=threshold){broke();return {};}
        say("Success!\n");chest.contents&=127;chest.trapped=false;q->write(q->context,index,chest);return {};
    }}
    say("No lock!\n");return {};
}
}
