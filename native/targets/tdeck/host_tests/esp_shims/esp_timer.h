#pragma once
// Batch 11 host-test seam. Minimal stand-in for ESP-IDF's esp_timer.h.
// esp_timer_get_time() is used in alpha_runtime.cpp only for perf-metric
// logging (command_high_us_ etc.) and wall-clock presentation timers
// (quake/magic-ceremony/map-reveal windows, the scene pacers) -- never to
// decide gameplay state. A host-side monotonic microsecond clock is a
// faithful, harmless substitute for the real hardware timer.
//
// Batch 51: a test may pin the clock instead, so presentation timing is
// asserted on a deterministic virtual timeline rather than by sleeping.
// A negative value (the default) keeps the real steady clock.
#include <chrono>
#include <cstdint>

inline int64_t &openu5_host_virtual_clock_us() {
    static int64_t now = -1;
    return now;
}

inline int64_t esp_timer_get_time() {
    const int64_t pinned = openu5_host_virtual_clock_us();
    if (pinned >= 0) return pinned;
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}
