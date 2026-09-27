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
// The combined report (section 19.3): one Developer screen, row by row.
// ---------------------------------------------------------------------------
/** One DeviceDebugScreen row holds 51 characters and the terminator. */
constexpr size_t kPerfReportLineBytes = 52;
constexpr size_t kPerfReportMaxLines = 48;

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
};

/**
 * Formats the report into up to `max_lines` lines of at most
 * kPerfReportLineBytes - 1 characters. Returns the lines written.
 */
size_t format_perf_report(const PerfReportInput &, char (*lines)[kPerfReportLineBytes], size_t max_lines);

} // namespace openu5
