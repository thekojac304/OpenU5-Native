#include "openu5/blackthorn.h"
#include "openu5/shrine.h"
#include "openu5/dialogue_orchestration.h"
#include "openu5/shops.h"
#include <algorithm>
#include <cstdlib>
#include <string>
namespace openu5 {
int32_t count_living(const GameState &g){int n=0;for(int i=0;i<g.party.party_size && i<g.party.character_count;++i)n+=g.party.characters[i].status!='D';return n;}
int32_t pick_interrogation_shrine(const GameState &g){for(int i=0;i<8;++i)if(i>=g.quest.destroyed_count || !g.quest.shrine_destroyed[i])return i;return -1;}
// R-23. BLCKTHRN.OVL 0x0438 `sacrifice_member`, whose only roster bound is
// g_party_size (0x043d; no `cmp si,6` -- re/notes/blackthorn-cota-party-size.md):
//   044c cmp byte [si],'D' / inc cx / cmp cx,2 -> the victim is the 2nd LIVING
//   046c TEMP = record[victim]            (repne movsw cx=0x10, the whole 32 B)
//   0487 cmp ax,0xf / jge                 -> a victim already at 15 skips the shift
//   04ab record[i] = record[i+1] until si == 0x57a8 -- the end of ALL SIXTEEN
//        records at DS 0x55a8, NOT the party bound
//   04c2 record[15] = TEMP                (DS:0x5788 = 0x55a8 + 15*32)
//   04cf byte [0x57A7] = 0x7f             (record +0x1F = partyStatus)
//   04d4 dec [g_party_size]               -> last, and exactly once
// Nothing here re-indexes g_active_char: that block is SHOPPES3 0x03dd-0x0400,
// the inn (L)eave (inn_leave in shops.cpp), and this body has no counterpart --
// the same faithful asymmetry the inn (P)ickup carries.
std::string sacrifice_first_companion(GameState &g){int living=0;for(int i=0;i<g.party.party_size && i<g.party.character_count;++i)if(g.party.characters[i].status!='D' && ++living==2){auto victim=g.party.characters[i];std::string result=victim.name;for(int j=i+1;j<kRosterCapacity;++j)g.party.characters[j-1]=g.party.characters[j];victim.party_status=127;g.party.characters[15]=victim;g.party.character_count=kRosterCapacity;g.party.party_size=std::max<int32_t>(0,g.party.party_size-1);return result;}return "";}
GuardDemand guard_demand(GameState &g,const TurnState &t,TalkText text,bool agree){
    if(g.position.map.location==18)return {0,t.time_spell=='\x1d'&&quest_text_equal(text.substr(0,4),u"IMPE")?0:1,0};
    int kind=g.position.map.location==5?1:2;if(!agree)return {kind,1,0};int amount=kind==1?g.gold-(g.gold/2):count_living(g)*10;if(g.gold<amount)return {kind,1,0};g.gold=uint16_t(g.gold-amount);return {kind,0,amount};
}
namespace {
void event(EventSink s,GameEventKind kind,const std::string &text=""){GameEvent e;e.kind=kind;e.text=text.empty()?nullptr:text.c_str();if(s.emit)s.emit(s.context,e);}
std::string str(TalkText s){std::string out;for(auto ch:s)out+=char(ch);return out;}
std::string name(const CharacterState &c){std::string s=c.name;auto a=s.find_first_not_of(" \t\n\r\v\f"),b=s.find_last_not_of(" \t\n\r\v\f");return a==std::string::npos?"Avatar":s.substr(a,b-a+1);}
const char *record(CommandContext &c,int i){return c.shrine_services&&c.shrine_services->record?c.shrine_services->record(c.shrine_services->context,i):nullptr;}
void deposit(CommandContext &c){c.game.position={{10,7},{18,-1}};c.game.keys=0;c.game.transport=TransportMode::Foot;c.turn.transport_tile=28;}
void question(CommandContext &c,EventSink sink){auto &s=*c.blackthorn;std::string q=record(c,s.round<3?s.round:3);if(s.round<3){if(c.shrine_services->data)q+=str(c.shrine_services->data->virtues[s.shrine]);q+="?\"";}event(sink,GameEventKind::BlackthornPrompt,q);}
NpcActor *adjacent(CommandContext &c){NpcActor *winner=nullptr;if(!c.actors)return nullptr;for(size_t i=0;i<c.actors->count;++i){auto &a=c.actors->actors[i];if(a.location!=c.game.position.map.location||a.z!=c.game.position.map.floor||std::abs(a.x-c.game.position.xy.x)+std::abs(a.y-c.game.position.xy.y)!=1)continue;auto idx=schedule_index(a.schedule.times,uint8_t(c.game.time.hour));int ai=a.schedule.ai[idx];if(ai<=3||((ai==4||ai==5)&&!a.schedule.dialog))continue;if(!winner||a.schedule.slot>=winner->schedule.slot)winner=&a;}return winner;}
// #324 / R-32 -- the staged half of the capture. Present only when the caller
// wired the packed throne-room grid; otherwise every emission below is skipped
// and the capture stays the text-only stream every parity fixture observes.
BlackthornSceneServices *capture_scene(CommandContext &c){
    auto *s=c.blackthorn_scene;return s&&s->capture_tiles&&s->state&&s->script?s:nullptr;
}
void emit_scene(const BlackthornSceneServices &s,EventSink sink){
    GameEvent e;e.kind=GameEventKind::BlackthornScene;e.blackthorn_scene=s.script;if(sink.emit)sink.emit(sink.context,e);
}
// kernel getkey_with_redraw 0x266c. The capture has five of its own (0x0894,
// 0x08cd, 0x053f, 0x04f6, 0x0510); same event kind as the shrine rite uses.
void key_wait(EventSink s){event(s,GameEventKind::ShrineKeyWait);}
// explosion_fx_at_cell (kernel 0x3522) over slot 1's LAST coordinates
// (0x0414-0x041e). The scene window is centred on (5,5), so the cell travels
// as an offset from the centre, exactly like every other CellExplosion.
void emit_sacrifice_explosion(const BlackthornSceneServices &s,EventSink sink){
    int x=0,y=0;sacrifice_victim_cell(*s.state,x,y);
    GameEvent e;e.kind=GameEventKind::CellExplosion;
    e.cell_fx={int16_t(x-kBlackthornSceneCols/2),int16_t(y-kBlackthornSceneRows/2),1,0,0};
    if(sink.emit)sink.emit(sink.context,e);
}
void password(CommandContext &c,EventSink s){c.blackthorn->password=true;event(s,GameEventKind::GuardPasswordPrompt,"\"Give now the\npassword, bearer\nof the Badge!\"\n\nYour response?");}
}
CommandStatus talk_guard(CommandContext &c,const NpcActor &npc,EventSink sink){if(!c.blackthorn)return CommandStatus::InvalidContext;if(c.game.position.map.location==18){if(c.turn.time_spell=='\x1d')password(c,sink);return CommandStatus::Success;}c.blackthorn->tribute=true;c.blackthorn->npc_slot=npc.schedule.slot;GameEvent e;e.kind=GameEventKind::GuardTributePrompt;e.note=c.game.position.map.location==5?-1:count_living(c.game)*10;if(sink.emit)sink.emit(sink.context,e);return CommandStatus::Success;}
CommandStatus blackthorn_action(CommandContext &c,BlackthornAction action,TalkText response,bool agree,EventSink sink,Rand rand){
    if(!c.blackthorn)return CommandStatus::InvalidContext;
    auto &s=*c.blackthorn;auto &g=c.game;
    if(action==BlackthornAction::Password){if(!s.password)return CommandStatus::NoOp;s.password=false;if(!guard_demand(g,c.turn,response,false).ret){event(sink,GameEventKind::Message,"\"Pass, friend!\"");return CommandStatus::Success;}action=BlackthornAction::Capture;}
    if(action==BlackthornAction::Tribute){if(!s.tribute)return CommandStatus::NoOp;s.tribute=false;if(!guard_demand(g,c.turn,{},agree).ret)event(sink,GameEventKind::PartyChanged);else{s.arrest=true;event(sink,GameEventKind::GuardArrestPrompt);}return CommandStatus::Success;}
    if(action==BlackthornAction::Arrest){if(!s.arrest)return CommandStatus::NoOp;s.arrest=false;if(agree){event(sink,GameEventKind::Message,"Yes\n\nThe guard strikes thee unconscious!\n");event(sink,GameEventKind::Message,"\nThou dost awaken to...\n");g.position={{25,4},{4,0}};g.keys=0;c.travel.shadowlord_here=-1;if(g.time.hour!=8){g.time.hour=8;g.time.minute=0;}event(sink,GameEventKind::MapChanged);event(sink,GameEventKind::PartyChanged);}else{event(sink,GameEventKind::Message,"No\n\n\"Then defend thyself, rogue!\"\n");if(c.actors){dialogue_alarm(*c.actors,g.position.map.location,rand);for(size_t i=0;i<c.actors->count;++i){auto &npc=c.actors->actors[i];if(npc.location==g.position.map.location && npc.z==g.position.map.floor && npc.schedule.slot==s.npc_slot)return town_attack_commit(c,npc,true,sink);}}}return CommandStatus::Success;}
    if(action!=BlackthornAction::Capture && (action!=BlackthornAction::Answer || s.shrine<0))return CommandStatus::NoOp;
    for(int i=0;i<12;++i)if(!record(c,i))return CommandStatus::InvalidContext;
    if(action==BlackthornAction::Capture){s.shrine=int8_t(pick_interrogation_shrine(g));s.round=0;s.living=int8_t(count_living(g));if(s.shrine<0){deposit(c);event(sink,GameEventKind::Message,"\nThou art subdued and blindfolded!");event(sink,GameEventKind::MapChanged);event(sink,GameEventKind::PartyChanged);return CommandStatus::Success;}
        const char *texts[4]={"\nThou art subdued and blindfolded!","\n\nStrong guards drag thee away!","\n\nThou hast been chained and manacled!","\n\nFootsteps!"};
        const std::string greeting="\n\nBlackthorn says:\n\n\"Ah, "+name(g.party.characters[0])+"!\n'Tis indeed an honour to meet thee at last! ";
        const int gender=g.party.characters[0].gender;
        const std::string guard_order=std::string("\n\nGUARD! Release this good")+(gender==12?" lady ":gender==11?"man ":"")+"at once!\"";
        auto *scene=capture_scene(c);
        if(scene){
            // The prints of the binary are INTERLEAVED with the scene: the
            // blindfold blackout, the drag, the room mounting with the party
            // chained, the guards marching in, the fizzle of Blackthorn and
            // the two getkey points. Order and offsets: BLCKTHRN 0x0652 ->
            // 0x08d0, mirrored one for one from runCaptureScene() in the
            // TypeScript reference.
            char classes[6]{};const int seated=std::min<int>(s.living,6);
            for(int i=0;i<seated && i<g.party.character_count;++i)classes[i]=g.party.characters[i].character_class;
            init_capture_scene(*scene->state,classes,seated);
            event(sink,GameEventKind::Message,texts[0]);                                  // 0x0652
            build_blackout_intro_script(*scene->script);emit_scene(*scene,sink);          // 0x0672-0x06ae
            event(sink,GameEventKind::Message,texts[1]);                                  // 0x06b0
            build_throne_mount_script(*scene->state,scene->capture_tiles,*scene->script);
            emit_scene(*scene,sink);                                                      // 0x06b9-0x07d1
            event(sink,GameEventKind::Message,texts[2]);                                  // 0x07dc
            build_chained_pause_script(*scene->script);emit_scene(*scene,sink);           // 0x07df
            event(sink,GameEventKind::Message,texts[3]);                                  // 0x07ea
            build_blackthorn_entry_script(*scene->state,*scene->script);
            emit_scene(*scene,sink);                                                      // 0x07ed-0x0878
            event(sink,GameEventKind::Message,greeting);                                  // 0x087f-0x0891
            key_wait(sink);                                                               // 0x0894
            event(sink,GameEventKind::Message,guard_order);                               // 0x0897-0x08bc
            build_guard_release_script(*scene->state,*scene->script);emit_scene(*scene,sink); // 0x08bf
            event(sink,GameEventKind::Message,record(c,11));                              // 0x08c6
            key_wait(sink);                                                               // 0x08cd
            question(c,sink);return CommandStatus::AwaitingResponse;
        }
        for(auto text:texts)event(sink,GameEventKind::Message,text);
        event(sink,GameEventKind::Message,greeting);event(sink,GameEventKind::Message,guard_order);event(sink,GameEventKind::Message,record(c,11));question(c,sink);return CommandStatus::AwaitingResponse;
    }
    auto trimmed=talk_trim(response);TalkText mantra=c.shrine_services->data?c.shrine_services->data->mantras[s.shrine]:TalkText{};bool matched=quest_text_contains(trimmed.substr(0,14),mantra);
    auto *scene=capture_scene(c);
    if(!matched && s.round>=1 && s.living>1)advance_clock(g,c.turn,2,&rand,c.sky);
    if(!matched && s.living>1 && s.round<3){
        if(!s.round){
            event(sink,GameEventKind::Message,record(c,7));
            // anim_vm 0x36da runs BETWEEN rec7 and rec8 (0x0523): the guard
            // marches the companion to the torture table and the hourglass is
            // planted full. The getkey at 0x053f follows the die! line.
            if(scene){build_warning_script(*scene->state,*scene->script);emit_scene(*scene,sink);}
            event(sink,GameEventKind::Message,std::string(record(c,8))+(g.party.character_count>1?g.party.characters[1].name:"")+" die!\" \n\n");
            if(scene)key_wait(sink);
        }
        // Escalation: the sand falls on a failed round 1 or 2 (0x05da/0x05e2).
        if(scene && s.round>=1 && build_hourglass_script(s.round,*scene->script))emit_scene(*scene,sink);
        ++s.round;question(c,sink);return CommandStatus::AwaitingResponse;}
    if(matched){g.quest.shrine_destroyed[s.shrine]=255;g.quest.destroyed_count=std::max<uint8_t>(g.quest.destroyed_count,uint8_t(s.shrine+1));g.karma=uint8_t(g.karma<=5?0:g.karma-5);if(s.living>1)sacrifice_first_companion(g);event(sink,GameEventKind::Message,record(c,s.living>1?5:9));
        if(scene){
            // Betrayal. With companions, rec5 is printed inside
            // sacrifice_member(0) before the siren (0x03c2); alone it is the
            // pardon at 0x058e and nothing is sacrificed. Both then take the
            // common tail: getkey 0x0510, then anim_vm 0x369e.
            if(s.living>1){build_sacrifice_script(*scene->state,*scene->script);emit_scene(*scene,sink);emit_sacrifice_explosion(*scene,sink);}
            key_wait(sink);
            build_finale_script(*scene->state,*scene->script);emit_scene(*scene,sink);
        }}
    else if(s.living<2){event(sink,GameEventKind::Message,record(c,10));
        if(scene){key_wait(sink);build_finale_script(*scene->state,*scene->script);emit_scene(*scene,sink);}}
    else{auto victim=sacrifice_first_companion(g);event(sink,GameEventKind::Message,record(c,4));
        if(scene){build_sacrifice_script(*scene->state,*scene->script);emit_scene(*scene,sink);emit_sacrifice_explosion(*scene,sink);}
        event(sink,GameEventKind::Message,"\n\n"+victim+" is sliced in half! ");
        if(scene)key_wait(sink);                                          // 0x04f6
        event(sink,GameEventKind::Message,record(c,6));
        // 0x08d9: only if slot 8 is still standing -- the 0x369e exits have
        // already removed him, the pendulum has not.
        if(scene && blackthorn_on_stage(*scene->state)){build_blackthorn_exit_script(*scene->state,*scene->script);emit_scene(*scene,sink);}}
    s.shrine=-1;deposit(c);event(sink,GameEventKind::MapChanged);event(sink,GameEventKind::PartyChanged);return CommandStatus::Success;
}
bool blackthorn_turn_effect(CommandContext &c,CommandEffect effect,EventSink sink,Rand rand){
    if(!c.blackthorn)return false;
    auto &s=*c.blackthorn;auto npc=adjacent(c);
    if(effect==CommandEffect::Capture){bool alive=false;for(int i=0;i<c.game.party.party_size && i<c.game.party.character_count;++i)alive|=c.game.party.characters[i].status=='G'||c.game.party.characters[i].status=='P'||c.game.party.characters[i].status=='S';if(c.game.position.map.location!=18||!alive||!npc||npc->schedule.type!=112)return false;if(c.turn.time_spell=='\x1d')password(c,sink);else blackthorn_action(c,BlackthornAction::Capture,{},false,sink,rand);return true;}
    if(effect!=CommandEffect::Tribute || !c.game.position.map.location || s.tribute||s.arrest||!npc)return false;
    int idx=schedule_index(npc->schedule.times,uint8_t(c.game.time.hour)),ai=npc->schedule.ai[idx];
    if(ai>5){if(npc->schedule.dialog==254){event(sink,GameEventKind::Message,"\"Begone,\nvermin!\"\n");if(npc->schedule.type>=64 && npc->schedule.type<116){npc->schedule.dialog=253;for(auto &a:npc->schedule.ai)a=3;}return true;}if(npc->schedule.type==112 && c.game.position.map.location!=18){s.arrest=true;s.npc_slot=npc->schedule.slot;event(sink,GameEventKind::GuardArrestPrompt);return true;}if(npc->schedule.type>=64)return town_attack_commit(c,*npc,true,sink)==CommandStatus::Success;for(size_t i=0;i<c.actors->count;++i)if(&c.actors->actors[i]==npc){for(size_t j=i+1;j<c.actors->count;++j)c.actors->actors[j-1]=c.actors->actors[j];--c.actors->count;break;}return false;}
    if(c.game.position.map.location==18)return false;
    if(ai==4)npc->schedule.ai[idx]=1;
    if(npc->schedule.dialog!=255){
        const int dialog=npc->schedule.dialog;
        GameEvent e;e.npc=npc;
        if(dialog>=128 && dialog<=252){
            if(!shop_is_open(npc->schedule.times,uint8_t(c.game.time.hour))){event(sink,GameEventKind::Message,"A merchant says:\n\"Come see me at\nmy shoppe, when\nit's open!\"\n");return true;}
            if((c.turn.transport_tile&252)==16 && dialog!=131){event(sink,GameEventKind::Message,"A merchant says:\n\"GET THAT HORSE OUT OF HERE!\"\n");return true;}
            e.kind=GameEventKind::NpcInitiatesShop;
        }else{
            if(!c.dialogue_services || !talk_script_for(c.dialogue_services->registry,c.game.position.map.location,dialog))return false;
            e.kind=GameEventKind::NpcInitiatesTalk;
        }
        if(sink.emit)sink.emit(sink.context,e);
        return true;
    }
    talk_guard(c,*npc,sink);return true;
}
}
