#pragma once
// Batch 11 host-test seam. Minimal stand-in for ESP-IDF's esp_timer.h.
// esp_timer_get_time() is used in alpha_runtime.cpp only for perf-metric
// logging (command_high_us_ etc.) and wall-clock presentation timers
// (quake/magic-ceremony/map-reveal windows) -- never to decide gameplay
// state. A host-side monotonic microsecond clock is a faithful, harmless
// substitute for the real hardware timer.
#include <chrono>
#include <cstdint>

inline int64_t esp_timer_get_time() {
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}
