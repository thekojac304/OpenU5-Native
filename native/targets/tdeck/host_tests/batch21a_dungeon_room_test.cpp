// Batch 21A -- the dungeon room-entry freeze / stuck-turn family
// (H-149, H-150, H-151, H-152), reproduced on the REAL production runtime.
//
// Seam: the Batch 11 AlphaRuntime host-test seam, extended in this batch with
// the four dungeon/room-combat resources production reads out of the SD pack
// (dungeon data, room arenas, enemy definitions, the overworld location table).
// Nothing in this file mirrors runtime logic: every step below goes
//
//   RawInputEvent -> tdeck::UiInputAdapter -> openu5::UiSession
//     -> AlphaRuntime::dispatch()/command()  (the ACTUAL class the T-Deck runs)
//       -> openu5::dispatch_world_command() -> execute_dungeon_command()
//         -> dungeon_action() -> DungeonEventKind::Room -> dungeon_encounter()
//           -> start_fixed_combat() -> initialize_combat() -> Engine::current()
//
// and every assertion reads the same DungeonState/CombatState/UiSession objects
// those functions wrote.
#include "../main/alpha_runtime.h"
#include "../main/ui_input_adapter.h"

#include "openu5/combat.h"
#include "openu5/commands.h"
#include "openu5/dungeon.h"
#include "openu5/dungeon_encounters.h"
#include "openu5/quest_state.h"
#include "openu5/ui_session.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

using namespace openu5;

namespace {

int g_failures = 0, g_checks = 0;
bool g_verbose = false;

void expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-6s %s\n", ok ? "GREEN" : "RED", id, what);
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

// --------------------------------------------------------------------------
// Authored-shaped fixture resources.
//
// Deceit is location 33.  Cell encoding is the dungeon's own (high nibble =
// CellType, low nibble = sub): 0x00 corridor, 0x1n ladder-up, 0x2n ladder-down,
// 0xBn wall, 0xFn room n, 0xAn already-cleared room n.
// --------------------------------------------------------------------------
constexpr uint8_t kDeceit = 33;

struct Fixture {
    // --- world ------------------------------------------------------------
    std::vector<uint8_t> world_tiles = std::vector<uint8_t>(256 * 256, 5);
    uint8_t location_x[40]{}, location_y[40]{};

    // --- dungeon ----------------------------------------------------------
    DungeonData dungeons[1]{};

    // --- room arenas ------------------------------------------------------
    // Two physically different rooms that share room number 1: one authored on
    // floor 1, one on floor 3.  Their combat-map indices are the same, because
    // dungeon_room_map() keys the arena by (dungeon, room number) -- which is
    // what the 1988 binary does too (roomCombatMapIndex / DNGLOOK 0x0844).
    CombatMap room_map{};
    uint8_t room_sprites[16]{};
    DungeonArena arenas[1]{};

    CombatEnemy enemy_storage[64]{};
    const CombatEnemy *enemy_defs[64]{};

    std::unique_ptr<tdeck::AlphaRuntime> owner{new tdeck::AlphaRuntime()};
    tdeck::AlphaRuntime &rt = *owner;
    int64_t clock_us = 1'000'000;

    explicit Fixture(int party = 3) {
        for (int i = 0; i < 64; ++i) {
            auto &d = enemy_storage[i];
            d.index = i;
            d.name = "Wanderer";
            d.group_name = "";
            d.hp = 12; d.strength = 10; d.dexterity = 6; d.intelligence = 6;
            d.armor = 1; d.damage = 3; d.range = 1; d.max_per_map = 2;
            enemy_defs[i] = &d;
        }

        // Deceit's cells.  Everything is wall by default so the party can only
        // walk where this fixture authors a corridor.
        dungeons[0].location = kDeceit;
        std::memset(dungeons[0].cells, 0xb0, sizeof(dungeons[0].cells));
        auto cell = [&](int f, int x, int y) -> uint8_t & {
            return dungeons[0].cells[f * 64 + y * 8 + x];
        };
        // Floor 0: entrance ladder-up at (1,1), corridor east, room #1 at (3,1),
        // and a ladder-down at (2,1) so the party can reach floor 1..3.
        cell(0, 1, 1) = 0x10;             // LadderUp -- dungeon_load's entrance
        cell(0, 2, 1) = 0x20;             // LadderDown
        cell(0, 3, 1) = 0x00;             // corridor
        cell(0, 4, 1) = 0xf1;             // ROOM #1 (floor 0)
        cell(0, 5, 1) = 0x00;             // corridor beyond the room
        // Floor 1: a physically different ROOM #1, reachable by klimbing down
        // from (2,1) and walking east.
        cell(1, 2, 1) = 0x10;             // LadderUp back to floor 0
        cell(1, 3, 1) = 0x00;
        cell(1, 4, 1) = 0xf1;             // ROOM #1 (floor 1) -- same number!
        cell(1, 5, 1) = 0x00;

        // The authored arena for Deceit room 1.
        room_map.index = dungeon_room_map(kDeceit, 1);
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

        // Deceit's overworld entrance tile.  location_at() returns index+1, so
        // slot 32 is location 33.  Tile 24 is the authored "Enter dungeon" tile
        // commands.cpp's (E)nter gate looks for.
        location_x[32] = 128; location_y[32] = 128;
        world_tiles[128 * 256 + 128] = 24;

        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = WorldData{world_tiles.data(), world_tiles.data(),
                             world_tiles.size(), world_tiles.size()};
        hf.dungeons = dungeons; hf.dungeon_count = 1;
        hf.arenas = arenas; hf.arena_count = 1;
        hf.enemy_defs = enemy_defs; hf.enemy_def_count = 64;
        hf.location_x = location_x; hf.location_y = location_y; hf.location_count = 40;
        rt.attach_host_test_fixture(hf);

        auto &g = rt.game();
        g.party.character_count = g.party.party_size = int8_t(party);
        for (int i = 0; i < party; ++i) {
            auto &ch = g.party.characters[i];
            std::snprintf(ch.name, sizeof(ch.name), "Hero%d", i + 1);
            ch.strength = 20; ch.intelligence = 15;
            ch.dexterity = uint8_t(10 + i);
            ch.current_hp = ch.max_hp = 240;
            ch.status = 'G';
            ch.party_status = 0;          // 0 = in the party (default is 0xff)
            ch.character_class = i ? 'F' : 'A';
        }
        g.party.active_character = 255;
        g.position.xy.x = 128; g.position.xy.y = 128;
        g.position.map.location = 0; g.position.map.floor = 0;
        g.torch_turns = 500; g.torches = 5;
        // quest_world.cpp:313 masks a dungeon entrance to tile 223 until the
        // party knows that dungeon's Word of Power; the (E)nter gate needs the
        // real tile 24.
        set_quest_flag(g.quest, QuestFlag::Word33);
    }

    bool key(uint8_t code) {
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.code = code;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        return rt.handle(raw);
    }
    bool ball(tdeck::RawInputKind kind) {
        tdeck::RawInputEvent raw{};
        raw.kind = kind;
        raw.timestamp_us = (clock_us += 100000);
        return rt.handle(raw);
    }
    // The four cardinal inputs, exactly as the device delivers them.
    bool north() { return ball(tdeck::RawInputKind::TrackballUp); }
    bool south() { return ball(tdeck::RawInputKind::TrackballDown); }
    bool east()  { return ball(tdeck::RawInputKind::TrackballRight); }
    bool west()  { return ball(tdeck::RawInputKind::TrackballLeft); }

    const DungeonState &d() const { return rt.dungeon_state(); }
    const CombatState &cs() const { return rt.combat_state(); }
    const CommandContext &ctx() const { return rt.command_context(); }
    UiMode mode() const { return rt.ui() ? rt.ui()->mode() : UiMode::Exploration; }

    // Let production's own combat service loop run for up to `seconds` of real
    // time, feeding it (space) = pass whenever it is a player's turn. Each
    // rt.handle() call runs AlphaRuntime::service_combat() first, exactly as the
    // device's input path does.
    void play_out(double seconds) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
        while (ctx().combat && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            key(' ');
        }
    }

    // Walk the party off one arena border until the encounter closes, letting
    // production's own enemy beat run between attempts.
    void retreat(tdeck::RawInputKind dir, double seconds) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
        while (ctx().combat && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ball(dir);
        }
    }

    // Enter Deceit through the real (E)nter command path.
    void enter_deceit() {
        if (g_verbose) {
            const auto &w = ctx().world;
            const auto *ts = ctx().transport_services;
            std::printf("       probe: raw=%d resolved=%d location_at=%d small_maps=%zu\n",
                        w.overworld ? int(w.overworld[128 * 256 + 128]) : -1,
                        ts && ts->tile_at ? int(ts->tile_at(ts->context, 128, 128)) : -2,
                        int(location_at(ctx().locations, 128, 128)), w.small_map_count);
        }
        key('e');
    }

    // Advance the dungeon-facing until it points at `f` (0=N,1=E,2=S,3=W),
    // then step forward once.  Turning is the same production Left/Right the
    // device sends; it never moves the party.
    void face(int f) {
        for (int i = 0; i < 4 && int(d().pos.facing) != f; ++i) east();   // Right = turn right
    }
    void advance() { north(); }   // North = DungeonAction::Forward

    void dump(const char *label) const {
        if (!g_verbose) return;
        std::printf("       [%s] ui=%s ctx.dungeon=%d ctx.combat=%d "
                    "dng(active=%d f=%u x=%u y=%u face=%d cell=0x%02x) "
                    "combat(init=%d ended=%d victory=%d room=%d count=%ld current=%ld)\n",
                    label, mode_name(mode()), ctx().dungeon, ctx().combat,
                    d().active, unsigned(d().pos.floor), unsigned(d().pos.x),
                    unsigned(d().pos.y), int(d().pos.facing),
                    unsigned(dungeon_cell(d(), d().pos.floor, d().pos.x, d().pos.y)),
                    cs().initialized, cs().ended, cs().victory, cs().room,
                    long(cs().count), long(cs().current));
    }
};

// Count how many DISTINCT living party members get handed a turn over `rounds`
// scheduler observations, driving only production's own service/scheduler path.
struct TurnCensus {
    bool seen[8]{};
    int distinct = 0;
    int observations = 0;
};

TurnCensus census(Fixture &f, int rounds) {
    TurnCensus c;
    for (int i = 0; i < rounds; ++i) {
        // Reading the scheduler through the core's own accessor -- the same
        // call AlphaRuntime::schedule_combat() makes every service tick.
        auto &cc = const_cast<CombatState &>(f.cs());
        if (!cc.initialized || cc.ended) break;
        if (cc.current >= 0 && cc.current < cc.count) {
            const auto &a = cc.actors[cc.current];
            if (a.member != 255 && a.member < 8 && !c.seen[a.member]) {
                c.seen[a.member] = true;
                ++c.distinct;
            }
            ++c.observations;
        }
        // Production's own "pass this turn" input: space, routed through
        // UiInputAdapter -> UiSession -> AlphaRuntime -> combat_action(Pass).
        f.key(' ');
    }
    return c;
}

} // namespace

// ===========================================================================
// RED-1 -- reused room-number collision.
//
// A room number is 4 bits and is reused across a dungeon's 8 floors.  The 1988
// binary keys the persistent cleared bitmap by (dungeon, room number) only --
// DNGLOOK 0x0844/0x08d4, cloned in dungeon.cpp's cleared_bit() and pinned by
// game/src/core/dungeon/dungeon.ts dungeonClearedBitIndex().  So clearing room 1
// on floor 0 marks the PHYSICALLY DIFFERENT room 1 on floor 1 as cleared too.
//
// That collision is ORIGINAL and must be preserved.  What this case pins is the
// state the runtime is left in when the collision sends a genuinely fresh room
// down the message-only branch.
// ===========================================================================
void red1_room_number_collision() {
    std::printf("RED-1  reused room-number collision (floor 0 room 1 clears floor 1 room 1)\n");
    Fixture f;
    f.enter_deceit();
    f.dump("entered");

    // Mark Deceit room 1 cleared exactly the way a victory would: through the
    // production core, not by poking the bitmap.
    auto &g = f.rt.game();
    auto &d = f.rt.dungeon_state_for_test();
    dungeon_mark_room(g, d, 0, 4, 1);

    expect(dungeon_room_cleared(g, kDeceit, 1), "R1-1",
           "clearing floor-0 room 1 sets the (dungeon, room) bit");
    expect(dungeon_cell(f.d(), 1, 4, 1) == 0xf1, "R1-2",
           "the floor-1 room cell is still authored 0xF1 -- it was never visited");
    expect(dungeon_room_cleared(g, kDeceit, 1), "R1-3",
           "...yet dungeon_room_cleared() reports the floor-1 room cleared too "
           "(ORIGINAL 4-bit collision, preserved)");
}

// ===========================================================================
// RED-3 -- cleared-room / message-only transition leaves a coherent runtime.
//
// The reference (dungeon.ts onEnterCell) prints "Entering room..." and stays in
// the dungeon: no arena, no combat, and the party may walk on.  Assert exactly
// that, and that no stale combat state survives.
// ===========================================================================
void red3_cleared_room_message_only() {
    std::printf("RED-3  cleared room emits the message only and stays walkable\n");
    Fixture f;
    f.enter_deceit();
    auto &g = f.rt.game();
    auto &dw = f.rt.dungeon_state_for_test();
    // Pre-clear room 1 so the very next entry takes the message-only branch.
    dungeon_mark_room(g, dw, 0, 4, 1);
    dw.cells[0 * 64 + 1 * 8 + 4] = 0xa1;   // 0x00f5's `&0xAF`, as the core does

    f.face(1);                              // face East
    f.advance();                            // (2,1) ladder-down cell
    f.advance();                            // (3,1) corridor
    f.dump("before-room");
    f.advance();                            // (4,1) the CLEARED room
    f.dump("in-cleared-room");

    expect(f.d().pos.x == 4 && f.d().pos.y == 1, "R3-1",
           "the party actually stepped onto the cleared room cell");
    expect(!f.ctx().combat && !f.cs().initialized, "R3-2",
           "no combat was started (reference: a cleared room places no monsters)");
    expect(f.mode() == UiMode::Dungeon, "R3-3",
           "the session stays in Dungeon mode -- no phantom arena");
    const auto x0 = f.d().pos.x;
    f.advance();                            // step off, east to (5,1)
    f.dump("after-room");
    expect(f.d().pos.x != x0, "R3-4",
           "movement out of a cleared room still works (no four-way Blocked!)");
}

// ===========================================================================
// RED-2 / H-152 -- room entry actor availability.
//
// A healthy multi-member party enters a fresh room through the production path.
// Every eligible player actor must eventually be handed a turn.
// ===========================================================================
void red2_room_actor_availability() {
    std::printf("RED-2  every healthy party member gets a turn inside a room\n");
    Fixture f;
    f.enter_deceit();
    f.face(1);
    f.advance(); f.advance();
    f.dump("before-room");
    f.advance();                            // step into the FRESH room
    f.dump("in-room");

    expect(f.cs().initialized && f.ctx().combat, "R2-1",
           "a fresh room really did start room combat");
    expect(f.cs().room, "R2-2", "CombatState::room is set for an authored room");
    expect(f.mode() == UiMode::Combat, "R2-3",
           "the session switched to Combat mode with the arena");

    auto c = census(f, 40);
    std::printf("       distinct player actors scheduled=%d over %d observations\n",
                c.distinct, c.observations);
    expect(c.distinct >= 3, "R2-4",
           "all three healthy party members are handed a turn inside the room");
}

// ===========================================================================
// RED-4 / H-151 first half -- battle loss -> continued movement -> next room.
//
// "BATTLE IS LOST!" is what finish_encounter_combat() prints for ANY non-victory
// exit, the tester's retreat included.  Driving it by retreat keeps the party
// alive (the wipe case is an ordinary game-over, where refusing movement is
// correct) and needs no enemy-AI beat to be deterministic.
// ===========================================================================
void red4_loss_then_next_room() {
    std::printf("RED-4  lost battle, keep moving, enter another room\n");
    Fixture f(3);
    // Harmless enemies: the party leaves by retreating, not by dying, exactly
    // like the Destard sequence that preceded the hardware freeze.
    for (auto &d : f.enemy_storage) { d.damage = 0; d.hp = 200; }
    // Put the entry-side start cells on the arena's south border so one step
    // south is a retreat.  (Authored .CBT boards place them further in; this is
    // fixture geometry, not a behaviour change.)
    for (int i = 0; i < 6; ++i) f.room_map.starts[1][i] = {int16_t(3 + i), 10};
    f.enter_deceit();
    f.face(1);
    f.advance(); f.advance();
    f.advance();                            // room combat on floor 0
    f.dump("first-room");
    expect(f.ctx().combat, "R4-0", "the first room really started combat");

    // Retreat south, through production's own service loop.  AlphaRuntime::
    // service_combat() paces enemy turns off the real wall clock
    // (kEnemyBeatUs = 400ms) and the host shim's esp_timer_get_time() is a real
    // monotonic clock, so the test must let that time actually pass -- a tight
    // key loop would only fill combat_input_queue_ and prove nothing.
    f.retreat(tdeck::RawInputKind::TrackballDown, 8.0);
    f.dump("after-loss");
    expect(!f.ctx().combat, "R4-1", "combat ownership was released after the battle");
    expect(f.mode() == UiMode::Dungeon, "R4-2", "the session is back in Dungeon mode");
    expect(f.d().active, "R4-3", "the dungeon session survived the encounter");
    expect(!f.cs().victory, "R4-4", "the encounter really ended as a loss/retreat");

    // Now walk on and enter ANOTHER room -- floor 1's room number 1, the same
    // 4-bit collision Destard hit.
    f.face(3);                              // west
    f.advance(); f.advance();               // back to the ladder-down at (2,1)
    f.key('k');                             // Klimb down to floor 1
    f.dump("after-klimb");
    expect(f.d().pos.floor == 1, "R4-5", "the party could still klimb after the loss");
    f.face(1);                              // east
    f.advance();                            // (3,1)
    f.advance();                            // (4,1) -- the floor-1 room
    f.dump("second-room");
    expect(f.d().pos.x == 4 && f.d().pos.y == 1 && f.d().pos.floor == 1, "R4-6",
           "the party reached the second room's cell -- movement never locked up");
    // Whatever that room resolved to, the runtime must still own a coherent state.
    expect(!f.ctx().combat || (f.cs().initialized && !f.cs().ended && f.cs().count > 0),
           "R4-7", "a second encounter, if armed, has a live roster");
    if (f.ctx().combat) {
        auto c = census(f, 30);
        expect(c.distinct >= 1, "R4-8", "the second room's scheduler can advance");
    } else {
        expect(true, "R4-8", "no second encounter armed (room 1 already cleared)");
    }
}

// ===========================================================================
// RED-6 / H-150 + H-152 -- Set Active Player inside a dungeon room.
//
// COMBAT:0x063E 0666-067f auto-passes every player whose turn comes up while
// g_active_char names a DIFFERENT, still-living member.  That is original
// behaviour (game/src/core/combat/combat.ts skipsForActiveChar), so the port
// must reproduce it -- but only while the player has actually chosen a member,
// and the player must be able to get back to party mode.
// ===========================================================================
void red6_active_character_in_room() {
    std::printf("RED-6  Set Active Player inside a room (H-150/H-152)\n");
    Fixture f;
    f.enter_deceit();
    f.key('2');                              // Set Active Player -> member 2 (index 1)
    expect(f.rt.game().party.active_character == 1, "R6-1",
           "the dungeon digit key really set the active character");
    f.face(1);
    f.advance(); f.advance(); f.advance();
    f.dump("in-room-with-active");
    expect(f.ctx().combat, "R6-2", "the room started combat");
    auto c = census(f, 40);
    std::printf("       distinct player actors scheduled=%d over %d observations\n",
                c.distinct, c.observations);
    expect(c.distinct == 1 && c.seen[1], "R6-3",
           "ONLY the chosen member acts -- the 1988 Set Active Player auto-pass");
    expect(f.ctx().combat, "R6-4",
           "the arena is still live (the auto-pass is not a stall that ends combat)");
}


// ===========================================================================
// RED-5 -- teardown invariants after every terminal path.
// ===========================================================================
void red5_teardown_invariants() {
    std::printf("RED-5  mutually exclusive states really are exclusive\n");
    Fixture f;
    f.enter_deceit();
    f.face(1);
    f.advance(); f.advance(); f.advance();
    f.dump("in-room");

    const bool combat_live = f.ctx().combat;
    expect(!combat_live || f.mode() == UiMode::Combat, "R5-1",
           "combat ownership implies Combat mode");
    expect(!combat_live || (f.cs().initialized && !f.cs().ended), "R5-2",
           "combat ownership implies a live, un-ended CombatState");
    expect(!f.ctx().combat || f.cs().count > 0, "R5-3",
           "a live arena has at least one actor");
    // Drain the arena through production's own service path, in real time so
    // the enemy beat actually fires (see Fixture::play_out).
    f.play_out(6.0);
    f.dump("drained");
    expect(!f.ctx().combat || f.cs().initialized, "R5-4",
           "context_.combat is never true over a torn-down CombatState");
    expect(f.ctx().combat || f.mode() != UiMode::Combat, "R5-5",
           "Combat mode is never held without combat ownership");
}

// ===========================================================================
// RED-7 / H-151 -- the total freeze.
//
// combat_growth_reserve() returns max(count,63)+1 as soon as ANY actor in the
// arena is an enemy with ability bit 0x1000 (divide-on-hit).  Every entry point
// into combat_action() first demands
//
//     actors.capacity() - count >= combat_growth_reserve(state)
//
// and the storage AlphaRuntime gives it is kCombatActors(22) + 32 overflow =
// 54 slots.  54 - count can never reach 64, so EVERY combat command -- the
// player's, and the runtime's own CombatEnemyStep beat -- is refused with
// NeedsActorStorage and does nothing at all.
//
// The two shipped definitions that carry 0x1000 are def 24 Slime and def 30
// Gargoyle (verified byte-for-byte in native/assets/openu5-alpha1-resources.bin).
//
// Downstream, that is the hardware freeze: current() keeps naming the same
// enemy, so AlphaRuntime::combat_ai_turn() stays true, so handle() routes every
// keypress into combat_input_queue_ (capacity 8) instead of the session -- which
// is why movement, Alt+M and the Mic key all stopped answering while a fully
// rendered arena stayed on screen.
// ===========================================================================
void red7_divider_arena_is_playable() {
    std::printf("RED-7  an arena containing a divide-on-hit enemy stays playable (H-151)\n");
    Fixture f;
    // The room's two units resolve to enemy definition 5; make it a divider,
    // exactly like the shipped Slime (0x1100) and Gargoyle (0x9000).
    for (auto &d : f.enemy_storage) d.abilities = 0x1000;
    f.enter_deceit();
    f.face(1);
    f.advance(); f.advance(); f.advance();
    f.dump("in-divider-room");
    expect(f.ctx().combat, "R7-1", "the room started combat");

    const auto *actor = current_combat_actor(
        const_cast<CombatContext &>(*f.ctx().combat_context));
    expect(actor != nullptr, "R7-2", "the scheduler names an actor");
    expect(combat_growth_reserve(f.cs()) <= f.cs().actors.capacity() - f.cs().count, "R7-3",
           "the per-action growth reserve is satisfiable with the storage the runtime owns");

    // A single production combat command must actually do something.
    const auto before_current = f.cs().current;
    const auto before_count = f.cs().count;
    Command pass;
    pass.kind = CommandKind::CombatPass;
    const auto status = dispatch_world_command(
        const_cast<CommandContext &>(f.ctx()), pass);
    std::printf("       CombatPass status=%d current=%ld->%ld count=%ld reserve=%ld capacity=%ld\n",
                int(status.status), long(before_current), long(f.cs().current),
                long(before_count), long(combat_growth_reserve(f.cs())),
                long(f.cs().actors.capacity()));
    expect(status.status != CommandStatus::NeedsStorage, "R7-4",
           "a combat command is not refused for storage before anything has divided");
    expect(f.cs().current != before_current || f.cs().ended, "R7-5",
           "the scheduler actually advanced -- the arena is not frozen");
}

// ===========================================================================
// RED-8 / H-149 -- the Deceit chest alcove.
//
// ADJUDICATION, not a bug fix.  Deceit's ONLY chest cell in all eight floors is
// floor index 7 (displayed L8) at (5,5), cell 0x41, and its four cardinal
// neighbours in DUNGEON.DAT are wall, wall, wall and the UNREVEALED secret door
// at (5,4) -- while Deceit floor 6 (5,5) is a pit trap (0x69), which is how a
// party arrives there without ever having revealed that door.  Every direction
// reporting `Blocked!` is therefore the authored, reference-faithful outcome;
// the way out is (S)earch, not movement.  (Verified against
// native/core/fixtures/dungeon-maps.txt, the same authored data dungeon_parity
// reads.)
//
// This case reproduces that geometry on the production path and pins BOTH
// halves: the four-way block is real, AND the authored escape still works.  A
// future change that "helpfully" opened the alcove, or that broke Search's
// reveal, would fail here.
// ===========================================================================
void red8_sealed_chest_alcove_is_authored() {
    std::printf("RED-8  the pit-fed chest alcove blocks all four ways and Search opens it (H-149)\n");
    Fixture f;
    auto cell = [&](int fl, int x, int y) -> uint8_t & {
        return f.dungeons[0].cells[fl * 64 + y * 8 + x];
    };
    // Deceit's real shape, reproduced: a corridor to a pit, and beneath it a
    // locked-chest cell whose only non-wall neighbour is a hidden door.
    cell(0, 1, 2) = 0x00;
    cell(0, 1, 3) = 0x00;
    cell(0, 2, 3) = 0x61;   // pit trap (type 6, sub 1), like Deceit f6 (5,5)
    cell(1, 2, 3) = 0x41;   // locked chest, like Deceit f7 (5,5)
    cell(1, 1, 3) = 0xd0;   // the hidden door, like Deceit f7 (5,4)

    f.enter_deceit();
    f.face(2);                              // south
    f.advance(); f.advance();               // (1,2) then (1,3)
    f.face(1);                              // east
    f.advance();                            // into the pit at (2,3) -> falls
    f.dump("landed");
    expect(f.d().pos.floor == 1 && f.d().pos.x == 2 && f.d().pos.y == 3, "R8-1",
           "the pit dropped the party onto the sealed chest cell one floor down");

    f.key('o');                             // (O)pen the chest
    f.key('g');                             // (G)et its contents
    expect(dungeon_cell(f.d(), 1, 2, 3) >> 4 == 0, "R8-2",
           "the chest was opened and looted, exactly as on hardware");

    // All four facings, advance from each.  None may move the party.
    const auto here = f.d().pos;
    int blocked = 0;
    for (int dir = 0; dir < 4; ++dir) {
        f.face(dir);
        f.advance();
        if (f.d().pos.x == here.x && f.d().pos.y == here.y && f.d().pos.floor == here.floor)
            ++blocked;
    }
    expect(blocked == 4, "R8-3",
           "all four directions are Blocked! -- the authored 1988 outcome, not a lock-up");

    // The authored way out: (S)earch ahead at the hidden door, then walk.
    f.face(3);                              // west, toward (1,3)
    f.key('s');                             // Search -- arms the "Dir-" prompt
    f.north();                              // trackball up = Ahead
    f.dump("after-search");
    f.advance();
    f.dump("after-escape");
    expect(f.d().pos.x == 1 && f.d().pos.y == 3 && f.d().pos.floor == 1, "R8-4",
           "(S)earch reveals the hidden door and the party walks out -- the alcove is escapable");
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--verbose")) g_verbose = true;
    std::printf("Batch 21A -- dungeon room-entry freeze / stuck-turn family\n\n");
    red1_room_number_collision();
    red3_cleared_room_message_only();
    red2_room_actor_availability();
    red4_loss_then_next_room();
    red6_active_character_in_room();
    red7_divider_arena_is_playable();
    red8_sealed_chest_alcove_is_authored();
    red5_teardown_invariants();
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
