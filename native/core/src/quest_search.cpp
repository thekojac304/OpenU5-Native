#include "openu5/quest_world.h"
#include "openu5/loot.h"
#include "openu5/transport.h"
#include <algorithm>
#include <string>
namespace openu5 {
int32_t search_at(GameState &g,TurnState &t,const SearchObject *objects,size_t count,int32_t x,int32_t y,bool occupied){
    if(!objects)return -1;
    for(size_t i=0;i<count && i<113;++i){const auto &o=objects[i];if(o.location!=g.position.map.location||o.floor!=g.position.map.floor||o.x!=x||o.y!=y)continue;
        bool findable=i==13?g.keys==0&&!occupied:i==14?g.time.day!=t.skull_tree_day:i==15?g.equipment_quantities[39]==0&&!occupied:!(g.quest.search_found[i/8]&(1u<<(i%8)));
        if(!findable)continue;
        if(i==14)t.skull_tree_day=g.time.day;else if(i!=13 && i!=15){g.quest.search_found[i/8]|=uint8_t(1u<<(i%8));g.quest.search_present[i/8]|=uint8_t(1u<<(i%8));}return int32_t(i);
    }return -1;
}
int32_t apply_search_grant(GameState &g,QuestWorldServices &s,int32_t id,int32_t quality){
    if(id==25){if(quality>=0 && size_t(quality)<s.moonstone_count){s.moonstones[quality].buried=false;s.moonstones[quality].location=255;}return 1;}
    if(id==4 && quality==255){g.hms_cape=true;return 1;}
    if(id==7){int n=quality>127?quality&127:quality;auto &qty=quality>127?g.skull_keys:g.keys;qty=int32_t(std::min<int64_t>(99,int64_t(qty)+n));return n;}
    if(id==5 || id==6 || (id>=9&&id<=12)){if(quality<0 || quality>=g.equipment_count)return 0;int n=quality==27||quality==29?5:1;g.equipment_quantities[quality]=int32_t(std::min<int64_t>(99,int64_t(g.equipment_quantities[quality])+n));return n;}
    return quality; // Reference intentionally grants no other Search item kinds.
}
namespace {
void emit(EventSink s,GameEventKind k,const char *text=nullptr){GameEvent e;e.kind=k;e.text=text;if(s.emit)s.emit(s.context,e);}
bool at(const QuestObject &o,const GameState &g,int x,int y){return o.location==g.position.map.location&&o.floor==g.position.map.floor&&o.x==x&&o.y==y;}
bool ready(QuestWorldServices *s){return s&&s->count&&s->read&&s->reserve&&s->append&&s->erase;}
void target(const GameState &g,const Direction *d,int &x,int &y){x=g.position.xy.x;y=g.position.xy.y;if(d){auto v=direction_delta(*d);x+=v.dx;y+=v.dy;if(!g.position.map.location){x&=255;y&=255;}}}
const char *prose(int tile){switch(tile){case 43:return "\nIn the stump\nthou dost find\n";case 79:return "\nIn the wall\nthou dost find\n";case 90:return "\nOn the shelf\nthou dost find\n";case 92:case 93:return "\nIn the bookshelf\nthou dost find\n";case 161:return "\nNear the well\nthou dost find\n";case 165:return "\nIn the desk\nthou dost find\n";case 166:return "\nIn the barrel\nthou dost find\n";case 168:return "\nIn the vanity\nthou dost find\n";case 171:case 172:return "\nUnder the bed\nthou dost find\n";case 173:return "\nIn the dresser\nthou dost find\n";case 175:return "\nIn the trunk\nthou dost find\n";case 178:return "\nIn the brazier\nthou dost find\n";case 188:return "\nIn the fireplace\nthou dost find\n";default:return "\nThou dost find\n";}}
}
QuestCommandResult get_quest_object(CommandContext &c,Direction dir,EventSink sink){
    auto *s=c.quest_world;if(!ready(s))return {CommandStatus::InvalidContext};int x,y;target(c.game,&dir,x,y);size_t count=s->count(s->context),search=count,loot=count,first=count;
    for(size_t i=0;i<count;++i){auto o=s->read(s->context,i);if(!at(o,c.game,x,y))continue;if(first==count)first=i;if(o.search&&search==count)search=i;if(o.loot)loot=i;}
    if(search<count || loot<count){size_t i=search<count?search:loot;auto o=s->read(s->context,i);if(!o.search&&o.item_id==1){emit(sink,GameEventKind::Message,"Open it first!");return {};}
        if(o.search)apply_search_grant(c.game,*s,o.item_id,o.quality);else apply_loot_grant(c.game,{o.item_id,o.quality});s->erase(s->context,i);char name[256];loot_item_name({o.item_id,o.quality},name,sizeof(name));emit(sink,GameEventKind::Message,name);
        if(o.search&&o.item_id==4&&o.quality==255)emit(sink,GameEventKind::Message,"Ship rigged for double speed!");
        emit(sink,GameEventKind::PartyChanged);emit(sink,GameEventKind::MapChanged);return {CommandStatus::Success,true};
    }
    if(first<count){auto o=s->read(s->context,first);if(o.plot&&o.item!=PlotItem::None)return pickup_plot(c,first,sink);
        if(o.torch){c.game.torches=std::min<int32_t>(99,c.game.torches+1);s->erase(s->context,first);char name[64];loot_item_name({13,1},name,sizeof(name));emit(sink,GameEventKind::Message,name);emit(sink,GameEventKind::PartyChanged);emit(sink,GameEventKind::MapChanged);return {CommandStatus::Success,true};}}
    const auto base=get_active_map(c.world,c.game.position.map).value.tile_at(x,y);
    const auto tile=quest_world_tile(c,c.game.position.map,x,y,base);
    if(tile==283){
        if(first>=count&&(!c.transport_services||!c.transport_services->remove_boarded))return {CommandStatus::InvalidContext};
        emit(sink,GameEventKind::Message,grant_plot_item(c.game,PlotItem::Carpet));
        if(first<count)s->erase(s->context,first);else c.transport_services->remove_boarded(c.transport_services->context,{{uint8_t(x),uint8_t(y)},c.game.position.map},tile);
        emit(sink,GameEventKind::PartyChanged);emit(sink,GameEventKind::MapChanged);return {CommandStatus::Success,true};
    }
    if(tile==176||tile==177){if(!s->volatile_tile)return {CommandStatus::InvalidContext};s->volatile_tile(s->context,x,y,68);c.game.torch_turns=100;emit(sink,GameEventKind::Message,"Borrowed!");emit(sink,GameEventKind::Sfx,"torch-borrowed");emit(sink,GameEventKind::MapChanged);return {CommandStatus::Success,true};}
    if(tile==154||tile==155||tile==156||tile==45){
        const auto d=direction_delta(dir);bool reachable=tile==154?d.dy==1:tile==155?d.dy==-1:tile==156?d.dx==0:true;
        if(!reachable){emit(sink,GameEventKind::Message,"Can't reach plate!");return {};}
        if(!s->volatile_tile)return {CommandStatus::InvalidContext};
        const int next=tile==45?44:tile==156?(d.dy==1?155:154):149;s->volatile_tile(s->context,x,y,next);++c.game.food;c.game.karma=uint8_t(c.game.karma?c.game.karma-1:0);emit(sink,GameEventKind::Message,tile==45?"Crops picked!":"Mmmmm...!");return {CommandStatus::Success,true};
    }
    emit(sink,GameEventKind::Message,"Nothing to get!");return {};
}
QuestCommandResult search_world(CommandContext &c,const Direction *dir,EventSink sink,Rand rand,int32_t searcher){
    auto *s=c.quest_world;if(!ready(s)||!s->tile_at || (s->search_count&&!s->search_objects))return {CommandStatus::InvalidContext};int x,y;target(c.game,dir,x,y);int tile=s->tile_at(s->context,x,y);std::string prefix=prose(tile);
    if(dir&&tile==78){if(!s->volatile_tile)return {CommandStatus::InvalidContext};s->volatile_tile(s->context,x,y,c.game.position.map.floor>=128?184:185);auto text=prefix+"a hidden door!\n";emit(sink,GameEventKind::Message,text.c_str());emit(sink,GameEventKind::MapChanged);return {CommandStatus::Success,true};}
    if(dir){
        for(size_t i=0;i<s->count(s->context);++i){
            auto o=s->read(s->context,i);if(!at(o,c.game,x,y))continue;
            if(o.chest){
                int member=searcher<0?(c.game.party.active_character==255?0:c.game.party.active_character):searcher;
                if(member<0 || member>=c.game.party.character_count)member=0;
                if(!c.game.party.character_count)return {CommandStatus::InvalidContext};
                int stat=c.game.party.characters[member].intelligence,diff=o.contents&127;
                bool trapped=(o.contents&128)!=0;
                int threshold=uint16_t((trapped?diff:0)-stat+30)>>1;
                bool success=rand(1,30)>=threshold;
                const char *message=success!=trapped?"no trap!":success?(diff<10?"a simple trap!":diff>20?"a complex trap!":"a trap!"):"a trap!";
                auto text=std::string("\nThou dost find\n")+message;emit(sink,GameEventKind::Message,text.c_str());return {CommandStatus::Success,true};
            }
            break; // worldObjectAt returns the first object, regardless of kind.
        }
    }
    bool occupied=false;for(size_t i=0;i<s->count(s->context);++i)occupied|=at(s->read(s->context,i),c.game,x,y);
    if(c.actors)for(size_t i=0;i<c.actors->count;++i){auto &a=c.actors->actors[i];occupied|=a.location==c.game.position.map.location&&a.z==c.game.position.map.floor&&a.x==x&&a.y==y;}
    auto place=[&](int id,int quality){bool exists=false;for(size_t i=0;i<s->count(s->context);++i){auto o=s->read(s->context,i);exists|=at(o,c.game,x,y)&&o.search;}if(!exists){QuestObject o;o.location=c.game.position.map.location;o.floor=c.game.position.map.floor;o.x=x;o.y=y;o.tile=id;o.search=true;o.item_id=id;o.quality=quality;s->append(s->context,o);}};
    int moon=-1;for(size_t i=0;i<s->moonstone_count;++i){auto &m=s->moonstones[i];if(m.buried&&m.x==x&&m.y==y&&m.z==c.game.position.map.floor){moon=int(i);break;}}
    if(moon>=0){if(!s->reserve(s->context,1))return {CommandStatus::NeedsStorage};place(25,moon);auto text=prefix+loot_open_line(25);emit(sink,GameEventKind::Message,text.c_str());emit(sink,GameEventKind::MapChanged);return {CommandStatus::Success,true};}
    constexpr int px[]={182,97,44},py[]={54,165,137};for(size_t i=0;i<3;++i)if(x==px[i]&&y==py[i]&&c.game.time.hour==0&&c.turn.reagent_days[i]!=c.game.time.day){c.turn.reagent_days[i]=c.game.time.day;int qty=rand(2,15),r=i==2?6:7;c.game.reagent_quantities[r]=int32_t(std::min<int64_t>(99,int64_t(c.game.reagent_quantities[r])+qty));auto text=prefix+std::to_string(qty)+" sprigs of\n"+(i==2?"nightshade!":"mandrake root!")+"\n";emit(sink,GameEventKind::Message,text.c_str());return {CommandStatus::Success,true};}
    if(!s->reserve(s->context,1))return {CommandStatus::NeedsStorage};
    auto index=search_at(c.game,c.turn,s->search_objects,s->search_count,x,y,occupied);
    std::string text=prefix;if(index<0)text+="nothing of note.\n";else{auto &o=s->search_objects[index];place(o.id,o.quality);text+=loot_open_line(o.id);}emit(sink,GameEventKind::Message,text.c_str());if(index>=0)emit(sink,GameEventKind::MapChanged);return {CommandStatus::Success,true};
}
}

