// Batch 21A.3 -- Set Active Player REACHABILITY during combat.
//
// Hardware observation (physical T-Deck, Batch 20/21A device passes): with Set
// Active Player engaged BEFORE a fight, only that member is ever prompted for a
// manual turn -- which is the `active_character` scheduler rule working exactly
// as specified -- but the player has no way to clear or change the selection
// once the arena is up, so a forgotten selection locks the whole battle to one
// character.
//
// REFERENCE ADJUDICATION (re/notes/combat-commands.md + the derivation in
// re/notes/combate-hotfix-20260722.md #3, corroborated by the TypeScript port's
// Game.combatActivePlayer / Combat.playerYieldTurn):
//
//   COMBAT.OVL's player-turn dispatcher (COMBAT:0x063E) reads its key at 0x0838
//   and DOES dispatch digits:
//     '0'      @0x09ec          -> g_active_char = 0xFF, "Set active plr:\nNone!\n"
//                                  (DS 0x6e66); falls to the tail @0x0b56 with
//                                  [bp-2]=0 and, because the key is in '0'..'6'
//                                  (@0x0b79-0x0b83), the turn function RETURNS --
//                                  the actor in hand CEDES its turn with no action
//                                  and WITHOUT the end-of-action housekeeping
//                                  (SJOG 0x2012 spell-turn decay).
//     '1'-'6'  @0x0aaa-0x0ab4   -> 0x09fe `call 0xffffdab6` = stub 0x7d46 =
//                                  SJOG.OVL 0x1F7A.  Prints "Set active plr:\n"
//                                  (DS 0x8f3a) ALWAYS, then sweeps the 32 arena
//                                  slots for a party slot (flags&0x80) with that
//                                  charIdx; flags&0x2c (asleep/gone) => invalid.
//                                  VALID   -> g_active_char = idx, prints the NAME
//                                             (COMSUBS 0x0094), ret 1 -> same tail
//                                             => the turn is CEDED.
//                                  INVALID -> prints "Invalid!\n" (DS 0x8f4c),
//                                             ret 0 -> COMBAT @0x0a11->0x0a41 sets
//                                             [bp-2]=1 and jumps to 0x06F1, which
//                                             re-prompts the SAME actor -- NO turn
//                                             is spent.
//     '7'-'9'  default @0x0ab7  -> "What?\n" (DS 0x6ee6), re-prompt without a
//                                  banner, no turn.
//
//   So the answer to "should Set Active Player be changeable or clearable while
//   combat is active?" is YES, for both, with an exact action cost: a successful
//   change or clear COSTS the current actor's turn; a rejected one costs nothing.
//
// Everything below drives the REAL production routing -- tdeck::UiInputAdapter ->
// UiSession::handle_combat -> dispatch -> execute_command -> combat_action -- never
// a direct core call standing in for the input path, because reachability is
// precisely the question under test.
#include "../../targets/tdeck/main/ui_input_adapter.h"
#include "openu5/combat.h"
#include "openu5/commands.h"
#include "openu5/ui_session.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

using namespace openu5;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *what) {
    ++checks;
    std::cerr << (ok ? "[PASS] " : "[FAIL] ") << "batch21a3 check " << checks << ": " << what
              << "\n";
    if (!ok) ++failures;
}

UiAction ch(char c) {
    UiAction a;
    a.kind = UiActionKind::Character;
    a.character = char16_t(c);
    return a;
}

CombatEnemy make_foe() {
    CombatEnemy e;
    e.name = "Orc";
    e.index = 1;
    e.tile = 64;
    e.hp = 50;
    e.dexterity = 1;
    return e;
}
const CombatEnemy kFoe = make_foe();

// A three-member party in a live two-sided arena, wired so that a UiIntent
// leaving UiSession is executed by the REAL execute_command() and its events are
// fed back into the same session's transcript -- the production loop
// AlphaRuntime runs, minus the board driver.
struct Fixture {
    GameState game{};
    TurnState turn{};
    CombatState battle{};
    TravelState travel{};
    CommandState commands{};
    std::vector<uint8_t> tiles;
    WorldData world{};
    CombatContext arena;
    CommandContext context;
    UiTextBlock blocks[32]{};
    UiSession ui;
    std::vector<Command> dispatched;
    std::vector<CommandStatus> statuses;

    explicit Fixture(int members = 3)
        : tiles(256 * 256, 5), world{tiles.data(), tiles.data(), tiles.size(), tiles.size()},
          arena{game, turn, battle}, context{game, turn, travel, commands, world},
          ui{{blocks, 32}, {this, &Fixture::send}} {
        game.party.party_size = game.party.character_count = int8_t(members);
        static const char *const kNames[] = {"Avatar", "Shamino", "Iolo", "Dupre"};
        for (int i = 0; i < members; ++i) {
            auto &m = game.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "%s", kNames[i]);
            m.party_status = 0;
            m.status = 'G';
            m.current_hp = m.max_hp = 100;
            m.dexterity = 20;
        }
        battle.initialized = true;
        battle.count = int16_t(members + 1);
        for (int i = 0; i < members; ++i) {
            auto &a = battle.actors[i];
            a.id = int16_t(i + 1);
            a.member = uint8_t(i);
            a.status = CombatStatus::Active;
            a.position = {int8_t(3 + i), 5};
            a.counter = 1; // everyone due, so current() always has to scan
            a.speed = 12;
            a.hp = a.max_hp = 100;
        }
        auto &foe = battle.actors[members];
        foe.id = int16_t(members + 1);
        foe.member = 255;
        foe.enemy = &kFoe;
        foe.status = CombatStatus::Active;
        foe.position = {9, 5};
        foe.counter = 200; // far from its turn; never steals the scan
        foe.speed = 1;
        foe.hp = foe.max_hp = 50;
        battle.current = -1;
        context.combat = true;
        context.combat_context = &arena;
        context.events = ui.event_sink();
        GameEvent started{};
        started.kind = GameEventKind::CombatStarted;
        ui.consume(started);
    }

    static void send(void *p, const UiIntent &i) {
        auto &f = *static_cast<Fixture *>(p);
        if (i.kind != UiIntentKind::Command) return;
        f.dispatched.push_back(i.command);
        f.statuses.push_back(execute_command(f.context, i.command).status);
    }

    // The member index the arena would prompt next, or -1.  NOTE: this runs the
    // REAL scheduler, so it consumes counters and bumps action_count -- prime it
    // BEFORE capturing any cost baseline.
    int scheduled_member() {
        auto *a = current_combat_actor(arena);
        return a ? int(a->member) : -1;
    }
    int current_slot() const { return int(battle.current); }
    void yield_turn() {
        Command yield{};
        yield.kind = CommandKind::CombatYield;
        execute_command(context, yield);
    }
    bool transcript_has(const char *needle) const {
        for (size_t i = 0; i < ui.transcript_size(); ++i) {
            const auto *b = ui.transcript_at(i);
            if (b && b->text && std::strstr(b->text, needle)) return true;
        }
        return false;
    }
};

// ---------------------------------------------------------------------------
// C1 -- CONTROL (GREEN before and after).  The pre-combat active member is
// inherited by the arena and is the ONLY member the scheduler ever offers a
// manual turn.  This is the rule the hardware report describes, and it is
// correct: COMBAT @0x0666-0x067f auto-passes every other party actor.  Nothing
// in this batch may weaken it.
void c1_pre_combat_active_member_is_the_only_one_scheduled() {
    Fixture f;
    f.game.party.active_character = 1; // engaged BEFORE the fight
    bool only_member_one = true;
    for (int round = 0; round < 6; ++round) {
        if (f.scheduled_member() != 1) only_member_one = false;
        f.yield_turn();
    }
    check(only_member_one,
          "C1: with active_character set before combat, only that member is scheduled");
    check(f.game.party.active_character == 1,
          "C1: combat inherits the pre-combat selection and does not silently reset it");
}

// ---------------------------------------------------------------------------
// C2 -- a digit pressed during combat must REACH SetActivePlayer.
// Reference: COMBAT @0x0aaa-0x0ab4 dispatches '1'-'6' to SJOG 0x1F7A.
void c2_digit_during_combat_reaches_set_active_player() {
    Fixture f;
    check(f.ui.mode() == UiMode::Combat, "C2: fixture is in combat");
    f.ui.handle_input(ch('2'));
    check(!f.dispatched.empty() && f.dispatched.back().kind == CommandKind::SetActivePlayer &&
              f.dispatched.back().member == 2,
          "C2: digit '2' in combat dispatches SetActivePlayer(member=2), not the unknown-key "
          "default");
    check(!f.transcript_has("What?"),
          "C2: a digit in combat is a command, not \"What?\"");
}

// ---------------------------------------------------------------------------
// C3 -- '0' CLEARS the selection during combat.
// Reference: COMBAT @0x09ec writes g_active_char = 0xFF and prints
// "Set active plr:\nNone!\n" (DS 0x6e66).
void c3_zero_clears_active_player_during_combat() {
    Fixture f;
    f.game.party.active_character = 1;
    f.ui.handle_input(ch('0'));
    check(f.game.party.active_character == 255,
          "C3: '0' during combat clears active_character to 0xFF (COMBAT @0x09ec)");
    check(f.transcript_has("Set active plr:"),
          "C3: the combat echo is COMBAT.OVL's own \"Set active plr:\" (DS 0x6e66/0x8f3a), not "
          "the kernel's \"Set Active Plr:\" (DS 0xa396)");
    check(f.transcript_has("None!"), "C3: clearing prints \"None!\"");
    // Immediately effective on scheduling: with no selection every live party
    // member is offered a turn again.
    bool saw_other = false;
    for (int round = 0; round < 6 && !saw_other; ++round) {
        if (f.scheduled_member() != 1) saw_other = true;
        f.yield_turn();
    }
    check(saw_other, "C3: clearing applies IMMEDIATELY to actor scheduling -- the rest of the "
                     "party is prompted again in the same battle");
}

// ---------------------------------------------------------------------------
// C4 -- a digit CHANGES the selection from one member to another mid-combat.
void c4_digit_changes_active_member_during_combat() {
    Fixture f;
    f.game.party.active_character = 0;
    f.ui.handle_input(ch('2')); // member index 1
    check(f.game.party.active_character == 1,
          "C4: '2' during combat re-points active_character at member index 1 (SJOG @0x1fdc)");
    check(f.transcript_has("Shamino"),
          "C4: a valid combat selection prints the chosen member's NAME (COMSUBS 0x0094)");
    bool only_member_one = true;
    for (int round = 0; round < 6; ++round) {
        if (f.scheduled_member() != 1) only_member_one = false;
        f.yield_turn();
    }
    check(only_member_one, "C4: after the change, only the NEW member is manually scheduled");
}

// ---------------------------------------------------------------------------
// C5 -- ACTION COST, pinned exactly.
//   valid '1'-'6' and '0' => the actor in hand CEDES its turn (tail @0x0b79-0x0b83).
//   invalid '1'-'6'       => "Invalid!" and a re-prompt of the SAME actor, no turn.
//   '7'-'9'               => "What?", no turn, no banner (default @0x0ab7).
void c5_action_cost_is_pinned() {
    { // valid selection costs the turn
        Fixture f;
        const int before_member = f.scheduled_member(); // prime: member 0 is in hand
        const int before_slot = f.current_slot();
        f.ui.handle_input(ch('2'));
        check(f.statuses.size() == 1 && f.statuses.back() == CommandStatus::Success,
              "C5a: a valid combat Set Active Player succeeds through execute_command()");
        check(before_member == 0 && f.current_slot() != before_slot &&
                  f.scheduled_member() == 1,
              "C5a: a valid selection CEDES the current actor's turn (COMBAT tail @0x0b79) and "
              "the arena moves on to the newly selected member");
    }
    { // '0' costs the turn too
        Fixture f;
        f.game.party.active_character = 2;
        const int before_member = f.scheduled_member(); // prime: member 2 is in hand
        const int before_slot = f.current_slot();
        f.ui.handle_input(ch('0'));
        check(before_member == 2 && f.current_slot() != before_slot,
              "C5b: '0' also cedes the turn -- it falls through the SAME tail as '1'-'6'");
    }
    { // invalid selection is free
        Fixture f;
        f.game.party.characters[2].status = 'D'; // dead => flags&0x2c => invalid
        f.battle.actors[2].status = CombatStatus::Dead;
        f.scheduled_member(); // prime BEFORE the cost baseline
        const int before_slot = f.current_slot();
        const auto actions_before = f.battle.action_count;
        f.ui.handle_input(ch('3'));
        check(f.transcript_has("Invalid!"),
              "C5c: an arena-invalid member prints \"Invalid!\" (DS 0x8f4c)");
        check(f.game.party.active_character == 255,
              "C5c: a rejected selection does not write g_active_char");
        check(f.battle.action_count == actions_before && f.current_slot() == before_slot,
              "C5c: a REJECTED selection costs NO turn -- COMBAT @0x0a41 re-prompts the SAME "
              "actor (jump to 0x06F1)");
    }
    { // '7'-'9' stay the unknown-key default
        Fixture f;
        const auto actions_before = f.battle.action_count;
        f.ui.handle_input(ch('8'));
        check(f.dispatched.empty(), "C5d: '8' in combat constructs no command (default @0x0ab7)");
        check(f.transcript_has("What?") && f.battle.action_count == actions_before,
              "C5d: '7'-'9' print \"What?\" (DS 0x6ee6) and cost no turn");
    }
}

// ---------------------------------------------------------------------------
// C6 -- the T-Deck literal zero.  The handheld has no digit row: '0' is the
// hardware symbol-layer chord Symbol+Mic (kSymbol[0][6]).  Y-29 already made
// that chord bypass the Mic Cancel/Movement-Mode state machine, but in Combat
// UiActionKind::Cancel is CombatEscapeQuick, so if the chord ever regressed to
// the plain Mic path the player would try to clear the selection and FLEE.
// This drives the real adapter with the real chord and the real combat session.
void c6_tdeck_symbol_mic_zero_reaches_combat_set_active_player() {
    tdeck::RawInputEvent raw{};
    raw.kind = tdeck::RawInputKind::Keyboard;
    raw.code = '0';
    raw.transition = tdeck::KeyTransition::Pressed;
    raw.modifiers.symbol = true;
    raw.column = tdeck::kMicrophoneKeyColumn;
    raw.row = tdeck::kMicrophoneKeyRow;
    raw.timestamp_us = 1000;

    tdeck::UiInputAdapter adapter;
    UiAction action{};
    tdeck::DeviceShortcut shortcut{};
    const bool translated = adapter.translate(raw, UiMode::Combat, action, shortcut, false);
    check(translated && action.kind == UiActionKind::Character && action.character == u'0',
          "C6: Symbol+Mic in COMBAT still yields a literal '0' character, not Cancel/Escape");
    check(shortcut == tdeck::DeviceShortcut::None,
          "C6: the chord fires no device shortcut while combat is active");

    Fixture f;
    f.game.party.active_character = 1;
    f.ui.handle_input(action);
    check(!f.dispatched.empty() && f.dispatched.back().kind == CommandKind::SetActivePlayer &&
              f.dispatched.back().member == 0 && f.game.party.active_character == 255,
          "C6: the adapter's literal '0' travels the ORDINARY digit route and clears the "
          "selection -- no T-Deck-only command is involved");
}
} // namespace

int main() {
    c1_pre_combat_active_member_is_the_only_one_scheduled();
    c2_digit_during_combat_reaches_set_active_player();
    c3_zero_clears_active_player_during_combat();
    c4_digit_changes_active_member_during_combat();
    c5_action_cost_is_pinned();
    c6_tdeck_symbol_mic_zero_reaches_combat_set_active_player();
    std::cout << "batch21a3 combat active player: " << checks << " checks, " << failures
              << " failures\n";
    return failures ? 1 : 0;
}
