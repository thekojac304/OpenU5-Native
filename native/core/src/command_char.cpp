#include "openu5/command_char.h"

#include <algorithm>

namespace openu5 {
namespace {
// The roster the census at @0x49dc sweeps.  party_size is the authoritative
// bound (it is what UiMode::PartySelection lists and what the TypeScript
// picker uses); character_count only clamps it to the records that exist.
int32_t roster_bound(const PartyState &p) {
    return std::max<int32_t>(0, std::min<int32_t>(p.party_size, int32_t(p.character_count)));
}
} // namespace

bool command_char_eligible(char status) {
    return status == 'G' || status == 'P'; // 0x47 / 0x50
}

CommandCharResult resolve_command_char(const PartyState &party) {
    const int32_t bound = roster_bound(party);

    // Branch 2 (@0x49b2).  Compared against 0xFF ONLY: no status test here, so
    // an active character who is dead or asleep is still handed straight back.
    // The range test is native's own guard against a roster that shrank under
    // a stale index (the reference keeps g_active_char in step by deselecting
    // on death at 0x2a8c/0x2b14 and on New Order at 0x2b1b).
    if (party.active_character != 0xff && int32_t(party.active_character) < bound)
        return {CommandCharOutcome::Resolved, int32_t(party.active_character)};

    // Branch 3 (@0x49dc-@0x49f2): count the eligible, remembering the last one
    // seen.  `di` holds that index, which is why a single eligible member can
    // be returned at @0x49fa without a second pass.
    int32_t eligible = 0, last = -1;
    for (int32_t i = 0; i < bound; ++i) {
        if (!command_char_eligible(party.characters[i].status)) continue;
        ++eligible;
        last = i;
    }

    // @0x49fa `cmp [bp-6],1` / `jle 0x4a5f`: one auto-selects, zero falls
    // through to the epilogue still carrying -1.
    if (eligible == 1) return {CommandCharOutcome::Resolved, last};
    if (eligible == 0) return {CommandCharOutcome::None, -1};
    return {CommandCharOutcome::Prompt, -1}; // Branch 4 (@0x4a02).
}

bool command_char_accepts(const PartyState &party, int32_t member) {
    if (member < 0 || member >= roster_bound(party)) return false;
    return command_char_eligible(party.characters[member].status);
}

const char *command_char_prompt() { return "Player: "; }
const char *command_char_disabled() { return "Disabled!"; }
const char *command_char_none() { return "None!"; }

} // namespace openu5
