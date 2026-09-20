#include "openu5/blackthorn.h"
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

 // Batch 4.5B hardware correction (presentation seam, RED 1): a Blackthorn
 // interrogation question long enough to have overflowed the 96-byte
 // kUiPromptBytes prompt line before the fix must land in the transcript,
 // leaving only the short response cue in the modal prompt.
 {
     UiTextBlock bt_blocks[16]{};Spy bt_spy;
     UiSession bt{{bt_blocks,16},{&bt_spy,Spy::send},{38,8,12}};
     const char *question="\"What sayest thou to this, that thy honesty in the face "
         "of the Mystic Shrine of Compassion shall now be tested before Lord "
         "Blackthorn himself?\"";
     check(std::strlen(question)>kUiPromptBytes-1);
     GameEvent bt_event{};bt_event.kind=GameEventKind::BlackthornPrompt;bt_event.text=question;
     bt.consume(bt_event);
     check(bt.mode()==UiMode::TextEntry&&bt.request()==UiRequestId::Blackthorn&&
           bt.base_mode()==UiMode::ShrineSpecial);
     check(std::strcmp(bt.prompt(),"Your response?")==0);
     check(bt.transcript_at(bt.transcript_size()-1)&&
           std::strstr(bt.transcript_at(bt.transcript_size()-1)->text,"Mystic Shrine of Compassion")!=nullptr);
 }
 // Batch 4.5C (generic transcript scroll/page UX, T1-T7): overflowed
 // dialogue/quest/jail text must stay reachable by paging, and paging must
 // never disturb an active modal or a live conversation underneath it.
 {
     // T1: default view follows newest; PageUp exposes older content and
     // dispatches no gameplay command.
     UiTextBlock t1_blocks[32]{};Spy t1_spy;
     UiSession t1{{t1_blocks,32},{&t1_spy,Spy::send},{10,4,12}};
     for(int i=0;i<20;++i){char line[16];std::snprintf(line,sizeof(line),"line%d",i);t1.append(UiTextChannel::Message,line);}
     check(t1.wrapped_line_count()==20);
     UiRenderedLine t1_out[4]{};
     check(t1.visible_lines(t1_out,4)==4&&std::string(t1_out[0].text)=="line16"&&std::string(t1_out[3].text)=="line19");
     const auto t1_intents_before=t1_spy.intents.size();
     check(t1.handle_input(action(UiActionKind::PageUp)));
     check(t1_spy.intents.size()==t1_intents_before);
     check(t1.scroll_offset_lines()==4);
     check(t1.visible_lines(t1_out,4)==4&&std::string(t1_out[0].text)=="line12"&&std::string(t1_out[3].text)=="line15");

     // T2: paging further up moves further back; paging down moves toward
     // newest again, and reaching the bottom restores follow-newest (offset 0).
     check(t1.handle_input(action(UiActionKind::PageUp)));
     check(t1.scroll_offset_lines()==8);
     check(t1.handle_input(action(UiActionKind::PageDown)));
     check(t1.scroll_offset_lines()==4);
     check(t1.visible_lines(t1_out,4)==4&&std::string(t1_out[0].text)=="line12");
     check(t1.handle_input(action(UiActionKind::PageDown)));
     check(t1.scroll_offset_lines()==0);

     // T3: while scrolled away from the newest text, a newly appended message
     // must not yank the view back to the bottom -- the same lines stay on
     // screen until the player pages down to the new content themselves.
     check(t1.handle_input(action(UiActionKind::PageUp)));
     check(t1.scroll_offset_lines()==4);
     check(t1.visible_lines(t1_out,4)==4&&std::string(t1_out[0].text)=="line12"&&std::string(t1_out[3].text)=="line15");
     t1.append(UiTextChannel::Message,"line20");
     check(t1.transcript_at(t1.transcript_size()-1)&&std::strcmp(t1.transcript_at(t1.transcript_size()-1)->text,"line20")==0);
     check(t1.visible_lines(t1_out,4)==4&&std::string(t1_out[0].text)=="line12"&&std::string(t1_out[3].text)=="line15");
     check(t1.handle_input(action(UiActionKind::PageDown)));
     check(t1.handle_input(action(UiActionKind::PageDown)));
     check(t1.scroll_offset_lines()==0);
     check(t1.visible_lines(t1_out,4)==4&&std::string(t1_out[3].text)=="line20");

     // T4: while already following the newest text, a new message is shown
     // immediately with no extra input required.
     t1.append(UiTextChannel::Message,"line21");
     check(t1.scroll_offset_lines()==0);
     check(t1.visible_lines(t1_out,4)==4&&std::string(t1_out[3].text)=="line21");
 }
 {
     // T5: an active TextEntry modal survives transcript navigation -- typed
     // text, the modal itself and the prompt are untouched, and paging
     // dispatches nothing.
     UiTextBlock t5_blocks[16]{};Spy t5_spy;
     UiSession t5{{t5_blocks,16},{&t5_spy,Spy::send},{10,3,12}};
     for(int i=0;i<10;++i){char line[16];std::snprintf(line,sizeof(line),"log%d",i);t5.append(UiTextChannel::Message,line);}
     t5.begin_text(UiRequestId::Custom,"Name?",6);
     t5.handle_input(ch('a'));t5.handle_input(ch('b'));t5.handle_input(ch('c'));
     check(t5.mode()==UiMode::TextEntry&&t5.input_length()==3);
     const auto t5_intents_before=t5_spy.intents.size();
     check(t5.handle_input(action(UiActionKind::PageUp)));
     check(t5.handle_input(action(UiActionKind::PageDown)));
     check(t5.mode()==UiMode::TextEntry&&t5.input_length()==3);
     check(t5.input_buffer()[0]=='a'&&t5.input_buffer()[1]=='b'&&t5.input_buffer()[2]=='c');
     check(t5_spy.intents.size()==t5_intents_before);
     check(std::strcmp(t5.prompt(),"Name?")==0);
 }
 {
     // T6: a real dialogue conversation long enough to overflow the visible
     // transcript stays entirely reachable by paging, and text that arrives
     // while the player is reviewing older lines does not disturb the view.
     UiTextBlock t6_blocks[32]{};
     UiSession t6{{t6_blocks,32},{},{12,4,12}};
     DialogueOutput t6_line;t6_line.kind=DialogueOutputKind::Line;
     DialogueEvent t6_de{DialogueEventKind::Output,&t6_line};
     GameEvent t6_ge{};t6_ge.kind=GameEventKind::Dialogue;t6_ge.dialogue=&t6_de;
     const char16_t *t6_texts[]={u"Gorn says greetings traveler",u"The jailer paces the corridor",
         u"Blackthorn will see thee shortly",u"The chains rattle in the dark",
         u"A guard mutters under his breath",u"Thou shouldst not have come here"};
     for(auto *text:t6_texts){t6_line.text=text;t6.consume(t6_ge);}
     check(t6.wrapped_line_count()>4);
     check(t6.transcript_at(0)&&std::strstr(t6.transcript_at(0)->text,"Gorn")!=nullptr);
     UiRenderedLine t6_out[4]{};
     check(t6.visible_lines(t6_out,4)==4);
     bool gorn_visible_default=false;for(auto&l:t6_out)if(std::strstr(l.text,"Gorn"))gorn_visible_default=true;
     check(!gorn_visible_default);
     bool gorn_reachable=false;
     for(int guard=0;guard<20&&!gorn_reachable;++guard){
         t6.handle_input(action(UiActionKind::PageUp));
         t6.visible_lines(t6_out,4);
         for(auto&l:t6_out)if(std::strstr(l.text,"Gorn"))gorn_reachable=true;
     }
     check(gorn_reachable);
     UiRenderedLine t6_before_new[4]{};std::memcpy(t6_before_new,t6_out,sizeof(t6_out));
     const auto t6_scroll_before_new=t6.scroll_offset_lines();
     t6_line.text=u"The talk is not yet finished";t6.consume(t6_ge);
     t6.visible_lines(t6_out,4);
     for(size_t i=0;i<4;++i)check(std::string(t6_out[i].text)==std::string(t6_before_new[i].text));
     check(t6.scroll_offset_lines()>=t6_scroll_before_new);
     for(int i=0;i<20;++i)t6.handle_input(action(UiActionKind::PageDown));
     check(t6.scroll_offset_lines()==0);
     t6.visible_lines(t6_out,4);
     bool new_line_reachable=false;for(auto&l:t6_out)if(std::strstr(l.text,"finished"))new_line_reachable=true;
     check(new_line_reachable);
 }
 {
     // T7: a long BlackthornPrompt narrative, driven through the real event
     // path used by the Batch 4.5B presentation fix, stays reviewable via
     // transcript paging while "Your response?" remains the live modal and
     // the player's partial answer is never disturbed.
     UiTextBlock t7_blocks[24]{};Spy t7_spy;
     UiSession t7{{t7_blocks,24},{&t7_spy,Spy::send},{20,4,12}};
     for(int i=0;i<8;++i){char line[16];std::snprintf(line,sizeof(line),"jail%d",i);t7.append(UiTextChannel::Quest,line);}
     const char *narrative="\"Thou standest accused before Lord Blackthorn himself, and "
         "shalt answer for thy trespass against the Shrine of Compassion "
         "ere the pendulum finds thee wanting.\"";
     GameEvent t7_event{};t7_event.kind=GameEventKind::BlackthornPrompt;t7_event.text=narrative;
     t7.consume(t7_event);
     check(t7.mode()==UiMode::TextEntry&&t7.request()==UiRequestId::Blackthorn);
     check(std::strcmp(t7.prompt(),"Your response?")==0);
     bool narrative_in_transcript=false;
     for(size_t i=0;i<t7.transcript_size();++i)
         if(std::strstr(t7.transcript_at(i)->text,"pendulum"))narrative_in_transcript=true;
     check(narrative_in_transcript);
     t7.handle_input(ch('y'));t7.handle_input(ch('e'));t7.handle_input(ch('s'));
     check(t7.input_length()==3);
     const auto t7_intents_before=t7_spy.intents.size();
     // The narrative was appended immediately before the modal, so it starts
     // out as the newest text; only the earliest words ("standest accused")
     // may already have scrolled above the default view. Page up exactly
     // until they surface, the same generic search T6 uses, rather than
     // assuming a fixed page count.
     UiRenderedLine t7_out[4]{};
     bool narrative_visible=false;
     for(int guard=0;guard<20&&!narrative_visible;++guard){
         t7.visible_lines(t7_out,4);
         for(size_t i=0;i<4;++i)
             if(std::strstr(t7_out[i].text,"standest")||std::strstr(t7_out[i].text,"accused")||std::strstr(t7_out[i].text,"Blackthorn"))
                 narrative_visible=true;
         if(!narrative_visible)t7.handle_input(action(UiActionKind::PageUp));
     }
     check(narrative_visible);
     check(t7.mode()==UiMode::TextEntry&&t7.input_length()==3&&std::strcmp(t7.prompt(),"Your response?")==0);
     check(t7.input_buffer()[0]=='y'&&t7.input_buffer()[1]=='e'&&t7.input_buffer()[2]=='s');
     check(t7_spy.intents.size()==t7_intents_before);
     for(int i=0;i<5;++i)t7.handle_input(action(UiActionKind::PageDown));
     check(t7.scroll_offset_lines()==0);
     t7.handle_input(action(UiActionKind::Confirm));
     check(t7_spy.intents.back().kind==UiIntentKind::Command&&
           t7_spy.intents.back().command.kind==CommandKind::BlackthornAction&&
           t7_spy.intents.back().command.item==int16_t(BlackthornAction::Answer));
 }
 std::cout<<checks<<" UI session checks passed; sizeof(UiSession)="<<sizeof(UiSession)<<" block="<<sizeof(UiTextBlock)<<"\n";
}
