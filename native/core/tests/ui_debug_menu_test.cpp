#include "openu5/ui_debug_menu.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
using namespace openu5;
namespace {int checks=0;void check(bool v,const char*msg=nullptr){++checks;if(!v){std::cerr<<"debug menu check "<<checks<<" failed";if(msg)std::cerr<<": "<<msg;std::cerr<<"\n";std::exit(1);}}UiAction act(UiActionKind k){UiAction a;a.kind=k;return a;}UiAction pick(int i){UiAction a;a.kind=UiActionKind::SelectIndex;a.index=i;return a;}UiAction chr(char16_t c){UiAction a;a.kind=UiActionKind::Character;a.character=c;return a;}
// Sets the field the cursor currently sits on by typing digits, then confirms.
// Faster and less error-prone than repeated Next/Previous taps for wide ranges
// like destination indices or x/y coordinates.
void type_and_confirm(UiDebugMenu &m, const char *digits) {
    for (const char *p = digits; *p; ++p) m.handle_input(chr(char16_t(*p)));
    m.handle_input(act(UiActionKind::Confirm));
}}
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

 // --- Batch 4.5A-1 RED: Developer Default Entrance floor selection (T1, T2, T5) ---
 // Blackthorn (location 18) and Serpent's Hold (location 32) both authored a
 // basement below their standard z=0 entrance. Selecting a destination resets
 // the menu's floor cursor to ordinal index 0 (see apply_value, cursor_==0
 // case in ui_debug_menu.cpp), and applying immediately resolves that ordinal
 // via debug_floor_at -- the *lowest sorted* floor, i.e. the basement, not
 // signed z=0. This fixture reproduces that exact authored shape.
 {
  GameState g2; TurnState t2; TravelState tr2; CommandState cs2;
  static uint8_t large2[65536]{};
  std::vector<uint8_t> bt_b1(1024, 5), bt_z0(1024, 5), bt_z1(1024, 5), bt_z2(1024, 5), bt_z3(1024, 5);
  bt_b1[30 * kSmallMapSize + 15] = 255; // z=-1: void at the standard entrance cell
  bt_z0[30 * kSmallMapSize + 15] = 68;  // z=0: authored walkable standard entrance
  std::vector<uint8_t> sh_b1(1024, 5), sh_z0(1024, 5), sh_z1(1024, 5);
  sh_b1[30 * kSmallMapSize + 15] = 255; // z=-1: void at the standard entrance cell
  sh_z0[30 * kSmallMapSize + 15] = 68;  // z=0: authored walkable standard entrance
  MapData maps2[] = {
      {{18, -1}, bt_b1.data(), bt_b1.size()}, {{18, 0}, bt_z0.data(), bt_z0.size()},
      {{18, 1}, bt_z1.data(), bt_z1.size()},  {{18, 2}, bt_z2.data(), bt_z2.size()},
      {{18, 3}, bt_z3.data(), bt_z3.size()},  {{32, -1}, sh_b1.data(), sh_b1.size()},
      {{32, 0}, sh_z0.data(), sh_z0.size()},  {{32, 1}, sh_z1.data(), sh_z1.size()},
  };
  WorldData w2{large2, large2, sizeof(large2), sizeof(large2), maps2, sizeof(maps2) / sizeof(maps2[0])};
  CommandContext c2{g2, t2, tr2, cs2, w2};
  g2.party.character_count = 1; g2.party.party_size = 1; g2.party.active_character = 255;
  UiDebugMenu menu2(c2); menu2.open();

  // Destination order: Britannia(0), Underworld(1), then small-map locations
  // sorted ascending -- Blackthorn(18) at index 2, Serpent's Hold(32) at index 3.
  menu2.handle_input(pick(int(UiDebugCategory::Teleport)));

  // T1: Blackthorn Default Entrance.
  menu2.handle_input(pick(0)); type_and_confirm(menu2, "2");
  menu2.handle_input(pick(5));
  check(menu2.view().has_result && menu2.view().last_teleport_status == DebugTeleportStatus::Applied,
        "T1: default entrance teleport reports Applied");
  check(g2.position.map.location == 18 && g2.position.map.floor == 0 &&
            g2.position.xy.x == 15 && g2.position.xy.y == 30,
        "T1 RED: Blackthorn Default Entrance must resolve signed z=0, not floor ordinal 0 "
        "(currently resolves the basement, z=-1)");

  // T2: Serpent's Hold Default Entrance -- identical shape, different location.
  menu2.handle_input(pick(0)); type_and_confirm(menu2, "3");
  menu2.handle_input(pick(5));
  check(menu2.view().has_result && menu2.view().last_teleport_status == DebugTeleportStatus::Applied,
        "T2: default entrance teleport reports Applied");
  check(g2.position.map.location == 32 && g2.position.map.floor == 0 &&
            g2.position.xy.x == 15 && g2.position.xy.y == 30,
        "T2 RED: Serpent's Hold Default Entrance must resolve signed z=0, not floor ordinal 0 "
        "(currently resolves the basement, z=-1)");

  // T5: passability must eventually reach UiDebugMenuView. Today the view
  // exposes only last_teleport_status (DebugTeleportStatus), which is Applied
  // regardless of destination-cell passability -- there is no passable/
  // passability_known field on UiDebugMenuView at all, so a walkable explicit
  // teleport and an impassable explicit teleport are indistinguishable at
  // this layer. Drive both through the same Blackthorn map (explicit
  // coordinates, standard_entry=false, so this does not depend on T1's fix):
  // floor ordinal 1 is signed z=0 (walkable 68 at 15,30); floor ordinal 0 is
  // signed z=-1 (void 255 at 15,30).
  menu2.handle_input(pick(0)); type_and_confirm(menu2, "2");  // destination = Blackthorn
  menu2.handle_input(pick(2)); type_and_confirm(menu2, "15"); // x = 15
  menu2.handle_input(pick(3)); type_and_confirm(menu2, "30"); // y = 30
  menu2.handle_input(pick(4)); type_and_confirm(menu2, "0");  // standard_entry = false
  menu2.handle_input(pick(1)); type_and_confirm(menu2, "1");  // floor ordinal 1 -> z=0 (walkable)
  menu2.handle_input(pick(5));
  const auto walkable_status = menu2.view().last_teleport_status;
  const auto walkable_view = menu2.view();
  menu2.handle_input(pick(1)); type_and_confirm(menu2, "0");  // floor ordinal 0 -> z=-1 (void)
  menu2.handle_input(pick(5));
  const auto impassable_status = menu2.view().last_teleport_status;
  const auto impassable_view = menu2.view();
  check(walkable_status == DebugTeleportStatus::Applied && impassable_status == DebugTeleportStatus::Applied,
        "T5 setup: both explicit teleports apply today (picker parity: warn, never block)");
  // T5 GREEN: DebugTeleportStatus alone cannot distinguish these (both
  // Applied, by design -- explicit manual coordinates are never refused).
  // UiDebugMenuView now carries the authoritative passability metadata
  // alongside the status so device presentation can tell them apart.
  check(walkable_view.teleport_passability_known && walkable_view.teleport_passable,
        "T5: walkable explicit teleport reports Applied(walkable) via teleport_passable");
  check(impassable_view.teleport_passability_known && !impassable_view.teleport_passable,
        "T5: impassable explicit teleport reports Applied(impassable) via teleport_passable");
 }

 std::cout<<checks<<" debug menu checks passed; sizeof(UiDebugMenu)="<<sizeof(UiDebugMenu)<<"\n";
}
