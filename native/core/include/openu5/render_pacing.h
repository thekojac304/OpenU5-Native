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

} // namespace openu5
