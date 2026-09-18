#include "openu5/combat.h"
#include "openu5/dialogue_orchestration.h"
#include "openu5/dungeon.h"
#include "openu5/ui_session.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace openu5;

namespace {
int checks=0;
void check(bool v){++checks;if(!v){std::cerr<<"ui session check "<<checks<<" failed\n";std::exit(1);}}
UiAction ch(char c){UiAction a;a.kind=UiActionKind::Character;a.character=char16_t(c);return a;}
UiAction action(UiActionKind k){UiAction a;a.kind=k;return a;}
UiAction dir(Direction d){UiAction a;a.kind=UiActionKind::Direction;a.direction=d;return a;}
struct Spy { std::vector<UiIntent> intents; static void send(void *p,const UiIntent&i){static_cast<Spy*>(p)->intents.push_back(i);} };
size_t item_count(void*){return 3;} UiSelectionItem item(void*,size_t i){static const char*names[]={"Avatar","Shamino","Iolo"};return {names[i],i!=1};}
}
int main(){
 UiTextBlock blocks[12];Spy spy;UiSession ui{{blocks,12},{&spy,Spy::send},{8,3,12}};
 // World command routing and a direction-gated command.
 check(ui.handle_input(dir(Direction::East)));check(spy.intents.back().command.kind==CommandKind::Move&&spy.intents.back().command.direction==Direction::East);
 ui.handle_input(ch('t'));check(ui.mode()==UiMode::TargetSelection);ui.handle_input(dir(Direction::North));check(spy.intents.back().command.kind==CommandKind::Talk&&spy.intents.back().command.has_direction);
 ui.handle_input(ch('f'));check(ui.mode()==UiMode::TargetSelection&&ui.target_x()==5&&ui.target_y()==5&&!ui.target_has_direction());const auto fire_before=spy.intents.size();ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.size()==fire_before&&ui.mode()==UiMode::TargetSelection);ui.handle_input(dir(Direction::South));check(ui.mode()==UiMode::TargetSelection&&ui.target_x()==5&&ui.target_y()==6&&ui.target_has_direction());ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().command.kind==CommandKind::Fire&&spy.intents.back().command.has_direction&&spy.intents.back().command.direction==Direction::South);
 ui.handle_input(ch('f'));check(ui.mode()==UiMode::TargetSelection);ui.handle_input(action(UiActionKind::Cancel));check(ui.mode()==UiMode::Exploration&&spy.intents.back().kind==UiIntentKind::ModalResponse&&spy.intents.back().request==UiRequestId::Direction&&!spy.intents.back().value.accepted);
 ui.handle_input(ch('c'));check(ui.transcript_at(ui.transcript_size()-1)->channel==UiTextChannel::CommandEcho);
 // Text entry is isolated from world commands and bounded.
 const auto before=spy.intents.size();const auto transcript_before_prompt=ui.transcript_size();ui.begin_text(UiRequestId::Custom,"Name?",4);check(ui.transcript_size()==transcript_before_prompt);ui.handle_input(ch('t'));ui.handle_input(ch('e'));ui.handle_input(ch('s'));ui.handle_input(ch('t'));ui.handle_input(ch('x'));check(spy.intents.size()==before&&ui.input_length()==4);ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().kind==UiIntentKind::ModalResponse&&spy.intents.back().value.accepted);
 check(ui.prompt()[0]==0&&ui.input_length()==0);
 // Yes/no ignores unrelated input and cancel unless explicitly mapped to No.
 ui.begin_yes_no(UiRequestId::Custom,"Sure?",false);ui.handle_input(ch('x'));check(ui.mode()==UiMode::YesNo);ui.handle_input(action(UiActionKind::Cancel));check(ui.mode()==UiMode::YesNo);ui.handle_input(ch('n'));check(!spy.intents.back().value.yes);
 // Numeric editing, deletion and clamping.
 ui.begin_number(UiRequestId::Custom,"Gold?",2,99,2);ui.handle_input(ch('1'));ui.handle_input(ch('x'));ui.handle_input(ch('2'));ui.handle_input(action(UiActionKind::DeleteCharacter));ui.handle_input(ch('9'));ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().value.number==19);
 // Selection navigation, disabled rows and cancellation hierarchy.
 ui.begin_selection(UiMode::PartySelection,UiRequestId::Party,"Who?",{nullptr,item_count,item});ui.handle_input(action(UiActionKind::Next));ui.handle_input(action(UiActionKind::Confirm));check(ui.mode()==UiMode::PartySelection);ui.handle_input(action(UiActionKind::Cancel));
 // Re-open and select explicitly.
 ui.begin_selection(UiMode::PartySelection,UiRequestId::Party,"Who?",{nullptr,item_count,item});UiAction pick;pick.kind=UiActionKind::SelectIndex;pick.index=2;ui.handle_input(pick);check(spy.intents.back().value.index==2);
 check(ui.mode()==UiMode::Exploration&&ui.prompt()[0]==0);
 ui.begin_selection(UiMode::InventorySelection,UiRequestId::Inventory,"Item?",{nullptr,item_count,item});ui.handle_input(action(UiActionKind::Cancel));check(!spy.intents.back().value.accepted&&ui.mode()==UiMode::Exploration);
 check(ui.prompt()[0]==0&&ui.input_length()==0);
 // Dialogue event ordering and prompt routing.
 DialogueOutput line;line.kind=DialogueOutputKind::Line;line.text=u"Greetings";DialogueEvent de{DialogueEventKind::Output,&line};GameEvent ge;ge.kind=GameEventKind::Dialogue;ge.dialogue=&de;ui.consume(ge);
 DialogueOutput prompt;prompt.kind=DialogueOutputKind::Prompt;prompt.question=false;de.output=&prompt;ui.consume(ge);check(ui.mode()==UiMode::TextEntry&&ui.base_mode()==UiMode::Dialogue);ui.handle_input(ch('j'));ui.handle_input(ch('o'));ui.handle_input(ch('b'));ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().command.kind==CommandKind::DialogueText);
 // Shop flow is visible, isolated, and emits typed ShopInput.
 ShopRecord shop_record{2,ShopType::Blacksmith,0,"The Hammer and Anvil","Gwenneth"};
 ShopSession ss;ss.phase=ShopPhase::Menu;ss.type=ShopType::Blacksmith;ss.record=&shop_record;
 ShopEvent se{ShopEventKind::Entered,&ss};ge={};ge.kind=GameEventKind::Shop;ge.shop=&se;ui.consume(ge);
 check(ui.mode()==UiMode::Shop&&std::strstr(ui.prompt(),"B Buy")!=nullptr);
 check(ui.transcript_at(ui.transcript_size()-1)&&std::strstr(ui.transcript_at(ui.transcript_size()-1)->text,"Hammer and Anvil")!=nullptr);
 ui.handle_input(ch('b'));check(spy.intents.back().kind==UiIntentKind::Shop&&spy.intents.back().shop.action==ShopAction::Buy);
 ui.handle_input(ch('s'));check(spy.intents.back().kind==UiIntentKind::Shop&&spy.intents.back().shop.action==ShopAction::Sell);
 ss.phase=ShopPhase::Greeting;se.kind=ShopEventKind::State;ui.consume(ge);ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().shop.action==ShopAction::Confirm);
 ss.phase=ShopPhase::BuyDeal;ui.consume(ge);ui.handle_input(action(UiActionKind::Back));check(spy.intents.back().shop.action==ShopAction::Decline);
 ss.phase=ShopPhase::RumorText;ui.consume(ge);check(ui.mode()==UiMode::TextEntry);ui.handle_input(ch('n'));ui.handle_input(ch('e'));ui.handle_input(ch('w'));ui.handle_input(ch('s'));ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().kind==UiIntentKind::Shop&&spy.intents.back().shop.action==ShopAction::Text&&spy.intents.back().shop.length==4);
 ss.phase=ShopPhase::RationsQuantity;ui.consume(ge);check(ui.mode()==UiMode::NumericEntry);ui.handle_input(ch('2'));ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().kind==UiIntentKind::Shop&&spy.intents.back().shop.action==ShopAction::Text&&spy.intents.back().shop.length==1);
 ss.phase=ShopPhase::HealerMember;se.kind=ShopEventKind::State;ui.consume(ge);check(ui.mode()==UiMode::Shop&&ui.accepts_direction_input());ui.set_shop_offer_count(3);const auto before_shop_move=ui.transcript_size();ui.handle_input(dir(Direction::South));check(ui.shop_cursor()==1&&ui.transcript_size()==before_shop_move);ui.handle_input(dir(Direction::South));ui.handle_input(dir(Direction::South));check(ui.shop_cursor()==2);ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().shop.action==ShopAction::SelectMember&&spy.intents.back().shop.value==2);UiAction member_pick;member_pick.kind=UiActionKind::SelectIndex;member_pick.index=1;ui.handle_input(member_pick);check(spy.intents.back().shop.action==ShopAction::SelectMember&&spy.intents.back().shop.value==1);
 ui.handle_input(action(UiActionKind::Back));check(spy.intents.back().kind==UiIntentKind::Shop&&spy.intents.back().shop.action==ShopAction::Cancel);
 // A transactional result remains visible after the selector closes.
 ShopResult broke{false,"Thou profaneth my shoppe with thy empty purse!",ShopFailure::Gold};
 se.kind=ShopEventKind::Result;se.result=&broke;ui.consume(ge);ss.phase=ShopPhase::Closed;se.kind=ShopEventKind::Exited;se.result=nullptr;ui.consume(ge);
 check(ui.mode()==UiMode::Exploration&&ui.transcript_at(ui.transcript_size()-1)&&std::strstr(ui.transcript_at(ui.transcript_size()-1)->text,"empty purse")!=nullptr);
 // Combat target/confirm flow and mode restoration.
 ge={};ge.kind=GameEventKind::CombatStarted;ui.consume(ge);check(ui.mode()==UiMode::Combat);ui.handle_input(dir(Direction::West));check(spy.intents.back().command.kind==CommandKind::CombatMove);ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().command.kind==CommandKind::CombatPass);ui.set_combat_aim(5,4,1,5,4);ui.handle_input(ch('o'));check(ui.mode()==UiMode::TargetSelection&&std::strcmp(ui.prompt(),"Direction?")==0&&ui.target_x()==-1&&ui.target_y()==-1&&!ui.target_has_cell());ui.handle_input(dir(Direction::East));int16_t open_marker_x=-1,open_marker_y=-1;check(ui.mode()==UiMode::Combat&&spy.intents.back().command.kind==CommandKind::CombatOpen&&spy.intents.back().command.has_direction&&spy.intents.back().command.direction==Direction::East&&spy.intents.back().command.combat_x==6&&spy.intents.back().command.combat_y==4&&ui.take_target_render_marker(open_marker_x,open_marker_y)&&open_marker_x==6&&open_marker_y==4);ui.set_combat_aim(4,5,1,4,5);ui.handle_input(ch('g'));check(ui.mode()==UiMode::TargetSelection&&std::strcmp(ui.prompt(),"Direction?")==0);ui.handle_input(dir(Direction::West));check(spy.intents.back().command.kind==CommandKind::CombatGet&&spy.intents.back().command.has_direction&&spy.intents.back().command.direction==Direction::West);ui.set_combat_aim(4,5,1,4,5);ui.handle_input(ch('s'));check(ui.mode()==UiMode::TargetSelection&&std::strcmp(ui.prompt(),"Direction?")==0);ui.handle_input(dir(Direction::North));check(spy.intents.back().command.kind==CommandKind::CombatSearch&&spy.intents.back().command.has_direction&&spy.intents.back().command.direction==Direction::North);ui.set_combat_aim(4,5,1,4,5);ui.handle_input(ch('a'));check(ui.mode()==UiMode::TargetSelection&&ui.target_x()==4&&ui.target_y()==5&&std::strcmp(ui.prompt(),"Aim")==0);check(std::strstr(ui.transcript_at(ui.transcript_size()-1)->text,"Aim")==nullptr);const auto before_self_confirm=spy.intents.size();ui.handle_input(action(UiActionKind::Confirm));check(ui.mode()==UiMode::TargetSelection&&spy.intents.size()==before_self_confirm);ui.handle_input(dir(Direction::East));ui.handle_input(dir(Direction::East));check(ui.target_x()==5&&ui.target_y()==5);ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().command.kind==CommandKind::CombatAttack&&spy.intents.back().command.has_target&&spy.intents.back().command.combat_x==5&&spy.intents.back().command.combat_y==5&&ui.prompt()[0]==0);
 const auto before_attack_cancel=spy.intents.size();ui.handle_input(ch('a'));ui.handle_input(action(UiActionKind::Cancel));check(ui.mode()==UiMode::Combat&&spy.intents.size()==before_attack_cancel);
 ui.set_combat_aim(4,5,3,6,5);ui.handle_input(ch('a'));check(ui.target_x()==6&&ui.target_y()==5);ui.handle_input(dir(Direction::East));check(ui.target_x()==7);ui.handle_input(dir(Direction::East));check(ui.target_x()==7);ui.handle_input(action(UiActionKind::Cancel));
 Command cast;cast.kind=CommandKind::Cast;cast.item=14;ui.begin_target(UiRequestId::Target,"Spell aim",cast,4,5);ui.handle_input(dir(Direction::North));ui.handle_input(action(UiActionKind::Confirm));check(spy.intents.back().command.kind==CommandKind::Cast&&spy.intents.back().command.has_target&&spy.intents.back().command.combat_y==4);
 // Ordered event transcript, bounded history, long blocks and paging.
 ui.append(UiTextChannel::Message,"one");ui.append(UiTextChannel::Combat,"two");ui.append(UiTextChannel::Quest,"three");const auto n=ui.transcript_size();check(n<=12);for(size_t i=1;i<n;++i)check(ui.transcript_at(i-1)->sequence<ui.transcript_at(i)->sequence);
 const auto before_sfx=ui.transcript_size();ge={};ge.kind=GameEventKind::Sfx;ge.text="internal-sfx-id";ui.consume(ge);check(ui.transcript_size()==before_sfx);
 ui.append(UiTextChannel::Dialogue,"abcdefghijklmnopqrstuvwxyz");check(ui.wrapped_line_count(8)>=4);UiRenderedLine lines[3];check(ui.visible_lines(lines,3,8)<=3);ui.handle_input(action(UiActionKind::PageUp));check(ui.scroll_offset_lines()>0);ui.handle_input(action(UiActionKind::PageDown));check(ui.scroll_offset_lines()==0);
 // A line split between storage blocks must wrap as one continuous text run.
 UiTextBlock wrap_blocks[4];UiSession wrapped{{wrap_blocks,4},{},{38,8,12}};char long_line[192];std::memset(long_line,'x',191);long_line[191]=0;wrapped.append(UiTextChannel::Dialogue,long_line);check(wrapped.wrapped_line_count()==6);UiRenderedLine wrap_lines[8];check(wrapped.visible_lines(wrap_lines,8)==6);
 // Word-aware wrapping keeps ordinary tokens and punctuation together,
 // preserves explicit newlines, trims wrapped indentation, and hard-breaks
 // only a token that is itself wider than the line.
 auto expect_lines=[](const char *text,size_t columns,std::initializer_list<const char*> expected){
     UiTextBlock storage[8]{};UiSession session{{storage,8},{},{uint16_t(columns),16,12}};
     session.append(UiTextChannel::Dialogue,text);UiRenderedLine actual[16]{};
     const auto count=session.visible_lines(actual,16,columns);if(count!=expected.size()){std::cerr<<"wrap count for '"<<text<<"': "<<count<<" expected "<<expected.size()<<"\n";for(size_t n=0;n<count;++n)std::cerr<<"  ["<<actual[n].text<<"]\n";}check(count==expected.size());
     size_t i=0;for(const auto *value:expected){if(std::string(actual[i].text)!=value)std::cerr<<"wrap line "<<i<<" for '"<<text<<"': ["<<actual[i].text<<"] expected ["<<value<<"]\n";check(std::string(actual[i].text)==value);++i;}
 };
 expect_lines("alpha beta gamma",10,{"alpha beta","gamma"});
 expect_lines("Hello, world!",8,{"Hello,","world!"});
 expect_lines("one\ntwo",8,{"one","two"});
 expect_lines("12345678 next",8,{"12345678","next"});
 expect_lines("abcdefghij",4,{"abcd","efgh","ij"});
 expect_lines("one   two",9,{"one   two"});
 expect_lines("one   two",7,{"one","two"});
 expect_lines("Long dialogue keeps every ordinary word intact, even when the paragraph crosses a storage-block boundary and continues for several readable lines.",21,
              {"Long dialogue keeps","every ordinary word","intact, even when the","paragraph crosses a","storage-block","boundary and","continues for several","readable lines."});
 // Repeated blocked movement attempts from one actor are a single semantic
 // transcript entry; a different actor or intervening event starts a new one.
 UiTextBlock combat_blocks[8]{};UiSession combat_text{{combat_blocks,8},{},{20,8,12}};
 CombatEvent blocked_event{};blocked_event.kind=CombatEventKind::Message;blocked_event.text="Blocked!";blocked_event.actor=17;
 GameEvent combat_event{};combat_event.kind=GameEventKind::Combat;combat_event.combat=&blocked_event;
 combat_text.consume(combat_event);combat_text.consume(combat_event);combat_text.consume(combat_event);
 check(combat_text.transcript_size()==1&&std::strcmp(combat_text.transcript_at(0)->text,"Blocked! x3")==0);
 check(combat_text.blocked_events_generated()==3&&combat_text.blocked_events_presented()==1);
 blocked_event.actor=18;combat_text.consume(combat_event);
 check(combat_text.transcript_size()==2&&combat_text.blocked_events_generated()==4&&combat_text.blocked_events_presented()==2);
 // Prompt-derived command path and mode switch back out of combat.
 ge={};ge.kind=GameEventKind::TownExitPrompt;ui.consume(ge);check(ui.mode()==UiMode::YesNo);ui.handle_input(ch('y'));check(spy.intents.back().command.kind==CommandKind::Exit);
 // The mode captured at CombatStarted was Exploration, so the return is known
 // exactly.  Asserting only "not Combat" cannot observe a stale return register.
 ge={};ge.kind=GameEventKind::CombatEnded;ui.consume(ge);check(ui.mode()==UiMode::Exploration&&ui.base_mode()==UiMode::Exploration);

 // One-letter handheld shortcuts use the same semantic exploration command
 // paths both normally and immediately after the real CombatEnded event.
 UiTextBlock shortcut_blocks[32]{};Spy shortcut_spy;
 UiSession shortcuts{{shortcut_blocks,32},{&shortcut_spy,Spy::send},{20,8,12}};
 auto expect_direction_shortcut=[&](char key,CommandKind kind){
     const auto before_count=shortcut_spy.intents.size();
     check(shortcuts.handle_input(ch(key)));check(shortcuts.mode()==UiMode::TargetSelection);
     check(shortcut_spy.intents.size()==before_count);
     shortcuts.handle_input(dir(Direction::East));
     check(shortcut_spy.intents.back().kind==UiIntentKind::Command);
     check(shortcut_spy.intents.back().command.kind==kind);
     check(shortcut_spy.intents.back().command.has_direction);
 };
 auto expect_exploration_shortcuts=[&](){
     expect_direction_shortcut('l',CommandKind::Look);
     expect_direction_shortcut('o',CommandKind::Open);
     expect_direction_shortcut('g',CommandKind::Get);
     expect_direction_shortcut('t',CommandKind::Talk);
     expect_direction_shortcut('a',CommandKind::Attack);
     const auto before_count=shortcut_spy.intents.size();
     check(shortcuts.handle_input(ch('u')));
     check(shortcut_spy.intents.size()==before_count+1);
     check(shortcut_spy.intents.back().kind==UiIntentKind::OpenInventorySelection);
     const auto before_ready=shortcut_spy.intents.size(),ready_log=shortcuts.transcript_size();
     check(shortcuts.handle_input(ch('r')));
     check(shortcut_spy.intents.size()==before_ready+1&&shortcut_spy.intents.back().kind==UiIntentKind::OpenEquipmentSelection);
     check(shortcuts.transcript_size()==ready_log+1&&std::strcmp(shortcuts.transcript_at(shortcuts.transcript_size()-1)->text,"Ready")==0);
 };
 expect_exploration_shortcuts();
 GameEvent shortcut_event{};shortcut_event.kind=GameEventKind::CombatStarted;
 shortcuts.consume(shortcut_event);check(shortcuts.mode()==UiMode::Combat);
 shortcuts.set_combat_aim(4,5,1,4,5);shortcuts.handle_input(ch('a'));
 check(shortcuts.mode()==UiMode::TargetSelection);
 shortcut_event.kind=GameEventKind::CombatEnded;shortcuts.consume(shortcut_event);
 check(shortcuts.mode()==UiMode::Exploration&&shortcuts.base_mode()==UiMode::Exploration);
 expect_exploration_shortcuts();

 // Dungeon navigation is first-person semantic input: Up/Down move relative
 // to facing and Left/Right turn.  Shared menus must return to the dungeon,
 // never to exploration or a retained surface coordinate.
 UiTextBlock dungeon_blocks[24]{};Spy dungeon_spy;
 UiSession dungeon_ui{{dungeon_blocks,24},{&dungeon_spy,Spy::send},{20,8,12}};
 dungeon_ui.set_base_mode(UiMode::Dungeon);
 dungeon_ui.handle_input(dir(Direction::North));
 check(dungeon_spy.intents.back().command.kind==CommandKind::DungeonCommand&&
       dungeon_spy.intents.back().command.item==int16_t(DungeonAction::Forward));
 dungeon_ui.handle_input(dir(Direction::South));
 check(dungeon_spy.intents.back().command.item==int16_t(DungeonAction::Back));
 dungeon_ui.handle_input(dir(Direction::West));
 check(dungeon_spy.intents.back().command.item==int16_t(DungeonAction::Left));
 dungeon_ui.handle_input(dir(Direction::East));
 check(dungeon_spy.intents.back().command.item==int16_t(DungeonAction::Right));
 dungeon_ui.handle_input(ch('v'));
 check(dungeon_spy.intents.back().command.kind==CommandKind::ViewGem);
 dungeon_ui.handle_input(ch('z'));
 check(dungeon_spy.intents.back().kind==UiIntentKind::OpenStatusSelection);
 dungeon_ui.handle_input(ch('m'));
 check(dungeon_spy.intents.back().kind==UiIntentKind::OpenSpellSelection&&
       dungeon_spy.intents.back().request==UiRequestId::Spell);
 dungeon_ui.begin_selection(UiMode::InventorySelection,UiRequestId::Inventory,"Use?",{nullptr,item_count,item});
 dungeon_ui.handle_input(action(UiActionKind::Cancel));
 check(dungeon_ui.mode()==UiMode::Dungeon&&dungeon_ui.base_mode()==UiMode::Dungeon);

 // Text-bearing contexts still receive shortcut letters literally.
 UiTextBlock literal_blocks[8]{};Spy literal_spy;
 UiSession literal{{literal_blocks,8},{&literal_spy,Spy::send},{20,8,12}};
 literal.begin_text(UiRequestId::Custom,"Letters?",8);
 const auto literal_intents=literal_spy.intents.size();
 for(char key:std::string("loguta"))check(literal.handle_input(ch(key)));
 check(literal.mode()==UiMode::TextEntry&&literal.input_length()==6);
 check(literal_spy.intents.size()==literal_intents);
 for(size_t i=0;i<6;++i)check(literal.input_buffer()[i]==char16_t(std::string("loguta")[i]));
 std::cout<<checks<<" UI session checks passed; sizeof(UiSession)="<<sizeof(UiSession)<<" block="<<sizeof(UiTextBlock)<<"\n";
}
