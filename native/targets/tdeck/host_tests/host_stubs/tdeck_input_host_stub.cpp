// Batch 11 host-test seam.
//
// tdeck::raw_input_name() (input_events.h) is a pure name lookup used only
// for ESP_LOGx diagnostics -- never a gameplay decision. Its real definition
// lives in tdeck_input.cpp alongside the physical I2C/GPIO keyboard-matrix
// driver, which this host target has no reason to link. Reproduced verbatim
// (not reachable from any command-routing decision, so there is nothing to
// drift out of sync with).
//
// Never linked into the T-Deck firmware: only this CMake host test target
// compiles it. tdeck_input.cpp (the real device implementation) is untouched.
#include "input_events.h"

namespace tdeck {

const char *raw_input_name(RawInputKind kind) {
    switch (kind) {
    case RawInputKind::Keyboard: return "keyboard";
    case RawInputKind::KeyboardResynchronized: return "keyboard-resynchronized";
    case RawInputKind::TrackballUp: return "trackball-up";
    case RawInputKind::TrackballDown: return "trackball-down";
    case RawInputKind::TrackballLeft: return "trackball-left";
    case RawInputKind::TrackballRight: return "trackball-right";
    case RawInputKind::None: return "none";
    }
    return "unknown";
}

} // namespace tdeck
