// Batch 17 -- the quest_parity host crash.
// See native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md section 14 (Y-34).
//
// quest_parity died with STATUS_ACCESS_VIOLATION (0xC0000005, exit 3221225477)
// from Batch 8 to Batch 16 and was carried as "baseline noise". It was neither
// noise nor a production defect: 5377 of 5377 quest cases agree with the
// TypeScript reference. The crash was the HARNESS escape used to observe the
// two cases in which Faulinei's theft loop does not terminate.
//
// The contract this file pins, at the real seam openu5_test::observe_faulinei_theft
// (native/core/tests/quest_theft_watchdog.h) that quest_driver.cpp itself calls:
//
//   1. The original re-rolls rand(0,2) until it lands on a stocked category --
//      TALK.OVL 0x11e7/0x11fd/0x1209 jump BACKWARDS to 0x11c7
//      (re/notes/shadowlord-urbano-acta.md 2.3). That loop is unbounded on both
//      parity sides and MUST stay unbounded; only the observation is bounded.
//   2. When the draw budget is exhausted the watchdog must hand control back to
//      its CALLER and let it keep running. The old setjmp/longjmp escape could
//      not: on x86_64-w64-mingw32, setjmp() records __builtin_frame_address(0),
//      which is not the Win64 SEH establisher frame, so msvcrt!longjmp gave
//      ntdll!RtlUnwindEx a TargetFrame nothing matched and the unwinder walked
//      off the top of the thread stack.
//   3. A tripped watchdog must leave the party untouched and the RNG stream
//      exactly where the reference leaves it.
//   4. Terminating observations must be indistinguishable from an unwatched
//      call: same loot, same draw count, same mutation.
//
// Group C re-walks the same (here, mode, seed) axis check-quests.ts walks and
// pins the non-terminating set exactly, so neither "the loop now always
// terminates" nor "the loop now never terminates" can pass unnoticed.
#include "quest_theft_watchdog.h"

#include <iostream>
#include <string>
#include <vector>

using namespace openu5;
using openu5_test::kTheftDrawBudget;
using openu5_test::observe_faulinei_theft;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string &what) {
    ++checks;
    std::cerr << (ok ? "[PASS]" : "[FAIL]") << " batch17 check " << checks << ": " << what << "\n";
    if (!ok) ++failures;
}

// The nine stocked-inventory shapes check-quests.ts sweeps for `theft`:
//   mode 0-2 keys/gems/torches = 2, mode 3-5 equipment[47]/potion[7]/scroll[7] = 1,
//   mode 6-8 gold = 0/5/100. Everything else matches the fixture's `initial()`.
GameState theft_state(int mode, int32_t seed) {
    GameState g{};
    g.gold = 500;
    g.rng.seed(seed);
    if (mode == 0) g.keys = 2;
    else if (mode == 1) g.gems = 2;
    else if (mode == 2) g.torches = 2;
    else if (mode == 3) g.equipment_quantities[47] = 1;
    else if (mode == 4) g.potion_quantities[7] = 1;
    else if (mode == 5) g.scroll_quantities[7] = 1;
    else g.gold = uint16_t(mode == 6 ? 0 : mode == 7 ? 5 : 100);
    return g;
}

// ---------------------------------------------------------------------------
// Group A -- the seam returns to its caller. This is the Batch 17 defect.
// ---------------------------------------------------------------------------
void group_a() {
    // A canary in THIS frame. The pre-Batch-17 longjmp escape destroyed the
    // process while unwinding towards this frame; anything that reads correctly
    // after the two observations below proves control really came back here.
    const std::vector<int> canary{11, 22, 33};
    const std::string label = "batch17-caller-frame";

    // check-quests.ts input line 3452: op=theft, here=0, seed=20, keys=2.
    // Reference result: {nonterminating:true, seed:20}, inventory untouched.
    GameState keys_only = theft_state(0, 20);
    const auto a = observe_faulinei_theft(keys_only, 0);
    check(a.nonterminating, "A1 keys-only/seed 20/here 0 trips the watchdog (reference line 3452)");
    check(a.draws == kTheftDrawBudget + 1,
          "A1 the tripping draw is the 65537th, so exactly 65536 reach the RNG");
    check(a.result.kind == TheftKind::None && a.result.amount == 0,
          "A1 a tripped watchdog yields no loot");
    check(keys_only.keys == 2 && keys_only.gems == 0 && keys_only.torches == 0,
          "A2 a tripped watchdog leaves the stocked categories untouched");
    check(keys_only.gold == 500, "A2 and never falls through to the gold branch");
    check(keys_only.rng.get_seed() == 20,
          "A3 the RNG orbit from seed 20 closes after 65536 draws, as the reference records");

    // check-quests.ts input line 3484: the same seed with gems instead of keys.
    GameState gems_only = theft_state(1, 20);
    const auto b = observe_faulinei_theft(gems_only, 0);
    check(b.nonterminating, "A4 gems-only/seed 20/here 0 trips the watchdog (reference line 3484)");
    check(gems_only.gems == 2 && gems_only.rng.get_seed() == 20,
          "A4 with the same untouched state and closed orbit");

    // A5 is the whole point: a second observation ran AFTER the first tripped,
    // and this frame's locals survived both.
    check(canary.size() == 3 && canary[0] == 11 && canary[2] == 33,
          "A5 the caller frame survives a tripped watchdog (vector canary intact)");
    check(label == "batch17-caller-frame",
          "A5 the caller frame survives a tripped watchdog (string canary intact)");
}

// ---------------------------------------------------------------------------
// Group B -- the watchdog does not perturb anything that terminates.
// ---------------------------------------------------------------------------
void group_b() {
    // Seed 1 lands on keys quickly; the loop is rejection sampling, so the only
    // thing asserted about the count is that it is bounded well under the budget.
    GameState g = theft_state(0, 1);
    const auto keys = observe_faulinei_theft(g, 0);
    check(!keys.nonterminating && keys.result.kind == TheftKind::Keys,
          "B1 a terminating draw still reports its loot");
    check(g.keys == 1, "B1 and applies the -1 with a floor of 0 (CS 0x3f36)");
    check(keys.draws >= 1 && keys.draws < kTheftDrawBudget,
          "B1 inside the budget, so the watchdog never entered the picture");

    // 0x1187: the gate is a VALUE test on the placement flag. here != 0 does nothing.
    for (int32_t here : {-1, 1, 2}) {
        GameState gated = theft_state(0, 20);
        const auto none = observe_faulinei_theft(gated, here);
        check(!none.nonterminating && none.result.kind == TheftKind::None && none.draws == 0,
              "B2 here=" + std::to_string(here) + " closes the gate without a single draw");
        check(gated.keys == 2 && gated.rng.get_seed() == 20,
              "B2 here=" + std::to_string(here) + " leaves state and RNG stream untouched");
    }

    // 0x1210/0x1232/0x124a: the long cascade is a linear sweep, no randomness.
    const struct { int mode; TheftKind kind; int32_t index; } cascade[] = {
        {3, TheftKind::Equipment, 47}, {4, TheftKind::Potion, 7}, {5, TheftKind::Scroll, 7}};
    for (const auto &c : cascade) {
        GameState s = theft_state(c.mode, 20);
        const auto got = observe_faulinei_theft(s, 0);
        check(!got.nonterminating && got.result.kind == c.kind && got.result.index == c.index,
              "B3 mode " + std::to_string(c.mode) + " takes the highest non-empty slot");
        check(got.draws == 0 && s.rng.get_seed() == 20,
              "B3 mode " + std::to_string(c.mode) + " consumes no RNG draw");
    }

    // 0x1262: gold is one rand(1,15) with a floor of 0.
    GameState broke = theft_state(6, 20);
    const auto gold = observe_faulinei_theft(broke, 0);
    check(!gold.nonterminating && gold.result.kind == TheftKind::Gold && gold.draws == 1,
          "B4 an empty party falls through to gold on exactly one draw");
    check(broke.gold == 0, "B4 with the subtraction clamped at 0 (kernel 0x3f54)");

    GameState rich = theft_state(8, 20);
    const auto taken = observe_faulinei_theft(rich, 0);
    check(taken.result.amount >= 1 && taken.result.amount <= 15 &&
              rich.gold == 100 - taken.result.amount,
          "B4 and rand(1,15) coins otherwise");
}

// ---------------------------------------------------------------------------
// Group C -- the non-terminating set is exactly what the reference records.
// check-quests.ts sweeps here in {-1,0,1,2} x mode in 0..8 x seed in 0..31,
// 1152 cases, and its coverage report states nonterminatingObservations: 2.
// ---------------------------------------------------------------------------
void group_c() {
    std::vector<std::string> tripped;
    int cases = 0;
    for (int32_t here : {-1, 0, 1, 2})
        for (int mode = 0; mode < 9; ++mode)
            for (int32_t seed = 0; seed < 32; ++seed) {
                GameState g = theft_state(mode, seed);
                const auto got = observe_faulinei_theft(g, here);
                ++cases;
                if (got.nonterminating)
                    tripped.push_back("here=" + std::to_string(here) + " mode=" +
                                      std::to_string(mode) + " seed=" + std::to_string(seed));
            }
    check(cases == 1152, "C1 the sweep is the fixture's own 4 x 9 x 32 theft axis");
    const std::vector<std::string> expected{"here=0 mode=0 seed=20", "here=0 mode=1 seed=20"};
    if (tripped != expected)
        for (const auto &t : tripped) std::cerr << "       tripped: " << t << "\n";
    check(tripped == expected,
          "C1 exactly two combinations do not terminate, and they are the reference's two");
}

// ---------------------------------------------------------------------------
// Group D -- the escape must UNWIND, not jump.
//
// This is the Batch 17 defect stated as a contract instead of as a stack trace.
// [csetjmp.syn]/2 makes longjmp undefined whenever replacing it with throw/catch
// would run a non-trivial destructor, and on x86_64-w64-mingw32 that undefined
// behaviour is not benign: msvcrt!longjmp asks ntdll!RtlUnwindEx to unwind to
// the frame setjmp() recorded, GCC records __builtin_frame_address(0) rather
// than the Win64 SEH establisher frame, and for quest_driver.cpp main() the two
// differ by main's ~39 KB frame, so the unwinder walks off the top of the
// thread stack (STATUS_ACCESS_VIOLATION 0xC0000005 / exit 3221225477).
//
// observe_faulinei_theft() puts a scope guard in the callback frame on the
// tripping path. A real unwind destroys it; a longjmp escape jumps straight
// past it. Asserting the guard ran is therefore a direct, portable check that
// the watchdog leaves the callback by unwinding -- it turns RED the moment
// anyone reaches for setjmp/longjmp here again, without waiting for the
// 5377-case parity replay to crash.
// ---------------------------------------------------------------------------
void group_d() {
    GameState keys_only = theft_state(0, 20);
    const auto a = observe_faulinei_theft(keys_only, 0);
    check(a.nonterminating && a.unwound,
          "D1 the tripped watchdog unwinds the callback frame instead of jumping out of it");

    GameState gems_only = theft_state(1, 20);
    const auto b = observe_faulinei_theft(gems_only, 0);
    check(b.nonterminating && b.unwound,
          "D1 on the second non-terminating combination too");

    // The guard must not fire when nothing trips: it marks the escape, not the call.
    GameState terminates = theft_state(0, 1);
    const auto c = observe_faulinei_theft(terminates, 0);
    check(!c.nonterminating && !c.unwound,
          "D2 and never fires on a draw that terminates normally");
}
} // namespace

int main() {
    group_a();
    group_b();
    group_c();
    group_d();
    std::cout << "batch17 theft watchdog checks=" << checks << " failures=" << failures << "\n";
    return failures ? 1 : 0;
}
