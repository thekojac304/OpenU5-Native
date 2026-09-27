#include "openu5/render_pacing.h"

namespace openu5 {

// Upper case marks the legacy behaviour, as "ON" marks a probe that is on.
const char *tft_pacing_name(TftPacing p) { return p == TftPacing::TickSleep ? "TICK" : "yield"; }
const char *loop_pacing_name(LoopPacing p) { return p == LoopPacing::Spin ? "SPIN" : "idle-wait"; }

void IdleServiceGuard::reset(uint32_t count, uint64_t now_us) {
    started_ = true;
    last_count_ = count;
    last_service_us_ = now_us;
    stats_ = IdleServiceStats{};
}

void IdleServiceGuard::reset_stats() { stats_ = IdleServiceStats{}; }

bool IdleServiceGuard::due(uint32_t count, uint64_t now_us) {
    if (!started_) reset(count, now_us);
    ++stats_.checks;
    if (count != last_count_) { // the idle loop ran (and with it the watchdog's hook)
        last_count_ = count;
        last_service_us_ = now_us;
        ++stats_.services;
        return false;
    }
    const uint64_t gap = now_us > last_service_us_ ? now_us - last_service_us_ : 0;
    const uint32_t gap32 = gap > UINT32_MAX ? UINT32_MAX : uint32_t(gap);
    if (gap32 > stats_.max_gap_us) stats_.max_gap_us = gap32;
    return gap >= kIdleServiceBudgetUs;
}

void IdleServiceGuard::note_enforcement(uint32_t sleeps, uint32_t us, bool serviced) {
    ++stats_.enforcements;
    stats_.forced_sleeps += sleeps;
    stats_.forced_us += us;
    if (!serviced) ++stats_.unserviced;
}

} // namespace openu5
