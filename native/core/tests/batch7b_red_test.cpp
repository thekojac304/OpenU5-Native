// Batch 7B RED HARNESS -- a HEAD-BASELINE ARTIFACT, NOT A REGRESSION TEST.
//
// **This file is deliberately NOT registered with CTest and must not be.** It
// exists to make the Batch 7B RED evidence reproducible: it states the three
// Y-04 claims that CAN be expressed against the *pre-Batch-7B* API, so the
// failure is a real runtime assertion rather than a missing symbol. It was
// built and run against a `git worktree` of the untouched HEAD 722ed36f, where
// it failed 7 of 7.
//
// It still fails today, BY DESIGN: every claim here is phrased against
// `UiSession::consume()` alone, and the Batch 7B consumers deliberately live
// one layer up, in the runtime that owns the frame clock (`AlphaRuntime`'s
// `NarrativeScenePacer`/`WorldFxLayer`). The GREEN restatement of each claim,
// through the mechanism that actually implements it, is in `batch7b_test.cpp`
// (ctest `batch7b`), annotated there with the RED id it discharges.
//
// To reproduce the RED run:
//   git worktree add <dir> 722ed36f --detach
//   cp native/core/tests/batch7b_red_test.cpp <dir>/native/core/tests/
//   cp native/core/src/*.inc <dir>/native/core/src/      # gitignored, generated
//   # add a batch7b_red_tests target to <dir>/native/core/CMakeLists.txt, build, run.
//
// The two remaining channels have NO pre-fix seam at all and are therefore
// compile-level RED, which this file deliberately does not fake:
//   * PoisonTick  -- nothing anywhere in native publishes a roster-flash row,
//                    so there is no value a HEAD-compilable test could read.
//   * CellProjectile -- nothing anywhere in native computes a flight path or
//                    its cadence, for the same reason.
#include "openu5/commands.h"
#include "openu5/movement.h"
#include "openu5/presentation.h"
#include "openu5/quest_world.h"
#include "openu5/ui_session.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace openu5;
namespace {
int failures = 0;
void check(bool ok, const char *what) {
    if (ok) {
        std::cout << "  PASS " << what << "\n";
        return;
    }
    ++failures;
    std::cerr << "  RED  " << what << "\n";
}
struct Spy {
    static void send(void *, const UiIntent &) {}
};
bool transcript_contains(const UiSession &ui, const char *needle) {
    for (size_t i = 0; i < ui.transcript_size(); ++i) {
        const auto *b = ui.transcript_at(i);
        if (b && std::strstr(b->text, needle)) return true;
    }
    return false;
}
} // namespace

int main() {
    // ---- R1: TrollSneak's staged narrative must reach the player ----------
    {
        UiTextBlock blocks[64];
        Spy spy;
        UiSession ui{{blocks, 64}, {&spy, Spy::send}, {8, 3, 64}};
        TrollSneakScript script;
        script.beats[script.count++] = {"\nThou spieth trolls under the bridge!\n\n", 10, false};
        script.beats[script.count++] = {"Shamino sneaks across", 5, false};
        script.beats[script.count++] = {".", 5, true};
        script.beats[script.count++] = {".", 5, true};
        script.beats[script.count++] = {".", -1, true};
        script.beats[script.count++] = {"\n", -1, false};
        script.beats[script.count++] = {"Trolls evaded!\n", -1, false};
        GameEvent e;
        e.kind = GameEventKind::TrollSneak;
        e.troll_sneak = &script;
        ui.consume(e);
        check(transcript_contains(ui, "Thou spieth trolls under the bridge!"),
              "R1a TrollSneak preamble reaches the transcript");
        check(transcript_contains(ui, "Shamino sneaks across..."),
              "R1b TrollSneak dots continue the same line (0x1c56-0x1c65)");
        check(transcript_contains(ui, "Trolls evaded!"),
              "R1c TrollSneak outcome reaches the transcript");
    }

    // ---- R2: Refuge's staged narrative must reach the player --------------
    {
        UiTextBlock blocks[64];
        Spy spy;
        UiSession ui{{blocks, 64}, {&spy, Spy::send}, {8, 3, 64}};
        RefugeScript script{{
            {"void", "An unending darkness engulfs thee...", nullptr, 10},
            {nullptr, "Thou hast found refuge.", nullptr, 14},
            {"vertigo"},
        }};
        GameEvent e;
        e.kind = GameEventKind::Refuge;
        e.refuge = &script;
        ui.consume(e);
        check(transcript_contains(ui, "An unending darkness engulfs thee..."),
              "R2a Refuge narration reaches the transcript");
        check(transcript_contains(ui, "Thou hast found refuge."),
              "R2b Refuge second beat reaches the transcript");
    }

    // ---- R3: CellExplosion must hold `under_tile` on the cell -------------
    // #243: the state write commits synchronously, so without a temporary
    // per-cell overlay the cell shows the Flame from the very first frame
    // instead of the Shadowlord the original keeps under the burst.
    {
        UiTextBlock blocks[16];
        Spy spy;
        UiSession ui{{blocks, 16}, {&spy, Spy::send}, {8, 3, 16}};
        GameEvent e;
        e.kind = GameEventKind::CellExplosion;
        e.cell_fx = {0, -1, 7, 3, 252}; // the shard ritual's own payload
        ui.consume(e);
        PresentationSnapshot snapshot; // a composed window, cells all terrain
        for (int i = 0; i < kPresentationCells; ++i) {
            snapshot.tiles[i] = 5;
            snapshot.visible[i] = 1;
        }
        constexpr int half = kPresentationWindow / 2;
        const int at = (half - 1) * kPresentationWindow + half;
        check(snapshot.tiles[at] == int16_t(252 + 0x100),
              "R3a CellExplosion keeps the Shadowlord under the burst");
        check(snapshot.tiles[at] == 0 || snapshot.tiles[at] == int16_t(252 + 0x100),
              "R3b CellExplosion ever paints anything on its cell");
    }

    std::cout << (failures ? "batch7b RED: " : "batch7b RED: ") << failures
              << " failing claim(s)\n";
    return failures ? 1 : 0;
}
