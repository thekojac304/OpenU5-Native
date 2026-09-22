// Batch 9D -- the NON-OVERWORLD combat transition regression suite.
//
// Physical T-Deck testing after Batch 9C reported three defects that the whole
// existing suite misses:
//
//   1. combat started from inside a dungeon corridor (an (A)ttack on an
//      adjacent wanderer) opens the combat scene, but normal combat controls
//      do nothing;
//   2. a dungeon ROOM reached by a ladder does the same;
//   3. opening developer options and teleporting out of that state freezes.
//
// Overworld combat is unaffected, so the combat COMMANDS themselves are fine:
// what differs is the ENTRY/EXIT integration around them.  Nothing in the
// existing suite could see it, because:
//
//   * combat_parity / advanced_combat_parity drive combat_action() DIRECTLY --
//     no UiSession, no mode, no key;
//   * dungeon_input_regression drives keys but hardcodes combat=false in its
//     mode-resync tail, so a dungeon that starts a fight is out of its scope;
//   * ui_mode_regression proves CombatStarted/CombatEnded mode capture with a
//     synthetic event, never through a real dungeon encounter.
//
// So this file drives the REAL device path end to end, for three entry paths
// side by side:
//
//   RawInputEvent -> tdeck::UiInputAdapter::translate()
//     -> UiSession::handle_input() -> handle_dungeon()/handle_combat()
//     -> UiIntent -> dispatch_world_command()
//     -> execute_dungeon_command() -> dungeon_action() -> dungeon_encounter()
//     -> start_fixed_combat() -> CombatStarted -> UiSession::consume()
//     -> AlphaRuntime::command()'s mode tail + ui_mode_policy.h resync
//
// AlphaRuntime cannot be host-compiled (esp_log/esp_timer/FreeRTOS/Board), so
// its per-input and per-command tails are mirrored here arm by arm against
// alpha_runtime.cpp -- every mirrored member below names the production
// function it copies.  Everything else is production code.

#include "../main/ui_input_adapter.h"
#include "../main/ui_mode_policy.h"

#include "openu5/combat.h"
#include "openu5/commands.h"
#include "openu5/debug_map_picker.h"
#include "openu5/dungeon.h"
#include "openu5/dungeon_encounters.h"
#include "openu5/hud.h"
#include "openu5/ui_session.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;

namespace {

int g_failures = 0;
int g_checks = 0;

void expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-6s %-12s %s\n", ok ? "GREEN" : "RED", id, what);
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

// AlphaRuntime's own enemy-beat constant (alpha_runtime.cpp kEnemyBeatUs).
constexpr int64_t kEnemyBeatUs = 220000;



// ---------------------------------------------------------------------------
// Authored fixtures.  Both are the same files the parity suites read; CTest
// passes their paths.  Nothing here is synthesised -- a test that invented its
// own dungeon could not have told this batch apart from the last one.
// ---------------------------------------------------------------------------
struct AuthoredBoard {
    CombatMap map{};
    uint8_t sprites[16]{};
};
struct Authored {
    bool ok = false;
    const char *why = "";
    uint8_t cells[8][512]{};          // indexed by location - 33
    std::vector<AuthoredBoard> boards; // ARRAY POSITION order: 16 britannia, then 112 dungeon
    const uint8_t *dungeon(int loc) const { return cells[loc - 33]; }
    uint8_t cell(int loc, int f, int x, int y) const {
        return dungeon(loc)[f * 64 + y * 8 + x];
    }
};

Authored load_authored(const char *dungeon_fixture, const char *board_fixture) {
    Authored a;
    if (!dungeon_fixture || !board_fixture) {
        a.why = "no fixture paths given (CTest passes them; run via ctest)";
        return a;
    }
    {
        std::ifstream df(dungeon_fixture);
        if (!df) { a.why = "dungeon-maps.txt did not open"; return a; }
        int loc;
        int seen = 0;
        while (df >> loc) {
            if (loc < 33 || loc > 40) { a.why = "unexpected dungeon id"; return a; }
            for (int i = 0; i < 512; ++i) { int v; df >> v; a.cells[loc - 33][i] = uint8_t(v); }
            ++seen;
        }
        if (seen != 8) { a.why = "expected 8 authored dungeons"; return a; }
    }
    {
        std::ifstream mf(board_fixture);
        if (!mf) { a.why = "fixed-maps.txt did not open"; return a; }
        int index;
        while (mf >> index) {
            AuthoredBoard b;
            b.map.index = index;
            for (auto &t : b.map.tiles) mf >> t;
            for (int d = 0; d < 4; ++d) {
                int n; mf >> n;
                b.map.start_count[d] = uint8_t(n);
                for (int i = 0; i < n; ++i) mf >> b.map.starts[d][i].x >> b.map.starts[d][i].y;
            }
            int n; mf >> n;
            b.map.unit_count = uint8_t(n);
            for (int i = 0; i < n; ++i) {
                int s;
                mf >> b.map.units[i].x >> b.map.units[i].y >> s;
                b.sprites[i] = uint8_t(s);
            }
            mf >> n;
            b.map.trigger_count = uint8_t(n);
            for (int i = 0; i < n; ++i) {
                auto &t = b.map.triggers[i];
                mf >> t.tile >> t.at.x >> t.at.y >> t.first.x >> t.first.y >> t.second.x >>
                    t.second.y;
            }
            a.boards.push_back(b);
        }
        if (a.boards.size() != 128) { a.why = "expected 128 authored combat boards"; return a; }
    }
    a.ok = true;
    return a;
}

// ---------------------------------------------------------------------------
// One device fixture that can enter combat three ways.
// ---------------------------------------------------------------------------
struct Runtime {
    GameState g;
    TurnState turn;
    TravelState travel;
    CommandState commands;
    std::vector<uint8_t> world_tiles = std::vector<uint8_t>(256 * 256, 5);
    WorldData world{world_tiles.data(), world_tiles.data(), world_tiles.size(),
                    world_tiles.size()};
    CommandContext c{g, turn, travel, commands, world};

    // --- combat ownership, exactly AlphaRuntime's (alpha_runtime.h:63).
    CombatState combat;
    CombatContext combat_context{g, turn, combat};
    CombatField fields[16]{};

    // --- dungeon ownership.
    DungeonState dungeon;
    DungeonScratch scratch;
    DungeonContext dctx{dungeon, scratch};
    DungeonData data[1]{};
    DungeonEncounters encounters{};
    DungeonArena arenas[1]{};
    CombatMap room_map{};
    uint8_t room_sprites[16]{};

    // --- enemy definitions.  The corridor builder asks for 0x40 + type*4, so
    // every wanderer type must resolve; index 5 is the one the room uses.
    CombatEnemy enemy_defs_storage[64]{};
    const CombatEnemy *enemy_defs[64]{};

    // --- overworld reference path resources.
    CombatMap overworld_map{};
    const CombatMap *overworld_maps[1]{&overworld_map};
    CombatResources overworld_resources{};

    UiTextBlock blocks[96]{};
    UiSession ui{{blocks, 96}, {this, &Runtime::thunk}, {64, 8, 12}};
    tdeck::UiInputAdapter input;

    // --- AlphaRuntime's own combat-input queue and enemy beat.
    std::vector<UiAction> combat_input_queue;
    int64_t next_enemy_step_us = 0;
    int64_t clock_us = 1000000;

    // --- observability for the tests.
    int dungeon_dispatches = 0, combat_dispatches = 0;
    int last_dungeon_action = -1;
    CommandKind last_command = CommandKind::Pass;
    CommandStatus last_status = CommandStatus::Success;

    Runtime() {
        for (int i = 0; i < 64; ++i) {
            auto &d = enemy_defs_storage[i];
            d.index = i;
            d.name = "Wanderer";
            d.group_name = "";
            d.hp = 12;
            d.strength = 10;
            d.dexterity = 6;
            d.intelligence = 6;
            d.armor = 1;
            d.damage = 3;
            d.range = 1;
            d.max_per_map = 2;
            enemy_defs[i] = &d;
        }
        combat_context.enemy_defs = enemy_defs;
        combat_context.enemy_def_count = 64;

        // Deceit (33).  Floor 0 is open corridor with a down-ladder at (4,4);
        // floor 1 carries a ROOM cell (0xF0|1) directly beneath it, so one
        // (K)limb-Down lands the party in a combat room -- the exact hardware
        // repro for broken path 2.
        data[0].location = 33;
        std::memset(data[0].cells, 0, sizeof(data[0].cells));
        data[0].cells[4 * 8 + 4] = 0x20;        // LadderDown, floor 0
        data[0].cells[64 + 4 * 8 + 4] = 0xf1;   // floor 1: room #1
        dctx.data = data;
        dctx.count = 1;
        c.dungeon_context = &dctx;
        c.combat_context = &combat_context;
        c.events = ui.event_sink();

        // The authored room arena: an 11x11 floor, party start cells on every
        // side, and two units the fixed setup resolves to enemy index 5.
        room_map.index = dungeon_room_map(33, 1);
        for (auto &t : room_map.tiles) t = 5;
        for (int d = 0; d < 4; ++d) {
            room_map.start_count[d] = 6;
            for (int i = 0; i < 6; ++i)
                room_map.starts[d][i] = {int16_t(3 + i), int16_t(8 - d)};
        }
        room_map.unit_count = 2;
        room_map.units[0] = {4, 3};
        room_map.units[1] = {6, 3};
        room_sprites[0] = room_sprites[1] = uint8_t(0x40 + 5 * 4);
        arenas[0] = {&room_map, room_sprites};

        encounters.combat = &combat_context;
        encounters.arenas = arenas;
        encounters.count = 1;
        dctx.encounters = &encounters;

        // The overworld reference arena.
        overworld_map.index = 0;
        for (auto &t : overworld_map.tiles) t = 5;
        for (int d = 0; d < 4; ++d) {
            overworld_map.start_count[d] = 6;
            for (int i = 0; i < 6; ++i)
                overworld_map.starts[d][i] = {int16_t(3 + i), int16_t(8 - d)};
        }
        overworld_map.unit_count = 2;
        overworld_map.units[0] = {4, 3};
        overworld_map.units[1] = {6, 3};
        overworld_resources.maps = overworld_maps;
        overworld_resources.map_count = 1;
        overworld_resources.enemies = enemy_defs;
        overworld_resources.enemy_count = 64;

        // A full six-member party, like the device's.  A one-member fixture
        // would hide every turn-scheduling arm that only fires with more than
        // one player actor (Engine::current()'s active-character skip).
        g.party.character_count = g.party.party_size = 6;
        for (int i = 0; i < 6; ++i) {
            auto &ch = g.party.characters[i];
            std::snprintf(ch.name, sizeof(ch.name), "Hero%d", i + 1);
            ch.strength = 20;
            ch.intelligence = 15;
            ch.dexterity = uint8_t(10 + i);
            ch.current_hp = ch.max_hp = 200;
            ch.status = 'G';
            ch.character_class = 'A';
            // 0 == "in the active party" (state.cpp party_members()).  The
            // default 0xff would leave the arena with NO player actors at all,
            // which is itself the dead-controls shape and must not be the way
            // this fixture reaches it.
            ch.party_status = 0;
            ch.helmet = ch.weapon = ch.shield = 255;
            ch.armor = ch.ring = ch.amulet = 255;
        }
        g.party.active_character = 255;
        g.torch_turns = 200;
        g.torches = 5;
        g.position = {{0, 0}, {80, 80}};
    }

    static void thunk(void *p, const UiIntent &i) {
        static_cast<Runtime *>(p)->dispatch(i);
    }

    // ---- AlphaRuntime::finish_combat_if_needed() (alpha_runtime.cpp:751).
    bool finish_combat_if_needed() {
        if (!c.combat || !combat.initialized || !combat.ended) return true;
        const auto status = finish_encounter_combat(c, combat);
        if (status != CombatResult::Ok) return false;
        c.combat = false;
        next_enemy_step_us = 0;
        combat_input_queue.clear();
        ui.set_base_mode(dungeon.active ? UiMode::Dungeon : UiMode::Exploration);
        return true;
    }

    // ---- AlphaRuntime::combat_ai_turn() (alpha_runtime.cpp:645).
    bool combat_ai_turn() {
        if (!c.combat || !combat.initialized || combat.ended) return false;
        auto *actor = current_combat_actor(combat_context);
        if (!actor) return false;
        return actor->member == 255 || actor->charmed;
    }

    // ---- AlphaRuntime::schedule_combat() (alpha_runtime.cpp:795).
    void schedule_combat() {
        if (!finish_combat_if_needed()) return;
        if (!c.combat) return;
        auto *actor = current_combat_actor(combat_context);
        if (!actor) {
            // Batch 9D -- AlphaRuntime::schedule_combat()'s stranded-arena arm.
            close_stranded_combat(combat_context);
            if (combat.ended) finish_combat_if_needed();
            return;
        }
        if (actor->member != 255 && !actor->charmed) {
            const int range = actor->range < 1 ? 1 : int(actor->range);
            ui.set_combat_aim(actor->position.x, actor->position.y, int16_t(range),
                              actor->position.x, actor->position.y);
            next_enemy_step_us = 0;
            return;
        }
        if (!next_enemy_step_us) next_enemy_step_us = clock_us + kEnemyBeatUs;
    }

    // ---- AlphaRuntime::service_combat() (alpha_runtime.cpp:815).
    void service_combat() {
        if (!finish_combat_if_needed()) return;
        if (!c.combat) return;
        schedule_combat();
        if (next_enemy_step_us && clock_us >= next_enemy_step_us) {
            Command step;
            step.kind = CommandKind::CombatEnemyStep;
            dispatch_world_command(c, step);
            next_enemy_step_us = 0;
            if (!finish_combat_if_needed()) return;
            schedule_combat();
        }
        if (c.combat && !combat_ai_turn() && !combat_input_queue.empty()) {
            const auto pending = combat_input_queue.front();
            combat_input_queue.erase(combat_input_queue.begin());
            ui.handle_input(pending);
        }
    }

    // Run the enemy beat until it is the party's turn again.  The device's own
    // render loop calls service_combat() every frame; nothing here blocks.
    void settle_combat(int budget = 64) {
        for (int i = 0; i < budget && c.combat && combat_ai_turn(); ++i) {
            clock_us += kEnemyBeatUs;
            service_combat();
        }
        service_combat();
    }

    // ---- AlphaRuntime::command()'s tail (alpha_runtime.cpp:637-642).
    void dispatch(const UiIntent &i) {
        if (i.kind != UiIntentKind::Command) return;
        if (i.command.kind == CommandKind::DungeonCommand) {
            ++dungeon_dispatches;
            last_dungeon_action = i.command.item;
        }
        if (i.command.kind >= CommandKind::CombatMove &&
            i.command.kind <= CommandKind::CombatEnemyStep)
            ++combat_dispatches;
        last_command = i.command.kind;
        const auto result = dispatch_world_command(c, i.command);
        last_status = result.status;
        if (!finish_combat_if_needed()) return;
        c.combat = c.combat_context && c.combat_context->combat.initialized &&
                   !c.combat_context->combat.ended;
        c.dungeon = dungeon.active;
        if (c.combat) ui.set_base_mode(UiMode::Combat);
        else if (c.dungeon) ui.set_base_mode(UiMode::Dungeon);
        schedule_combat();
    }

    // ---- AlphaRuntime::refresh_session_context() (alpha_runtime.cpp:932),
    //      restricted to the mirrors a dungeon/combat key actually reads.  The
    //      dungeon half goes through the shared ui_mode_policy.h seam, so this
    //      test and the device publish the prompts by the same rule.
    void refresh_session_context() {
        ui.set_sail_context((turn.transport_tile & 0xf8) == 0x20,
                            g.position.map.location < 0x80);
        ui.set_harpsichord_active(false);
        tdeck::publish_dungeon_prompt_context(ui, g, dungeon, c.dungeon && dungeon.active);
    }

    // ---- AlphaRuntime::handle()'s gameplay tail (alpha_runtime.cpp:1246-1252
    //      plus synchronize_after_debug()).
    void route(const UiAction &a) {
        if (c.combat && combat_ai_turn()) {
            combat_input_queue.push_back(a);
            return;
        }
        refresh_session_context();
        ui.handle_input(a);
        c.dungeon = dungeon.active;
        c.combat = c.combat_context && c.combat_context->combat.initialized &&
                   !c.combat_context->combat.ended;
        ui.set_base_mode(
            tdeck::resolve_synchronized_base_mode(ui.base_mode(), c.combat, dungeon.active));
    }

    // The whole physical path.  service_combat() first, exactly like handle().
    bool press(uint8_t code) {
        service_combat();
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.code = code;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        return deliver(raw);
    }
    bool ball(tdeck::RawInputKind kind) {
        service_combat();
        tdeck::RawInputEvent raw{};
        raw.kind = kind;
        raw.timestamp_us = (clock_us += 100000);
        return deliver(raw);
    }
    // The mic key is the device's Cancel on a short press.
    bool cancel_key() {
        service_combat();
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.column = tdeck::kMicrophoneKeyColumn;
        raw.row = tdeck::kMicrophoneKeyRow;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        deliver(raw);
        raw.transition = tdeck::KeyTransition::Released;
        raw.timestamp_us = (clock_us += 50000);
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
            tdeck::resolve_synchronized_base_mode(ui.base_mode(), c.combat, dungeon.active));
    }

    void put_at(int x, int y, DungeonFacing f) {
        dungeon.pos.x = uint8_t(x);
        dungeon.pos.y = uint8_t(y);
        dungeon.pos.facing = f;
    }

    // Park a wanderer of a known type in the cell the party faces, so (A)ttack
    // resolves the way the hardware report describes.
    void place_wanderer_ahead(int type) {
        static constexpr int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
        const int f = int(dungeon.pos.facing);
        auto &w = dungeon.wanderer;
        w = {};
        w.type = uint8_t(type);
        w.bank = 0;
        w.floor = dungeon.pos.floor;
        w.x = w.prev_x = uint8_t((dungeon.pos.x + dx[f]) & 7);
        w.y = w.prev_y = uint8_t((dungeon.pos.y + dy[f]) & 7);
    }

    // Batch 9E -- swap this fixture over to AUTHORED data: the real DUNGEON.DAT
    // cells for `loc` and the real DUNGEON.CBT board for its room, so the repro
    // runs on shipped geometry instead of a synthetic map.  Call before
    // enter_dungeon().
    void install_authored(const Authored &a, int loc, int room) {
        data[0].location = uint8_t(loc);
        std::memcpy(data[0].cells, a.dungeon(loc), sizeof(data[0].cells));
        const auto &board = a.boards[size_t(dungeon_room_map(loc, room))];
        room_map = board.map;
        room_map.index = dungeon_room_map(loc, room);
        std::memcpy(room_sprites, board.sprites, sizeof(room_sprites));
        arenas[0] = {&room_map, room_sprites};
    }

    // Batch 9E -- mark an authored SECRET DOOR revealed, which is what a player
    // does with (S)earch before they can walk through it.  Deceit floor 0 (5,3),
    // the LadderDown into room 0, is reached ONLY through the 0xD0 secret door at
    // (5,4); a fixture that parachutes the party onto the ladder without this has
    // silently removed the way out again.
    void reveal(int floor, int x, int y) {
        const int n = floor * 64 + y * 8 + x;
        dungeon.revealed[n >> 3] |= uint8_t(1 << (n & 7));
    }

    // Batch 9E -- the AUTHORED in-arena escape: every conscious member steps onto
    // the board's klimb tile and presses (K).  Driven through the real key path
    // (UiInputAdapter -> UiSession -> CommandKind::CombatKlimb -> CombatAction::
    // Klimb), not by poking CombatState.  Only the standing-on-the-tile part is
    // staged, exactly as walk_party_off_north() stages the board edge.
    void klimb_party_out(int tile_x, int tile_y, int budget = 64) {
        for (int i = 0; i < budget && c.combat; ++i) {
            service_combat();
            settle_combat();
            if (!c.combat) break;
            auto *actor = current_combat_actor(combat_context);
            if (!actor) break;
            if (actor->member == 255) break;
            actor->position.x = int16_t(tile_x);
            actor->position.y = int16_t(tile_y);
            press('k');
        }
        service_combat();
    }

    // The overworld reference entry: exactly what outdoor.cpp does when a
    // roaming encounter lands on the party.
    CombatResult start_overworld_combat() {
        return start_encounter_combat(c, combat, overworld_resources, 5, 5, 0,
                                      CombatDirection::South, true);
    }

    // The reference's own way out of an arena, corridor or room alike: walk
    // every conscious party member off the board edge (Engine::move ->
    // borderForCell -> escape()).  EscapeQuick deliberately refuses while a
    // hostile is standing ("Escape-Not yet!") and refuses outright in a room
    // ("Escape-Not here!"), so it is NOT the general exit.
    void walk_party_off_north(int budget = 64) {
        UiAction north{};
        north.kind = UiActionKind::Direction;
        north.direction = Direction::North;
        for (int i = 0; i < budget && c.combat; ++i) {
            service_combat();
            settle_combat();
            if (!c.combat) break;
            auto *actor = current_combat_actor(combat_context);
            if (!actor) break;
            if (actor->member == 255) break;
            actor->position.y = 0;      // park it on the top row
            route(north);               // one step north leaves the arena
        }
        service_combat();
    }

    // The stuck shape the hardware describes: combat is live and on screen,
    // but nothing can act -- either no actor is scheduled at all, or every
    // input is being parked in the AI queue with no beat to drain it.
    bool combat_is_stuck() {
        if (!c.combat) return false;
        if (!current_combat_actor(combat_context)) return true;
        return combat_ai_turn() && !next_enemy_step_us;
    }
};

// A compact one-line transition dump, so a RED run shows the state split
// instead of only the assertion that noticed it.
void dump(const char *label, Runtime &r) {
    auto *actor = r.c.combat ? current_combat_actor(r.combat_context) : nullptr;
    std::printf("      [%-18s] count=%d current=%d actor=%s member=%d ai=%d beat=%lld victory=%d\n",
                label, r.combat.count, r.combat.current,
                actor ? (actor->enemy ? "enemy" : "player") : "NULL",
                actor ? int(actor->member) : -1, r.c.combat ? r.combat_ai_turn() : 0,
                (long long)r.next_enemy_step_us, r.combat.victory);
    std::printf("      [%-18s] ui.mode=%-14s ui.base=%-14s ctx.combat=%d ctx.dungeon=%d "
                "initialized=%d ended=%d dungeon.active=%d queued=%u\n",
                label, mode_name(r.ui.mode()), mode_name(r.ui.base_mode()), r.c.combat,
                r.c.dungeon, r.combat.initialized, r.combat.ended, r.dungeon.active,
                unsigned(r.combat_input_queue.size()));
}

// ---------------------------------------------------------------------------
// D9D-1  Known-good reference: overworld encounter -> combat keys work.
// ---------------------------------------------------------------------------
void d9d_1_overworld_reference() {
    std::printf("D9D-1 -- overworld combat entry routes the next key to combat\n");
    auto r = std::make_unique<Runtime>();
    expect(r->start_overworld_combat() == CombatResult::Ok, "D9D-1a",
           "overworld encounter starts combat");
    r->schedule_combat();
    r->settle_combat();
    dump("overworld-entry", *r);
    expect(r->ui.mode() == UiMode::Combat, "D9D-1b",
           "UiSession is in UiMode::Combat after overworld entry");

    const int before = r->combat_dispatches;
    expect(r->press(' '), "D9D-1c", "space translates");
    expect(r->combat_dispatches > before, "D9D-1d",
           "the next key reaches a combat command, not the world");
}

// ---------------------------------------------------------------------------
// D9D-2  Broken path 1: dungeon corridor (A)ttack on an adjacent wanderer.
// ---------------------------------------------------------------------------
void d9d_2_dungeon_wanderer_combat() {
    std::printf("D9D-2 -- dungeon (A)ttack opens combat AND routes the next key to it\n");
    auto r = std::make_unique<Runtime>();
    r->enter_dungeon(33);
    r->put_at(4, 4, DungeonFacing::North);
    r->place_wanderer_ahead(0x14);
    expect(r->dungeon.active && r->ui.mode() == UiMode::Dungeon, "D9D-2a",
           "the party is in a corridor before the fight");

    expect(r->press('a'), "D9D-2b", "(A)ttack translates");
    dump("dungeon-attack", *r);
    expect(r->c.combat && r->combat.initialized, "D9D-2c",
           "(A)ttack against an adjacent wanderer starts a corridor combat");
    expect(r->ui.mode() == UiMode::Combat, "D9D-2d",
           "UiSession is in UiMode::Combat after dungeon entry");

    r->settle_combat();
    dump("dungeon-settled", *r);
    const int combat_before = r->combat_dispatches;
    const int dungeon_before = r->dungeon_dispatches;
    expect(r->press(' '), "D9D-2e", "the next key translates");
    dump("dungeon-after-key", *r);
    expect(r->combat_dispatches > combat_before, "D9D-2f",
           "the next mapped combat key reaches the COMBAT handler");
    expect(r->dungeon_dispatches == dungeon_before, "D9D-2g",
           "the next mapped combat key does NOT reach the dungeon handler");
}

// ---------------------------------------------------------------------------
// D9D-3  Broken path 2: a ladder into an authored dungeon ROOM.
// ---------------------------------------------------------------------------
void d9d_3_dungeon_room_combat() {
    std::printf("D9D-3 -- a ladder into a combat room routes the next key to combat\n");
    auto r = std::make_unique<Runtime>();
    r->enter_dungeon(33);
    r->put_at(4, 4, DungeonFacing::North);
    expect(r->ui.mode() == UiMode::Dungeon, "D9D-3a", "the party starts in the corridor");

    // (K)limb on a down-only ladder resolves directly -- no prompt.
    expect(r->press('k'), "D9D-3b", "(K)limb translates");
    dump("room-entry", *r);
    expect(r->dungeon.pos.floor == 1, "D9D-3c", "the ladder lands the party on floor 1");
    expect(r->c.combat && r->combat.initialized && r->combat.room, "D9D-3d",
           "the room cell starts a ROOM combat");
    expect(r->ui.mode() == UiMode::Combat, "D9D-3e",
           "UiSession is in UiMode::Combat after room entry");

    r->settle_combat();
    dump("room-settled", *r);
    const int combat_before = r->combat_dispatches;
    const int dungeon_before = r->dungeon_dispatches;
    expect(r->press(' '), "D9D-3f", "the next key translates");
    dump("room-after-key", *r);
    expect(r->combat_dispatches > combat_before, "D9D-3g",
           "the next mapped combat key reaches the COMBAT handler");
    expect(r->dungeon_dispatches == dungeon_before, "D9D-3h",
           "the next mapped combat key does NOT reach the dungeon handler");
}

// ---------------------------------------------------------------------------
// D9D-4  The full dungeon combat command set, not just Pass.
// ---------------------------------------------------------------------------
void d9d_4_dungeon_combat_command_set() {
    std::printf("D9D-4 -- the combat command set works after a dungeon entry\n");
    auto r = std::make_unique<Runtime>();
    r->enter_dungeon(33);
    r->put_at(4, 4, DungeonFacing::North);
    r->place_wanderer_ahead(0x14);
    r->press('a');
    r->settle_combat();
    expect(r->c.combat, "D9D-4a", "corridor combat is live");

    // (A)ttack opens the aim reticle -- combat's own modal, not the dungeon's.
    const auto mode_before = r->ui.mode();
    r->press('a');
    expect(mode_before == UiMode::Combat && r->ui.mode() == UiMode::TargetSelection &&
               r->ui.target_command_kind() == CommandKind::CombatAttack,
           "D9D-4b", "(A)ttack opens the combat aim reticle");

    r->cancel_key();
    expect(!r->c.combat || r->ui.mode() == UiMode::Combat, "D9D-4c",
           "cancelling the reticle returns to Combat, not Dungeon");

    if (r->c.combat) {
        r->settle_combat();
        const int before = r->combat_dispatches;
        r->ball(tdeck::RawInputKind::TrackballUp);
        expect(r->combat_dispatches > before, "D9D-4d",
               "a direction is a combat MOVE while combat is live");
    } else {
        expect(true, "D9D-4d", "combat already resolved; move case not applicable");
    }
}

// ---------------------------------------------------------------------------
// D9D-5  Return path: combat ends -> the dungeon takes the very next key.
// ---------------------------------------------------------------------------
void d9d_5_return_to_dungeon() {
    std::printf("D9D-5 -- combat exit restores dungeon mode and immediate input\n");
    auto r = std::make_unique<Runtime>();
    r->enter_dungeon(33);
    r->put_at(4, 4, DungeonFacing::North);
    r->place_wanderer_ahead(0x14);
    r->press('a');
    expect(r->c.combat, "D9D-5a", "corridor combat is live");
    r->settle_combat();
    expect(!r->combat_is_stuck(), "D9D-5a2", "the corridor fight is not stuck on entry");

    r->walk_party_off_north();
    dump("after-escape", *r);
    expect(!r->c.combat && !r->combat.initialized, "D9D-5b",
           "walking the party off the arena edge ends the corridor fight");
    expect(r->dungeon.active, "D9D-5c", "the dungeon session survives the fight");
    expect(r->ui.mode() == UiMode::Dungeon, "D9D-5d",
           "UiSession returns to UiMode::Dungeon");

    const int before = r->dungeon_dispatches;
    expect(r->press(' '), "D9D-5e", "the first key after the fight translates");
    expect(r->dungeon_dispatches > before, "D9D-5f",
           "the FIRST key after combat reaches the dungeon -- no wake-up press");
}

// ---------------------------------------------------------------------------
// D9D-6  Room completion returns to the dungeon the same way.
// ---------------------------------------------------------------------------
void d9d_6_room_return() {
    std::printf("D9D-6 -- room combat exit restores dungeon mode and immediate input\n");
    auto r = std::make_unique<Runtime>();
    r->enter_dungeon(33);
    r->put_at(4, 4, DungeonFacing::North);
    r->press('k');
    expect(r->c.combat && r->combat.room, "D9D-6a", "room combat is live");
    r->settle_combat();
    expect(!r->combat_is_stuck(), "D9D-6a2", "the room fight is not stuck on entry");

    r->walk_party_off_north();
    dump("after-room-escape", *r);
    expect(!r->c.combat, "D9D-6b", "the room fight ends");
    expect(r->dungeon.active && r->ui.mode() == UiMode::Dungeon, "D9D-6c",
           "the party is back in the dungeon in UiMode::Dungeon");

    const int before = r->dungeon_dispatches;
    r->press(' ');
    expect(r->dungeon_dispatches > before, "D9D-6d",
           "the FIRST key after the room reaches the dungeon");
}

// ---------------------------------------------------------------------------
// D9D-7  The third entry path: the wanderer walks ONTO the party during an
//        ordinary move (dungeon.cpp tick(), Corridor value 0).  No key starts
//        this one, so it is the purest test of the transition alone.
// ---------------------------------------------------------------------------
void d9d_7_ambush_entry() {
    std::printf("D9D-7 -- an ambush during a move routes the next key to combat\n");
    auto r = std::make_unique<Runtime>();
    r->enter_dungeon(33);
    r->put_at(2, 2, DungeonFacing::North);
    // Park the wanderer ON the party cell so tick() resolves the collision
    // into an ambush rather than a random walk.
    auto &w = r->dungeon.wanderer;
    w = {};
    w.type = 0x1b;  // 0x1b never wanders, so the ambush is deterministic.
    w.bank = 0;
    w.floor = r->dungeon.pos.floor;
    w.x = w.prev_x = r->dungeon.pos.x;
    w.y = w.prev_y = r->dungeon.pos.y;

    UiAction pass{};
    pass.kind = UiActionKind::Character;
    pass.character = u' ';
    r->route(pass);
    dump("ambush", *r);
    expect(r->c.combat && r->combat.initialized, "D9D-7a",
           "the wanderer's ambush starts a corridor combat with no key of its own");
    expect(r->ui.mode() == UiMode::Combat, "D9D-7b",
           "UiSession is in UiMode::Combat after an ambush");
    r->settle_combat();
    expect(!r->combat_is_stuck(), "D9D-7c", "the ambush fight is not stuck");
    const int before = r->combat_dispatches;
    r->press(' ');
    expect(r->combat_dispatches > before, "D9D-7d",
           "the next key after an ambush reaches the COMBAT handler");
}

// ---------------------------------------------------------------------------
// D9D-8  Modal ownership.  No dungeon prompt may survive combat entry, and
//        no combat modal may survive the return to the dungeon.
// ---------------------------------------------------------------------------
void d9d_8_modal_ownership() {
    std::printf("D9D-8 -- no dungeon modal survives combat entry\n");
    auto r = std::make_unique<Runtime>();
    r->enter_dungeon(33);
    r->put_at(4, 4, DungeonFacing::North);

    // (S)earch opens the dungeon's own "Dir-" TargetSelection.  It must be
    // gone by the time a fight starts, or it owns combat's keys.
    r->press('s');
    expect(r->ui.mode() == UiMode::TargetSelection &&
               r->ui.target_command_kind() == CommandKind::DungeonCommand,
           "D9D-8a", "(S)earch opens the dungeon Dir- prompt");

    // Answer it while a wanderer is adjacent, then attack.
    r->place_wanderer_ahead(0x14);
    r->ball(tdeck::RawInputKind::TrackballUp);
    expect(r->ui.mode() == UiMode::Dungeon, "D9D-8b",
           "answering Dir- closes the dungeon prompt before dispatch");

    r->place_wanderer_ahead(0x14);
    r->press('a');
    dump("attack-after-prompt", *r);
    expect(!r->c.combat || r->ui.mode() == UiMode::Combat, "D9D-8c",
           "combat entry after a dungeon prompt still lands in UiMode::Combat");
    expect(r->ui.mode() != UiMode::TargetSelection || !r->c.combat ||
               r->ui.target_command_kind() != CommandKind::DungeonCommand,
           "D9D-8d", "no dungeon TargetSelection survives into combat");
}

// ---------------------------------------------------------------------------
// D9D-9  The two dungeon prompt mirrors must be live on the device.
//        UiSession owns no dungeon state, so the owner has to push these in
//        before each key (the R-19/R-20 narrow-mirror contract).  If nothing
//        does, (K)limb can never descend from an up+down ladder and (D)rink
//        can never ask "Will you drink?".  Batch 9B wired the call into the
//        host test's own input tail but never into AlphaRuntime's, so the two
//        prompts were unreachable on hardware only.  This drives the shared
//        ui_mode_policy.h seam that both now call.
// ---------------------------------------------------------------------------
void d9d_9_prompt_mirrors_are_pushed() {
    std::printf("D9D-9 -- the dungeon prompt mirrors reach UiSession on the device path\n");
    auto r = std::make_unique<Runtime>();
    // A cell with a ladder BOTH ways, and a fountain cell next to it.
    r->data[0].cells[5 * 8 + 5] = 0x30;  // LadderUpDown
    r->data[0].cells[64 + 5 * 8 + 5] = 0x30;
    r->data[0].cells[6 * 8 + 6] = 0x50;  // Fountain
    r->enter_dungeon(33);

    // Deliberately do NOT call UiSession::refresh_dungeon_context() directly:
    // this asserts that the PRODUCTION per-input path publishes the mirrors,
    // which is refresh_session_context()'s job on both sides.
    r->put_at(5, 5, DungeonFacing::North);
    r->ui.set_dungeon_prompt_context(false, false);  // start from a stale mirror
    r->refresh_session_context();
    expect(r->ui.dungeon_klimb_prompt(), "D9D-9a",
           "the per-input refresh publishes the Klimb up/down choice");

    r->put_at(6, 6, DungeonFacing::North);
    r->ui.set_dungeon_prompt_context(false, false);
    r->refresh_session_context();
    expect(r->ui.dungeon_fountain_prompt(), "D9D-9b",
           "the per-input refresh publishes the fountain-underfoot fact");
}

// ---------------------------------------------------------------------------
// D9D-10  Developer teleport is not a combat exit.
//
// The picker already owns this policy -- DebugTeleportStatus::ActiveCombat
// exists, debug_labels.cpp already prints "Blocked by active combat", and
// debug_map_picker_test already pins the CROSS-DUNGEON case.  What nothing
// covered is a teleport OUT of a live arena, which is what the hardware
// session did: leaving to Britannia / the Underworld / a small map tears the
// dungeon session down and moves the party while CombatState is still mounted
// and owns the screen and the keys.  There is then no reachable way back.
// ---------------------------------------------------------------------------
void d9d_10_developer_teleport_during_combat() {
    std::printf("D9D-10 -- developer teleport is refused while an arena is live\n");
    auto r = std::make_unique<Runtime>();
    r->enter_dungeon(33);
    r->put_at(4, 4, DungeonFacing::North);
    r->place_wanderer_ahead(0x14);
    r->press('a');
    expect(r->c.combat && r->combat.initialized, "D9D-10a", "corridor combat is live");

    const auto position_before = r->g.position;
    const auto dungeon_before = r->dungeon.pos;

    DebugTeleportRequest out_of_dungeon{};
    out_of_dungeon.kind = DebugDestinationKind::Britannia;
    out_of_dungeon.x = 90;
    out_of_dungeon.y = 90;
    auto teleport = apply_debug_teleport(r->c, out_of_dungeon);
    dump("after-teleport-out", *r);
    expect(teleport.status == DebugTeleportStatus::ActiveCombat, "D9D-10b",
           "teleport OUT of a live arena is refused as ActiveCombat");
    expect(r->dungeon.active && r->dungeon.pos.dungeon == dungeon_before.dungeon, "D9D-10c",
           "the refused teleport leaves the dungeon session intact");
    expect(r->g.position.map.location == position_before.map.location &&
               r->g.position.xy.x == position_before.xy.x &&
               r->g.position.xy.y == position_before.xy.y,
           "D9D-10d", "the refused teleport mutates no world position");
    expect(r->c.combat && !r->combat_is_stuck(), "D9D-10e",
           "the arena is untouched and still playable after the refusal");

    // A SAME-dungeon floor/cell move skips the EnterDungeon arm entirely, so
    // it had no combat guard at all: it rewrote DungeonState::pos underneath a
    // live arena.
    DebugTeleportRequest same_dungeon{};
    same_dungeon.kind = DebugDestinationKind::Dungeon;
    same_dungeon.location = 33;
    same_dungeon.floor = 3;
    same_dungeon.x = 2;
    same_dungeon.y = 2;
    teleport = apply_debug_teleport(r->c, same_dungeon);
    expect(teleport.status == DebugTeleportStatus::ActiveCombat, "D9D-10f",
           "a same-dungeon teleport is refused while an arena is live");
    expect(r->dungeon.pos.floor == dungeon_before.floor &&
               r->dungeon.pos.x == dungeon_before.x && r->dungeon.pos.y == dungeon_before.y,
           "D9D-10g", "the refused same-dungeon teleport mutates no dungeon position");

    // And once the fight is genuinely over, the same teleport must work.
    r->walk_party_off_north();
    expect(!r->c.combat, "D9D-10h", "the fight ends normally");
    teleport = apply_debug_teleport(r->c, out_of_dungeon);
    expect(teleport.status == DebugTeleportStatus::Applied, "D9D-10i",
           "teleport works again once combat is over -- the guard is not a ban");
}

// ---------------------------------------------------------------------------
// D9D-11  A stranded arena must not wedge the runtime.
//
// combat_over() is true as soon as no party actor is active, but CombatState::
// ended is set only by Engine::end(), which only runs from inside an action.
// An arena that reaches "nobody can ever act again" WITHOUT passing through
// one of those points leaves the owner with:
//
//   current_combat_actor()  -> nullptr, so no beat is ever scheduled and
//                              nothing is queued;
//   combat_action(anything) -> CombatResult::Ok having done nothing;
//   finish_combat_if_needed -> declines, because ended is false;
//
// i.e. the combat scene stays mounted, owns every key, and answers none of
// them -- the exact hardware report.  This pins the escape hatch.
// ---------------------------------------------------------------------------
void d9d_11_stranded_arena_recovers() {
    std::printf("D9D-11 -- a stranded arena closes itself instead of wedging\n");
    auto r = std::make_unique<Runtime>();
    r->enter_dungeon(33);
    r->put_at(4, 4, DungeonFacing::North);
    r->place_wanderer_ahead(0x14);
    r->press('a');
    expect(r->c.combat && r->combat.initialized, "D9D-11a", "corridor combat is live");

    // Strand it: retire every party actor without going through escape()/
    // advance(), so nothing ever calls Engine::end().
    for (int i = 0; i < r->combat.count; ++i) {
        auto &a = r->combat.actors[i];
        if (!a.enemy) a.status = CombatStatus::Fled;
    }
    r->combat.current = -1;
    dump("stranded", *r);
    expect(!r->combat.ended && r->c.combat, "D9D-11b",
           "the arena is live but can never schedule another actor");

    // The owner's ordinary service tick must recover on its own -- no key.
    r->clock_us += kEnemyBeatUs;
    r->service_combat();
    dump("after-service", *r);
    expect(!r->c.combat && !r->combat.initialized, "D9D-11c",
           "the service tick closes the stranded arena through normal teardown");
    expect(r->dungeon.active && r->ui.mode() == UiMode::Dungeon, "D9D-11d",
           "the party is returned to the dungeon");

    const int before = r->dungeon_dispatches;
    r->press(' ');
    expect(r->dungeon_dispatches > before, "D9D-11e",
           "dungeon input works immediately after the recovery");
}

// ===========================================================================
// Batch 9E -- dungeon combat-ROOM escape.  ADJUDICATION + COVERAGE.
// ===========================================================================
//
// Physical testing after Batch 9D reported that leaving a ladder-entered
// dungeon combat room by walking off the board edge returned the party into a
// sealed 1x1 dungeon cell.  The room has been identified from the authored
// data as DECEIT (33) floor 1 (5,3), room 0 -- combat board array position 16
// = DUNGEON.CBT #0, two chests, a mimic and eleven slimes, which is the same
// "slimes and a chest" room the Batch 9D session had been using (16 Phase 6C
// step 33u).  Entry is the 0x20 LadderDown at Deceit floor 0 (5,3).
//
// THAT RETURN IS REFERENCE-FAITHFUL AND IS NOT A DEFECT.  dng_enter_room saves
// g_party_x/y on entry (DUNGEON.OVL 0x0084/0x008c) and restores them on BOTH
// exit branches (0x00fa-0x0103), so the arena's exit border never moves the
// party in the maze; a fled room is legitimately not cleared, so its cell keeps
// 0xFn; and the cleared-room ladder-pair de-seal in dungeon.cpp caps() is gated
// on 0xAn by design (the reference pins the same property).  Deceit room 0 has
// four wall neighbours, so the party is sealed in.
//
// What the player was meant to use is on the BOARD, not in the maze: the room's
// own .CBT carries an in-arena (K)limb tile -- 0xC8 up, 0xC9 down, or the
// room-gated 0x86 grate (SJOG cmd_klimb_combat 0x1df4 + test [g_unk_58a1],0x80)
// -- which sets CombatState::escape_floor_delta, and dungeon_combat_return()
// applies that delta on a flee.  Deceit room 0's board has 0xC8 at (5,2), and
// it lands the party back on the very ladder cell they came down.
//
// The structural finding, censused below: 13 of the 14 authored sealed room
// cells carry exactly the klimb tile that leads back the way the party came in.
//
// These tests therefore pin BOTH exits as correct, and the difference between
// them, so neither can be "fixed" into the other.


// The three in-arena escape tiles.  0x86 is the grate, and it only klimbs
// inside a ROOM (combat.cpp's `tile == 0x86 && c.combat.room`).
constexpr int kLadderUpTile = 0xc8, kLadderDownTile = 0xc9, kGrateTile = 0x86;

int count_tile(const CombatMap &m, int tile) {
    int n = 0;
    for (auto t : m.tiles) if (t == tile) ++n;
    return n;
}
bool find_tile(const CombatMap &m, int tile, int &x, int &y) {
    for (int i = 0; i < 121; ++i)
        if (m.tiles[i] == tile) { x = i % 11; y = i / 11; return true; }
    return false;
}

// The hardware room, named once.
constexpr int kDeceit = 33, kDeceitRoom = 0;
constexpr int kDeceitRoomFloor = 1, kDeceitRoomX = 5, kDeceitRoomY = 3;
constexpr int kDeceitLadderFloor = 0;      // (5,3) on floor 0 is 0x20 LadderDown.
// The representative GRATE room, so the coverage is not Deceit-specific.
constexpr int kDestard = 35, kDestardRoom = 0;
constexpr int kDestardRoomFloor = 0, kDestardRoomX = 3, kDestardRoomY = 1;
constexpr int kDestardLadderFloor = 1;     // (3,1) on floor 1 is 0x18 LadderUp.

// Neighbours the MOVER would let the party step onto -- the same predicate
// dungeon.cpp Forward/Back applies, including the revealed-secret-door arm
// (an unrevealed 0xD is wall; a found one is a door you walk through).
int passable_neighbours(const DungeonState &d) {
    static constexpr int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        const int x = (d.pos.x + dx[i]) & 7, y = (d.pos.y + dy[i]) & 7;
        const int t = dungeon_cell(d, d.pos.floor, x, y) >> 4;
        if (t == 11 || t == 12) continue;
        if (t == 13) {
            const int k = int(d.pos.floor) * 64 + y * 8 + x;
            if (!(d.revealed[k >> 3] & (1u << (k & 7)))) continue;
        }
        ++n;
    }
    return n;
}

void dump_dungeon(const char *label, const Runtime &r) {
    std::printf("      [%-20s] dungeon=%d floor=%d x=%d y=%d facing=%d cell=%02X "
                "nbrs=%d delta=%d victory=%d\n",
                label, r.dungeon.pos.dungeon, r.dungeon.pos.floor, r.dungeon.pos.x,
                r.dungeon.pos.y, int(r.dungeon.pos.facing),
                dungeon_cell(r.dungeon, r.dungeon.pos.floor, r.dungeon.pos.x, r.dungeon.pos.y),
                passable_neighbours(r.dungeon), r.combat.escape_floor_delta, r.combat.victory);
}

// ---------------------------------------------------------------------------
// B9E-0  The authored census (evidence, not gameplay logic).
// ---------------------------------------------------------------------------
void b9e_0_sealed_room_census(const Authored &a) {
    std::printf("B9E-0 -- the authored sealed-room census and the hardware room\n");
    if (!a.ok) { expect(false, "B9E-0", a.why); return; }

    int sealed = 0, with_escape = 0;
    std::string exceptions;
    for (int loc = 33; loc <= 40; ++loc)
        for (int f = 0; f < 8; ++f)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    const uint8_t v = a.cell(loc, f, x, y);
                    if ((v >> 4) != 0xf) continue;
                    static constexpr int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
                    bool all_wall = true, secret = false;
                    for (int i = 0; i < 4; ++i) {
                        const int k = a.cell(loc, f, (x + dx[i]) & 7, (y + dy[i]) & 7) >> 4;
                        if (k == 13) secret = true;
                        if (!(k == 11 || k == 12)) all_wall = false;
                    }
                    if (!all_wall || secret) continue;
                    ++sealed;
                    const int board = dungeon_room_map(loc, v & 15);
                    const auto &m = a.boards[size_t(board)].map;
                    const int esc = count_tile(m, kLadderUpTile) + count_tile(m, kLadderDownTile) +
                                    count_tile(m, kGrateTile);
                    if (esc) ++with_escape;
                    else {
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), "%d:%d:(%d,%d) r%d ", loc, f, x, y, v & 15);
                        exceptions += buf;
                    }
                }
    std::printf("      sealed room cells=%d  with an in-arena klimb/grate escape=%d\n", sealed,
                with_escape);
    std::printf("      without one: %s\n", exceptions.c_str());
    expect(sealed == 14, "B9E-0a", "DUNGEON.DAT holds exactly 14 sealed room cells");
    // MEASURED: twelve.  The two without are BOTH in Doom, and one of them is the
    // ENDGAME room -- Doom 40:7:(5,7) r15 = combat board 127 = Lord British's, which
    // is reached by a PIT (no entry ladder to hand back) and must stay sealed.  Set
    // that one aside and the ordinary sealed rooms are 12 of 13, with Doom room 6
    // the single ordinary cell the authored data leaves without any in-arena way out.
    expect(with_escape == 12, "B9E-0b",
           "12 of the 14 carry an authored in-arena klimb/grate escape on their own board");
    expect(exceptions == "40:2:(5,5) r6 40:7:(5,7) r15 ", "B9E-0c",
           "and the two that do not are Doom room 6 and the Doom ENDGAME room");

    // And the hardware room itself, identified from the authored data.
    const uint8_t room_cell = a.cell(kDeceit, kDeceitRoomFloor, kDeceitRoomX, kDeceitRoomY);
    const uint8_t ladder = a.cell(kDeceit, kDeceitLadderFloor, kDeceitRoomX, kDeceitRoomY);
    const auto &board = a.boards[size_t(dungeon_room_map(kDeceit, kDeceitRoom))];
    int chests = 0, slimes = 0;
    for (int i = 0; i < board.map.unit_count; ++i) {
        if (board.sprites[i] == 1) ++chests;
        if (board.sprites[i] == 0x40 + 24 * 4) ++slimes;   // enemy 24 = SLIME
    }
    int kx = 0, ky = 0;
    const bool has_up = find_tile(board.map, kLadderUpTile, kx, ky);
    std::printf("      Deceit 1 (5,3): cell=%02X ladder above=%02X board=%d chests=%d slimes=%d "
                "0xC8@(%d,%d)=%d\n",
                room_cell, ladder, dungeon_room_map(kDeceit, kDeceitRoom), chests, slimes, kx, ky,
                has_up);
    expect(room_cell == 0xf0, "B9E-0d", "Deceit 33:1:(5,3) is authored room 0 (0xF0)");
    expect(ladder == 0x20, "B9E-0e", "and Deceit floor 0 (5,3) is the 0x20 LadderDown above it");
    expect(chests == 2 && slimes == 11, "B9E-0f",
           "its board is the 'slimes and a chest' room the hardware session used");
    expect(has_up && kx == 5 && ky == 2, "B9E-0g",
           "and the board carries the authored 0xC8 up-ladder at (5,2)");
}

// ---------------------------------------------------------------------------
// B9E-1  Deceit room 0, EDGE-WALK exit.  The hardware observation.
//        The sealed return is EXPECTED -- this test asserts it, it does not
//        report it.
// ---------------------------------------------------------------------------
void b9e_1_deceit_edge_walk_is_faithful(const Authored &a) {
    std::printf("B9E-1 -- Deceit room 0: the edge-walk exit returns to the sealed cell, as it must\n");
    if (!a.ok) { expect(false, "B9E-1", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kDeceitRoom);
    r->enter_dungeon(kDeceit);
    r->dungeon.pos.floor = uint8_t(kDeceitLadderFloor);
    // The authored approach: the 0xD0 secret door south of the ladder, already
    // found with (S)earch -- otherwise the fixture starts the party somewhere no
    // player could be standing.
    r->reveal(kDeceitLadderFloor, kDeceitRoomX, kDeceitRoomY + 1);
    r->put_at(kDeceitRoomX, kDeceitRoomY, DungeonFacing::North);
    r->press('k');   // 0x20 is down-only: no U/D prompt, it resolves directly.
    dump_dungeon("deceit-room-entry", *r);
    expect(r->c.combat && r->combat.room && r->dungeon.pos.floor == kDeceitRoomFloor, "B9E-1a",
           "the ladder down from Deceit floor 0 (5,3) starts the authored room fight");

    r->walk_party_off_north();
    dump_dungeon("deceit-edge-walk", *r);
    expect(!r->c.combat && !r->combat.victory, "B9E-1b", "the party leaves by the board edge");
    expect(r->combat.escape_floor_delta == 0, "B9E-1c",
           "an edge-walk sets NO escape_floor_delta -- the border never moves the party");
    expect(r->dungeon.pos.dungeon == kDeceit && r->dungeon.pos.floor == kDeceitRoomFloor &&
               r->dungeon.pos.x == kDeceitRoomX && r->dungeon.pos.y == kDeceitRoomY,
           "B9E-1d", "so the party is restored to the room-entry cell, 33:1:(5,3)");
    expect(dungeon_cell(r->dungeon, kDeceitRoomFloor, kDeceitRoomX, kDeceitRoomY) == 0xf0,
           "B9E-1e", "the fled room keeps its authored 0xF0 -- it is not cleared");
    expect(!dungeon_room_cleared(r->g, kDeceit, kDeceitRoom), "B9E-1f",
           "and the room-cleared bit stays unset");
    expect(passable_neighbours(r->dungeon) == 0, "B9E-1g",
           "all four dungeon neighbours are wall -- the sealed cell of the report");
    expect(!dungeon_klimb_choice(r->g, r->dungeon), "B9E-1h",
           "no dungeon-side Klimb choice is offered on an uncleared room cell");

    // (K) and ordinary movement, through the real device path.
    const int floor_before = r->dungeon.pos.floor;
    r->press('k');
    expect(r->dungeon.pos.floor == floor_before, "B9E-1i",
           "EXPECTED: dungeon-side (K)limb does not lift the party out of a FLED room");
    r->ball(tdeck::RawInputKind::TrackballUp);
    expect(r->dungeon.pos.x == kDeceitRoomX && r->dungeon.pos.y == kDeceitRoomY, "B9E-1j",
           "EXPECTED: ordinary movement stays Blocked! -- this is 1988, not a defect");
    expect(r->dungeon.active && r->ui.mode() == UiMode::Dungeon, "B9E-1k",
           "the dungeon session and UiMode are intact throughout");
}

// ---------------------------------------------------------------------------
// B9E-2  Deceit room 0, the AUTHORED in-arena (K)limb escape.  The mechanism
//        the player was meant to use, and the coverage this batch exists for.
// ---------------------------------------------------------------------------
void b9e_2_deceit_in_arena_klimb(const Authored &a) {
    std::printf("B9E-2 -- Deceit room 0: the authored in-arena (K)limb escape\n");
    if (!a.ok) { expect(false, "B9E-2", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kDeceitRoom);
    r->enter_dungeon(kDeceit);
    r->dungeon.pos.floor = uint8_t(kDeceitLadderFloor);
    // The authored approach: the 0xD0 secret door south of the ladder, already
    // found with (S)earch -- otherwise the fixture starts the party somewhere no
    // player could be standing.
    r->reveal(kDeceitLadderFloor, kDeceitRoomX, kDeceitRoomY + 1);
    r->put_at(kDeceitRoomX, kDeceitRoomY, DungeonFacing::North);
    r->press('k');
    expect(r->c.combat && r->combat.room, "B9E-2a", "the same authored room fight is live");

    int kx = 0, ky = 0;
    expect(find_tile(r->room_map, kLadderUpTile, kx, ky) && kx == 5 && ky == 2, "B9E-2b",
           "the arena carries the authored 0xC8 up-ladder at board (5,2)");

    // Every conscious member steps onto the ladder tile and presses (K) -- the
    // real key, through UiInputAdapter -> UiSession -> CombatKlimb.
    r->klimb_party_out(kx, ky);
    dump_dungeon("deceit-in-arena-K", *r);

    expect(!r->c.combat && !r->combat.victory, "B9E-2c", "combat ends without a victory");
    expect(r->combat.escape_floor_delta == -1, "B9E-2d",
           "the 0xC8 tile sets escape_floor_delta = -1 (Klimb-Up!)");
    expect(r->dungeon.pos.floor == kDeceitLadderFloor, "B9E-2e",
           "dungeon_combat_return() applies that delta on a FLEE: floor 1 -> 0");
    expect(r->dungeon.pos.dungeon == kDeceit && r->dungeon.pos.x == kDeceitRoomX &&
               r->dungeon.pos.y == kDeceitRoomY,
           "B9E-2f", "x/y are the saved room-entry coordinates, restored unchanged");
    expect(dungeon_cell(r->dungeon, r->dungeon.pos.floor, r->dungeon.pos.x, r->dungeon.pos.y) ==
               0x20,
           "B9E-2g", "so the party stands back on the very LadderDown cell they came down");
    expect(dungeon_cell(r->dungeon, kDeceitRoomFloor, kDeceitRoomX, kDeceitRoomY) == 0xf0,
           "B9E-2h", "the room is still UNCLEARED -- fleeing wins nothing");
    expect(!dungeon_room_cleared(r->g, kDeceit, kDeceitRoom), "B9E-2i",
           "and its cleared bit stays unset");
    expect(dungeon_cell(r->dungeon, kDeceitLadderFloor, kDeceitRoomX, kDeceitRoomY + 1) == 0xd0,
           "B9E-2j",
           "the ladder cell is served by the authored 0xD0 secret door to the south");
    expect(passable_neighbours(r->dungeon) > 0, "B9E-2j2",
           "with that door found, the party is back somewhere navigable");
    r->put_at(kDeceitRoomX, kDeceitRoomY, DungeonFacing::South);
    r->ball(tdeck::RawInputKind::TrackballUp);
    expect(r->dungeon.pos.y == kDeceitRoomY + 1 && r->dungeon.pos.floor == kDeceitLadderFloor,
           "B9E-2j3", "and can walk back out through it -- the escape is a real exit");
    expect(r->dungeon.active && r->ui.mode() == UiMode::Dungeon, "B9E-2k",
           "UiSession is back in UiMode::Dungeon");
    const int before = r->dungeon_dispatches;
    r->press(' ');
    expect(r->dungeon_dispatches > before, "B9E-2l",
           "and the FIRST key after the escape reaches the dungeon");
}

// ---------------------------------------------------------------------------
// B9E-3  The other direction and the other tile: Destard room 0's GRATE.
//        0x86 only klimbs inside a room, and it goes DOWN (+1).
// ---------------------------------------------------------------------------
void b9e_3_destard_grate_escape(const Authored &a) {
    std::printf("B9E-3 -- Destard room 0: the room-gated 0x86 grate escape goes DOWN\n");
    if (!a.ok) { expect(false, "B9E-3", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDestard, kDestardRoom);
    r->enter_dungeon(kDestard);
    r->dungeon.pos.floor = uint8_t(kDestardLadderFloor);
    r->put_at(kDestardRoomX, kDestardRoomY, DungeonFacing::North);
    expect(dungeon_cell(r->dungeon, kDestardLadderFloor, kDestardRoomX, kDestardRoomY) == 0x18,
           "B9E-3a", "Destard floor 1 (3,1) is the authored 0x18 LadderUp");
    r->press('k');   // up-only: resolves directly.
    expect(r->c.combat && r->combat.room && r->dungeon.pos.floor == kDestardRoomFloor, "B9E-3b",
           "klimbing UP lands in the authored room on floor 0 and starts its fight");

    int kx = 0, ky = 0;
    expect(find_tile(r->room_map, kGrateTile, kx, ky), "B9E-3c",
           "the arena carries the authored 0x86 grate");
    expect(count_tile(r->room_map, kLadderUpTile) == 0 &&
               count_tile(r->room_map, kLadderDownTile) == 0,
           "B9E-3d", "and no 0xC8/0xC9 -- so a pass here really is the grate arm");

    r->klimb_party_out(kx, ky);
    dump_dungeon("destard-grate", *r);
    expect(!r->c.combat && !r->combat.victory, "B9E-3e", "combat ends without a victory");
    expect(r->combat.escape_floor_delta == 1, "B9E-3f",
           "the grate sets escape_floor_delta = +1 (Klimb-Down!), like 0xC9");
    expect(r->dungeon.pos.floor == kDestardLadderFloor, "B9E-3g",
           "the party drops back to floor 1, the way they came up");
    expect(dungeon_cell(r->dungeon, r->dungeon.pos.floor, r->dungeon.pos.x, r->dungeon.pos.y) ==
               0x18,
           "B9E-3h", "onto the same 0x18 LadderUp cell");
    expect(dungeon_cell(r->dungeon, kDestardRoomFloor, kDestardRoomX, kDestardRoomY) == 0xf0,
           "B9E-3i", "the fled room stays uncleared here too");
    const int before = r->dungeon_dispatches;
    r->press(' ');
    expect(r->dungeon_dispatches > before, "B9E-3j", "first key after the escape reaches the dungeon");
}

// ---------------------------------------------------------------------------
// B9E-4  VICTORY is not flee.  Same room, same edge-walk, different bookkeeping.
// ---------------------------------------------------------------------------
void b9e_4_victory_is_not_flee(const Authored &a) {
    std::printf("B9E-4 -- Deceit room 0: victory clears the room, fleeing does not\n");
    if (!a.ok) { expect(false, "B9E-4", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kDeceitRoom);
    r->enter_dungeon(kDeceit);
    r->dungeon.pos.floor = uint8_t(kDeceitLadderFloor);
    // The authored approach: the 0xD0 secret door south of the ladder, already
    // found with (S)earch -- otherwise the fixture starts the party somewhere no
    // player could be standing.
    r->reveal(kDeceitLadderFloor, kDeceitRoomX, kDeceitRoomY + 1);
    r->put_at(kDeceitRoomX, kDeceitRoomY, DungeonFacing::North);
    r->press('k');
    expect(r->c.combat && r->combat.room, "B9E-4a", "the authored room fight is live");

    for (int i = 0; i < r->combat.count; ++i) {
        auto &actor = r->combat.actors[i];
        if (actor.enemy) { actor.hp = 0; actor.status = CombatStatus::Dead; }
    }
    r->walk_party_off_north();   // combat_over() stays false while players live
    dump_dungeon("deceit-victory", *r);

    expect(!r->c.combat && r->combat.victory, "B9E-4b", "the room is won");
    expect(r->dungeon.pos.floor == kDeceitRoomFloor && r->dungeon.pos.x == kDeceitRoomX &&
               r->dungeon.pos.y == kDeceitRoomY,
           "B9E-4c", "victory restores the same entry cell -- the border moves nobody here either");
    expect(dungeon_cell(r->dungeon, kDeceitRoomFloor, kDeceitRoomX, kDeceitRoomY) == 0xa0,
           "B9E-4d", "but the victory latch degrades the authored cell 0xF0 -> 0xA0");
    expect(dungeon_room_cleared(r->g, kDeceit, kDeceitRoom), "B9E-4e",
           "and records the room as cleared");
    expect(passable_neighbours(r->dungeon) == 0, "B9E-4f",
           "the authored geometry is untouched -- the cell is still walled in");
    expect(dungeon_cell(r->dungeon, kDeceitLadderFloor, kDeceitRoomX, kDeceitRoomY) == 0x20,
           "B9E-4g", "the LadderDown above is still there");
    const int floor_before = r->dungeon.pos.floor;
    r->press('k');
    expect(r->dungeon.pos.floor == kDeceitLadderFloor && floor_before == kDeceitRoomFloor,
           "B9E-4h",
           "and on a CLEARED room the port-authorized ladder-pair de-seal lifts the party out");
    const int before = r->dungeon_dispatches;
    r->press(' ');
    expect(r->dungeon_dispatches > before, "B9E-4i", "first key after the return reaches the dungeon");

    // Re-entry: a cleared room does not refight.
    r->press('k');
    expect(!r->c.combat && r->dungeon.pos.floor == kDeceitRoomFloor, "B9E-4j",
           "klimbing back down into a CLEARED room starts no fight");
}

// ---------------------------------------------------------------------------
// B9E-5  Re-entry after FLEEING: the room is still there and fights again.
// ---------------------------------------------------------------------------
void b9e_5_fled_room_refights(const Authored &a) {
    std::printf("B9E-5 -- a fled room is still owed a fight\n");
    if (!a.ok) { expect(false, "B9E-5", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kDeceitRoom);
    r->enter_dungeon(kDeceit);
    r->dungeon.pos.floor = uint8_t(kDeceitLadderFloor);
    // The authored approach: the 0xD0 secret door south of the ladder, already
    // found with (S)earch -- otherwise the fixture starts the party somewhere no
    // player could be standing.
    r->reveal(kDeceitLadderFloor, kDeceitRoomX, kDeceitRoomY + 1);
    r->put_at(kDeceitRoomX, kDeceitRoomY, DungeonFacing::North);
    r->press('k');
    int kx = 0, ky = 0;
    find_tile(r->room_map, kLadderUpTile, kx, ky);
    r->klimb_party_out(kx, ky);
    expect(!r->c.combat && r->dungeon.pos.floor == kDeceitLadderFloor, "B9E-5a",
           "the party klimbed out of the room without clearing it");
    r->press('k');   // straight back down the same ladder
    expect(r->c.combat && r->combat.room, "B9E-5b",
           "re-entering an UNCLEARED room fights it again");
}

// ---------------------------------------------------------------------------
// B9E-6  The gate: a sealed room nobody has entered gains nothing.  This is the
//        guard against re-broadening the cleared-room de-seal to room cells at
//        large (which Batch 9E briefly did, and which is a parity divergence).
// ---------------------------------------------------------------------------
void b9e_6_unentered_room_gate(const Authored &a) {
    std::printf("B9E-6 -- an unentered sealed room is not klimbable\n");
    if (!a.ok) { expect(false, "B9E-6", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kDeceitRoom);
    r->enter_dungeon(kDeceit);
    // Deceit 33:2:(1,1) is authored room 2, sealed, and NEVER entered here.
    r->dungeon.pos.floor = 2;
    r->put_at(1, 1, DungeonFacing::North);
    expect(dungeon_cell(r->dungeon, 2, 1, 1) == 0xf2, "B9E-6a",
           "33:2:(1,1) is authored room 2, sealed and unfought");
    expect(!r->c.combat, "B9E-6b", "no fight has happened on this cell");
    expect(passable_neighbours(r->dungeon) == 0, "B9E-6c", "it is walled in");
    // Its ladder pair exists (0x20 above, 0x30 below), so only the cleared-room
    // gate keeps it shut -- which is exactly the property under guard.
    expect(dungeon_cell(r->dungeon, 1, 1, 1) == 0x20 && dungeon_cell(r->dungeon, 3, 1, 1) == 0x30,
           "B9E-6d", "and a ladder pair DOES straddle it, so the gate is what holds");
    const int floor_before = r->dungeon.pos.floor;
    r->press('k');
    expect(r->dungeon.pos.floor == floor_before, "B9E-6e",
           "(K)limb still refuses -- an unfought room is not a staircase");
    r->ball(tdeck::RawInputKind::TrackballUp);
    expect(r->dungeon.pos.x == 1 && r->dungeon.pos.y == 1, "B9E-6f", "and movement stays blocked");
}

// ---------------------------------------------------------------------------
// B9E-7  Corridor control.  The wanderer return is the one the binary really
//        repositions, and nothing in this batch may touch it.
// ---------------------------------------------------------------------------
void b9e_7_corridor_return_unchanged() {
    std::printf("B9E-7 -- corridor returns keep their own, different rules\n");
    {
        auto r = std::make_unique<Runtime>();
        r->enter_dungeon(33);
        r->put_at(2, 2, DungeonFacing::North);
        auto &w = r->dungeon.wanderer;
        w = {};
        w.type = 0x1b;
        w.floor = r->dungeon.pos.floor;
        w.x = w.prev_x = r->dungeon.pos.x;
        w.y = w.prev_y = r->dungeon.pos.y;
        UiAction pass{};
        pass.kind = UiActionKind::Character;
        pass.character = u' ';
        r->route(pass);
        expect(r->c.combat && !r->combat.room, "B9E-7a", "the ambush opens a CORRIDOR fight");
        r->walk_party_off_north();
        expect(!r->c.combat && r->dungeon.pos.x == 2 && r->dungeon.pos.y == 1 &&
                   r->dungeon.pos.facing == DungeonFacing::North,
               "B9E-7b", "an ambush escape steps one cell through the exit border");
        expect(r->dungeon.wanderer.type != 255, "B9E-7c",
               "and the corridor return re-arms the wanderer");
    }
    {
        auto r = std::make_unique<Runtime>();
        r->enter_dungeon(33);
        r->put_at(4, 4, DungeonFacing::North);
        r->place_wanderer_ahead(0x14);
        r->press('a');
        expect(r->c.combat && !r->combat.room, "B9E-7d", "(A)ttack opens a CORRIDOR fight");
        r->walk_party_off_north();
        expect(!r->c.combat && r->dungeon.pos.x == 4 && r->dungeon.pos.y == 4 &&
                   r->dungeon.pos.floor == 0,
               "B9E-7e", "an attack escape does NOT reposition the party");
    }
}

// ===========================================================================
// Batch 21A.1 -- the "Klimb Down from Deceit L1 came back on L8" report.
// ADJUDICATION.
//
// Physical testing after Batch 21A reported, in this order: entered Deceit,
// HUD read L1, the only apparent route was a ladder, (K)limb Down, a top-down
// room board with "Water Serpent" enemies opened immediately, the party left
// the room without winning, and the 3D view came back reading DECEIT L8 on a
// corridor full of doors that was plainly not one level below L1.
//
// THAT IS THE AUTHORED OUTCOME.  There is no floor corruption anywhere in the
// path -- no signed/unsigned slip, no 0xff or -1 sentinel, no wrap, no stale
// snapshot.  The party really is on floor index 7, and L8 is the correct
// rendering of 7.  What actually happened is a six-deep authored PIT SHAFT:
//
//   1. Deceit floor 0 (1,3) is authored 0x60 -- CellType Trap, sub 0.  Every
//      trap cell is down-klimbable: caps() takes `t == 6`, which is exactly
//      DUNGEON:0x1E79-0x1E8B ("escalera-abajo O ambas O hoyo (0x6)"), cloned
//      in game/src/core/dungeon/dungeon.ts klimbCaps() as `cell.type ===
//      CellType.Trap`.  So (K) resolves DOWN with no U/D prompt.
//   2. level() steps to floor 1 and runs enter() on the DESTINATION cell,
//      as change_level does (DUNGEON:0x1C6A, klimb() -> onEnterCell()).
//      Deceit floor 1 (1,3) is 0x69 -- a pit trap.
//   3. enter() therefore runs the pit chain (DUNGEON:0x0A4C, dungeon.ts
//      pitFall()), which falls one floor per pit and KEEPS FALLING while it
//      lands on another pit.  Deceit (1,3) is 0x61 on floors 2, 3, 4, 5 and 6.
//      Six falls: floor 1 -> 7.  Each consumed pit is rewritten 0x6n -> 0x60.
//   4. The chain stops on floor 7 (1,3) = 0xFA -- room 10, uncleared -- and
//      the pit tail opens its fight, the same gate enter() uses.
//   5. DUNGEON.CBT #10 holds four units with sprite 0x88.  initialize_combat
//      resolves a sprite to (sprite - 0x40) / 4 = 18, and def 18 is the SEA
//      SERPENT (game/src/core/world/enemies.ts:34, `def.tile = 0x140 +
//      defIndex*4`).  That is the reporter's "Water Serpent".
//   6. Room 10's board carries NO in-arena escape tile, so leaving it sets
//      escape_floor_delta = 0 and dungeon_combat_return() moves no floor at
//      all.  The party is left on floor 7 -- L8.
//
// So every one of the six observations is authored data doing its job, and
// the one thing that WOULD be a defect -- a floor that left 0..7 -- provably
// never happens.  These cases pin the whole trajectory so that it can never
// be "fixed" into a clamp, and so that a real wrap would fail loudly.
// ===========================================================================

constexpr int kShaftX = 1, kShaftY = 3;          // Deceit's pit shaft column.
constexpr int kShaftRoom = 10, kShaftRoomFloor = 7;
constexpr int kSeaSerpentSprite = 0x88, kSeaSerpentDef = 18;

const char *hud_level(const DungeonState &d) {
    static char text[8];
    const auto bands = hud_dungeon_bands(d, true);
    std::snprintf(text, sizeof(text), "%s", bands.level);
    return text;
}

// How many of the shaft's six pit cells have been consumed.  A consumed pit
// keeps its CellType (6) and drops only the PitFall subtype: the write is
// `0x60 | (cur & 8)`, i.e. the reference's `curSub & 0xf8`, which PRESERVES
// the lit bit.  Deceit's floor-1 pit is 0x69, so it lands on 0x68, not 0x60 --
// matching on the whole byte would quietly miss it.
int consumed_shaft_cells(const DungeonState &d) {
    int n = 0;
    for (int f = 1; f <= 6; ++f) {
        const uint8_t v = dungeon_cell(d, f, kShaftX, kShaftY);
        if ((v >> 4) == 6 && (v & 7) == 0)
            ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// B21A1-0  The authored census.  Evidence, not gameplay logic: this reads the
//          shipped DUNGEON.DAT and DUNGEON.CBT bytes and nothing else.
// ---------------------------------------------------------------------------
void b21a1_0_pit_shaft_census(const Authored &a) {
    std::printf("B21A1-0 -- the authored Deceit pit shaft and the room it lands in\n");
    if (!a.ok) { expect(false, "B21A1-0", a.why); return; }

    expect(a.cell(kDeceit, 0, kShaftX, kShaftY) == 0x60, "B21A1-0a",
           "Deceit floor 0 (1,3) is the authored 0x60 -- a Trap cell, not a ladder");

    int shaft = 0;
    for (int f = 1; f <= 6; ++f) {
        const uint8_t v = a.cell(kDeceit, f, kShaftX, kShaftY);
        if ((v >> 4) == 6 && (v & 7) == 1) ++shaft;
    }
    std::printf("      shaft cells floors 1..6 at (1,3): %d  floor 7 cell=0x%02X\n",
                shaft, a.cell(kDeceit, kShaftRoomFloor, kShaftX, kShaftY));
    expect(shaft == 6, "B21A1-0b",
           "floors 1..6 at (1,3) are ALL chaining pit traps -- a six-deep shaft");
    expect(a.cell(kDeceit, kShaftRoomFloor, kShaftX, kShaftY) == 0xfa, "B21A1-0c",
           "and floor 7 (1,3) is authored room 10 (0xFA) -- what the shaft lands on");

    const auto &board = a.boards[size_t(dungeon_room_map(kDeceit, kShaftRoom))];
    int serpents = 0;
    for (int i = 0; i < board.map.unit_count; ++i)
        if (board.sprites[i] == kSeaSerpentSprite) ++serpents;
    expect(serpents == 4, "B21A1-0d",
           "DUNGEON.CBT #10 places four sprite-0x88 units -- enemy def 18, the Sea Serpent");
    expect((kSeaSerpentSprite - 0x40) / 4 == kSeaSerpentDef, "B21A1-0e",
           "and initialize_combat's own sprite->def rule resolves 0x88 to def 18");
    expect(count_tile(board.map, kLadderUpTile) == 0 &&
               count_tile(board.map, kLadderDownTile) == 0 &&
               count_tile(board.map, kGrateTile) == 0,
           "B21A1-0f",
           "room 10's board carries NO in-arena klimb/grate tile -- leaving it moves no floor");

    // The route is unique: no other Deceit floor-0 cell can reach floor 7 in
    // one (K)limb Down.  If a second one existed, the repro would be ambiguous.
    int deep = 0;
    std::string where;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const int t = a.cell(kDeceit, 0, x, y) >> 4;
            if (!(t == 2 || t == 3 || t == 6)) continue;      // caps(): down-capable
            int f = 1;
            uint8_t cur = a.cell(kDeceit, 1, x, y);
            while ((cur >> 4) == 6 && (cur & 7) == 1 && f < 8) {
                ++f;
                if (f >= 8) break;
                cur = a.cell(kDeceit, f, x, y);
            }
            if (f == kShaftRoomFloor) {
                ++deep;
                char buf[16];
                std::snprintf(buf, sizeof(buf), "(%d,%d) ", x, y);
                where += buf;
            }
        }
    std::printf("      floor-0 cells whose Klimb Down reaches floor 7: %d  %s\n", deep,
                where.c_str());
    expect(deep == 1 && where == "(1,3) ", "B21A1-0g",
           "exactly one Deceit floor-0 cell reaches floor 7 in a single Klimb Down: (1,3)");
}

// ---------------------------------------------------------------------------
// B21A1-1  RED-A.  The hardware sequence, through the real device path:
//          L1 -> (K)limb Down -> a room fight, six floors lower.
// ---------------------------------------------------------------------------
void b21a1_1_klimb_down_falls_to_floor_7(const Authored &a) {
    std::printf("B21A1-1 -- Deceit L1 (1,3): Klimb Down falls the authored shaft into room 10\n");
    if (!a.ok) { expect(false, "B21A1-1", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kShaftRoom);
    r->enter_dungeon(kDeceit);
    r->dungeon.pos.floor = 0;
    r->put_at(kShaftX, kShaftY, DungeonFacing::North);

    expect(std::string(hud_level(r->dungeon)) == "L1", "B21A1-1a",
           "the HUD reads L1 before the klimb -- floor index 0, as the reporter saw");
    expect(dungeon_cell(r->dungeon, 0, kShaftX, kShaftY) == 0x60, "B21A1-1b",
           "the party stands on the authored 0x60 Trap cell");
    expect(!dungeon_klimb_choice(r->g, r->dungeon), "B21A1-1c",
           "a Trap cell is down-only, so (K) resolves with no U/D prompt");

    r->press('k');
    dump_dungeon("shaft-klimb-down", *r);

    expect(r->dungeon.pos.floor == kShaftRoomFloor, "B21A1-1d",
           "one Klimb Down ends on floor index 7 -- the pit chain, not the ladder");
    expect(consumed_shaft_cells(r->dungeon) == 6, "B21A1-1e",
           "all six shaft pits were consumed: exactly six falls, floor 1 -> 7");
    expect(r->dungeon.pos.x == kShaftX && r->dungeon.pos.y == kShaftY, "B21A1-1f",
           "the fall is straight down -- (1,3) throughout");
    expect(r->c.combat && r->combat.room, "B21A1-1g",
           "and the landing cell's room fight opens immediately");

    int serpents = 0;
    for (int i = 0; i < r->combat.count; ++i)
        if (r->combat.actors[i].enemy && r->combat.actors[i].enemy->index == kSeaSerpentDef)
            ++serpents;
    expect(serpents == 4, "B21A1-1h",
           "the arena holds the board's four def-18 Sea Serpents -- the reported enemy");
    expect(std::string(hud_level(r->dungeon)) == "L8", "B21A1-1i",
           "so the HUD legitimately reads L8: floor index 7 + 1, not a clamp");
}

// ---------------------------------------------------------------------------
// B21A1-2  RED-B.  Leaving that room without winning returns the party to the
//          landing cell on floor 7 -- it does not move the floor again.
// ---------------------------------------------------------------------------
void b21a1_2_room_10_flee_returns_to_floor_7(const Authored &a) {
    std::printf("B21A1-2 -- Deceit room 10: leaving without a victory keeps floor 7\n");
    if (!a.ok) { expect(false, "B21A1-2", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kShaftRoom);
    r->enter_dungeon(kDeceit);
    r->dungeon.pos.floor = 0;
    r->put_at(kShaftX, kShaftY, DungeonFacing::North);
    r->press('k');
    expect(r->c.combat && r->combat.room && r->dungeon.pos.floor == kShaftRoomFloor, "B21A1-2a",
           "the shaft fight is live on floor 7");

    r->walk_party_off_north();
    dump_dungeon("room-10-flee", *r);

    expect(!r->c.combat && !r->combat.victory, "B21A1-2b",
           "the party leaves by the board edge -- BATTLE IS LOST!, the reporter's message");
    expect(r->combat.escape_floor_delta == 0, "B21A1-2c",
           "room 10 has no in-arena klimb tile, so escape_floor_delta stays 0");
    expect(r->dungeon.pos.floor == kShaftRoomFloor && r->dungeon.pos.x == kShaftX &&
               r->dungeon.pos.y == kShaftY,
           "B21A1-2d", "dungeon_combat_return() restores the room-entry cell, 33:7:(1,3)");
    expect(std::string(hud_level(r->dungeon)) == "L8", "B21A1-2e",
           "the 3D view comes back on L8 -- the authored result, not a corruption");
    expect(dungeon_cell(r->dungeon, kShaftRoomFloor, kShaftX, kShaftY) == 0xfa, "B21A1-2f",
           "the fled room keeps its authored 0xFA -- it is not cleared");
    expect(r->dungeon.active && r->ui.mode() == UiMode::Dungeon, "B21A1-2g",
           "the dungeon session and UiMode survive the whole trip");
}

// ---------------------------------------------------------------------------
// B21A1-3  RED-C.  The floor never leaves 0..7.  This is the case that would
//          catch the wrap/sentinel defect the report hypothesised, so it must
//          fail loudly if one is ever introduced.
// ---------------------------------------------------------------------------
void b21a1_3_no_floor_wrap(const Authored &a) {
    std::printf("B21A1-3 -- no wrap: the floor is an honest 7, never -1/0xff/255\n");
    if (!a.ok) { expect(false, "B21A1-3", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kShaftRoom);
    r->enter_dungeon(kDeceit);
    r->dungeon.pos.floor = 0;
    r->put_at(kShaftX, kShaftY, DungeonFacing::North);
    r->press('k');
    r->walk_party_off_north();

    const unsigned floor = r->dungeon.pos.floor;
    std::printf("      floor byte = %u (0x%02X)  hud = %s\n", floor, floor,
                hud_level(r->dungeon));
    expect(floor == 7, "B21A1-3a",
           "the stored floor byte is exactly 7 -- not 255, not 0xff, not a masked -1");
    expect(int(floor) + 1 == 8, "B21A1-3b",
           "so the HUD level arithmetic yields 8 directly, with nothing to clamp");

    // The decisive wrap detector: run_dungeon_command refuses EVERY command
    // when pos.floor > 7.  A wrapped floor would make the dungeon unplayable,
    // which is emphatically not what the hardware showed.
    const int before = r->dungeon_dispatches;
    r->ball(tdeck::RawInputKind::TrackballUp);
    expect(r->dungeon_dispatches > before, "B21A1-3c",
           "movement still reaches the dungeon -- the floor>7 gate in the command path passes");
    expect(r->last_status != CommandStatus::InvalidContext, "B21A1-3d",
           "and it is not refused as InvalidContext, which a wrapped floor would force");

    // The landing cell is NOT sealed: (1,4) on floor 7 is corridor, so the
    // party can walk out.  The reporter's "Blocked!" is the north wall at
    // (1,2), which is authored wall.
    expect(dungeon_cell(r->dungeon, kShaftRoomFloor, kShaftX, kShaftY + 1) == 0x00, "B21A1-3e",
           "floor 7 (1,4) is authored corridor -- the party is not sealed in");
    expect((dungeon_cell(r->dungeon, kShaftRoomFloor, kShaftX, kShaftY - 1) >> 4) == 11,
           "B21A1-3f", "while (1,2) is authored wall -- the reported Blocked! going north");
}

// ---------------------------------------------------------------------------
// B21A1-4  RED-D.  Control: the OTHER Deceit ladder-into-room transition is
//          untouched.  (5,3) is a real 0x20 LadderDown and still moves exactly
//          one floor, into room 0's slimes.
// ---------------------------------------------------------------------------
void b21a1_4_ladder_room_control(const Authored &a) {
    std::printf("B21A1-4 -- control: the (5,3) LadderDown still moves exactly one floor\n");
    if (!a.ok) { expect(false, "B21A1-4", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kDeceitRoom);
    r->enter_dungeon(kDeceit);
    r->dungeon.pos.floor = uint8_t(kDeceitLadderFloor);
    r->reveal(kDeceitLadderFloor, kDeceitRoomX, kDeceitRoomY + 1);
    r->put_at(kDeceitRoomX, kDeceitRoomY, DungeonFacing::North);
    r->press('k');
    dump_dungeon("ladder-room-control", *r);

    expect(r->dungeon.pos.floor == kDeceitRoomFloor, "B21A1-4a",
           "a LadderDown is one floor and no more: floor 0 -> 1");
    expect(consumed_shaft_cells(r->dungeon) == 0, "B21A1-4b",
           "and no pit anywhere was consumed -- the shaft is specific to (1,3)");
    expect(r->c.combat && r->combat.room, "B21A1-4c", "room 0's fight opens as before");
    expect(std::string(hud_level(r->dungeon)) == "L2", "B21A1-4d",
           "the HUD reads L2 -- the ordinary one-level result Batch 9E already pins");
}

// ---------------------------------------------------------------------------
// B21A1-5  RED-E.  Control: the authored in-arena (K)limb out of Deceit room 0
//          still lands the party back on floor 0, exactly as Batch 9E proved.
// ---------------------------------------------------------------------------
void b21a1_5_in_arena_klimb_control(const Authored &a) {
    std::printf("B21A1-5 -- control: room 0's authored in-arena Klimb still returns to floor 0\n");
    if (!a.ok) { expect(false, "B21A1-5", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kDeceitRoom);
    r->enter_dungeon(kDeceit);
    r->dungeon.pos.floor = uint8_t(kDeceitLadderFloor);
    r->reveal(kDeceitLadderFloor, kDeceitRoomX, kDeceitRoomY + 1);
    r->put_at(kDeceitRoomX, kDeceitRoomY, DungeonFacing::North);
    r->press('k');
    expect(r->c.combat && r->combat.room, "B21A1-5a", "room 0's fight is live on floor 1");

    int kx = 0, ky = 0;
    expect(find_tile(r->room_map, kLadderUpTile, kx, ky), "B21A1-5b",
           "its board carries the authored 0xC8 up-ladder");
    r->klimb_party_out(kx, ky);
    dump_dungeon("in-arena-klimb-control", *r);
    expect(r->combat.escape_floor_delta == -1, "B21A1-5c",
           "0xC8 sets escape_floor_delta = -1, unchanged by this batch");
    expect(r->dungeon.pos.floor == kDeceitLadderFloor, "B21A1-5d",
           "and dungeon_combat_return() applies it: floor 1 -> 0");
    expect(std::string(hud_level(r->dungeon)) == "L1", "B21A1-5e",
           "the HUD is back to L1 -- the authored escape is intact");
}

// ---------------------------------------------------------------------------
// B21A1-6  The LAST observation: "I teleported back to the beginning of Deceit
//          and the dungeon looked different."  It does, and both reasons are
//          real and intended -- neither is a corruption.
// ---------------------------------------------------------------------------
void b21a1_6_teleport_back_looks_different(const Authored &a) {
    std::printf("B21A1-6 -- why the standard entry looks different after the fall\n");
    if (!a.ok) { expect(false, "B21A1-6", a.why); return; }
    auto r = std::make_unique<Runtime>();
    r->install_authored(a, kDeceit, kShaftRoom);
    r->enter_dungeon(kDeceit);
    const auto facing_on_entry = r->dungeon.pos.facing;
    r->dungeon.pos.floor = 0;
    r->put_at(kShaftX, kShaftY, DungeonFacing::North);
    r->press('k');
    r->walk_party_off_north();
    // Turn on the spot the way a player orienting themselves on L8 would.
    r->ball(tdeck::RawInputKind::TrackballRight);
    const auto facing_before_teleport = r->dungeon.pos.facing;

    DebugTeleportRequest back{};
    back.kind = DebugDestinationKind::Dungeon;
    back.location = kDeceit;
    back.floor = 0;
    back.standard_entry = true;
    const auto teleport = apply_debug_teleport(r->c, back);
    dump_dungeon("teleport-standard-entry", *r);

    expect(teleport.status == DebugTeleportStatus::Applied, "B21A1-6a",
           "the developer teleport back to Deceit's standard entry succeeds");
    expect(r->dungeon.pos.floor == 0 && r->dungeon.pos.x == 1 && r->dungeon.pos.y == 1,
           "B21A1-6b", "it lands on the authored entrance stair, 33:0:(1,1)");
    expect(std::string(hud_level(r->dungeon)) == "L1", "B21A1-6c",
           "the HUD reads L1 again -- the entry level is correct and stable");

    // Reason 1: a same-dungeon teleport deliberately PRESERVES facing, so the
    // same cell is rendered down a different axis than on first entry.
    expect(r->dungeon.pos.facing == facing_before_teleport, "B21A1-6d",
           "facing is preserved, as debug_map_picker documents for same-dungeon moves");
    expect(facing_before_teleport != facing_on_entry, "B21A1-6e",
           "and it differs from the facing dungeon_load gave on first entry -- a different view");

    // Reason 2: the shaft is genuinely spent.  The pit chain rewrites every pit
    // it consumes, and a same-dungeon teleport does NOT re-run dungeon_load, so
    // the map really has changed.  Walking back onto (1,3) no longer falls.
    expect(consumed_shaft_cells(r->dungeon) == 6, "B21A1-6f",
           "the six consumed pits stay consumed -- the teleport does not reload the map");
    r->put_at(kShaftX, kShaftY, DungeonFacing::North);
    r->press('k');
    dump_dungeon("second-klimb-down", *r);
    expect(r->dungeon.pos.floor == 1 && !r->c.combat, "B21A1-6g",
           "so a SECOND Klimb Down at (1,3) now stops on floor 1 -- the shaft is spent");
}

} // namespace

int main(int argc, char **argv) {
    std::printf("Batch 9D -- non-overworld combat transition regressions\n\n");
    d9d_1_overworld_reference();
    d9d_2_dungeon_wanderer_combat();
    d9d_3_dungeon_room_combat();
    d9d_4_dungeon_combat_command_set();
    d9d_5_return_to_dungeon();
    d9d_6_room_return();
    d9d_7_ambush_entry();
    d9d_8_modal_ownership();
    d9d_9_prompt_mirrors_are_pushed();
    d9d_10_developer_teleport_during_combat();
    d9d_11_stranded_arena_recovers();

    std::printf("\nBatch 9E -- dungeon combat-room escape (adjudication + coverage)\n\n");
    const auto authored = load_authored(argc > 1 ? argv[1] : nullptr,
                                        argc > 2 ? argv[2] : nullptr);
    b9e_0_sealed_room_census(authored);
    b9e_1_deceit_edge_walk_is_faithful(authored);
    b9e_2_deceit_in_arena_klimb(authored);
    b9e_3_destard_grate_escape(authored);
    b9e_4_victory_is_not_flee(authored);
    b9e_5_fled_room_refights(authored);
    b9e_6_unentered_room_gate(authored);
    b9e_7_corridor_return_unchanged();

    std::printf("\nBatch 21A.1 -- the Deceit L1 -> Klimb Down -> L8 report (adjudication)\n\n");
    b21a1_0_pit_shaft_census(authored);
    b21a1_1_klimb_down_falls_to_floor_7(authored);
    b21a1_2_room_10_flee_returns_to_floor_7(authored);
    b21a1_3_no_floor_wrap(authored);
    b21a1_4_ladder_room_control(authored);
    b21a1_5_in_arena_klimb_control(authored);
    b21a1_6_teleport_back_looks_different(authored);
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
