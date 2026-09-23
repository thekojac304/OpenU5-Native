#include "openu5/quest_world.h"
#include "openu5/world_terrain.h"
#include <string>
#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include "openu5/shrine.h"
#include "openu5/dialogue_orchestration.h"
#include "openu5/rest.h"
namespace openu5 {
namespace {
void event(EventSink s,GameEventKind k,const char *text=nullptr) { GameEvent e; e.kind=k;e.text=text;if(s.emit)s.emit(s.context,e); }
bool pool(const QuestWorldServices *s) { return s && s->count && s->read && s->reserve && s->append && s->erase; }
bool here(const QuestObject &o,const GameState &g) { return o.location==g.position.map.location && o.floor==g.position.map.floor; }
}
static bool hydrate_underworld_plot_impl(GameState &g,QuestWorldServices &s,bool reserved) {
    if(g.position.map.floor!=255) return true;
    if(!pool(&s) || (s.spawn_count && !s.spawns)) return false;
    QuestObject objects[4]; size_t n=0;
    if(!g.quest.artifacts[0]) objects[n++]={0,255,105,225,183,243,PlotItem::Amulet,true,false};
    for(size_t i=0;i<3 && i<s.spawn_count;++i) if(!g.quest.shards[i] && !quest_flag(g.quest,QuestFlag(i))) {
        const auto &p=s.spawns[i];objects[n++]={0,255,p.x,p.y,180,p.z,PlotItem(5+i),true,false};
    }
    if(!reserved && !s.reserve(s.context,n)) return false;
    for(size_t i=s.count(s.context);i>0;--i) {auto o=s.read(s.context,i-1);if(o.plot && o.floor==255)s.erase(s.context,i-1);}
    for(size_t i=0;i<n;++i)s.append(s.context,objects[i]);
    return true;
}
bool hydrate_underworld_plot(GameState &g,QuestWorldServices &s){return hydrate_underworld_plot_impl(g,s,false);}
void discard_interior_objects(GameState &g,QuestWorldServices &s,int32_t loc){
    if(!pool(&s))return;
    for(size_t i=s.count(s.context);i>0;--i){auto o=s.read(s.context,i-1);if(o.location==loc && (o.chest||o.prop||o.loot||o.plot||o.shadowlord||o.search))s.erase(s.context,i-1);}
    bool summoned=false;for(size_t i=0;i<s.count(s.context);++i)summoned|=s.read(s.context,i).shadowlord;if(!summoned){g.quest.summoned=-1;g.quest.optional_present&=uint8_t(~8u);}
}
bool hydrate_interior_objects(CommandContext &c,int32_t loc){
    const InteriorHydrationTrace *t=c.quest_world?c.quest_world->hydration_trace:nullptr;
    auto done=[&](bool result,const char *reason){if(t && t->end)t->end(t->context,loc,result,reason);return result;};
    if(!loc || !c.actors)return done(true,!loc?"no-location":"no-actor-owner");
    auto *s=c.quest_world;if(!pool(s))return done(false,"pool-services-missing");
    const NpcLocationData *data=nullptr;for(size_t i=0;i<c.npc_data_count;++i)if(c.npc_data[i].location==loc)data=&c.npc_data[i];
    if(t && t->begin)t->begin(t->context,loc,data,c.npc_data_count);
    if(!s->reserve(s->context,data?data->count:0))return done(false,"reserve-failed");
    discard_interior_objects(c.game,*s,loc);if(!data)return done(true,"no-source-table");
    for(size_t i=0;i<data->count;++i){const auto &n=data->slots[i];auto idx=schedule_index(n.times,uint8_t(c.game.time.hour));
        auto drop=[&](const char *reason){if(t && t->slot)t->slot(t->context,i,n,idx,reason,nullptr);};
        if(!n.slot){drop("empty-slot");continue;}auto item=plot_item_for_npc_type(n.type);if(item==PlotItem::None && n.type!=1 && n.type!=30){drop("npc-type-not-object");continue;}
        bool taken=item==PlotItem::Crown?c.game.quest.artifacts[1]:item==PlotItem::Sceptre?c.game.quest.artifacts[2]:item==PlotItem::WoodenBox?c.game.wooden_box:false;if(taken){drop("plot-item-already-taken");continue;}
        QuestObject o;o.location=loc;o.floor=n.z[idx]==255?-1:n.z[idx];o.x=n.x[idx];o.y=n.y[idx];o.tile=item==PlotItem::None || item==PlotItem::Carpet?n.type+256:n.type;o.item=item;o.plot=item!=PlotItem::None;o.chest=n.type==1;o.prop=n.type==30;o.contents=kInteriorChestContents;
        if(t && t->slot){t->slot(t->context,i,n,idx,nullptr,&o);}
        s->append(s->context,o);
    }return done(true,"ok");
}
QuestCommandResult yell_in_world(CommandContext &c,TalkText word,EventSink sink) {
    auto *s=c.quest_world;

    if(!s) return {CommandStatus::InvalidContext};
    auto &g=c.game;
    if(!g.position.map.location) {
        int32_t adjacent[4];size_t n=0;constexpr int dx[]={0,1,0,-1},dy[]={-1,0,1,0};
        for(int i=0;i<4;++i){auto id=location_at(c.locations,(g.position.xy.x+dx[i])&255,(g.position.xy.y+dy[i])&255);if(id>=33 && id<=40)adjacent[n++]=id;}
        const auto r=yell_word_of_power(s->words,word,{adjacent,n});
        for(uint8_t i=0;i<r.text.count;++i){event(sink,GameEventKind::Message,r.text.lines[i]);if(i==0 && r.uttered){event(sink,GameEventKind::Quake);
            event(sink,GameEventKind::Sfx,"quake");}}
        if(r.opened){auto f=QuestFlag(int(QuestFlag::Word33)+r.location-33);set_quest_flag(g.quest,f,!quest_flag(g.quest,f));}
        return {CommandStatus::Success,r.opened,false};
    }
    if(g.position.map.location<30 || g.position.map.location>32){event(sink,GameEventKind::Message,"\nNo effect!\n");return {};}
    if(!pool(s))return {CommandStatus::InvalidContext};
    bool alive[3],present=false;for(int i=0;i<3;++i)alive[i]=!quest_flag(g.quest,QuestFlag(i));
    for(size_t i=0;i<s->count(s->context);++i){auto o=s->read(s->context,i);present|=here(o,g)&&o.tile==252;}
    auto idx=summon_shadowlord(word,g.position.xy.y,alive,present);
    if(idx<0){event(sink,GameEventKind::Message,"\nNo effect!\n");return {};}
    if(!s->reserve(s->context,1))return {CommandStatus::NeedsStorage};
    g.quest.summoned=idx;g.quest.optional_present|=8;
    s->append(s->context,{g.position.map.location,g.position.map.floor,g.position.xy.x,g.position.xy.y-2,252,0,PlotItem::None,false,true});
    return {CommandStatus::Success,true,true};
}
QuestCommandResult pickup_plot(CommandContext &c,size_t index,EventSink sink) {
    auto *s=c.quest_world;if(!pool(s))return {CommandStatus::InvalidContext};
    if(index>=s->count(s->context))return {CommandStatus::NoOp};
    auto o=s->read(s->context,index);if(!o.plot || o.item==PlotItem::None || !here(o,c.game))return {CommandStatus::NoOp};
    // Reserve before acquisition; hydration can append at most four objects.
    if(c.game.position.map.floor==255){if(s->spawn_count && !s->spawns)return {CommandStatus::InvalidContext};if(!s->reserve(s->context,4))return {CommandStatus::NeedsStorage};}
    auto text=grant_plot_item(c.game,o.item);
    if(c.game.position.map.floor==255){if(!hydrate_underworld_plot_impl(c.game,*s,true))return {CommandStatus::NeedsStorage};}
    else s->erase(s->context,index);
    event(sink,GameEventKind::Message,text);event(sink,GameEventKind::PartyChanged);event(sink,GameEventKind::MapChanged);
    return {CommandStatus::Success,true,false};
}
CommandStatus use_quest_item(CommandContext &c,int32_t id,EventSink sink) {
    auto message=[&](const char *t){event(sink,GameEventKind::Message,t);};
    if(id==18 || id==36){char spell=id==18?'\x0e':'\x1d';message(id==18?"Amulet":"Badge");if(c.turn.time_spell==spell){c.turn.time_spell=0;c.turn.spell_turns=-1;message("Removed!");}else{c.turn.time_spell=spell;c.turn.spell_turns=255;message(id==18?"Wearing the Amulet of Lord British...":"Badge worn!");}return CommandStatus::Success;}
    if(id==19){message("Crown");c.game.worn_crown=!c.game.worn_crown;message(c.game.worn_crown?"Thou dost don the Crown of Lord British...":"Removed!");return CommandStatus::Success;}
    auto *s=c.quest_world;
    if(id==33){message("Plans");if(c.game.transport==TransportMode::Ship){c.game.hms_cape=true;message("The ship is already rigged.");}else message("Only usable on shipboard!");return CommandStatus::Success;}
    if(id==20){
        if(c.combat){
            if(!c.combat_context)return CommandStatus::InvalidContext;
            int count=combat_sceptre_fields(*c.combat_context);
            message("Sceptre");message("Wielding the Sceptre of Lord British...");if(!count)message("No effect!");return CommandStatus::Success;
        }
        if(!c.dungeon && (!s||!s->tile_at||!s->volatile_tile))return CommandStatus::InvalidContext;
        message("Sceptre");message("Wielding the Sceptre of Lord British...");event(sink,GameEventKind::Sfx,"sceptre");
        if(c.dungeon && c.dungeon_context){auto &d=c.dungeon_context->state;constexpr int dx[]={0,1,0,-1},dy[]={-1,0,1,0};int f=int(d.pos.facing);int x=(d.pos.x+dx[f]+8)%8,y=(d.pos.y+dy[f]+8)%8;int cell=dungeon_cell(d,d.pos.floor,x,y);bool dissolved=(cell>>4)==8;if(dissolved)d.cells[d.pos.floor*64+y*8+x]=uint8_t(cell&8);message(dissolved?"Field dissolved!":"No effect!");}
        else{bool dissolved=false;for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy){int x=c.game.position.xy.x+dx,y=c.game.position.xy.y+dy;if((s->tile_at(s->context,x,y)&240)==112){s->volatile_tile(s->context,x,y,5);dissolved=true;}}if(!dissolved)message("No effect!");}return CommandStatus::Success;
    }
    if(id>=29 && id<=31){
        if(!pool(s))return CommandStatus::InvalidContext;
        const auto &g=c.game;bool above=false;
        for(size_t i=0;i<s->count(s->context);++i){auto o=s->read(s->context,i);above|=here(o,g)&&o.tile==252&&o.x==g.position.xy.x&&o.y==g.position.xy.y-1;}
        auto r=cast_shard_into_flame(id-29,g.position.xy.x,g.position.xy.y,g.position.map.location,g.position.map.floor,above?252:0,g.quest.summoned);
        message(r.text.lines[0]);event(sink,GameEventKind::Sfx,"shard-sweep");for(uint8_t i=1;i<r.text.count;++i)message(r.text.lines[i]);
        if(r.destroyed){
            for(int i=0;i<3;++i)event(sink,GameEventKind::Quake);
            event(sink,GameEventKind::Sfx,"quake");
            GameEvent e;e.kind=GameEventKind::CellExplosion;e.cell_fx={0,-1,7,3,252};if(sink.emit)sink.emit(sink.context,e);
            event(sink,GameEventKind::Sfx,"victory-fanfare");apply_shard_destruction(c.game,c.turn,id-29);
            for(size_t i=s->count(s->context);i>0;--i){auto o=s->read(s->context,i-1);if(here(o,g)&&o.tile==252&&o.x==g.position.xy.x&&o.y==g.position.xy.y-1)s->erase(s->context,i-1);}
            event(sink,GameEventKind::MapChanged);
        }return CommandStatus::Success;
    }
    return CommandStatus::Unsupported;
}
CommandStatus play_harpsichord(CommandContext &c,int32_t digit,EventSink sink) {
    auto *s=c.quest_world;if(!s)return CommandStatus::InvalidContext;
    GameEvent e;e.kind=GameEventKind::Sfx;e.text="instrument-note";e.note=digit;if(sink.emit)sink.emit(sink.context,e);
    auto r=advance_melody(s->melody_progress,digit);s->melody_progress=r.progress;
    if(r.complete && c.game.position.map.location==17 && c.game.position.map.floor==2){s->passage_open=true;event(sink,GameEventKind::Quake);
            event(sink,GameEventKind::Sfx,"quake");event(sink,GameEventKind::MapChanged);}
    return CommandStatus::Success;
}
int32_t harpsichord_passage_tile(const QuestWorldServices &s,MapId map,int32_t x,int32_t y,int32_t tile){return s.passage_open && map.location==17 && map.floor==2 && x==17 && y==13 && tile==79?68:tile;}
namespace {
std::string cardinal(int n) {
    constexpr const char *units[]={"","One","Two","Three","Four","Five","Six","Seven","Eight","Nine","Ten","Eleven","Twelve","Thirteen","Fourteen","Fifteen","Sixteen","Seventeen","Eighteen","Nineteen","Twenty"};
    constexpr const char *tens[]={"","","Twenty","Thirty","Forty","Fifty","Sixty","Seventy","Eighty","Ninety"};
    if(n<21)return n>=0?units[n]:"";
    std::string s=n/10<10?tens[n/10]:"";if(n%10)s+="-"+std::string(units[n%10]);return s;
}
std::string avatar_name(const GameState &g){int idx=0;for(int i=0;i<g.party.character_count;++i)if(g.party.characters[i].character_class=='A'){idx=i;break;}std::string name=g.party.character_count?g.party.characters[idx].name:"";auto first=name.find_first_not_of(" \t\r\n\v\f"),last=name.find_last_not_of(" \t\r\n\v\f");return first==std::string::npos?"Avatar":name.substr(first,last-first+1);}
std::string ordinal(int n){constexpr const char *o[]={"","First","Second","Third","Fourth","Fifth","Sixth","Seventh","Eighth","Ninth","Tenth","Eleventh","Twelfth"};if(n<13)return n>=0?o[n]:"";if(n<20)return cardinal(n)+"th";if(n==20)return "Twentieth";return "Twenty-"+std::string(n<33?o[n-20]:"");}
}
CommandStatus rescue_events(CommandContext &c,bool absorption,EventSink sink) {
    auto message=[&](const std::string &s){event(sink,GameEventKind::Message,s.c_str());};
    if(!absorption && !endgame_ready(c.game)){message("The way to Lord British remains barred. Thy quest is not yet complete.");return CommandStatus::NoOp;}
    if(!absorption && !quest_flag(c.game.quest,QuestFlag::InDoom)){message("Lord British languishes in the depths of Doom. Thou must seek him there.");return CommandStatus::NoOp;}
    const char *record=nullptr;
    if(c.game.wooden_box){auto *s=c.quest_world;if(!s || !s->end_record || !(record=s->end_record(s->context,9)))return CommandStatus::InvalidContext;}
    auto ending=rescue_lord_british(c.game,absorption);
    message("Bearing amulet, crown and sceptre, thou dost shatter the final seal of Doom.");
    message("Lord British rises, unbroken, and takes again the throne of Britannia.");
    if(ending==RescueEnding::Victory){
        message("Lord British carefully opens the box...");std::string text=record;while(!text.empty() && text.back()=='\n')text.pop_back();message(text);
        const auto &t=c.game.time;
        message("Be it known that on the "+ordinal(t.day)+" Day of the "+ordinal(t.month)+" Month");
        auto year=cardinal(t.year/100)+" Hundred "+cardinal(t.year%100);while(!year.empty() && year.back()==' ')year.pop_back();while(!year.empty() && year.front()==' ')year.erase(0,1);message("of the Year "+year);message("");
        message(avatar_name(c.game)+" the Avatar");message("");
        message("saved the life of our sovereign Lord British, thereby");message("saving our people and our land.");message("");message("THE QUEST OF THE AVATAR IS FOREVER");message("");
        auto p=endgame_playtime(c.game);std::string report="Report now, thy Quest compleat in\n";bool separator=false;int values[]={p.years,p.months,p.days};const char *units[]={"year","month","day"};for(int i=0;i<3;++i)if(values[i]){if(separator)report+=", ";separator=true;report+=std::to_string(values[i])+" "+units[i]+(values[i]>1?"s":"");}message(report+"\nto Lord British at Origin Systems!");
    }else{message("\"Well then, pull up a chair.\"");message("\"We shall be here a while.\"");}
    return CommandStatus::Success;
}
CommandStatus absorption_endgame(CommandContext &c,EventSink sink){
    if(quest_flag(c.game.quest,QuestFlag::GameWon))return CommandStatus::NoOp;
    if(c.game.wooden_box && (!c.quest_world || !c.quest_world->end_record || !c.quest_world->end_record(c.quest_world->context,9)))return CommandStatus::InvalidContext;
    set_quest_flag(c.game.quest,QuestFlag::InDoom);auto status=rescue_events(c,true,sink);
    if(status==CommandStatus::Success){auto ending=c.game.wooden_box?"victory":"stranded";event(sink,GameEventKind::GameWon,ending);
        auto *s=c.quest_world;EndgameScript script;
        if(s && s->endgame_script){script.victory=c.game.wooden_box;auto add=[&](const char *phase,const char *text=nullptr,const char *reply=nullptr,int delay=-1,int page=-1){script.beats[script.count++]={phase,text,reply,int16_t(delay),int16_t(page)};};add("greenScene",nullptr,nullptr,40);constexpr int victory[]={0,1,3,4,5,6,7,8,9},stranded[]={0,1,2,10};auto indices=script.victory?victory:stranded;int count=script.victory?9:4;
            for(int n=0;n<count;++n){int i=indices[n];auto text=s->end_record?s->end_record(s->context,i):nullptr;if(i==0){script.greeting=std::string(text?text:"")+avatar_name(c.game)+"!\"";text=script.greeting.c_str();}add("dialogue",text,i==1||i==2?(script.victory?"Yes":"No"):nullptr,!script.victory&&i==10?40:-1);if(script.victory&&i==3)add("dialogue","\n\nHe says:\n\n",nullptr,40);}
            if(script.victory){add("orbMoongate");add("dissolve");for(int i=0;i<6;++i)add(i<2?"storyHouse":"storyDream",s->end_narration?s->end_narration(s->context,i):nullptr,nullptr,-1,i);add("scroll");add("terminalFreeze");}else add("terminalPrison");
        }
        GameEvent e;e.kind=GameEventKind::Endgame;e.text=ending;e.endgame=s&&s->endgame_script?&script:nullptr;if(sink.emit)sink.emit(sink.context,e);
    }return status;
}
CommandStatus resolve_refuge(CommandContext &c,EventSink sink){
    if(!c.quest_world)return CommandStatus::InvalidContext;
    c.quest_world->refuge_pending=false;
    auto &g=c.game;
    for(int i=0;i<g.party.party_size && i<g.party.character_count;++i){auto &ch=g.party.characters[i];ch.current_hp=ch.max_hp;ch.status='G';}
    if(g.karma<75)g.karma=75;
    g.position={{10,10},{17,1}};g.transport=TransportMode::Foot;c.turn.transport_tile=28;
    c.turn.time_spell=0;c.turn.spell_turns=0;g.time.hour=6;g.time.minute=0;c.turn.light_spell_minutes=0;g.torch_turns=0;if(!g.food)g.food=63;
    c.dungeon=false;if(c.dungeon_context)c.dungeon_context->state.active=false;
    event(sink,GameEventKind::MapChanged);event(sink,GameEventKind::PartyChanged);return CommandStatus::Success;
}
CommandStatus check_refuge(CommandContext &c,EventSink sink){
    for(int i=0;i<c.game.party.party_size && i<c.game.party.character_count;++i){auto status=c.game.party.characters[i].status;if(status=='G'||status=='P'||status=='S')return CommandStatus::NoOp;}
    auto *s=c.quest_world;if(!s)return CommandStatus::InvalidContext;
    if(s->refuge_pending)return resolve_refuge(c,sink);
    int index=std::min<int32_t>(4,c.game.karma/20);
    const char *speech=s->karma_record?s->karma_record(s->context,index):c.rest_services&&c.rest_services->karma_record?c.rest_services->karma_record(c.rest_services->context,index):nullptr;
    if(!speech)return CommandStatus::InvalidContext;
    RefugeScript script{{
        {"void","An unending darkness engulfs thee...",nullptr,10},
        {nullptr,"Thou hast found refuge.",nullptr,14},
        {nullptr,"No evil lives here, only peace and darkness.",nullptr,28},
        {nullptr,"But thy slumber is disturbed!"}, {nullptr,"Someone shouts"},
        {nullptr,"\"FORTIS FORTUNA AVENTARI\"",nullptr,6},
        {"ghostLeft",nullptr,nullptr,4},{"ghostBoth",nullptr,nullptr,4},
        {nullptr,"There is a peal of thunder!"},{nullptr,nullptr,"refuge-thunder"},
        {nullptr,nullptr,"refuge-thunder",2},{"apparition",nullptr,nullptr,2},
        {nullptr,speech,nullptr,8},{nullptr,"Strange words are intoned.",nullptr,4},
        {nullptr,"Vertigo...",nullptr,4},{"vertigo"}
    }};
    s->refuge_pending=true;GameEvent e;e.kind=GameEventKind::Refuge;e.refuge=&script;if(sink.emit)sink.emit(sink.context,e);return CommandStatus::Success;
}
TrapdoorOutcome quest_trapdoor(CommandContext &c,EventSink sink){
    auto *s=c.quest_world;auto &g=c.game;
    if(g.position.map.location==29){
        if(!pool(s)||!s->volatile_tile)return TrapdoorOutcome::None;
        for(int y=0;y<32;++y)for(int x=0;x<32;++x)s->volatile_tile(s->context,x,y,143);
        if(c.terrain){c.terrain->wiped=true;c.terrain->wipe_map=g.position.map;c.terrain->wipe_tile=143;}
        for(size_t i=s->count(s->context);i>0;--i)if(here(s->read(s->context,i-1),g))s->erase(s->context,i-1);
        for(int i=0;i<g.party.character_count;++i){g.party.characters[i].current_hp=0;g.party.characters[i].status='D';}
        event(sink,GameEventKind::MapChanged);event(sink,GameEventKind::PartyChanged);return TrapdoorOutcome::PartyKilled;
    }
    auto below=g.position.map;--below.floor;if(!floor_exists(c.world,below))return TrapdoorOutcome::None;
    g.position.map=below;if(c.terrain)c.terrain->refresh(c.world,g);if(c.services.effect)c.services.effect(c.services.context,CommandEffect::RefreshHourTiles,sink);
    event(sink,GameEventKind::MapChanged);return TrapdoorOutcome::Fell;
}
CommandStatus urban_shadowlord(CommandContext &c,EventSink sink,Rand rand){
    const int loc=c.game.position.map.location,idx=c.travel.shadowlord_here;auto *s=c.quest_world;
    if(idx>=0){
        if(!pool(s)||!s->volatile_tile)return CommandStatus::InvalidContext;
        bool already=false;for(size_t i=0;i<s->count(s->context);++i){auto o=s->read(s->context,i);already|=o.location==loc&&o.floor==0&&o.tile==252;}
        if(!already && !s->reserve(s->context,1))return CommandStatus::NeedsStorage;
        auto map=get_active_map(c.world,c.game.position.map);if(map.error!=Error::None)return CommandStatus::InvalidContext;
        OriginalRng rng(uint16_t(c.game.time.day&255));
        for(int y=0;y<32;++y)for(int x=0;x<32;++x){int tile=map.value.tile_at(x,y);if((tile==45||tile==46)&&rng.next(0,7).value!=0)s->volatile_tile(s->context,x,y,tile==46?43:44);}
        constexpr int ys[]={0,4,9,15,8,17,10,11,10,2,0,3,0,1,0,1,0,2,0,3,0,3,0,10,15,20,0,15,10,3,20,15,20};
        if(!already)s->append(s->context,{loc,0,15,loc<=32?ys[loc]:0,252,0,PlotItem::None,false,true});
    }
    constexpr const char *messages[]={"\nAn air of\nfalsehood doth surround thee...\n","\nAn air of\nhatred doth surround thee...\n","\nAn air of\ncowardice doth surround thee...\n"};
    auto announce=[&](int i){event(sink,GameEventKind::Message,messages[i]);event(sink,GameEventKind::Sfx,"shadowlord-announce");};
    if(c.turn.has_shadowlords){if(loc==29){for(int i=2;i>=0;--i)if(c.turn.shadowlord_locations[size_t(i)]<128)announce(i);}else for(int i=0;i<3;++i)if(c.turn.shadowlord_locations[size_t(i)]==loc){announce(i);break;}}
    if(loc!=29 && (idx==1 || idx==2) && c.actors){
        const NpcLocationData *source=nullptr;for(size_t i=0;i<c.npc_data_count;++i)if(c.npc_data[i].location==loc)source=&c.npc_data[i];
        auto slot=[&](int n)->const NpcSlot *{if(source)for(size_t i=0;i<source->count;++i)if(source->slots[i].slot==n)return &source->slots[i];return nullptr;};
        auto four=slot(4);bool person=four&&four->type>=64&&four->type<116;
        for(int i=0;i<32;++i){auto npc=slot(i);bool present=npc&&(npc->times[0]||npc->times[1]||npc->times[2]||npc->times[3]);auto roll=rand(0,1);if(roll||!present||!person||(idx==2&&(npc->type<64||npc->type>=116)))continue;
            for(size_t j=0;j<c.actors->count;++j){auto &a=c.actors->actors[j];if(a.location==loc && a.schedule.slot==i){a.schedule.dialog=uint8_t(idx==1?254:253);for(auto &ai:a.schedule.ai)ai=uint8_t(idx==1?7:3);}}
        }
    }return CommandStatus::Success;
}
QuestCommandResult doom_entrance(CommandContext &c,int32_t dungeon,EventSink sink){
    if(dungeon!=40 || (c.turn.transport_tile<0?28:c.turn.transport_tile)!=28 || can_reach_doom(c.game))return {};
    auto *s=c.quest_world;const auto *r=s?s->combat_resources:nullptr;
    if(!r || !r->enemies || r->enemy_count<=47 || !r->enemies[47])return {};
    if(!s->encounter || &s->encounter->game!=&c.game || &s->encounter->turn!=&c.turn)return {CommandStatus::InvalidContext};
    if(!r->maps || !r->map_count || ((r->map_count<=10 || !r->maps[10]) && !r->maps[0]))return {CommandStatus::InvalidContext};
    event(sink,GameEventKind::Message,"\nAttacked at entrance!\n");auto resources=*r;resources.remove_enemy=nullptr;
    auto saved=c.events;c.events=sink;const auto *enemy=r->enemies[47];auto result=start_encounter_combat(c,s->encounter->combat,resources,47,enemy->tile>=0?enemy->tile:252,10,CombatDirection::South,false);c.events=saved;
    if(result!=CombatResult::Ok)return {CommandStatus::InvalidContext};
    c.combat_context=s->encounter;
    return {CommandStatus::Success,true,false}; // true means entrance was intercepted, not a world turn.
}
int32_t actor_attack_arena(int32_t t,int32_t c,int32_t transport,int32_t location){
    int creature=c&252;
    if(creature==252)return 10;
    bool ship=(transport&248)==32;
    bool water=t<4 || (t>=96&&t<=111&&t!=106&&t!=107) || (c&240)==128;
    if(ship&&creature==44)return 14;
    if(ship&&water)return 11;
    if(ship)return 13;
    if(creature==44)return 12;
    if(water)return 15;
    if(t>=4&&t<=7)return t-3;
    if(t==8)return 3;
    if(t>=9&&t<=10)return 5;
    if(t>=11&&t<=15)return 6;
    if(t>=30&&t<=31)return 4;
    if(t==29||t==72||t==73||t==106||t==107)return 7;
    if(t==68)return 8;
    return location?8:2;
}
CommandStatus town_attack_commit(CommandContext &c,const NpcActor &actor,bool hostile,EventSink sink){
    auto *s=c.quest_world;auto *r=s?s->combat_resources:nullptr;
    if(!r)return CommandStatus::NoOp;
    const NpcActor npc=actor;int type=npc.schedule.type&255,enemy=(type-64)>>2;
    if(enemy<0||!r->enemies||size_t(enemy)>=r->enemy_count||!r->enemies[enemy])return CommandStatus::NoOp;
    if(!s->encounter||&s->encounter->game!=&c.game||&s->encounter->turn!=&c.turn||!s->tile_at)return CommandStatus::InvalidContext;
    int tile=s->tile_at(s->context,npc.x,npc.y);
    int arena=actor_attack_arena(tile,type&252,(c.turn.transport_tile<0?28:c.turn.transport_tile)&255,c.game.position.map.location);
    if(preflight_encounter_combat(c,s->encounter->combat,*r,enemy,tile,arena)!=CombatResult::Ok)return CommandStatus::InvalidContext;
    dialogue_despawn(c.game,c.actors,npc);
    bool reclaim=(type&252)==252&&c.game.quest.artifacts[2];
    if(reclaim)c.game.quest.artifacts[2]=false;
    if(hostile)event(sink,GameEventKind::Message,"\nAttacked!\n");
    auto resources=*r;resources.remove_enemy=nullptr;auto saved=c.events;c.events=sink;
    auto result=start_encounter_combat(c,s->encounter->combat,resources,enemy,tile,arena,CombatDirection::South,false,reclaim?"The Sceptre is reclaimed!\n":nullptr);
    c.events=saved;
    if(result!=CombatResult::Ok)return CommandStatus::InvalidContext;
    s->encounter->combat.town_fight=true; // 0x09dc: town_load_map_chunk(0) once the fight returns
    c.combat_context=s->encounter;return CommandStatus::Success;
}
CommandStatus use_moonstone(CommandContext &c,int32_t phase,EventSink sink){
    auto *s=c.quest_world;if(!s || (s->moonstone_count&&!s->moonstones))return CommandStatus::InvalidContext;
    event(sink,GameEventKind::Message,"Moonstone");if(phase<0 || size_t(phase)>=s->moonstone_count || s->moonstones[phase].buried)return CommandStatus::Success;
    int loc=c.dungeon&&c.dungeon_context?c.dungeon_context->state.pos.dungeon:c.game.position.map.location;auto &p=c.game.position;
    if(loc>=33){event(sink,GameEventKind::Message,"cannot be buried here!");return CommandStatus::Success;}
    if(!s->tile_at)return CommandStatus::InvalidContext;
    int tile=s->tile_at(s->context,p.xy.x,p.xy.y);if(tile!=44&&tile!=45&&(tile<4||tile>10)){event(sink,GameEventKind::Message,"cannot be buried here!");return CommandStatus::Success;}
    s->moonstones[phase]={p.xy.x,p.xy.y,int16_t(p.map.floor),uint8_t(loc),true};event(sink,GameEventKind::Message,"buried!");return CommandStatus::Success;
}
int32_t active_gate_phase(const GameState &g,const TurnState &t,const QuestWorldServices &s){
    if(g.time.hour>=5 && g.time.hour<20)return -1;
    int a=t.felucca_phase,b=t.trammel_phase;
    if(a<48||a>55||b<48||b>55){int i=(g.time.day-1)*2;a=s.moon_phases&&i>=0&&size_t(i)<s.moon_phase_count?s.moon_phases[i]:48;b=s.moon_phases&&i+1>=0&&size_t(i+1)<s.moon_phase_count?s.moon_phases[i+1]:48;}
    return (g.time.hour<=4?a:b)-48;
}
bool moongate_at(const GameState &g,const TurnState &t,const QuestWorldServices &s){
    if(active_gate_phase(g,t,s)<0)return false;
    for(size_t i=0;i<s.moonstone_count;++i){auto &m=s.moonstones[i];if(m.buried&&m.location==g.position.map.location&&m.x==g.position.xy.x&&m.y==g.position.xy.y&&m.z==(g.position.map.floor==255?255:0))return true;}return false;
}
int32_t quest_world_tile(CommandContext &c,MapId map,int32_t x,int32_t y,int32_t tile){
    auto *s=c.quest_world;
    if(s&&s->count&&s->read)for(size_t i=0;i<s->count(s->context);++i){const auto o=s->read(s->context,i);if(o.location==map.location&&o.floor==map.floor&&o.x==x&&o.y==y&&!o.loot&&!o.search)return o.shadowlord?o.tile+256:o.tile;}
    if(c.terrain)tile=c.terrain->effective(c.world,map,x,y);
    else if(s&&s->tile_at)tile=s->tile_at(s->context,x,y);
    if(!map.location && !map.floor && tile==25 && c.shrine_services && c.shrine_services->data){auto v=shrine_index_at(*c.shrine_services->data,x,y);if(v>=0 && v<c.game.quest.destroyed_count && c.game.quest.shrine_destroyed[v])tile=26;}
    if(!map.location && (tile==22 || tile==23 || tile==24)){int id=location_at(c.locations,x,y);if(id>=33 && id<=40 && !quest_flag(c.game.quest,QuestFlag(int(QuestFlag::Word33)+id-33)))tile=223;}
    return s?harpsichord_passage_tile(*s,map,x,y,tile):tile;
}
}


