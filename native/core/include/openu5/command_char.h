#pragma once
#include "openu5/state.h"

#include <cstdint>

namespace openu5 {

// The command-character picker, kernel ULTIMA.EXE 0x4988 (body 0x4988-0x4a83,
// `ret` with no arguments).  ONE routine in the binary, so ONE seam here.
//
// Authority: re/notes/resolve-command-char-178c-acta.md (whole body read, plus
// its sibling select_party_member 0x2d7a and the 0x2e8e wrapper) and the
// caller-side derivations in re/notes/bola-144-acta.md section 2.2,
// re/notes/cast-input.md section 9 and re/notes/cmds.md section 11.  The
// TypeScript game/src/ui/pickers.ts::pickCommandChar is secondary
// corroboration only; it cites the same offsets.
//
// The call-site census (re/notes/resolve-command-char-178c-acta.md section 6.2,
// near-calls resolved over all 24 overlays with a positive and a negative
// control) is ELEVEN sites in FOUR overlays:
//   * LOOKOBJ.OVL:0x09ea  -- (L)ook at a crystal ball, tile 0x29
//   * SJOG.OVL x8         -- (S)earch, e.g. cmd_search 0x095c -> 0x09a0
//   * CAST.OVL:0x0dd5     -- (C)ast, before the "Spell name:" prompt
//   * DNGLOOK.OVL:0x0007  -- dungeon (L)ook
//
// FOUR branches, in the binary's own order:
//
//   1. @0x4995  `cmp byte [g_location], 0x80` / `jbe 0x49b2`
//      COMBAT: with g_location > 0x80 the member is the combat actor's roster
//      slot, read as field +3 of g_combat_actor_records[g_cmb_actor].  NOT
//      modelled here, deliberately.  Section 6.2 of the acta proves the only
//      reachable path into it is the combat (C)ast re-entry
//      (COMBAT.OVL:0x08f0 -> CAST.OVL:0x0dba -> 0x0dd5), where native already
//      takes the turn's actor directly (AlphaRuntime's "castingCombatPlayer"
//      arm reads current_combat_actor()->member).  The dungeon CORRIDOR does
//      NOT take this branch: g_location there is 0x21..0x28, not 0xFF -- the
//      acta closes that with an independent control (the DATA.OVL 0x1C90
//      window table gives Uus Por and Des Por the dungeon bit and nothing
//      else, so a corridor value above 0x7f would make them uncastable).
//      Modelling branch 1 would mean feeding this seam a combat context it has
//      no reason to own, to reproduce behaviour native already has.
//
//   2. @0x49b2  `cmp byte [g_active_char], 0xFF`
//      An active character is returned DIRECTLY, with no prompt -- and with no
//      eligibility test: the compare is against 0xFF and nothing else, so a
//      disabled active character is still what the command gets.
//
//   3. @0x49dc / @0x49fa
//      Otherwise the roster is swept for members whose status byte
//      ([0x55b3 + n*0x20]) is 'G' (0x47) or 'P' (0x50).  `cmp [bp-6],1` /
//      `jle 0x4a5f` means ONE eligible member is auto-selected without asking,
//      and ZERO eligible members fall through to the common epilogue with
//      [bp-8] still holding the 0xFFFF that @0x4990 put there.
//
//   4. @0x4a02
//      With two or more eligible the routine prints DS 0xa3c4 "Player: ",
//      opens the roster select (0x2e8e -> 0x2d7a) and echoes the chosen name
//      on the same row (@0x4a30-0x4a3a).  A pick whose status is neither 'G'
//      nor 'P' prints DS 0xa3ce "Disabled!" and RE-ASKS (@0x4a4e, then
//      @0x4a55/@0x4a57 jump back to the prompt because `di` is still 0).
//      Leaving with -1 -- cancel, or the zero-eligible fall-through of branch
//      3 -- prints DS 0xa3da "None!" from the common epilogue at @0x4a5f.
//
// Two documented pieces of 0x4988 are deliberately NOT modelled beyond branch
// 1, both adjudicated in the acta:
//   * the `-2` handling at @0x4a6e is dead code for this caller (the 0x2e8e
//     wrapper forces select_party_member's argument to 0, and -2 is only
//     produced with a non-zero argument) -- acta section 3;
//   * the 0x80-vs-0x7f threshold asymmetry against select_party_member is real
//     in the bytes but has no reachable discriminating value measured -- acta
//     section 2.  It only matters inside branch 1.

// @0x49dc: the census predicate.  'G' = 0x47 (Good), 'P' = 0x50 (Poisoned).
bool command_char_eligible(char status);

enum class CommandCharOutcome : uint8_t {
    // Branch 2 or the auto-single of branch 3: the caller has its member and
    // must NOT prompt.
    Resolved,
    // Branch 4: two or more eligible.  The caller raises its own prompt event
    // and comes back through the picker's answer path.
    Prompt,
    // Branch 3 with zero eligible.  The caller prints command_char_none() and
    // performs no effect -- LOOKOBJ 0x09f0's `inc ax / jne` proves the -1 skips
    // the whole tail of the command, roll included.
    None,
};

struct CommandCharResult {
    CommandCharOutcome outcome = CommandCharOutcome::None;
    int32_t member = -1; // Valid only for Resolved.
};

// Branches 2, 3 and 4.  The roster bound is party_size clamped to
// character_count, matching the rows AlphaRuntime::open_selection() builds for
// UiMode::PartySelection and the partySize the TypeScript picker sweeps.
CommandCharResult resolve_command_char(const PartyState &);

// @0x4a2e/@0x4a4e: the post-pick gate of branch 4.  False means the caller owes
// command_char_disabled() and a re-ask, not a rejection and not an effect.
bool command_char_accepts(const PartyState &, int32_t member);

// The three DATA.OVL literals, read with fileoff = DS + 0x10.  The trailing
// newlines are pruned at the call site the same way every other native message
// string is (the residual declared in bola-144-acta.md section 3.2), so these
// match the "None!" that fountain_drink_result() already returns.
const char *command_char_prompt();   // DS 0xa3c4 b'Player: \x00'
const char *command_char_disabled(); // DS 0xa3ce b'Disabled!\n\n\x00'
const char *command_char_none();     // DS 0xa3da b'None!\n\x00'

} // namespace openu5
