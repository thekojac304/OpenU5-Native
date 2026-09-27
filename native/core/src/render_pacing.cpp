#include "openu5/render_pacing.h"

namespace openu5 {

// Upper case marks the legacy behaviour, as "ON" marks a probe that is on.
const char *tft_pacing_name(TftPacing p) { return p == TftPacing::TickSleep ? "TICK" : "yield"; }
const char *loop_pacing_name(LoopPacing p) { return p == LoopPacing::Spin ? "SPIN" : "idle-wait"; }

} // namespace openu5
