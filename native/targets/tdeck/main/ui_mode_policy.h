#pragma once

#include "openu5/ui_session.h"

namespace tdeck {

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
