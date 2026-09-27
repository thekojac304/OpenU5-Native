#pragma once

#include <cstddef>
#include <cstdint>

#include "openu5/audio_stream.h"

// Alpha 3 A3-04B -- the render side of the performance picture and the one
// combined report the Developer menu shows (ALPHA3_AUDIO.md section 19).
//
// A3-04A measured only the audio task. The hardware report that opened
// A3-04B -- "with music on the overworld is laggy; at Music Volume 0 % it is
// much better" -- is about the OTHER core, so the game thread now counts
// its own frames the same way (RenderPerfCounters), the device adds what
// only FreeRTOS knows (per-core and per-task CPU, heap, stacks:
// SystemPerfSnapshot), and format_perf_report() turns the three into the
// lines of one screen that stays up until it is dismissed.
//
// Portable and allocation-free: the game thread owns the counters, the clock
// is the caller's, nothing here logs.
namespace openu5 {

// ---------------------------------------------------------------------------
// The game thread's frames (section 19.5). One window, from the last reset.
// Every duration is microseconds.
// ---------------------------------------------------------------------------
struct RenderPerfSnapshot {
    uint32_t window_us = 0;
    uint32_t frames = 0;  // gameplay frames drawn (world, combat, dungeon, scenes -- not the Developer screen)
    uint32_t frame_avg_us = 0, frame_max_us = 0, frame_p95_us = 0, frame_p99_us = 0;
    uint32_t compose_avg_us = 0, compose_max_us = 0; // presentation + rasterizer + post-processing, before the TFT
    uint32_t tiles_avg_us = 0, tiles_max_us = 0;     // the rasterizer alone (tiles / dungeon / gem view)
    uint32_t tft_avg_us = 0, tft_max_us = 0;         // Board::show_alpha: panels + SPI transfer (+ bus waits)
    uint32_t cadence_avg_us = 0, cadence_max_us = 0; // interval between two drawn frames
    uint32_t late_frames = 0;                        // frames longer than RenderPerfCounters::kLateFrameUs
    uint32_t inputs = 0, handle_avg_us = 0, handle_max_us = 0; // one input event's processing (handle())
    uint32_t shown = 0, input_avg_us = 0, input_max_us = 0;    // key capture -> end of the frame that shows it
    uint32_t busy_permille = 0; // (frames + input handling) / window: the game thread's measured work
};

class RenderPerfCounters {
  public:
    /** One tick of the original's 18.2 Hz timer (the animation clock): a frame longer than it is late. */
    static constexpr uint32_t kLateFrameUs = 55000;
    /** Frame-time histogram: 2 ms buckets up to 200 ms, plus overflow. */
    static constexpr uint32_t kBucketUs = 2000;
    static constexpr size_t kBuckets = 100;

    void reset(uint64_t now_us);
    /** A gameplay frame that started at `start_us` and took `total_us` (compose + TFT). */
    void on_frame(uint64_t start_us, uint32_t compose_us, uint32_t tiles_us, uint32_t tft_us, uint32_t total_us);
    /** One input event handled in `handle_us`. */
    void on_input(uint32_t handle_us);
    /** A frame just finished that shows an input captured `latency_us` ago. */
    void on_input_shown(uint32_t latency_us);
    void snapshot(uint64_t now_us, RenderPerfSnapshot &out) const;

  private:
    uint64_t start_us_ = 0, last_frame_start_us_ = 0;
    bool has_last_frame_ = false;
    uint32_t frames_ = 0, cadences_ = 0, inputs_ = 0, shown_ = 0, late_ = 0;
    uint64_t frame_sum_ = 0, compose_sum_ = 0, tiles_sum_ = 0, tft_sum_ = 0, cadence_sum_ = 0;
    uint64_t handle_sum_ = 0, input_sum_ = 0;
    uint32_t frame_max_ = 0, compose_max_ = 0, tiles_max_ = 0, tft_max_ = 0, cadence_max_ = 0;
    uint32_t handle_max_ = 0, input_max_ = 0;
    uint32_t histogram_[kBuckets + 1]{};
};

// ---------------------------------------------------------------------------
// The whole machine (section 19.4): what the device's FreeRTOS run-time
// statistics say each core and each task did over the window, plus heap and
// stack headroom. The host has no such source (valid stays false).
// ---------------------------------------------------------------------------
struct SystemPerfSnapshot {
    bool valid = false;         // run-time statistics were available for the whole window
    uint32_t window_us = 0;
    uint32_t core_busy_permille[2]{};   // 1000 - that core's idle task share
    uint32_t audio_permille = 0, main_permille = 0, input_permille = 0, sdlog_permille = 0, other_permille = 0;
    uint32_t heap_internal_free = 0, heap_internal_min = 0; // bytes; min = the lowest ever (heap_caps)
    uint32_t heap_psram_free = 0, heap_psram_min = 0;
    uint32_t stack_main_free = 0, stack_audio_free = 0, stack_input_free = 0; // high-water marks, bytes
};

class SystemPerfSource {
  public:
    virtual ~SystemPerfSource() = default;
    /** Start a new window (game thread; never waits). */
    virtual void system_perf_reset() = 0;
    /** The window since the last reset; false if the source has nothing yet. */
    virtual bool system_perf_snapshot(SystemPerfSnapshot &) = 0;
};

// ---------------------------------------------------------------------------
// Alpha 3 A3-04C (ALPHA3_AUDIO.md section 20): the contention map -- where
// a gameplay frame's time goes on core 0, split finely enough to tell a
// renderer slowed by the audio task on core 1 from one that is simply
// waiting on its own SPI bus or on the FreeRTOS tick.
// ---------------------------------------------------------------------------
/** One TFT transaction longer than this waited for something (the shared SPI bus, a preempting task). */
constexpr uint32_t kSlowXferUs = 1000;
/** The draw loops' vTaskDelay(1) returns at the next 10 ms tick; longer means core 0 was not handed back. */
constexpr uint32_t kLateYieldUs = 11000;

/**
 * What the device Board measured while writing ONE gameplay frame to the
 * TFT (Board::take_tft_timing() after show_alpha). Durations are core-0 CPU
 * cycles (cpu_mhz converts them; 0 = no timing). A row is "busy" when the
 * audio task on core 1 was running at both of its ends and "idle" when it
 * was blocked at both; a row that straddled a change counts in `rows` only.
 */
struct TftTiming {
    uint32_t cpu_mhz = 0;
    uint32_t transactions = 0;     // every spi_device_transmit, window commands included
    uint32_t rows = 0;             // pixel rows / fill chunks
    uint64_t xfer_cycles = 0;      // inside spi_device_transmit: bus acquire + DMA + completion
    uint32_t xfer_max_cycles = 0;
    uint32_t slow_xfers = 0, slow_xfers_busy = 0; // longer than kSlowXferUs (and of those, with audio running)
    uint32_t yields = 0;           // vTaskDelay(1) every 16 rows
    uint64_t yield_cycles = 0;
    uint32_t yield_max_cycles = 0;
    uint32_t late_yields = 0;      // longer than kLateYieldUs
    uint32_t rows_idle = 0, rows_busy = 0;
    uint64_t fill_cycles_idle = 0, fill_cycles_busy = 0; // building the row (the PSRAM viewport read, glyphs, byte swap)
    uint64_t xfer_cycles_idle = 0, xfer_cycles_busy = 0;
    uint32_t viewport_full = 0;    // the whole 176-px viewport was rewritten (a step, a new map)
};

/** The SD-log writer's storage bursts (its SPI bus is the TFT's). Device only. */
struct SdLogPerf {
    bool valid = false;
    uint32_t bursts = 0;           // writer wakes that wrote or flushed
    uint32_t busy_us = 0, max_us = 0;
};

struct ContentionSnapshot {
    uint32_t window_us = 0;
    uint32_t frames = 0;           // gameplay frames (as RenderPerfCounters)
    bool timed = false;            // at least one frame carried TFT timing
    uint32_t logic_avg_us = 0, logic_max_us = 0;          // render()'s game logic before the frame is composed
    uint32_t tft_fill_avg_us = 0;                         // per frame: TFT time outside transactions and yields
    uint32_t tft_xfer_avg_us = 0, tft_xfer_max_us = 0;    // per frame: inside SPI transactions
    uint32_t tft_yield_avg_us = 0, tft_yield_max_us = 0;  // per frame: in the draw loops' yields
    uint32_t viewport_frames = 0, viewport_tft_avg_us = 0, viewport_tft_max_us = 0; // whole viewport rewritten
    uint32_t panel_frames = 0, panel_tft_avg_us = 0, panel_tft_max_us = 0;          // everything else
    uint32_t transactions = 0, rows = 0;
    uint32_t xfer_max_us = 0, slow_xfers = 0, slow_xfers_busy = 0;
    uint32_t yields = 0, yield_max_us = 0, late_yields = 0;
    uint32_t rows_idle = 0, rows_busy = 0;
    uint32_t row_fill_idle_x10 = 0, row_fill_busy_x10 = 0; // per-row average, 0.1 us
    uint32_t row_xfer_idle_x10 = 0, row_xfer_busy_x10 = 0;
    uint32_t loops = 0, loop_avg_us = 0, loop_max_us = 0;  // main.cpp's loop passes
};

class ContentionCounters {
  public:
    void reset(uint64_t now_us);
    /** A gameplay frame: `logic_us` before composing, `tft_us` in show_alpha, `t` what the Board measured in it. */
    void on_frame(uint32_t logic_us, uint32_t tft_us, const TftTiming &t);
    /** One pass of main.cpp's loop (input, handling, drawing). */
    void on_loop(uint32_t loop_us);
    void snapshot(uint64_t now_us, ContentionSnapshot &out) const;

  private:
    uint64_t start_us_ = 0;
    uint32_t frames_ = 0, timed_frames_ = 0, cpu_mhz_ = 0;
    uint64_t logic_sum_ = 0, fill_sum_ = 0, xfer_sum_ = 0, yield_sum_ = 0;
    uint32_t logic_max_ = 0, xfer_frame_max_ = 0, yield_frame_max_ = 0;
    uint32_t viewport_frames_ = 0, panel_frames_ = 0, viewport_tft_max_ = 0, panel_tft_max_ = 0;
    uint64_t viewport_tft_sum_ = 0, panel_tft_sum_ = 0;
    uint64_t transactions_ = 0, rows_ = 0, rows_idle_ = 0, rows_busy_ = 0, yields_ = 0;
    uint32_t xfer_max_cycles_ = 0, yield_max_cycles_ = 0, slow_ = 0, slow_busy_ = 0, late_yields_ = 0;
    uint64_t fill_idle_cycles_ = 0, fill_busy_cycles_ = 0, xfer_idle_cycles_ = 0, xfer_busy_cycles_ = 0;
    uint32_t loops_ = 0, loop_max_ = 0;
    uint64_t loop_sum_ = 0;
};

/** What was playing, at which settings: the label that makes two runs comparable. */
struct PerfScenario {
    bool music_available = false;  // the audio pack's music capability
    uint8_t music_volume = 0, sfx_volume = 0;
    bool synth_bypass = false;     // the A3-04C Developer probe
};

// ---------------------------------------------------------------------------
// The combined report (section 19.3): one Developer screen, row by row.
// ---------------------------------------------------------------------------
/** One DeviceDebugScreen row holds 51 characters and the terminator. */
constexpr size_t kPerfReportLineBytes = 52;
constexpr size_t kPerfReportMaxLines = 64;

struct PerfReportInput {
    const char *title = nullptr;              // first line, e.g. "AUDIO/RENDER PERF  live"
    const AudioPerfSnapshot *audio = nullptr; // nullptr: "no audio played"
    const char *audio_heading = nullptr;      // e.g. "Music alone (idle)"
    const AudioPerfSnapshot *audio2 = nullptr; // the benchmark's second phase, or nullptr
    const char *audio2_heading = nullptr;
    const RenderPerfSnapshot *render = nullptr;
    const SystemPerfSnapshot *system = nullptr;
    bool has_guard = false;   // the benchmark priced A3-04's guard on this device
    uint32_t guard_ns = 0, guard_us_per_block = 0;
    // A3-04C (section 20): nullptr leaves the section out.
    const PerfScenario *scenario = nullptr;
    const ContentionSnapshot *contention = nullptr;
    const SdLogPerf *sdlog = nullptr;
};

/**
 * Formats the report into up to `max_lines` lines of at most
 * kPerfReportLineBytes - 1 characters. Returns the lines written.
 */
size_t format_perf_report(const PerfReportInput &, char (*lines)[kPerfReportLineBytes], size_t max_lines);

/**
 * A3-04C: the whole window on ONE line (the serial / SD-log `A3C_PERF`
 * heartbeat), in a fixed field order so two runs diff field by field.
 * Always NUL-terminated; returns the characters written (< kContentionLineBytes).
 */
constexpr size_t kContentionLineBytes = 720;
size_t format_contention_line(const PerfReportInput &, char *out, size_t cap);

} // namespace openu5
