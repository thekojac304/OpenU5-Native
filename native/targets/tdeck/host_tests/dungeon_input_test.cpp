// Batch 9B -- the dungeon RUNTIME regression suite (R-05 part 3).
//
// Batch 9 repaired what the dungeon DRAWS.  Physical T-Deck testing afterwards
// reported that the dungeon's CONTROLS do not work, and that the location strip
// named the wrong place.  Neither could be caught by the existing suite:
//
//   * dungeon_view_regression proves plan_dungeon_view(); it never presses a key.
//   * dungeon_parity / dungeon_flow_parity call dungeon_action() and
//     execute_dungeon_command() DIRECTLY.  Nothing anywhere asserted that
//     UiSession::handle_dungeon() -- the only thing a physical key ever reaches
//     -- can actually REACH each of those actions.
//   * the location caption is composed inside Board::show_alpha(), which pulls
//     in esp_lcd and cannot be host-built, from a value (GameState::position)
//     that a dungeon session deliberately does not update.
//
// So this file drives the REAL device path end to end:
//
//   RawInputEvent (physical matrix / trackball code)
//     -> tdeck::UiInputAdapter::translate()        main/ui_input_adapter.cpp
//     -> UiSession::handle_input()                 core/src/ui_session.cpp
//     -> UiSession::handle_dungeon() / handle_modal()
//     -> UiIntent -> dispatch_world_command()      core/src/commands.cpp
//     -> execute_dungeon_command() -> dungeon_action()
//     -> DungeonState mutation
//     -> tdeck::resolve_synchronized_base_mode()   main/ui_mode_policy.h
//
// AlphaRuntime itself still cannot be host-compiled (esp_log / esp_timer /
// esp_heap_caps / FreeRTOS / Board), so its per-input tail is exercised through
// the same ui_mode_policy.h seam AlphaRuntime calls, exactly as ui_mode_test.cpp
// does.  Everything else here is production code.
//
// The caption half is proved against tdeck::hud_location_caption(), the
// host-portable helper Board::show_alpha() now calls.

#include "../main/location_names.h"
#include "../main/ui_input_adapter.h"
#include "../main/ui_mode_policy.h"

#include "openu5/commands.h"
#include "openu5/dungeon.h"
#include "openu5/dungeon_view.h"
#include "openu5/hud.h"
#include "openu5/ui_session.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;

namespace {

int g_failures = 0;
int g_checks = 0;

void expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-6s %-14s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
}

const char *mode_name(UiMode m) {
    switch (m) {
    case UiMode::Exploration: return "Exploration";
    case UiMode::Dungeon: return "Dungeon";
    case UiMode::Combat: return "Combat";
    case UiMode::Dialogue: return "Dialogue";
    case UiMode::Shop: return "Shop";
    case UiMode::ShrineSpecial: return "ShrineSpecial";
    case UiMode::TextEntry: return "TextEntry";
    case UiMode::NumericEntry: return "NumericEntry";
    case UiMode::YesNo: return "YesNo";
    case UiMode::PartySelection: return "PartySelection";
    case UiMode::InventorySelection: return "InventorySelection";
    case UiMode::EquipmentSelection: return "EquipmentSelection";
    case UiMode::SpellSelection: return "SpellSelection";
    case UiMode::TargetSelection: return "TargetSelection";
    case UiMode::DebugMenu: return "DebugMenu";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Device fixture.  One dungeon, one party member, the real orchestration.
// ---------------------------------------------------------------------------
struct DungeonDevice {
    GameState g;
    TurnState turn;
    TravelState travel;
    CommandState commands;
    WorldData world;
    CommandContext c{g, turn, travel, commands, world};

    DungeonState dungeon;
    DungeonScratch scratch;
    DungeonContext dctx{dungeon, scratch};
    DungeonData data[2]{};

    UiTextBlock blocks[64]{};
    UiSession ui{{blocks, 64}, {this, &DungeonDevice::thunk}, {64, 8, 12}};
    tdeck::UiInputAdapter input;

    std::vector<UiIntent> intents;
    std::vector<std::string> messages;
    int dungeon_actions = 0;
    int last_dungeon_action = -1;
    int64_t clock_us = 1000000;

    DungeonDevice() {
        // Deceit (33) and Despise (34).  An open floor with one up-ladder at
        // (1,1) and an up/down ladder at (3,3) so Klimb has a real choice.
        for (int n = 0; n < 2; ++n) {
            data[n].location = uint8_t(33 + n);
            std::memset(data[n].cells, 0, sizeof(data[n].cells));
            data[n].cells[1 * 8 + 1] = 0x10;              // LadderUp, floor 0
            data[n].cells[3 * 8 + 3] = 0x30;              // LadderUpDown, floor 0
            data[n].cells[2 * 8 + 2] = 0x50;              // Fountain (cure), floor 0
            data[n].cells[64 + 3 * 8 + 3] = 0x30;         // floor 1 landing
        }
        dctx.data = data;
        dctx.count = 2;
        c.dungeon_context = &dctx;
        c.events = ui.event_sink();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        ch.intelligence = 15;
        ch.dexterity = 15;
        ch.current_hp = ch.max_hp = 100;
        ch.status = 'G';
        ch.character_class = 'A';
        g.torch_turns = 50;
        g.torches = 3;
    }

    static void thunk(void *p, const UiIntent &i) {
        static_cast<DungeonDevice *>(p)->dispatch(i);
    }

    // AlphaRuntime::dispatch(), restricted to the intents the dungeon raises.
    void dispatch(const UiIntent &i) {
        intents.push_back(i);
        if (i.kind != UiIntentKind::Command) return;
        if (i.command.kind == CommandKind::DungeonCommand) {
            ++dungeon_actions;
            last_dungeon_action = i.command.item;
        }
        dispatch_world_command(c, i.command);
        c.dungeon = dungeon.active;
    }

    // AlphaRuntime::handle()'s gameplay tail.
    void route(const UiAction &a) {
        ui.refresh_dungeon_context(g, dungeon, c.dungeon);
        ui.handle_input(a);
        c.dungeon = dungeon.active;
        ui.set_base_mode(
            tdeck::resolve_synchronized_base_mode(ui.base_mode(), false, dungeon.active));
    }

    // The whole physical path: a matrix key becomes a UiAction, or is dropped.
    bool press(uint8_t code) {
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.code = code;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        return deliver(raw);
    }
    bool enter_key() { return press('\r'); }
    bool ball(tdeck::RawInputKind kind) {
        tdeck::RawInputEvent raw{};
        raw.kind = kind;
        raw.timestamp_us = (clock_us += 100000);
        return deliver(raw);
    }
    bool deliver(const tdeck::RawInputEvent &raw) {
        UiAction action{};
        tdeck::DeviceShortcut shortcut{};
        if (!input.translate(raw, ui.mode(), action, shortcut, ui.accepts_direction_input()))
            return false;
        route(action);
        return true;
    }

    void enter_dungeon(uint8_t location) {
        Command e;
        e.kind = CommandKind::EnterDungeon;
        e.member = int16_t(location);
        dispatch_world_command(c, e);
        c.dungeon = dungeon.active;
        ui.set_base_mode(
            tdeck::resolve_synchronized_base_mode(ui.base_mode(), false, dungeon.active));
    }

    void go_dark() {
        g.torch_turns = 0;
        turn.light_spell_minutes = 0;
    }
    void put_at(int x, int y, DungeonFacing f) {
        dungeon.pos.x = uint8_t(x);
        dungeon.pos.y = uint8_t(y);
        dungeon.pos.facing = f;
    }
    // Did the LAST routed input reach the dungeon command dispatcher at all?
    int actions_after(int before) const { return dungeon_actions - before; }
};

// The reference's own dungeon dispatcher (game/src/main.ts, the DUNGEON branch
// around handleDungeonKey/dispatchDungeonCommand).  Every row here is a command
// the original accepts inside a corridor.
struct KeyExpectation {
    uint8_t key;
    const char *what;
};

// ---------------------------------------------------------------------------
// D-IN-1  Movement and turning reach the dispatcher and mutate the session.
// ---------------------------------------------------------------------------
void d_in_1_movement_and_turning() {
    std::printf("D-IN-1 -- trackball movement and turning reach dungeon_action()\n");
    auto d = std::make_unique<DungeonDevice>();
    d->enter_dungeon(33);
    expect(d->dungeon.active && d->ui.mode() == UiMode::Dungeon, "D-IN-1a",
           "entering Deceit leaves the session in UiMode::Dungeon");

    d->put_at(4, 4, DungeonFacing::North);
    const auto facing0 = d->dungeon.pos.facing;
    expect(d->ball(tdeck::RawInputKind::TrackballLeft), "D-IN-1b", "turn-left input translates");
    expect(d->dungeon.pos.facing != facing0, "D-IN-1c", "turn left changes facing");

    const auto facing1 = d->dungeon.pos.facing;
    expect(d->ball(tdeck::RawInputKind::TrackballRight), "D-IN-1d", "turn-right input translates");
    expect(d->dungeon.pos.facing != facing1, "D-IN-1e", "turn right changes facing");

    d->put_at(4, 4, DungeonFacing::North);
    const auto y0 = d->dungeon.pos.y;
    expect(d->ball(tdeck::RawInputKind::TrackballUp), "D-IN-1f", "advance input translates");
    expect(d->dungeon.pos.y != y0, "D-IN-1g", "advance moves the party");

    const auto y1 = d->dungeon.pos.y;
    expect(d->ball(tdeck::RawInputKind::TrackballDown), "D-IN-1h", "back input translates");
    expect(d->dungeon.pos.y != y1, "D-IN-1i", "back moves the party");
}

// ---------------------------------------------------------------------------
// D-IN-2  Every command the reference's dungeon dispatcher accepts must be
//         reachable from a physical key.  This is the hardware report.
// ---------------------------------------------------------------------------
void d_in_2_reference_command_coverage() {
    std::printf("D-IN-2 -- the reference dungeon command set is reachable\n");

    // Commands that dispatch a DungeonCommand or a world Command outright.
    struct Row { uint8_t key; const char *id; const char *what; };
    static const Row rows[] = {
        {'a', "D-IN-2a", "(A)ttack dispatches"},
        {'g', "D-IN-2b", "(G)et dispatches"},
        {'j', "D-IN-2c", "(J)immy dispatches"},
        {'o', "D-IN-2d", "(O)pen dispatches"},
        {'i', "D-IN-2e", "(I)gnite dispatches (dungeon torch -- commands.cpp:752)"},
        {'d', "D-IN-2f", "(D)rink dispatches (DungeonAction::Drink)"},
        {' ', "D-IN-2g", "Space passes"},
        {'1', "D-IN-2h", "a digit sets the active player"},
    };
    for (const auto &row : rows) {
        auto d = std::make_unique<DungeonDevice>();
        d->enter_dungeon(33);
        d->put_at(4, 4, DungeonFacing::North);
        const auto before = d->intents.size();
        d->press(row.key);
        expect(d->intents.size() > before, row.id, row.what);
    }

    // (H)ole up opens the same hours prompt the overworld uses.
    {
        auto d = std::make_unique<DungeonDevice>();
        d->enter_dungeon(33);
        d->press('h');
        expect(d->ui.mode() == UiMode::NumericEntry, "D-IN-2i",
               "(H)ole up opens the camp hours prompt in a dungeon");
        d->press('3');
        d->enter_key();
        expect(d->ui.mode() == UiMode::Dungeon, "D-IN-2j",
               "the camp prompt returns to UiMode::Dungeon, not Exploration");
    }

    // Enter / '.' are TURN AROUND in the reference, not Pass.
    {
        auto d = std::make_unique<DungeonDevice>();
        d->enter_dungeon(33);
        d->put_at(4, 4, DungeonFacing::North);
        d->enter_key();
        expect(d->dungeon.pos.facing == DungeonFacing::South, "D-IN-2k",
               "Enter is Turn Around (DungeonAction::TurnAround), not Pass");
        d->put_at(4, 4, DungeonFacing::East);
        d->press('.');
        expect(d->dungeon.pos.facing == DungeonFacing::West, "D-IN-2l",
               "'.' is Turn Around too");
    }

    // 'w' has the reference's own distinct rejection text (DUNGEON 0x3450).
    {
        auto d = std::make_unique<DungeonDevice>();
        d->enter_dungeon(33);
        const auto before = d->ui.transcript_size();
        d->press('w');
        bool found = false;
        for (size_t i = before; i < d->ui.transcript_size(); ++i) {
            const auto *b = d->ui.transcript_at(i);
            if (b && std::strstr(b->text, "W-What?")) found = true;
        }
        expect(found, "D-IN-2m", "'w' in a dungeon answers \"W-What?\"");
    }
}

// ---------------------------------------------------------------------------
// D-IN-3  Klimb can descend.  dungeon_klimb_choice() exists precisely for the
//         up+down cell and had no caller anywhere in the tree.
// ---------------------------------------------------------------------------
void d_in_3_klimb_traversal() {
    std::printf("D-IN-3 -- Klimb reaches BOTH directions\n");
    auto d = std::make_unique<DungeonDevice>();
    d->enter_dungeon(33);
    d->put_at(3, 3, DungeonFacing::North);   // 0x30 = ladder up AND down
    expect(dungeon_klimb_choice(d->g, d->dungeon), "D-IN-3a",
           "fixture: the cell really offers both directions");

    d->press('k');
    expect(d->ui.mode() == UiMode::TargetSelection, "D-IN-3b",
           "Klimb on an up+down cell opens the reference's U/D prompt");
    const auto floor0 = d->dungeon.pos.floor;
    d->ball(tdeck::RawInputKind::TrackballDown);
    expect(d->dungeon.pos.floor == floor0 + 1, "D-IN-3c",
           "the DOWN answer descends (previously unreachable: Klimb always went up)");
    expect(d->ui.mode() == UiMode::Dungeon, "D-IN-3d", "the prompt returns to Dungeon");

    // A single-way cell still resolves directly, with no prompt.  (Klimbing UP
    // from floor 0 leaves the dungeon, so the post-condition here is "no prompt
    // was opened", not "still in Dungeon".)
    auto e = std::make_unique<DungeonDevice>();
    e->enter_dungeon(33);
    e->put_at(1, 1, DungeonFacing::North);   // 0x10 = ladder up only
    const auto acted = e->dungeon_actions;
    e->press('k');
    expect(e->ui.mode() != UiMode::TargetSelection, "D-IN-3e",
           "a one-way ladder still resolves without a prompt");
    expect(e->dungeon_actions == acted + 1, "D-IN-3f",
           "and dispatches its Klimb on the keypress itself");
}

// ---------------------------------------------------------------------------
// D-IN-4  (S)earch reaches all four of the reference's Dir- targets.
// ---------------------------------------------------------------------------
void d_in_4_search_direction() {
    std::printf("D-IN-4 -- (S)earch offers the reference's Dir- choice\n");
    auto d = std::make_unique<DungeonDevice>();
    d->enter_dungeon(33);
    d->put_at(4, 4, DungeonFacing::North);
    d->press('s');
    expect(d->ui.mode() == UiMode::TargetSelection, "D-IN-4a",
           "(S)earch opens the Dir- prompt (SJOG 0x0672)");
    const auto before = d->dungeon_actions;
    d->ball(tdeck::RawInputKind::TrackballDown);   // "Here"
    expect(d->dungeon_actions > before, "D-IN-4b", "the Dir- answer dispatches a Search");
    expect(d->ui.mode() == UiMode::Dungeon, "D-IN-4c", "the Dir- prompt returns to Dungeon");
}

// ---------------------------------------------------------------------------
// D-IN-5  Darkness suppresses SIGHT, never input dispatch.
// ---------------------------------------------------------------------------
void d_in_5_dark_input_still_dispatches() {
    std::printf("D-IN-5 -- darkness does not suppress input dispatch\n");
    auto d = std::make_unique<DungeonDevice>();
    d->enter_dungeon(33);
    d->put_at(4, 4, DungeonFacing::North);

    expect(plan_dungeon_view(d->g, d->turn, d->dungeon).lit, "D-IN-5a",
           "fixture: the corridor is lit with a torch burning");
    d->go_dark();
    expect(!plan_dungeon_view(d->g, d->turn, d->dungeon).lit, "D-IN-5b",
           "with no light the plan is unlit (Batch 9 gate, unchanged)");

    const auto facing0 = d->dungeon.pos.facing;
    const auto before = d->dungeon_actions;
    expect(d->ball(tdeck::RawInputKind::TrackballLeft), "D-IN-5c",
           "a turn input still translates in the dark");
    expect(d->actions_after(before) == 1, "D-IN-5d",
           "a turn input still reaches dungeon_action() in the dark");
    expect(d->dungeon.pos.facing != facing0, "D-IN-5e", "facing still changes in the dark");

    const auto y0 = d->dungeon.pos.y;
    d->put_at(4, 4, DungeonFacing::North);
    d->ball(tdeck::RawInputKind::TrackballUp);
    expect(d->dungeon.pos.y != y0 || d->dungeon.pos.y != 4, "D-IN-5f",
           "movement still applies in the dark");

    // (I)gnite is the way OUT of the dark, and it must work FROM the dark.
    d->g.torches = 3;
    const auto lit_before = d->g.torch_turns;
    d->press('i');
    expect(d->g.torch_turns > lit_before, "D-IN-5g",
           "(I)gnite lights a torch from inside an unlit dungeon");
}

// ---------------------------------------------------------------------------
// D-IN-6  No modal survives a dungeon entry, an exit, or a re-entry.  This is
//         the "controls are dead everywhere afterwards" shape.
// ---------------------------------------------------------------------------
bool consumes_a_plain_key(DungeonDevice &d) {
    const auto before = d.intents.size();
    d.press('k');
    return d.intents.size() > before;
}

void d_in_6_no_modal_leak() {
    std::printf("D-IN-6 -- entry/exit/re-entry leave no modal owning the keyboard\n");
    auto d = std::make_unique<DungeonDevice>();
    d->enter_dungeon(33);
    expect(d->ui.mode() == UiMode::Dungeon, "D-IN-6a", "entry lands in Dungeon");
    expect(consumes_a_plain_key(*d), "D-IN-6b", "a plain key dispatches after entry");

    // Leave through the core's own exit, then come back.
    exit_dungeon(d->c, false);
    d->c.dungeon = d->dungeon.active;
    d->ui.set_base_mode(tdeck::resolve_synchronized_base_mode(d->ui.base_mode(), false, false));
    expect(d->ui.mode() == UiMode::Exploration, "D-IN-6c", "exit returns to Exploration");

    d->enter_dungeon(33);
    expect(d->ui.mode() == UiMode::Dungeon, "D-IN-6d", "re-entry lands in Dungeon again");
    expect(consumes_a_plain_key(*d), "D-IN-6e", "a plain key dispatches after re-entry");

    // A dungeon entry that happens WHILE a world modal is open must not leave
    // that modal's stale return register pointing at Exploration -- that is the
    // shape in which every later keypress lands in the wrong router.
    auto e = std::make_unique<DungeonDevice>();
    e->ui.begin_yes_no(UiRequestId::Custom, "Stale?");
    expect(e->ui.mode() == UiMode::YesNo, "D-IN-6f", "fixture: a world modal is open");
    e->enter_dungeon(33);
    e->press('y');
    expect(e->ui.mode() == UiMode::Dungeon, "D-IN-6g",
           "closing a modal that spanned a dungeon entry returns to Dungeon, not Exploration");
    expect(consumes_a_plain_key(*e), "D-IN-6h",
           "and a plain key dispatches afterwards");
}

// ---------------------------------------------------------------------------
// D-LOC-1  The location caption names the DUNGEON, not the stale surface map.
// ---------------------------------------------------------------------------
void d_loc_1_identity() {
    std::printf("D-LOC-1 -- the location strip names the dungeon\n");

    struct Row { uint8_t id; const char *name; };
    static const Row dungeons[] = {
        {33, "Deceit"},   {34, "Despise"}, {35, "Destard"}, {36, "Wrong"},
        {37, "Covetous"}, {38, "Shame"},   {39, "Hythloth"}, {40, "Doom"},
    };
    for (const auto &row : dungeons) {
        const char *got = tdeck::hud_location_caption(0, 0, true, row.id);
        expect(got && std::strcmp(got, row.name) == 0, "D-LOC-1a", row.name);
    }

    // The exact hardware report: standing in Deceit after having been in
    // Serpent's Hold (32).  The surface map id is stale by design -- a dungeon
    // session never rewrites GameState::position -- so the caption must not
    // read it at all while the session is live.
    const char *inside =
        tdeck::hud_location_caption(32, 0, true, 33);
    expect(inside && std::strcmp(inside, "Deceit") == 0, "D-LOC-1b",
           "inside Deceit with a stale Serpent's Hold surface id the strip says Deceit");

    // ... and the surface identity comes back on the way out.
    expect(std::strcmp(tdeck::hud_location_caption(32, 0, false, 33), "Serpent's Hold") == 0,
           "D-LOC-1c", "leaving the dungeon restores the surface name");
    expect(std::strcmp(tdeck::hud_location_caption(0, 0, false, 33), "Britannia") == 0,
           "D-LOC-1d", "the overworld is still Britannia");
    expect(std::strcmp(tdeck::hud_location_caption(0, -1, false, 33), "Underworld") == 0,
           "D-LOC-1e", "a negative floor is still the Underworld");
    expect(std::strcmp(tdeck::hud_location_caption(200, 0, false, 33), "Unknown place") == 0,
           "D-LOC-1f", "an unmapped surface id still falls back");
}

// ---------------------------------------------------------------------------
// D-LOC-2  The live session really does carry the dungeon id to the strip, and
//          a transition updates it on the frame that produced it.
// ---------------------------------------------------------------------------
void d_loc_2_transitions() {
    std::printf("D-LOC-2 -- surface -> dungeon -> surface identity\n");
    auto d = std::make_unique<DungeonDevice>();
    d->g.position.map.location = 32;    // Serpent's Hold, the reported stale id

    auto caption = [&] {
        const auto bands = hud_dungeon_bands(d->dungeon, d->c.dungeon);
        return tdeck::hud_location_caption(d->g.position.map.location, d->g.position.map.floor,
                                           bands.active, bands.dungeon_id);
    };
    expect(std::strcmp(caption(), "Serpent's Hold") == 0, "D-LOC-2a",
           "on the surface the strip is the surface map");

    d->enter_dungeon(33);
    expect(std::strcmp(caption(), "Deceit") == 0, "D-LOC-2b",
           "entering Deceit changes the strip to Deceit");
    expect(d->g.position.map.location == 32, "D-LOC-2c",
           "and the surface return context is deliberately untouched");

    exit_dungeon(d->c, false);
    d->c.dungeon = d->dungeon.active;
    expect(std::strcmp(caption(), "Britannia") == 0, "D-LOC-2d",
           "exiting to Britannia restores a surface identity");

    auto e = std::make_unique<DungeonDevice>();
    e->enter_dungeon(34);
    const auto bands = hud_dungeon_bands(e->dungeon, e->c.dungeon);
    expect(bands.active && bands.dungeon_id == 34, "D-LOC-2e",
           "a second dungeon carries its own id");
    expect(std::strcmp(tdeck::hud_location_caption(0, 0, bands.active, bands.dungeon_id),
                       "Despise") == 0,
           "D-LOC-2f", "and resolves to Despise");
}

} // namespace

int main() {
    std::printf("Batch 9B -- dungeon runtime regressions (R-05 part 3)\n\n");
    d_in_1_movement_and_turning();
    d_in_2_reference_command_coverage();
    d_in_3_klimb_traversal();
    d_in_4_search_direction();
    d_in_5_dark_input_still_dispatches();
    d_in_6_no_modal_leak();
    d_loc_1_identity();
    d_loc_2_transitions();
    std::printf("\n%d checks, %d failing\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
