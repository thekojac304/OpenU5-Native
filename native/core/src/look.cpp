#include "openu5/look.h"
#include "openu5/quest_world.h"
#include <algorithm>
#include <cstdio>
#include <string>
namespace openu5 {
namespace {
void emit(EventSink s,GameEventKind k,const char *text=nullptr){GameEvent e;e.kind=k;e.text=text;if(s.emit)s.emit(s.context,e);}
}
LookSign resolve_look_sign(const LookSignRecord *records,size_t count,MapId map,int32_t x,int32_t y){
    if(!records||x<0||x>255||y<0||y>255)return {};
    for(size_t i=0;i<count;++i){const auto &record=records[i];const bool floor=record.map.floor==map.floor||(map.location!=0&&record.map.floor==255&&map.floor==-1);if(record.map.location==map.location&&floor&&record.x==x&&record.y==y)return record.value;}
    return {};
}
void emit_zodiac(CommandContext &c,EventSink sink,Rand rand){
    ZodiacView view;
    for(auto &s:view.stars){s.x=uint8_t(rand(9,182));s.y=uint8_t(rand(9,172));}
    static constexpr int cols[8][22]={{4,11,18},{2,7,11,15,20},{2,5,8,11,14,17,20},{1,3,5,7,9,11,13,15,17,19,21},{0,2,4,6,8,9,11,13,14,16,18,19,21},{1,2,3,5,6,7,9,10,11,12,13,15,16,17,19,20,21},{1,2,3,4,5,6,8,9,10,11,12,13,14,16,17,18,19,20,21},{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21}};
    static constexpr int counts[]={3,5,7,11,13,17,19,22},base[]={18,2,8,15,11,6,4,2},rows[]={18,17,15,13,11,8,5,1};
    auto &t=c.game.time;int days=std::max(0,((int(t.year)%100)-39)*364+(int(t.month)-4)*28+int(t.day)-5);
    for(int i=0;i<8;++i){int j=0;while(cols[i][j]!=base[i])++j;int col=cols[i][((j-days)%counts[i]+counts[i])%counts[i]];auto &s=view.signs[i];s.star_x=uint8_t((col+1)*8);s.line_x=uint8_t(col*8);s.y=uint8_t(rows[i]*8);if(c.turn.has_shadowlords)for(auto loc:c.turn.shadowlord_locations)if(loc==i+1)s.has_line=true;}
    GameEvent e;e.kind=GameEventKind::Zodiac;e.zodiac=&view;if(sink.emit)sink.emit(sink.context,e);
}
CommandStatus world_look(CommandContext &c,Command cmd,const ActiveMap &map,EventSink sink,Rand rand){
    auto &g=c.game;auto say=[&](const char *s){emit(sink,GameEventKind::Message,s);};
    if(cmd.kind==CommandKind::MakeWish){
        if(!g.gold)return CommandStatus::Success;
        if(cmd.text_length&&!cmd.text)return CommandStatus::InvalidContext;
        std::u16string wish(cmd.text?cmd.text:u"",cmd.text_length);bool match=false;
        for(auto word:{u"Corvette",u"Ferrari",u"Lamborghini",u"Lotus",u"Porsche",u"Horse"})if(wish.find(word)!=std::u16string::npos)match=true;
        bool horse=match&&(g.position.map.location==22||g.position.map.location==31);int x=int(g.position.xy.x)+1,y=g.position.xy.y;auto tile=map.tile_at(x,y);auto props=tile_properties(tile);bool place=horse&&tile>=0&&props.error==Error::None&&props.value.walkable;
        if(place&&(!c.quest_world||!c.quest_world->persistent_tile))return CommandStatus::InvalidContext;
        --g.gold;
        if(wish.empty())say("Nothing\n");else if(!horse)say("\nNo effect...\n");else {if(place){c.quest_world->persistent_tile(c.quest_world->context,x,y,272);emit(sink,GameEventKind::MapChanged);}say("\nPoof!\n");}
        emit(sink,GameEventKind::PartyChanged);return CommandStatus::Success;
    }
    if(cmd.kind==CommandKind::DropCoin){say(cmd.member?"Yes\n":"No\n");if(cmd.member&&g.gold>0)emit(sink,GameEventKind::WellWishPrompt);return CommandStatus::Success;}
    if(cmd.kind==CommandKind::CrystalBall){
        if(cmd.member<0||cmd.member>=g.party.character_count)return CommandStatus::Rejected;
        if(g.party.characters[cmd.member].intelligence>rand(1,30)){say("Strange vision!");GameEvent e;e.kind=GameEventKind::GemView;e.gem_from_crystal=true;if(sink.emit)sink.emit(sink.context,e);}
        else{apply_damage(g,cmd.member,1);say("Death vision!");emit(sink,GameEventKind::PartyChanged);}return CommandStatus::Success;
    }
    auto d=direction_delta(cmd.direction);int x=g.position.xy.x+d.dx,y=g.position.xy.y+d.dy;if(map.geometry.wraps){x&=255;y&=255;}int tile=map.tile_at(x,y);
    if(c.actors)for(size_t i=0;i<c.actors->count;++i){auto &n=c.actors->actors[i];if(n.location==g.position.map.location&&n.z==g.position.map.floor&&n.x==x&&n.y==y){tile=n.schedule.type+256;break;}}
    if(tile<0){say("Thou dost see darkness.");return CommandStatus::Success;}
    if(tile==41||tile==161||(tile>=216&&tile<=219)){emit(sink,tile==41?GameEventKind::CrystalBallPrompt:tile==161?GameEventKind::WellDropPrompt:GameEventKind::FountainDrinkPrompt);return CommandStatus::Success;}
    if((tile==137||tile==138||tile==160||tile==164||tile==248)&&c.look&&c.look->sign){auto sign=c.look->sign(c.look->context,g.position.map,x,y);auto text=sign.text;if(text){
        std::string s(text);auto blank=[](const std::string &v){return v.find_first_not_of(" \t\r")==std::string::npos;};
        while(!s.empty()){auto p=s.find('\n');if(!blank(s.substr(0,p)))break;s=p==std::string::npos?"":s.substr(p+1);}
        while(!s.empty()){auto p=s.rfind('\n');if(!blank(s.substr(p==std::string::npos?0:p+1)))break;s=p==std::string::npos?"":s.substr(0,p);}
        say("Thou dost see");GameEvent e;e.kind=GameEventKind::Message;e.text=s.c_str();e.sign=true;e.sign_raw=sign.raw;e.sign_raw_size=sign.raw_size;if(sink.emit)sink.emit(sink.context,e);return CommandStatus::Success;
    }}
    const bool sun=g.time.hour>=6&&g.time.hour<18;
    std::string phrase;
    if(tile==89)phrase=sun?"the sun!":"the night sky!";
    else{if(!c.look||!c.look->describe)return CommandStatus::InvalidContext;auto text=c.look->describe(c.look->context,tile);if(!text)return CommandStatus::InvalidContext;phrase=text;
        if((tile&254)==250){char time[32];std::snprintf(time,sizeof(time),"%d:%02d %s.",int(g.time.hour%12?g.time.hour%12:12),int(g.time.minute),g.time.hour<=11?"AM":"PM");phrase+=time;}
        if(tile==222){auto loc=g.position.map.location;phrase+=loc==30?"Truth":loc==31?"Love":loc==32?"Courage":"";}
        if(tile==223){const int xs[]={58,72,91,126,128,156,239,240};const char *names[]={"Shame","Destard","Despise","Wrong","Doom","Covetous","Hythloth","Deceit"};for(int i=0;i<8;++i)if(x==xs[i])phrase+=names[i];}
    }
    phrase="Thou dost see "+phrase;say(phrase.c_str());
    if(tile==89){if(sun){apply_damage(g,g.party.active_character==255?0:g.party.active_character,1);emit(sink,GameEventKind::PartyChanged);}else emit_zodiac(c,sink,rand);}
    return CommandStatus::Success;
}
const char *fountain_drink_result(char status,bool cancelled){
    if(cancelled)return "None!";
    return (status=='D'||status=='S')?"Incapacitated!":"Refreshing...";
}
}
