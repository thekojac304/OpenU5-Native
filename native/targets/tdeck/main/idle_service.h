#pragma once
// Alpha 3 A3-04E.1 (ALPHA3_AUDIO.md section 23): the game thread guarantees
// core 0's idle task -- and with it the task watchdog -- a pass of its loop at
// least every openu5::kIdleServiceBudgetUs.
//
// core0_hook() is registered with esp_register_freertos_idle_hook_for_cpu(.., 0)
// (main.cpp). ESP-IDF runs every registered hook of the core in one
// esp_vApplicationIdleHook() pass, the watchdog's included, so its counter
// moving means the watchdog was fed. enforce() is called by the game thread at
// its cooperative points (the draw loops' pause, the end of a loop pass): it
// returns at once while the counter moves and otherwise sleeps one tick at a
// time (at most openu5::kIdleServiceMaxSleeps) until it does.
#include <cstdint>

#include "openu5/render_pacing.h"

namespace tdeck {

class IdleService {
  public:
    /** The core-0 idle hook: one count per idle-loop pass. Never blocks; keeps the core free to idle. */
    static bool core0_hook();
    static const volatile uint32_t *core0_count();

    /** Watch `count` (the device: core0_count(); host tests: a modelled counter). Null = off. */
    void attach(const volatile uint32_t *count);
    /** At a cooperative point of the game thread: sleeps only if the idle loop has not run for the budget. */
    bool enforce();
    /** A new statistics window (the Developer report's). */
    void reset_stats() { guard_.reset_stats(); }
    const openu5::IdleServiceStats &stats() const { return guard_.stats(); }

  private:
    const volatile uint32_t *count_ = nullptr;
    openu5::IdleServiceGuard guard_{};
};

} // namespace tdeck
