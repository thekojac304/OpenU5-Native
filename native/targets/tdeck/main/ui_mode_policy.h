#pragma once

#include "openu5/dungeon.h"
#include "openu5/ui_session.h"

namespace tdeck {

// Batch 9D.  The per-input publication of the two dungeon prompt mirrors
// UiSession cannot derive for itself (it owns no DungeonState): whether the
// cell under the party offers a Klimb BOTH ways, and whether it is a fountain.
// It exists here, beside resolve_synchronized_base_mode(), for the same reason
// that one does -- alpha_runtime.cpp cannot be host-compiled, so production and
// the host suite must share one definition of the rule or they drift.
//
// They drifted.  Batch 9B added UiSession::refresh_dungeon_context() and the
// Klimb-U/D- and "Will you drink?" prompts that read the mirrors, and wired the
// call into the host test's own input tail -- but nothing in AlphaRuntime ever
// called it.  On hardware both mirrors were therefore permanently false: (K)limb
// silently preferred up on an up+down ladder (the exact defect 9B set out to
// fix) and (D)rink never asked.  AlphaRuntime::refresh_session_context() calls
// this now, immediately before every key is routed, exactly like the sail (R-19)
// and harpsichord (R-20) mirrors beside it.
inline void publish_dungeon_prompt_context(openu5::UiSession &ui, const openu5::GameState &game,
                                           const openu5::DungeonState &dungeon,
                                           bool dungeon_active) {
    ui.refresh_dungeon_context(game, dungeon, dungeon_active);
}

// Pure, ESP-free extraction of the mode arbitration performed by
// AlphaRuntime::synchronize_after_debug() on every gameplay input.
//
// It exists only so the host suite can exercise that decision: alpha_runtime.cpp
// hard-depends on esp_log/esp_timer/esp_heap_caps/FreeRTOS/Board and cannot be
// host-compiled.  AlphaRuntime calls this function so production and the host
// tests share one definition of the rule and cannot drift.
//
// Combat and Dungeon are authoritative core states, not player-owned UI
// sessions: an input that starts or ends either one must always be able to
// force the corresponding mode, exactly like AlphaRuntime::command() already
// does.  Shop, Dialogue and ShrineSpecial are session-owned -- UiSession
// itself decides when they end (see the return-mode registers and the
// ShrineSpecial lifecycle in ui_session.cpp) -- so an ordinary per-input
// resync must leave them alone, the same way it already leaves a
// TextEntry/NumericEntry/YesNo/Selection/TargetSelection modal alone.
inline openu5::UiMode resolve_synchronized_base_mode(openu5::UiMode current_base_mode,
                                                     bool combat, bool dungeon) {
    if (combat) return openu5::UiMode::Combat;
    if (dungeon) return openu5::UiMode::Dungeon;
    if (current_base_mode == openu5::UiMode::Shop ||
        current_base_mode == openu5::UiMode::Dialogue ||
        current_base_mode == openu5::UiMode::ShrineSpecial) {
        return current_base_mode;
    }
    return openu5::UiMode::Exploration;
}

} // namespace tdeck
