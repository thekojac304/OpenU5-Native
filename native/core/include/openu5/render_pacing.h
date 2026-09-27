#pragma once
// Alpha 3 A3-04E (ALPHA3_AUDIO.md section 22): how the device's game thread
// paces itself -- the TFT draw loops' pause every band of rows, and what
// main.cpp's loop does at the end of a pass. Policy only; the device applies
// it (tdeck_board.cpp, main.cpp) and host tests pin it.
//
// Before A3-04E both were accidents of the tick rate (CONFIG_FREERTOS_HZ=100):
//   * every draw loop called vTaskDelay(1) every 16 pixel rows (every 32 fill
//     chunks). That is a sleep until the NEXT 10 ms tick, whatever the band
//     cost: a walking step's viewport is 158 rows = 9 sleeps (~90 ms), and the
//     full repaint after the Developer menu crosses ~37 (~370 ms) -- the
//     ~400 ms TFT maximum of every A3-04C/D window. The loops never needed it:
//     each row is a blocking spi_device_transmit, which already lets the idle
//     task run, and the input task (priority 4) preempts the game thread
//     (priority 1) whenever it has work.
//   * the loop ended with vTaskDelay(pdMS_TO_TICKS(5)) = vTaskDelay(0): a
//     reschedule, not a sleep, so the thread spun between frames.
//
// A3-04E keeps the draw loops' cooperative point where it was and makes it a
// yield (taskYIELD: no sleep), and makes an idle pass block on the input
// queue for one tick. Both old behaviours stay one Developer keypress away
// (Diagnostics > "Probe: legacy TFT pacing" / "Probe: legacy loop spin") so
// the device can measure each change on its own. Never saved.
#include <cstdint>

namespace openu5 {

/** The draw loops' pause. */
enum class TftPacing : uint8_t {
    TickSleep, // Alpha 2.0 .. A3-04D: vTaskDelay(1), until the next tick
    Yield,     // A3-04E: taskYIELD, no sleep
};

/** The end of one pass of main.cpp's loop. */
enum class LoopPacing : uint8_t {
    Spin,     // Alpha 2.0 .. A3-04D: vTaskDelay(pdMS_TO_TICKS(5)) = a reschedule at 100 Hz
    IdleWait, // A3-04E: an idle pass blocks on the input queue for kLoopIdleWaitTicks
};

struct PacingPolicy {
    TftPacing tft = TftPacing::Yield;
    LoopPacing loop = LoopPacing::IdleWait;
};

/** A3-04E production. */
constexpr PacingPolicy kPacingDefault{};
/** What every image up to A3-04D did (the Developer probes' "ON"). */
constexpr PacingPolicy kPacingLegacy{TftPacing::TickSleep, LoopPacing::Spin};

// ---------------------------------------------------------------------------
// The draw loops' cooperative points -- the cadence is Alpha 2.0's, unchanged:
// after every 16th pixel row of a row loop (`row>0&&(row&15)==0`), after every
// 32nd chunk of fill_rect (`(++chunks&31)==0`). Only what happens there changed.
// ---------------------------------------------------------------------------
constexpr int kTftRowsPerYield = 16;
constexpr int kTftChunksPerYield = 32;
/** Row loops: after sending row `row` (0-based). */
constexpr bool tft_row_yield_due(int row) { return row > 0 && row % kTftRowsPerYield == 0; }
/** fill_rect: after sending the `chunks`-th chunk (1-based). */
constexpr bool tft_chunk_yield_due(int chunks) { return chunks > 0 && chunks % kTftChunksPerYield == 0; }
/** How many ticks the pause blocks for: 1 = until the next tick, 0 = none (a yield). */
constexpr uint32_t tft_pause_ticks(TftPacing p) { return p == TftPacing::TickSleep ? 1u : 0u; }

// ---------------------------------------------------------------------------
// The loop's wait.
// ---------------------------------------------------------------------------
/** The pre-A3-04E wait, pdMS_TO_TICKS(5) at `tick_hz`: 0 at the device's 100 Hz -- no wait at all. */
constexpr uint32_t legacy_loop_delay_ticks(uint32_t tick_hz) { return uint32_t(uint64_t(5) * tick_hz / 1000u); }
/** A3-04E: an idle pass waits this many ticks for input -- a tick count, never 0 at any tick rate. */
constexpr uint32_t kLoopIdleWaitTicks = 1;
/**
 * Ticks to block on the input queue after a pass; 0 = reschedule and go on.
 * `may_sleep` is the runtime's answer to "is anything scheduled relative to
 * the moment it is serviced?" (AlphaRuntime::loop_may_sleep): while it is, a
 * one-tick sleep would stretch each of its steps, so the loop does not sleep.
 * Input never waits: a queued event ends the wait at once.
 */
constexpr uint32_t loop_wait_ticks(LoopPacing p, bool may_sleep) {
    return p == LoopPacing::IdleWait && may_sleep ? kLoopIdleWaitTicks : 0u;
}

const char *tft_pacing_name(TftPacing);   // "yield" / "TICK"
const char *loop_pacing_name(LoopPacing); // "idle-wait" / "SPIN"

// ---------------------------------------------------------------------------
// A3-04E.1 (ALPHA3_AUDIO.md section 23): the idle-service guarantee.
//
// A3-04E assumed the game thread's short blocks inside every TFT row
// (spi_device_transmit's result-queue wait) let IDLE0 complete a pass of its
// loop -- the pass whose esp_vApplicationIdleHook() feeds the task watchdog.
// Hardware disproved it: after ~71 s of play the watchdog fired on IDLE0 with
// `main` running. A yield (taskYIELD / vTaskDelay(0)) never hands core 0 to
// the lower-priority idle task, so the game thread must BLOCK for it.
//
// The guard does not assume anything about which blocks are long enough. The
// device registers a second core-0 idle hook, which ESP-IDF calls in the same
// pass as the watchdog's (every registered hook runs, no short-circuit), and
// the guard watches its counter: while it moves, nothing happens; when it has
// not moved for kIdleServiceBudgetUs, the game thread sleeps one tick (at a
// draw-loop pause or at the end of a loop pass) until it moves, at most
// kIdleServiceMaxSleeps ticks. Cost: none where the idle task already runs;
// otherwise at most one tick per budget -- never the 37 sleeps of a legacy
// repaint.
// ---------------------------------------------------------------------------
/** 25 x under the 5 s task-watchdog timeout; one tick every 200 ms is <= 5 % of a busy core. */
constexpr uint32_t kIdleServiceBudgetUs = 200000;
/** Sleeps one enforcement may take: the first can end at a tick a microsecond away. */
constexpr uint32_t kIdleServiceMaxSleeps = 3;

struct IdleServiceStats {
    uint32_t checks = 0;         // observations (draw-loop pauses, loop passes)
    uint32_t services = 0;       // observations that found the idle loop had run
    uint32_t max_gap_us = 0;     // longest time seen without an idle pass (the watchdog's view)
    uint32_t enforcements = 0;   // times the budget ran out and the game thread slept for it
    uint32_t forced_sleeps = 0;  // ticks slept for it
    uint64_t forced_us = 0;      // time slept for it
    uint32_t unserviced = 0;     // enforcements that gave up after kIdleServiceMaxSleeps
};

class IdleServiceGuard {
  public:
    /** Starts watching `count` (the idle hook's counter) at `now_us`; clears the stats. */
    void reset(uint32_t count, uint64_t now_us);
    /** A new statistics window; the service state is kept. */
    void reset_stats();
    /** Observes the counter. True = the idle loop has not run for kIdleServiceBudgetUs: block now. */
    bool due(uint32_t count, uint64_t now_us);
    /** One enforcement: `sleeps` ticks slept for `us`; `serviced` = the idle loop ran. */
    void note_enforcement(uint32_t sleeps, uint32_t us, bool serviced);
    const IdleServiceStats &stats() const { return stats_; }

  private:
    bool started_ = false;
    uint32_t last_count_ = 0;
    uint64_t last_service_us_ = 0;
    IdleServiceStats stats_{};
};

} // namespace openu5
