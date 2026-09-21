#pragma once
// Batch 17 -- the host-side draw-budget watchdog for apply_faulinei_theft().
//
// TALK.OVL 0x11c7 re-rolls rand(0,2) until it lands on a stocked category: the
// three `je` at 0x11e7/0x11fd/0x1209 jump BACKWARDS to 0x11c7
// (re/notes/shadowlord-urbano-acta.md 2.3, table rows 0x11e7/0x11f8/0x1204).
// The original bounds nothing, and neither do the port's two clones. Against
// the port's own deterministic OriginalRng stream -- the srand(clock) of
// 0x11AB is a declared parity ceiling, re/notes/rng.md "Techos de paridad" --
// some seeds orbit a cycle that never yields the stocked index, so the loop
// genuinely does not terminate. Both parity sides therefore OBSERVE the
// non-termination under a draw budget instead of hanging. The TypeScript side
// is native/core/tools/check-quests.ts `case 'theft'`, which throws after the
// same 65536 draws; this is the native counterpart, shared by quest_driver.cpp
// and batch17_theft_watchdog_test.cpp so both exercise one seam.
//
// The escape is a C++ exception and must stay one. It used to be
// setjmp/longjmp, written inline in quest_driver.cpp's main(), and that is what
// crashed quest_parity with STATUS_ACCESS_VIOLATION (0xC0000005, exit
// 3221225477) from Batch 8 to Batch 16:
//
//   * On x86_64-w64-mingw32 with __SEH__, <setjmp.h> expands setjmp(b) to
//     _setjmp(b, __builtin_frame_address(0)) whenever __builtin_sponentry is
//     unavailable -- and GCC 16, the compiler in this toolchain, no longer
//     provides that builtin.
//   * GCC's frame base is not the Win64 SEH establisher frame. Measured in
//     quest_driver's main(): jmp_buf.Frame = 0x...FD30 against an establisher
//     frame of 0x...62A0, the two separated by main's ~39 KB frame.
//   * msvcrt!longjmp hands that value to ntdll!RtlUnwindEx as TargetFrame.
//     Nothing ever matches it, so the unwinder walks past main, past the CRT
//     startup frames and off the top of the thread stack into MEM_RESERVE
//     pages -- an access violation raised inside exception dispatch, which
//     recurses until the process dies.
//   * At -O0 GCC keeps a frame pointer, the two values coincide exactly, and
//     the identical source survives. That is why this looked like flaky
//     platform noise for eight batches rather than a deterministic defect.
//
// [csetjmp.syn]/2 already forbids longjmp wherever throw/catch would run a
// non-trivial destructor. `unwound` below turns that from a comment into an
// assertion: the tripping path parks a scope guard in the callback frame, a
// real unwind destroys it, and a longjmp escape would jump straight past it.
#include "openu5/quest.h"
#include "openu5/rng.h"
#include "openu5/state.h"
#include <cstdint>

namespace openu5_test {

// The reference budget. check-quests.ts uses `if(++draws>65536)`, so the
// tripping draw is the 65537th and exactly 65536 draws reach the RNG.
inline constexpr int32_t kTheftDrawBudget = 65536;

// The watchdog escape. Thrown from the RNG callback, caught one frame below
// apply_faulinei_theft(), which holds only trivially destructible locals.
struct TheftWatchdogTripped {};

struct TheftObservation {
    openu5::TheftResult result{};
    bool nonterminating = false;
    int32_t draws = 0;
    // True when the escape actually UNWOUND the callback frame. A setjmp/longjmp
    // escape jumps out instead and leaves this false -- that is the undefined
    // behaviour Batch 17 removed. batch17_theft_watchdog_test.cpp group D asserts it.
    bool unwound = false;
};

inline TheftObservation observe_faulinei_theft(openu5::GameState &g, int32_t here,
                                               int32_t budget = kTheftDrawBudget) {
    struct Observe { openu5::OriginalRng &rng; int32_t draws; int32_t budget; bool unwound; };
    Observe observe{g.rng, 0, budget, false};
    TheftObservation out;
    try {
        out.result = openu5::apply_faulinei_theft(g, here, {&observe,
            [](void *p, int32_t lo, int32_t hi) -> int32_t {
                auto &o = *static_cast<Observe *>(p);
                if (++o.draws > o.budget) {
                    // Destroyed by the unwind; skipped by a jump. See the header note.
                    struct UnwindMark { Observe &o; ~UnwindMark() { o.unwound = true; } } mark{o};
                    throw TheftWatchdogTripped{};
                }
                return o.rng.next(lo, hi).value;
            }});
    } catch (const TheftWatchdogTripped &) {
        out.nonterminating = true;
    }
    out.draws = observe.draws;
    out.unwound = observe.unwound;
    return out;
}

} // namespace openu5_test
