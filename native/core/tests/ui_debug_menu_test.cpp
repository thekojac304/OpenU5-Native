#include "openu5/ui_debug_menu.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace openu5;
namespace {int checks=0;void check(bool v){++checks;if(!v){std::cerr<<"debug menu check "<<checks<<" failed\n";std::exit(1);}}UiAction act(UiActionKind k){UiAction a;a.kind=k;return a;}UiAction pick(int i){UiAction a;a.kind=UiActionKind::SelectIndex;a.index=i;return a;}}
int main(){
 GameState g;TurnState t;TravelState tr;CommandState cs;static uint8_t large[65536]{};WorldData w{large,large,sizeof(large),sizeof(large)};CommandContext c{g,t,tr,cs,w};g.party.character_count=3;g.party.party_size=1;for(int i=0;i<3;++i){std::strcpy(g.party.characters[i].name,i?"Companion":"Avatar");g.party.characters[i].party_status=i?255:0;g.party.characters[i].character_class=i?'F':'A';}
 UiDebugMenu menu(c);menu.open();check(menu.view().count==size_t(UiDebugCategory::Count)&&menu.view().category==-1);
 // Enter Stats, edit Strength to 1 and apply through the developer API.
 menu.handle_input(pick(int(UiDebugCategory::Stats)));check(menu.view().category==int(UiDebugCategory::Stats));menu.handle_input(pick(1));check(menu.view().editing);menu.handle_input(act(UiActionKind::Next));menu.handle_input(act(UiActionKind::Confirm));check(g.party.characters[0].strength==1);
 menu.handle_input(act(UiActionKind::Back));check(menu.view().category==-1);
 // Default Britannia destination exercises teleport application through the picker API.
 menu.handle_input(pick(int(UiDebugCategory::Teleport)));
 menu.handle_input(pick(4));check(menu.view().editing&&menu.view().value==1);menu.handle_input(act(UiActionKind::Cancel));
 menu.handle_input(pick(5));check(menu.view().has_result&&menu.view().last_status==DebugStatus::Applied&&menu.view().last_teleport_status==DebugTeleportStatus::Applied&&menu.view().teleport_request.standard_entry);menu.handle_input(act(UiActionKind::Back));
 // Exact physical Back hierarchy: destination editor -> Teleport -> Developer
 // -> prior gameplay mode, with no teleport applied while backing out.
 menu.open();menu.handle_input(pick(int(UiDebugCategory::Teleport)));menu.handle_input(pick(0));check(menu.view().editing&&menu.view().category==int(UiDebugCategory::Teleport));
 const auto before_cancel=g.position;menu.handle_input(act(UiActionKind::Cancel));check(!menu.view().editing&&menu.view().category==int(UiDebugCategory::Teleport));
 menu.handle_input(act(UiActionKind::Cancel));check(menu.view().category==-1&&menu.is_open());
 menu.handle_input(act(UiActionKind::Cancel));check(!menu.is_open()&&g.position.xy.x==before_cancel.xy.x&&g.position.xy.y==before_cancel.xy.y&&g.position.map.location==before_cancel.map.location);
 menu.open();
 // Shortcut hierarchy reaches the real party, resource, gear, and thin full-setup APIs.
 menu.handle_input(pick(int(UiDebugCategory::ShortcutsPresets)));menu.handle_input(pick(3));
 check(menu.view().confirming&&g.gold==0);menu.handle_input(act(UiActionKind::Confirm));
 check(g.party.party_size==1&&g.party.characters[0].level==8&&g.gold==9999&&
       g.party.characters[0].weapon!=255);
 menu.handle_input(act(UiActionKind::Back));menu.handle_input(act(UiActionKind::Back));check(!menu.is_open());
 // Session owns switching only; debug actions stay build gated.
 UiTextBlock blocks[2];UiSession ui{{blocks,2}};ui.attach_debug_menu(&menu);check(ui.open_debug_menu()&&ui.mode()==UiMode::DebugMenu);ui.handle_input(act(UiActionKind::Back));check(ui.mode()==UiMode::Exploration);
 std::cout<<checks<<" debug menu checks passed; sizeof(UiDebugMenu)="<<sizeof(UiDebugMenu)<<"\n";
}
