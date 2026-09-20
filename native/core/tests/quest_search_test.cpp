// Batch 4.5C.1 -- RED tests for the basement search-object floor mismatch
// (GAMEPLAY_INTEGRATION_AUDIT.md R-33): SearchObject::floor is authored as
// the raw DATA.OVL byte (0..255); a small-map basement is encoded there as
// 0xFF, but every runtime small-map floor (GameState::position.map.floor)
// uses the signed convention, -1 for that same basement. search_at()
// compared the two directly, so Gorn's Blackthorn-jail brazier keys (and
// four other basement entries) could never be found. The world map
// (location 0) has its OWN, already-correct convention where floor 255
// genuinely means the Underworld and must NOT be decoded -- these tests
// also guard that the fix does not blindly touch every 255.
#include "openu5/quest_world.h"
#include "openu5/state.h"
#include "openu5/turn.h"
#include <iostream>

using namespace openu5;
namespace {
int checks = 0;
void check(bool ok, const char *what) {
    ++checks;
    if (!ok) { std::cerr << "quest_search check failed: " << what << "\n"; std::exit(1); }
}
GameState state_at(uint8_t location, int16_t floor, uint8_t x, uint8_t y) {
    GameState g;
    g.position.map.location = location;
    g.position.map.floor = floor;
    g.position.xy = {x, y};
    return g;
}
} // namespace

int main() {
    // --- decode_authored_floor(): the shared boundary itself. ---
    check(decode_authored_floor(18, 255) == -1, "decode: small-map 0xFF -> -1 (basement)");
    check(decode_authored_floor(18, 0) == 0, "decode: small-map floor 0 unchanged");
    check(decode_authored_floor(18, 2) == 2, "decode: small-map floor 2 unchanged");
    check(decode_authored_floor(0, 255) == 255, "decode: WORLD map 255 (Underworld) left untouched");
    check(decode_authored_floor(0, 0) == 0, "decode: WORLD map floor 0 unchanged");

    // --- T1: Gorn's brazier keys (Blackthorn jail, real table data). ---
    // Real data.json entry: index 13, {id:7, quality:9, location:18,
    // floor:255, x:8, y:6}. Index 13 is one of the three special-gated
    // slots (search_at hardcodes i==13's "0 keys and cell free" gate), so
    // it must sit at literal array index 13, matching production layout.
    {
        SearchObject table[16]{};
        table[13] = {7, 9, 18, 255, 8, 6};
        TurnState t;
        GameState g = state_at(18, -1, 8, 6); // the real runtime floor of the jail.
        g.keys = 0;
        const auto index = search_at(g, t, table, 16, 8, 6, /*occupied=*/false);
        check(index == 13, "T1: Gorn's brazier is found at the real runtime floor -1");
        check(table[index].id == 7 && table[index].quality == 9,
              "T1: the found entry is the correct one (id 7 / quality 9 keys, not merely 'something')");
    }
    // Pre-fix regression guard: the raw, unnormalized floor byte itself must
    // NOT match -- proves the comparison decodes rather than coincidentally
    // widening to accept both.
    {
        SearchObject table[16]{};
        table[13] = {7, 9, 18, 255, 8, 6};
        TurnState t;
        GameState g = state_at(18, 255, 8, 6); // nobody's real position is ever floor 255 here.
        g.keys = 0;
        const auto index = search_at(g, t, table, 16, 8, 6, false);
        check(index < 0, "T1b: raw floor 255 is not itself a valid query floor for a small map");
    }

    // --- T2: a second, independently-affected basement entry (Yew, a
    // normal once-only bitmask slot, not one of the three special gates). ---
    // Real data.json entry: index 46, {id:5, quality:37, location:4,
    // floor:255, x:27, y:22}.
    {
        SearchObject table[47]{};
        table[46] = {5, 37, 4, 255, 27, 22};
        TurnState t;
        GameState g = state_at(4, -1, 27, 22);
        const auto index = search_at(g, t, table, 47, 27, 22, false);
        check(index == 46, "T2: the Yew basement entry is found at its real runtime floor -1");
        check(table[index].id == 5 && table[index].quality == 37,
              "T2: the found entry is the correct one (id 5 / quality 37)");
        // Once-only: searching the same cell again must not re-find it.
        const auto again = search_at(g, t, table, 47, 27, 22, false);
        check(again < 0, "T2: the once-only bitmask still gates re-finding after the fix");
    }

    // --- T3: a normal, non-negative floor entry must be completely
    // unaffected (no regression for the overwhelming majority of the table
    // that never used the 0xFF encoding at all). ---
    {
        SearchObject table[1]{{31, 1, 4, 0, 13, 2}}; // Yew's real ground-floor entry.
        TurnState t;
        GameState g = state_at(4, 0, 13, 2);
        const auto index = search_at(g, t, table, 1, 13, 2, false);
        check(index == 0, "T3: an ordinary floor-0 entry is unaffected by the decode");
        check(table[index].id == 31, "T3: the found entry is the correct one (id 31)");
    }

    // --- T4: the WORLD map's own floor==255 convention (Underworld) must
    // keep matching literally -- decoding it too would silently break every
    // Underworld search object. ---
    {
        SearchObject table[1]{{11, 15, 0, 255, 233, 233}}; // real data.json index 0.
        TurnState t;
        GameState g = state_at(0, 255, 233, 233); // genuinely in the Underworld.
        const auto index = search_at(g, t, table, 1, 233, 233, false);
        check(index == 0, "T4: a WORLD-map (location 0) floor-255 Underworld entry still matches raw 255");
        // And it must NOT match at floor -1 -- floor 255 for location 0 is
        // never an alias for -1; that encoding only exists for small maps.
        GameState wrong = state_at(0, -1, 233, 233);
        const auto missed = search_at(wrong, t, table, 1, 233, 233, false);
        check(missed < 0, "T4b: an Underworld entry does not spuriously match floor -1");
    }

    std::cout << checks << " quest_search checks passed\n";
}
