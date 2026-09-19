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

 // --- Batch 4.5A-2: row_label()/row_value() view-model (U1-U5) ---
 {
  GameState g3; TurnState t3; TravelState tr3; CommandState cs3;
  static uint8_t large3[65536]{};
  WorldData w3{large3, large3, sizeof(large3), sizeof(large3)};
  CommandContext c3{g3, t3, tr3, cs3, w3};
  g3.party.character_count = 2; g3.party.party_size = 2;
  std::strcpy(g3.party.characters[0].name, "Avatar");
  g3.party.characters[1].name[0] = 0; // U4 fallback: blank slot 1
  UiDebugMenu menu3(c3); menu3.open();

  // U1: Quest Item row is human-readable, not raw ordinal 0 -- now an explicit
  // named row (Batch 4.5A-3 Part 6) rather than a raw selector/value pair: the
  // canonical id lives in the row LABEL, and the value is the Yes/No
  // possession state itself.
  menu3.handle_input(pick(int(UiDebugCategory::QuestItems)));
  {
   check(std::strcmp(menu3.row_label(0), "Shard of Falsehood [29]") == 0, "U1: Quest Item row label");
   const auto rv = menu3.row_value(0);
   check(rv.kind == DebugRowValue::Kind::Boolean, "U1: Quest Item row is Boolean possession state");
  }
  menu3.handle_input(act(UiActionKind::Back));

  // U2: table-driven walk of every reachable row in every current category.
  // Action rows (no state) may report Kind::None; every other row must not.
  struct CategoryRows { UiDebugCategory category; size_t count; const int *action_rows; size_t action_count; };
  static constexpr int kTeleportActions[] = {5};
  static constexpr int kPartyActions[] = {2, 3, 4};
  static constexpr int kEquipmentActions[] = {3, 4, 5, 6};
  static constexpr int kQuestWorldActions[] = {6};
  static constexpr int kNpcActions[] = {7};
  static constexpr int kShortcutActions[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14};
  static constexpr int kDiagnosticActions[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  const CategoryRows tables[] = {
      {UiDebugCategory::Teleport, 6, kTeleportActions, 1},
      {UiDebugCategory::Party, 5, kPartyActions, 3},
      {UiDebugCategory::Stats, 9, nullptr, 0},
      {UiDebugCategory::Inventory, 14, nullptr, 0},
      {UiDebugCategory::Equipment, 7, kEquipmentActions, 4},
      {UiDebugCategory::Reagents, 2, nullptr, 0},
      // Quest Items/Special Items rows all report real Boolean/Unsupported
      // state (never Kind::None), even though Confirm fires immediately --
      // see item_edit_range()/apply_action() in ui_debug_menu.cpp.
      {UiDebugCategory::QuestItems, 6, nullptr, 0},
      {UiDebugCategory::SpecialItems, 7, nullptr, 0},
      {UiDebugCategory::QuestWorld, 7, kQuestWorldActions, 1},
      {UiDebugCategory::Time, 6, nullptr, 0},
      {UiDebugCategory::Transport, 6, nullptr, 0},
      {UiDebugCategory::NpcDungeonState, 8, kNpcActions, 1},
      {UiDebugCategory::ShortcutsPresets, 15, kShortcutActions, 15},
      {UiDebugCategory::Diagnostics, 16, kDiagnosticActions, 16},
  };
  for (const auto &table : tables) {
   menu3.handle_input(pick(int(table.category)));
   check(menu3.view().count == table.count, "U2: category row count matches");
   for (size_t row = 0; row < table.count; ++row) {
    bool is_action = false;
    for (size_t a = 0; a < table.action_count; ++a) if (size_t(table.action_rows[a]) == row) is_action = true;
    const auto rv = menu3.row_value(row);
    if (is_action) check(rv.kind == DebugRowValue::Kind::None, "U2: action row is Kind::None");
    else check(rv.kind != DebugRowValue::Kind::None, "U2: state row must not be Kind::None");
   }
   menu3.handle_input(act(UiActionKind::Back));
  }

  // U3: non-selected rows still expose values (guards the cursor-only bug).
  // Uses Next (not SelectIndex/Confirm) so the cursor moves without opening
  // the edit dialog on the destination row.
  menu3.handle_input(pick(int(UiDebugCategory::Stats)));
  const auto strength_before = menu3.row_value(1);
  menu3.handle_input(act(UiActionKind::Next));
  menu3.handle_input(act(UiActionKind::Next));
  menu3.handle_input(act(UiActionKind::Next));
  check(menu3.view().cursor == 3 && !menu3.view().editing, "U3: cursor moved away from row 1, not editing");
  const auto strength_after = menu3.row_value(1);
  check(strength_before.kind == strength_after.kind && strength_before.value == strength_after.value,
        "U3: non-selected row value unchanged after moving cursor");
  menu3.handle_input(act(UiActionKind::Back));

  // U4: character row uses party name, falling back safely when blank.
  menu3.handle_input(pick(int(UiDebugCategory::Party)));
  {
   const auto rv = menu3.row_value(0);
   check(rv.kind == DebugRowValue::Kind::Text, "U4: Character row is Text");
   check(rv.text && std::strcmp(rv.text, "Avatar") == 0, "U4: Character row uses party name");
  }
  menu3.handle_input(pick(0)); type_and_confirm(menu3, "1"); // select blank slot 1
  {
   const auto rv = menu3.row_value(0);
   check(rv.text && std::strcmp(rv.text, "Character 1") == 0, "U4: blank character falls back safely");
  }
  menu3.handle_input(act(UiActionKind::Back));

  // U5: Transport Mode row is human-readable, not a raw integer.
  menu3.handle_input(pick(int(UiDebugCategory::Transport)));
  {
   const auto rv = menu3.row_value(0);
   check(rv.kind == DebugRowValue::Kind::Text, "U5: Transport Mode row is Text");
   check(rv.text && std::strcmp(rv.text, "Foot") == 0, "U5: Transport Mode default is Foot");
  }
  menu3.handle_input(pick(0)); type_and_confirm(menu3, "1"); // Horse
  {
   const auto rv = menu3.row_value(0);
   check(rv.text && std::strcmp(rv.text, "Horse") == 0, "U5: Transport Mode reflects Horse after edit");
  }
  menu3.handle_input(act(UiActionKind::Back));
 }

 // --- Batch 4.5A-3: Special Items + Developer capability wiring (S1-S9) ---
 {
  GameState g4; TurnState t4; TravelState tr4; CommandState cs4;
  static uint8_t large4[65536]{};
  WorldData w4{large4, large4, sizeof(large4), sizeof(large4)};
  CommandContext c4{g4, t4, tr4, cs4, w4};
  UiDebugMenu menu4(c4); menu4.open();

  // S1: Special Items category exists with 7 rows in the exact spec order.
  menu4.handle_input(pick(int(UiDebugCategory::SpecialItems)));
  check(menu4.view().category == int(UiDebugCategory::SpecialItems) && menu4.view().count == 7,
        "S1: Special Items category has 7 rows");
  static constexpr const char *kSpecialLabels[] = {
      "Grapple", "Spyglass [32]", "HMS Cape Plans [33]", "Sextant [34]",
      "Pocket Watch [35]", "Black Badge [36]", "Wooden Box [37]"};
  for (size_t i = 0; i < 7; ++i)
   check(std::strcmp(menu4.row_label(i), kSpecialLabels[i]) == 0, "S1: Special Items row label");

  // S2: Black Badge toggle -- possession only, TurnState::time_spell untouched
  // (Part 4's semantic boundary). Cursor moves via Next (no side effect);
  // only Confirm mutates.
  for (int i = 0; i < 5; ++i) menu4.handle_input(act(UiActionKind::Next));
  check(menu4.view().cursor == 5, "S2 setup: cursor on Black Badge row");
  check(!g4.black_badge, "S2 setup: Black Badge starts unowned");
  menu4.handle_input(act(UiActionKind::Confirm));
  check(g4.black_badge && t4.time_spell == 0,
        "S2: Confirm sets game.black_badge, leaves turn.time_spell untouched");
  menu4.handle_input(act(UiActionKind::Confirm));
  check(!g4.black_badge && t4.time_spell == 0,
        "S2: second Confirm toggles possession back off, time_spell still untouched");

  // S3: Pocket Watch reports Unsupported and never mutates GameState.
  for (int i = 0; i < 5; ++i) menu4.handle_input(act(UiActionKind::Previous)); // back to row 0
  menu4.handle_input(act(UiActionKind::Next)); menu4.handle_input(act(UiActionKind::Next));
  menu4.handle_input(act(UiActionKind::Next)); menu4.handle_input(act(UiActionKind::Next));
  check(menu4.view().cursor == 4, "S3 setup: cursor on Pocket Watch row");
  check(menu4.row_value(4).kind == DebugRowValue::Kind::Unsupported, "S3: Pocket Watch row is Unsupported kind");
  GameState before_pw = g4;
  menu4.handle_input(act(UiActionKind::Confirm));
  check(menu4.view().has_result && menu4.view().last_status == DebugStatus::Unsupported,
        "S3: Confirm on Pocket Watch reports Result: Unsupported");
  check(std::memcmp(&before_pw, &g4, sizeof(GameState)) == 0,
        "S3: GameState is byte-identical after an unsupported request");
  menu4.handle_input(act(UiActionKind::Back));

  // S5: Quest Items are six explicit named rows -- no raw "Quest item: 0"
  // selector remains.
  menu4.handle_input(pick(int(UiDebugCategory::QuestItems)));
  check(menu4.view().count == 6, "S5: Quest Items category has 6 rows");
  static constexpr const char *kQuestItemLabels[] = {
      "Shard of Falsehood [29]", "Shard of Hatred [30]", "Shard of Cowardice [31]",
      "Amulet of Lord British [18]", "Crown of Lord British [19]", "Sceptre of Lord British [20]"};
  for (size_t i = 0; i < 6; ++i)
   check(std::strcmp(menu4.row_label(i), kQuestItemLabels[i]) == 0, "S5: Quest Item row label");

  // S6: Quest item toggles -- one shard, one Lord British artifact.
  check(!g4.quest.shards[0], "S6 setup: ShardFalsehood starts OFF");
  menu4.handle_input(pick(0));
  check(g4.quest.shards[0], "S6: ShardFalsehood toggled ON via Quest Items");
  check(!g4.quest.artifacts[0], "S6 setup: Amulet starts OFF");
  menu4.handle_input(pick(3));
  check(g4.quest.artifacts[0], "S6: Amulet toggled ON via Quest Items");
  menu4.handle_input(act(UiActionKind::Back));

  // S7: ShadowlordSummoned is reachable on the Quest / World page and reaches
  // the existing debug_set_quest_number() setter.
  menu4.handle_input(pick(int(UiDebugCategory::QuestWorld)));
  check(menu4.view().count == 7, "S7: Quest / World has 7 rows");
  check(std::strcmp(menu4.row_label(5), "Shadowlord Summoned") == 0, "S7: Shadowlord Summoned row label");
  menu4.handle_input(pick(5)); type_and_confirm(menu4, "3");
  check(g4.quest.summoned == 3, "S7: Shadowlord Summoned reaches debug_set_quest_number");
  menu4.handle_input(act(UiActionKind::Back));

  // S8: Karma and Torch Turns are reachable on the Inventory page.
  menu4.handle_input(pick(int(UiDebugCategory::Inventory)));
  check(menu4.view().count == 14, "S8: Inventory has 14 rows (Karma/Torch Turns added)");
  check(std::strcmp(menu4.row_label(7), "Karma") == 0 && std::strcmp(menu4.row_label(8), "Torch Turns") == 0,
        "S8: Karma/Torch Turns row labels");
  menu4.handle_input(pick(7)); type_and_confirm(menu4, "42");
  check(g4.karma == 42, "S8: Karma reaches debug_set_resource");
  menu4.handle_input(pick(8)); type_and_confirm(menu4, "77");
  check(g4.torch_turns == 77, "S8: Torch Turns reaches debug_set_resource");
  menu4.handle_input(act(UiActionKind::Back));

  // S9: dungeon slot's UI range excludes the always-invalid slot 7 (seven
  // dungeons, slots 0..6). The setter's own dungeon>=7 rejection remains a
  // defensive guard underneath.
  menu4.handle_input(pick(int(UiDebugCategory::NpcDungeonState)));
  menu4.handle_input(pick(4));
  check(menu4.view().editing && menu4.view().maximum == 6, "S9: dungeon slot max excludes invalid slot 7");
  menu4.handle_input(act(UiActionKind::Cancel));
  menu4.handle_input(act(UiActionKind::Back));
 }

 std::cout<<checks<<" debug menu checks passed; sizeof(UiDebugMenu)="<<sizeof(UiDebugMenu)<<"\n";
}
