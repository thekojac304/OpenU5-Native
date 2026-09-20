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
    CombatEnemy enemy_defs_storage[32]{};
    const CombatEnemy *enemy_defs[32]{};

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
        for (int i = 0; i < 32; ++i) {
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
        combat_context.enemy_def_count = 32;

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
        overworld_resources.enemy_count = 32;

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

} // namespace

int main() {
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
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
