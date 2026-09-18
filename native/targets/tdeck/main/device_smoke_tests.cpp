#include "device_smoke_tests.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include "openu5/debug_developer.h"
#include "openu5/display_names.h"
#include "openu5/gameplay_save.h"
#include "openu5/look.h"
#include "openu5/presentation.h"
#include "openu5/ui_debug_menu.h"
#include "ui_input_adapter.h"

namespace tdeck {
namespace {
struct Scenario { uint8_t group; const char *name; };
constexpr const char *groups[kSmokeGroupCount]={
    "Overworld","Local Maps","Dialogue","Shops / Inns","Inventory / Equipment",
    "Combat","Dungeons","Shrines / Special","Transport","Quest / Progression",
    "Persistence","Device Input","Debug Tools","Resources","Presentation"};
constexpr Scenario scenarios[kSmokeScenarioCount]={
    {0,"movement/passability state"},{0,"visibility/terrain snapshot"},{0,"Look/sign/direction prompt"},
    {1,"map entry/exit resources"},{1,"doors/object overlays"},{1,"local Look/boundaries"},
    {2,"Talk direction routing"},{2,"text/keyword isolation"},{2,"transcript wrap/Cancel"},
    {3,"shop records and identities"},{3,"offer names/prices/selections"},{3,"numeric/yes-no/Back/exit"},
    {4,"equipment name parity"},{4,"usable/reagent/spell names"},{4,"generic-id leakage scan"},
    {5,"arena/enemy identity"},{5,"target/marker presentation"},{5,"victory cleanup/shortcuts"},
    {6,"dungeon resource floors"},{6,"corridor/fixed encounter data"},{6,"loot/exit plumbing"},
    {7,"shrine authoritative text"},{7,"special modal prompts"},{7,"quest mutation plumbing"},
    {8,"transport state domain"},{8,"board/disembark commands"},{8,"direction/collision plumbing"},
    {9,"search/special fixtures"},{9,"Shadowlord/shard state"},{9,"endgame state plumbing"},
    {10,"JSON round trip"},{10,"GAM/OOL generation"},{10,"generation recovery rules"},
    {11,"trackball/Mic semantics"},{11,"WASD semantic directions"},{11,"text/numeric/shortcut isolation"},
    {12,"developer open/Back"},{12,"teleport/preset inventory"},{12,"value edit/gameplay restore"},
    {13,"resource pack identity"},{13,"maps/dialogue/shop/sign data"},{13,"enemy/NPC/location names"},
    {14,"viewport/animation hash"},{14,"transcript/shop/inventory UI"},{14,"reticle/active marker state"}
};

uint32_t hash_bytes(const void *data,size_t size){
    auto *p=static_cast<const uint8_t*>(data);uint32_t h=2166136261u;
    while(size--){h^=*p++;h*=16777619u;}return h;
}
DeviceSmokeTests::Result ok(uint32_t s=0,uint32_t u=0,uint32_t p=0){DeviceSmokeTests::Result r;r.pass=true;r.state_hash=s;r.ui_hash=u;r.presentation_hash=p;std::snprintf(r.reason,sizeof(r.reason),"ok");return r;}
DeviceSmokeTests::Result fail(const char *why){DeviceSmokeTests::Result r;r.pass=false;std::snprintf(r.reason,sizeof(r.reason),"%s",why?why:"failed");return r;}
struct Spy {openu5::UiIntent last{};size_t count=0;static void send(void*p,const openu5::UiIntent&i){auto&s=*static_cast<Spy*>(p);s.last=i;++s.count;}};
openu5::UiAction character(char c){openu5::UiAction a;a.kind=openu5::UiActionKind::Character;a.character=char16_t(c);return a;}
openu5::UiAction direction(openu5::Direction d){openu5::UiAction a;a.kind=openu5::UiActionKind::Direction;a.direction=d;return a;}
RawInputEvent key(char c,int64_t time=1){RawInputEvent r;r.kind=RawInputKind::Keyboard;r.code=uint8_t(c);r.transition=KeyTransition::Pressed;r.timestamp_us=time;return r;}
RawInputEvent mic(KeyTransition t,int64_t time){RawInputEvent r;r.kind=RawInputKind::Keyboard;r.transition=t;r.column=kMicrophoneKeyColumn;r.row=kMicrophoneKeyRow;r.timestamp_us=time;return r;}
bool contains_text(const openu5::UiSession &ui,const char *needle){for(size_t i=0;i<ui.transcript_size();++i){auto*b=ui.transcript_at(i);if(b&&std::strstr(b->text,needle))return true;}return false;}
}

const char *DeviceSmokeTests::group_name(size_t i){return i<kSmokeGroupCount?groups[i]:"Unknown";}

void DeviceSmokeTests::start(int group){
    selected_group_=group>=0&&group<int(kSmokeGroupCount)?group:-1;cursor_=passed_=failed_=completed_=0;
    total_=selected_group_<0?kSmokeScenarioCount:3;running_=true;complete_=false;current_[0]=first_failure_[0]=0;
    write_header();
}

SmokeView DeviceSmokeTests::view() const{
    const char*g=selected_group_<0?"All":group_name(size_t(selected_group_));
    return {running_,complete_,passed_,failed_,completed_,total_,g,current_,first_failure_};
}

bool DeviceSmokeTests::pump(){
    if(!running_)return false;
    while(cursor_<kSmokeScenarioCount&&(selected_group_>=0&&scenarios[cursor_].group!=selected_group_))++cursor_;
    if(cursor_>=kSmokeScenarioCount){running_=false;complete_=true;write_footer();return true;}
    std::snprintf(current_,sizeof(current_),"%s",scenarios[cursor_].name);
    auto result=run(cursor_);if(result.pass)++passed_;else{++failed_;if(!*first_failure_)std::snprintf(first_failure_,sizeof(first_failure_),"%.42s: %.66s",scenarios[cursor_].name,result.reason);}
    write_result(cursor_,result);++completed_;++cursor_;
    if(completed_>=total_){running_=false;complete_=true;write_footer();}
    return true;
}

DeviceSmokeTests::Result DeviceSmokeTests::run(size_t id) const{
    auto&e=environment_;if(!e.context||!e.resources||!e.resource_report||!e.tile_report||!e.ui)return fail("smoke environment not bound");
    auto&r=*e.resources;auto&c=*e.context;
    switch(id){
    case 0:{auto m=openu5::get_active_map(r.world,c.game.position.map);if(m.error!=openu5::Error::None)return fail("movement map unavailable");return openu5::in_bounds(c.game.position.xy.x,c.game.position.xy.y,m.value.geometry)?ok(hash_bytes(&c.game.position,sizeof(c.game.position))):fail("world position outside active map");}
    case 1:{auto m=openu5::get_active_map(r.world,c.game.position.map);if(m.error!=openu5::Error::None)return fail("active map unavailable");auto s=openu5::compose_world_presentation(c,m.value,c.game.position.xy,e.tile_report->avatar_tile);return ok(0,0,hash_bytes(&s,sizeof(s)));}
    case 2:{if(r.sign_count<3)return fail("fewer than three authoritative signs");Spy spy;openu5::UiTextBlock b[12]{};openu5::UiSession ui{{b,12},{&spy,Spy::send}};ui.handle_input(character('l'));ui.handle_input(direction(openu5::Direction::North));return spy.last.command.kind==openu5::CommandKind::Look&&spy.last.command.has_direction?ok(0,hash_bytes(b,sizeof(b))):fail("Look direction did not route semantically");}
    case 3:return r.world.small_map_count>=32?ok(uint32_t(r.world.small_map_count)):fail("local-map table incomplete");
    case 4:return r.search_count>20?ok(uint32_t(r.search_count)):fail("dynamic object/search overlay fixtures missing");
    case 5:{for(size_t i=0;i<std::min<size_t>(r.sign_count,8);++i)if(!r.signs[i].value.text||!*r.signs[i].value.text)return fail("blank authoritative sign text");return ok(uint32_t(r.sign_count));}
    case 6:{Spy spy;openu5::UiTextBlock b[8]{};openu5::UiSession ui{{b,8},{&spy,Spy::send}};ui.handle_input(character('t'));ui.handle_input(direction(openu5::Direction::East));return spy.last.command.kind==openu5::CommandKind::Talk?ok():fail("Talk direction mismatch");}
    case 7:{openu5::UiTextBlock b[8]{};openu5::UiSession ui{{b,8}};ui.begin_text(openu5::UiRequestId::Dialogue,"Response?");ui.handle_input(character('w'));return ui.input_length()==1&&ui.input_buffer()[0]==u'w'?ok():fail("text entry did not preserve literal W");}
    case 8:{openu5::UiTextBlock b[16]{};openu5::UiSession ui{{b,16},{},{8,3,20}};ui.append(openu5::UiTextChannel::Dialogue,"one two three four\nfive");return ui.wrapped_line_count()>=3?ok(0,hash_bytes(b,sizeof(b))):fail("explicit newline/wrapping regression");}
    case 9:{bool types[8]{};for(size_t i=0;i<r.shop_record_count;++i){auto&x=r.shop_records[i];if(!x.name||!*x.name||!x.keeper||!*x.keeper)return fail("shop identity missing");types[int(x.type)]=true;}for(bool x:types)if(!x)return fail("one or more service types absent");return ok(uint32_t(r.shop_record_count));}
    case 10:{for(int i=0;i<48;++i)if(!openu5::equipment_display_name(i))return fail("shop equipment name unresolved");for(int i=0;i<8;++i)if(!openu5::reagent_display_name(i))return fail("shop reagent name unresolved");return ok();}
    case 11:{openu5::UiTextBlock b[16]{};openu5::UiSession ui{{b,16}};openu5::ShopSession ss;ss.phase=openu5::ShopPhase::Menu;ss.type=openu5::ShopType::InnKeeper;openu5::ShopEvent se{openu5::ShopEventKind::State,&ss};openu5::GameEvent ge{};ge.kind=openu5::GameEventKind::Shop;ge.shop=&se;ui.consume(ge);return ui.mode()==openu5::UiMode::Shop&&std::strstr(ui.prompt(),"Rest")?ok(0,hash_bytes(b,sizeof(b))):fail("inn menu prompt not visible");}
    case 12:for(int i=0;i<48;++i)if(!openu5::equipment_display_name(i))return fail("equipment name unresolved");return ok();
    case 13:for(int i=0;i<8;++i)if(!openu5::potion_display_name(i)||!openu5::scroll_display_name(i)||!openu5::reagent_display_name(i))return fail("consumable name unresolved");for(int i=0;i<48;++i)if(!openu5::spell_display_name(i))return fail("spell name unresolved");return ok();
    case 14:return !openu5::is_generic_identifier_label(openu5::equipment_display_name(4))&&openu5::is_generic_identifier_label("Equipment 4")?ok():fail("generic-id detector failed");
    case 15:{if(r.combat_map_count<1||r.combat_enemy_count<40)return fail("combat resources incomplete");bool rat=false;for(size_t i=0;i<r.combat_enemy_count;++i){auto*x=r.combat_enemy_views[i];if(!x||!x->name||!*x->name)return fail("enemy name missing");if(std::strcmp(x->name,"Giant Rat")==0)rat=true;}return rat?ok(uint32_t(r.combat_enemy_count)):fail("Giant Rat identity missing");}
    case 16:{openu5::CombatState s{};for(auto&t:s.map.tiles)t=5;openu5::CombatEnemy enemy{};enemy.name="Probe";s.count=1;s.current=0;s.actors[0].enemy=&enemy;s.actors[0].position={5,5};s.actors[0].status=openu5::CombatStatus::Active;auto p=openu5::compose_combat_presentation(s,c.game);return p.combat&&p.active_x==5&&p.active_y==5?ok(0,0,hash_bytes(&p,sizeof(p))):fail("active actor marker missing");}
    case 17:{Spy spy;openu5::UiTextBlock b[16]{};openu5::UiSession ui{{b,16},{&spy,Spy::send}};openu5::GameEvent ge{};ge.kind=openu5::GameEventKind::CombatStarted;ui.consume(ge);ui.begin_target(openu5::UiRequestId::Target,"Aim",{});ge.kind=openu5::GameEventKind::CombatEnded;ui.consume(ge);ui.handle_input(character('l'));return ui.mode()==openu5::UiMode::TargetSelection&&spy.count==0?ok():fail("post-combat Look did not enter exploration direction prompt");}
    case 18:return e.resource_report->dungeon_count==8?ok(8):fail("expected eight dungeon resources");
    case 19:return r.combat_map_count>=16?ok(uint32_t(r.combat_map_count)):fail("fixed encounter arenas missing");
    case 20:return r.search_count&&r.shard_spawn_count==3?ok(uint32_t(r.search_count)):fail("dungeon loot/exit fixture plumbing missing");
    case 21:return r.shrine_data.count==8&&r.shrine_text?ok(8):fail("shrine text table incomplete");
    case 22:{openu5::UiTextBlock b[12]{};openu5::UiSession ui{{b,12}};openu5::GameEvent ge{};ge.kind=openu5::GameEventKind::ShrineDonatePrompt;ui.consume(ge);return ui.mode()==openu5::UiMode::NumericEntry&&contains_text(ui,"cycles")?ok():fail("shrine numeric prompt missing");}
    case 23:{auto g=c.game;const auto before=g.quest.shrine_visited;g.quest.shrine_visited^=1;return g.quest.shrine_visited!=before&&c.game.quest.shrine_visited==before?ok(hash_bytes(&g.quest,sizeof(g.quest))):fail("isolated quest mutation failed");}
    case 24:return int(c.game.transport)<=int(openu5::TransportMode::Ship)?ok(uint32_t(c.game.transport)):fail("transport mode outside domain");
    case 25:{Spy spy;openu5::UiTextBlock b[8]{};openu5::UiSession ui{{b,8},{&spy,Spy::send}};ui.handle_input(character('b'));if(spy.last.command.kind!=openu5::CommandKind::Board)return fail("Board shortcut mismatch");ui.handle_input(character('x'));return spy.last.command.kind==openu5::CommandKind::Disembark?ok():fail("Disembark shortcut mismatch");}
    case 26:return c.transport_services?ok():fail("transport collision/direction services not bound");
    case 27:return r.search_count>0&&r.search_objects?ok(uint32_t(r.search_count)):fail("Search fixtures missing");
    case 28:return r.shard_spawn_count==3?ok(hash_bytes(r.shard_spawns,r.shard_spawn_count*sizeof(*r.shard_spawns))):fail("shard spawn parity mismatch");
    case 29:return int(openu5::QuestFlag::Count)>3?ok(uint32_t(openu5::QuestFlag::Count)):fail("progression flag plumbing missing");
    case 30:{std::string text;auto retained=openu5::save::empty_sidecar();if(openu5::save::save_state(c.game,c.turn,retained,text)!=openu5::save::Error::None)return fail("save_state failed");openu5::GameState g{};openu5::TurnState t{};openu5::save::Json kept;if(openu5::save::load_state(text,g,t,kept)!=openu5::save::Error::None)return fail("load_state failed");return g.position.xy.x==c.game.position.xy.x?ok(openu5::save::save_crc32(reinterpret_cast<const uint8_t*>(text.data()),text.size())):fail("JSON round-trip state mismatch");}
    case 31:{openu5::save::Gam gam{};openu5::save::Json side;auto kept=openu5::save::empty_sidecar();auto error=openu5::save::export_native_state(c.game,c.turn,kept,r.initial_gam,r.initial_gam_size,gam,side);if(error!=openu5::save::Error::None)return fail("GAM export failed");auto ool=openu5::save::build_ool(side,r.initial_ool,r.initial_ool_size);return ok(openu5::save::save_crc32(gam.data(),gam.size()),0,openu5::save::save_crc32(ool.data(),ool.size()));}
    case 32:{openu5::save::Gam gam{};openu5::save::Json side;auto kept=openu5::save::empty_sidecar();if(openu5::save::export_native_state(c.game,c.turn,kept,r.initial_gam,r.initial_gam_size,gam,side)!=openu5::save::Error::None)return fail("recovery fixture export failed");openu5::save::Generation g[2]{};g[0].sequence=1;g[0].committed=true;g[0].identity_matches=true;g[0].gam=gam.data();g[0].gam_size=gam.size();g[0].gam_crc=openu5::save::save_crc32(gam.data(),gam.size());g[1]=g[0];g[1].sequence=2;g[1].gam_crc^=1;return openu5::save::select_generation(g,2)==0?ok():fail("corrupt-newest fallback selection mismatch");}
    case 33:{openu5::InputController input;openu5::Direction d{};RawInputEvent raw{};raw.kind=RawInputKind::TrackballLeft;raw.timestamp_us=100000;UiInputAdapter a;openu5::UiAction action{};DeviceShortcut shortcut{};if(!input.normalize(raw,d)||d!=openu5::Direction::West)return fail("trackball direction mismatch");if(a.translate(mic(KeyTransition::Pressed,0),openu5::UiMode::Exploration,action,shortcut))return fail("Mic press leaked");return a.translate(mic(KeyTransition::Released,100000),openu5::UiMode::Exploration,action,shortcut)&&action.kind==openu5::UiActionKind::Cancel?ok():fail("Mic Cancel mismatch");}
    case 34:{UiInputAdapter a;openu5::UiAction action{};DeviceShortcut shortcut{};a.translate(mic(KeyTransition::Pressed,0),openu5::UiMode::Exploration,action,shortcut);if(!a.update(1100000,openu5::UiMode::Exploration,shortcut))return fail("Movement Mode hold failed");const char keys[]="wasd";const openu5::Direction dirs[]={openu5::Direction::North,openu5::Direction::West,openu5::Direction::South,openu5::Direction::East};for(int i=0;i<4;++i)if(!a.translate(key(keys[i],1200000+i),openu5::UiMode::TargetSelection,action,shortcut,true)||action.kind!=openu5::UiActionKind::Direction||action.direction!=dirs[i])return fail("WASD prompt direction mismatch");return ok();}
    case 35:{UiInputAdapter a;openu5::UiAction action{};DeviceShortcut shortcut{};a.translate(mic(KeyTransition::Pressed,0),openu5::UiMode::Exploration,action,shortcut);a.update(1100000,openu5::UiMode::Exploration,shortcut);if(!a.translate(key('w'),openu5::UiMode::TextEntry,action,shortcut)||action.kind!=openu5::UiActionKind::Character)return fail("text-entry W was converted");if(!a.translate(key('7'),openu5::UiMode::NumericEntry,action,shortcut)||action.character!=u'7')return fail("numeric entry isolation failed");return ok();}
    case 36:{openu5::UiDebugMenu menu(c);menu.open();openu5::UiAction back{};back.kind=openu5::UiActionKind::Back;menu.handle_input(back);return !menu.is_open()?ok():fail("developer Back did not close root");}
    case 37:return openu5::debug_destination_count(c)>=40?ok(uint32_t(openu5::debug_destination_count(c))):fail("teleport destinations incomplete");
    case 38:{auto g=c.game;auto before=g.gold;auto rr=openu5::debug_set_resource(g,openu5::DebugResource::Gold,before+1);return rr.status==openu5::DebugStatus::Applied&&g.gold==before+1&&c.game.gold==before?ok():fail("debug value edit isolation failed");}
    case 39:return e.resource_report->firmware_match&&e.tile_report->firmware_match?ok(e.resource_report->payload_crc32,0,e.tile_report->payload_crc32):fail("resource pack identity mismatch");
    case 40:return r.world.overworld&&r.dialogue_data&&r.shop_record_count&&r.sign_count?ok(uint32_t(r.dialogue_data_size),uint32_t(r.shop_record_count),uint32_t(r.sign_count)):fail("cross-resource payload missing");
    case 41:{for(size_t i=0;i<r.combat_enemy_count;++i)if(!r.combat_enemy_views[i]->name||!*r.combat_enemy_views[i]->name)return fail("enemy label missing");for(size_t i=0;i<e.resource_report->npc_count;++i)if(r.npc_slots[i].dialog>0&&!r.dialogue_data)return fail("NPC dialogue name source missing");return ok(uint32_t(e.resource_report->npc_count));}
    case 42:{auto m=openu5::get_active_map(r.world,c.game.position.map);if(m.error!=openu5::Error::None)return fail("presentation map unavailable");auto p=openu5::compose_world_presentation(c,m.value,c.game.position.xy,e.tile_report->avatar_tile);openu5::ActorAnimationClock clock;clock.render(p,7);return ok(0,0,hash_bytes(&p,sizeof(p)));}
    case 43:{openu5::UiRenderedLine lines[12]{};auto n=e.ui->visible_lines(lines,12);for(size_t i=0;i<n;++i)if(openu5::is_generic_identifier_label(lines[i].text))return fail("generic id leaked into visible transcript");return ok(0,hash_bytes(lines,n*sizeof(*lines)));}
    case 44:{openu5::CombatState s{};for(auto&t:s.map.tiles)t=5;openu5::CombatEnemy enemy{};enemy.name="Probe";s.count=1;s.current=0;s.actors[0].enemy=&enemy;s.actors[0].position={5,5};s.actors[0].status=openu5::CombatStatus::Active;auto p=openu5::compose_combat_presentation(s,c.game);p.target_x=6;p.target_y=5;p.target_valid=true;return p.active_x==5&&p.target_x==6?ok(0,0,hash_bytes(&p,sizeof(p))):fail("marker/reticle state missing");}
    default:return fail("unknown scenario");
    }
}

void DeviceSmokeTests::write_header() const{
    mkdir("/sd/ultima5",0777);mkdir("/sd/ultima5/logs",0777);
    if(auto*f=std::fopen(kSmokeTestLogPath,"w")){std::fprintf(f,"OpenU5 T-Deck deterministic smoke tests\nscope=%s scenarios=%u\n",selected_group_<0?"all":group_name(size_t(selected_group_)),unsigned(total_));std::fclose(f);}
}
void DeviceSmokeTests::write_result(size_t id,const Result&r) const{
    if(auto*f=std::fopen(kSmokeTestLogPath,"a")){std::fprintf(f,"%s | %s | %s | reason=%s | state=%08lx ui=%08lx presentation=%08lx\n",group_name(scenarios[id].group),scenarios[id].name,r.pass?"PASS":"FAIL",r.reason,(unsigned long)r.state_hash,(unsigned long)r.ui_hash,(unsigned long)r.presentation_hash);std::fclose(f);}
}
void DeviceSmokeTests::write_footer() const{
    if(auto*f=std::fopen(kSmokeTestLogPath,"a")){std::fprintf(f,"TOTAL pass=%u fail=%u complete=%u/%u\nfirst_failure=%s\n",unsigned(passed_),unsigned(failed_),unsigned(completed_),unsigned(total_),*first_failure_?first_failure_:"none");std::fclose(f);}
}

} // namespace tdeck
