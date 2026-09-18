#include "openu5/ui_session.h"
#include "openu5/dialogue_orchestration.h"
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
#include "openu5/ui_debug_menu.h"
#endif
#include <algorithm>
#include <iostream>
#include <string>
using namespace openu5;

namespace {
const char *mode_name(UiMode m){static const char*n[]={"explore","dungeon","combat","dialogue","shop","shrine","text","number","yes/no","party","inventory","equipment","spell","target","debug"};return n[int(m)];}
size_t option_count(void*){return 4;}UiSelectionItem option(void*,size_t i){static const char*n[]={"Avatar","Sword","In Lor","Target"};return {n[i],true};}
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
void show_debug(const UiDebugMenu &debug){const auto v=debug.view();std::cout<<v.title<<" > "<<v.item;if(v.editing)std::cout<<" = "<<v.value<<" ["<<v.minimum<<".."<<v.maximum<<"]";std::cout<<"\n";}
#endif
struct Host { UiSession *ui=nullptr; static void dispatch(void *p,const UiIntent&i){auto&h=*static_cast<Host*>(p);if(i.kind==UiIntentKind::Command){std::cout<<"command "<<int(i.command.kind)<<"\n";if(i.command.kind==CommandKind::DialogueText){GameEvent e;e.kind=GameEventKind::Message;e.text="The NPC answers, then departs.";h.ui->consume(e);DialogueEvent d;d.kind=DialogueEventKind::Ended;e={};e.kind=GameEventKind::Dialogue;e.dialogue=&d;h.ui->consume(e);return;}GameEvent e;e.kind=GameEventKind::Message;e.text="[host accepted semantic command]";h.ui->consume(e);}else if(i.kind==UiIntentKind::Shop){std::cout<<"shop action "<<int(i.shop.action)<<"\n";ShopResult r{true,"Transaction accepted."};ShopSession s;s.phase=i.shop.action==ShopAction::End?ShopPhase::Closed:ShopPhase::Menu;ShopEvent se{i.shop.action==ShopAction::End?ShopEventKind::Exited:ShopEventKind::Result,&s,&r};GameEvent e;e.kind=GameEventKind::Shop;e.shop=&se;h.ui->consume(e);}else if(i.kind==UiIntentKind::OpenPartySelection)h.ui->begin_selection(UiMode::PartySelection,i.request,"Party member",{nullptr,option_count,option});else if(i.kind==UiIntentKind::OpenInventorySelection)h.ui->begin_selection(UiMode::InventorySelection,i.request,"Inventory item",{nullptr,option_count,option});else if(i.kind==UiIntentKind::OpenEquipmentSelection)h.ui->begin_selection(UiMode::EquipmentSelection,i.request,"Equipment",{nullptr,option_count,option});else if(i.kind==UiIntentKind::OpenSpellSelection)h.ui->begin_selection(UiMode::SpellSelection,i.request,"Spell",{nullptr,option_count,option});else std::cout<<"intent "<<int(i.kind)<<" request "<<int(i.request)<<"\n";} };
}
int main(){
 UiTextBlock history[32];Host host;UiSession ui{{history,32},{&host,Host::dispatch},{38,8,32}};host.ui=&ui;
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
 GameState game;TurnState turn;TravelState travel;CommandState commands;static uint8_t large_map[65536]{};WorldData world{large_map,large_map,sizeof(large_map),sizeof(large_map)};CommandContext context{game,turn,travel,commands,world};game.party.character_count=game.party.party_size=1;game.party.characters[0].party_status=0;game.party.characters[0].character_class='A';UiDebugMenu debug(context);ui.attach_debug_menu(&debug);
#endif
 std::cout<<"OpenU5 UI semantic harness. n/s/e/w, letters, enter, esc, backspace, pgup/pgdn, mode explore|dungeon|combat, dialogue, shop, endcombat, debug, quit\n";
 std::string line;
 while(std::cout<<"["<<mode_name(ui.mode())<<"] > "&&std::getline(std::cin,line)&&line!="quit"){
  UiAction a;char16_t text_input[64]{};
  if(line=="enter")a.kind=UiActionKind::Confirm;else if(line=="esc")a.kind=UiActionKind::Cancel;else if(line=="backspace")a.kind=UiActionKind::DeleteCharacter;else if(line=="pgup")a.kind=UiActionKind::PageUp;else if(line=="pgdn")a.kind=UiActionKind::PageDown;
  else if(ui.mode()==UiMode::TextEntry||ui.mode()==UiMode::NumericEntry){const size_t n=std::min(line.size(),size_t(63));for(size_t i=0;i<n;++i)text_input[i]=char16_t(static_cast<unsigned char>(line[i]));a.kind=UiActionKind::TextInput;a.text=text_input;a.text_length=n;}
  else if(ui.mode()==UiMode::YesNo&&line.size()==1){a.kind=UiActionKind::Character;a.character=char16_t(static_cast<unsigned char>(line[0]));}
  else if(line=="n"||line=="s"||line=="e"||line=="w"){a.kind=UiActionKind::Direction;a.direction=line=="n"?Direction::North:line=="s"?Direction::South:line=="e"?Direction::East:Direction::West;}
  else if(line=="dialogue"){DialogueOutput out;out.kind=DialogueOutputKind::Line;out.text=u"What wouldst thou know?";DialogueEvent d{DialogueEventKind::Output,&out};GameEvent e;e.kind=GameEventKind::Dialogue;e.dialogue=&d;ui.consume(e);out.kind=DialogueOutputKind::Prompt;out.question=false;ui.consume(e);continue;}
  else if(line=="shop"){ShopSession s;s.phase=ShopPhase::Menu;ShopEvent se{ShopEventKind::State,&s};GameEvent e;e.kind=GameEventKind::Shop;e.shop=&se;ui.consume(e);continue;}
  else if(line=="endcombat"){GameEvent e;e.kind=GameEventKind::CombatEnded;ui.consume(e);continue;}
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
  else if(line=="debug"){ui.open_debug_menu();show_debug(debug);continue;}
#endif
  else if(line.rfind("mode ",0)==0){ui.set_base_mode(line.substr(5)=="combat"?UiMode::Combat:line.substr(5)=="dungeon"?UiMode::Dungeon:UiMode::Exploration);continue;}
  else if(line.size()==1){a.kind=UiActionKind::Character;a.character=char16_t(static_cast<unsigned char>(line[0]));}else{std::cout<<"unknown\n";continue;}
  ui.handle_input(a);
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
  if(ui.mode()==UiMode::DebugMenu){show_debug(debug);continue;}
#endif
  UiRenderedLine rows[8];const auto count=ui.visible_lines(rows,8);for(size_t i=0;i<count;++i)std::cout<<rows[i].text<<"\n";
 }
}
