#include "openu5/perf_report.h"

#include <cstdarg>
#include <cstdio>

// Alpha 3 A3-04B -- see perf_report.h and ALPHA3_AUDIO.md section 19.
namespace openu5 {

// ===========================================================================
// RenderPerfCounters
// ===========================================================================
void RenderPerfCounters::reset(uint64_t now_us) {
    *this = RenderPerfCounters{};
    start_us_ = now_us;
}

void RenderPerfCounters::on_frame(uint64_t start_us, uint32_t compose_us, uint32_t tiles_us, uint32_t tft_us,
                                  uint32_t total_us) {
    ++frames_;
    frame_sum_ += total_us;
    compose_sum_ += compose_us;
    tiles_sum_ += tiles_us;
    tft_sum_ += tft_us;
    if (total_us > frame_max_) frame_max_ = total_us;
    if (compose_us > compose_max_) compose_max_ = compose_us;
    if (tiles_us > tiles_max_) tiles_max_ = tiles_us;
    if (tft_us > tft_max_) tft_max_ = tft_us;
    if (total_us > kLateFrameUs) ++late_;
    const uint32_t bucket = total_us / kBucketUs;
    ++histogram_[bucket < kBuckets ? bucket : kBuckets];
    if (has_last_frame_ && start_us >= last_frame_start_us_) {
        const uint32_t cadence = uint32_t(start_us - last_frame_start_us_);
        ++cadences_;
        cadence_sum_ += cadence;
        if (cadence > cadence_max_) cadence_max_ = cadence;
    }
    last_frame_start_us_ = start_us;
    has_last_frame_ = true;
}

void RenderPerfCounters::on_input(uint32_t handle_us) {
    ++inputs_;
    handle_sum_ += handle_us;
    if (handle_us > handle_max_) handle_max_ = handle_us;
}

void RenderPerfCounters::on_input_shown(uint32_t latency_us) {
    ++shown_;
    input_sum_ += latency_us;
    if (latency_us > input_max_) input_max_ = latency_us;
}

void RenderPerfCounters::snapshot(uint64_t now_us, RenderPerfSnapshot &out) const {
    out = RenderPerfSnapshot{};
    out.window_us = now_us > start_us_ ? uint32_t(now_us - start_us_) : 0;
    out.frames = frames_;
    out.late_frames = late_;
    if (frames_) {
        out.frame_avg_us = uint32_t(frame_sum_ / frames_);
        out.frame_max_us = frame_max_;
        out.compose_avg_us = uint32_t(compose_sum_ / frames_);
        out.compose_max_us = compose_max_;
        out.tiles_avg_us = uint32_t(tiles_sum_ / frames_);
        out.tiles_max_us = tiles_max_;
        out.tft_avg_us = uint32_t(tft_sum_ / frames_);
        out.tft_max_us = tft_max_;
        // Percentiles from the histogram: the upper edge of the bucket the
        // rank falls in, never above the true maximum (the AudioPerfCounters rule).
        const uint64_t rank95 = (uint64_t(frames_) * 95 + 99) / 100, rank99 = (uint64_t(frames_) * 99 + 99) / 100;
        uint64_t seen = 0;
        for (size_t b = 0; b <= kBuckets; ++b) {
            const uint64_t before = seen;
            seen += histogram_[b];
            const uint32_t edge = b < kBuckets ? uint32_t((b + 1) * kBucketUs) : frame_max_;
            const uint32_t clamped = edge < frame_max_ ? edge : frame_max_;
            if (before < rank95 && seen >= rank95) out.frame_p95_us = clamped;
            if (before < rank99 && seen >= rank99) out.frame_p99_us = clamped;
        }
    }
    if (cadences_) {
        out.cadence_avg_us = uint32_t(cadence_sum_ / cadences_);
        out.cadence_max_us = cadence_max_;
    }
    out.inputs = inputs_;
    if (inputs_) {
        out.handle_avg_us = uint32_t(handle_sum_ / inputs_);
        out.handle_max_us = handle_max_;
    }
    out.shown = shown_;
    if (shown_) {
        out.input_avg_us = uint32_t(input_sum_ / shown_);
        out.input_max_us = input_max_;
    }
    if (out.window_us) out.busy_permille = uint32_t((frame_sum_ + handle_sum_) * 1000 / out.window_us);
}

// ===========================================================================
// format_perf_report
// ===========================================================================
namespace {

// Line collector: a line past max_lines is dropped, never written out of bounds.
class Lines {
  public:
    Lines(char (*lines)[kPerfReportLineBytes], size_t max) : lines_(lines), max_(max) {}
    __attribute__((format(printf, 2, 3))) void add(const char *format, ...) {
        if (n_ >= max_) return;
        va_list args;
        va_start(args, format);
        std::vsnprintf(lines_[n_++], kPerfReportLineBytes, format, args);
        va_end(args);
    }
    size_t count() const { return n_; }

  private:
    char (*lines_)[kPerfReportLineBytes];
    size_t max_, n_ = 0;
};

/** Microseconds as milliseconds with `decimals` (1 or 2) digits: "12.34". */
struct Ms {
    char s[16];
    Ms(uint32_t us, int decimals) {
        if (decimals >= 2)
            std::snprintf(s, sizeof s, "%lu.%02lu", (unsigned long)(us / 1000), (unsigned long)((us % 1000) / 10));
        else
            std::snprintf(s, sizeof s, "%lu.%lu", (unsigned long)(us / 1000), (unsigned long)((us % 1000) / 100));
    }
};

/** A permille as a whole percentage, rounded. */
unsigned long pct(uint32_t permille) { return (unsigned long)((permille + 5) / 10); }

void audio_section(Lines &out, const char *heading, const AudioPerfSnapshot &s) {
    out.add("-- %.20s: %.22s --", heading ? heading : "Music", s.music_active ? music_song_title(s.song) : "no music");
    out.add("window %lu.%lus  blocks %lu  block %s ms", (unsigned long)(s.window_us / 1000000),
            (unsigned long)((s.window_us / 100000) % 10), (unsigned long)s.blocks, Ms(s.block_us, 1).s);
    out.add("render avg %s p99 %s max %s ms", Ms(s.render_avg_us, 2).s, Ms(s.render_p99_us, 2).s,
            Ms(s.render_max_us, 2).s);
    out.add("music avg %s max %s  audio CPU %lu%%", Ms(s.music_avg_us, 2).s, Ms(s.music_max_us, 2).s,
            pct(s.cpu_permille));
    out.add("missed %lu  underrun %lu  hw %lu  clip %lu", (unsigned long)s.missed_deadlines,
            (unsigned long)s.underruns, (unsigned long)s.hw_underruns, (unsigned long)s.mix_clipped);
    out.add("buffered min %lu max %lu ms  sched max %s", (unsigned long)(s.fill_min * s.block_us / 1000),
            (unsigned long)(s.fill_max * s.block_us / 1000), Ms(s.period_max_us, 1).s);
    out.add("voices max %lu  OPL ch avg %lu.%lu max %lu", (unsigned long)s.voices_max,
            (unsigned long)(s.channels_avg_x100 / 100), (unsigned long)((s.channels_avg_x100 / 10) % 10),
            (unsigned long)s.channels_max);
    out.add("SFX %lu (%lu w/music) q%lu p%lu  runaway %lu", (unsigned long)s.sfx_submitted,
            (unsigned long)s.sfx_during_music, (unsigned long)s.sfx_queue_max, (unsigned long)s.sfx_pending_max,
            (unsigned long)s.runaway_yields);
    out.add("audio stack min %lu B  failures %lu", (unsigned long)s.stack_free_min,
            (unsigned long)(s.write_failures + s.enable_failures));
}

} // namespace

size_t format_perf_report(const PerfReportInput &in, char (*lines)[kPerfReportLineBytes], size_t max_lines) {
    Lines out(lines, max_lines);
    out.add("%.51s", in.title ? in.title : "AUDIO/RENDER PERF");
    if (in.audio) audio_section(out, in.audio_heading, *in.audio);
    else out.add("-- Music: no audio played in this window --");
    if (in.audio2) audio_section(out, in.audio2_heading, *in.audio2);

    if (const RenderPerfSnapshot *r = in.render) {
        out.add("-- Render: %lu gameplay frames in %lu.%lus --", (unsigned long)r->frames,
                (unsigned long)(r->window_us / 1000000), (unsigned long)((r->window_us / 100000) % 10));
        if (r->frames) {
            out.add("frame avg %s p95 %s p99 %s", Ms(r->frame_avg_us, 1).s, Ms(r->frame_p95_us, 1).s,
                    Ms(r->frame_p99_us, 1).s);
            out.add("frame max %s ms  late (>%lu ms) %lu", Ms(r->frame_max_us, 1).s,
                    (unsigned long)(RenderPerfCounters::kLateFrameUs / 1000), (unsigned long)r->late_frames);
            out.add("compose avg %s max %s  tiles max %s", Ms(r->compose_avg_us, 1).s, Ms(r->compose_max_us, 1).s,
                    Ms(r->tiles_max_us, 1).s);
            out.add("tft avg %s max %s ms", Ms(r->tft_avg_us, 1).s, Ms(r->tft_max_us, 1).s);
            out.add("cadence avg %s max %s ms", Ms(r->cadence_avg_us, 1).s, Ms(r->cadence_max_us, 1).s);
        } else {
            out.add("(none drawn: stay in the game to measure them)");
        }
        out.add("input %lu shown avg %s max %s ms", (unsigned long)r->shown, Ms(r->input_avg_us, 1).s,
                Ms(r->input_max_us, 1).s);
        out.add("key handling max %s ms  game busy %lu%%", Ms(r->handle_max_us, 1).s, pct(r->busy_permille));
    }

    if (const SystemPerfSnapshot *s = in.system) {
        out.add("-- System --");
        if (s->valid) {
            out.add("CPU0 %lu%%  CPU1 %lu%%  (FreeRTOS run time)", pct(s->core_busy_permille[0]),
                    pct(s->core_busy_permille[1]));
            out.add("audio %lu%% main %lu%% input %lu%% sdlog %lu%%", pct(s->audio_permille),
                    pct(s->main_permille), pct(s->input_permille), pct(s->sdlog_permille));
            out.add("other tasks %lu%% (of one core)", pct(s->other_permille));
        } else {
            out.add("per-task CPU: no run-time statistics");
        }
        out.add("heap int %lu (min %lu) B", (unsigned long)s->heap_internal_free, (unsigned long)s->heap_internal_min);
        out.add("heap PSRAM %lu (min %lu) B", (unsigned long)s->heap_psram_free, (unsigned long)s->heap_psram_min);
        out.add("stack free B: main %lu audio %lu input %lu", (unsigned long)s->stack_main_free,
                (unsigned long)s->stack_audio_free, (unsigned long)s->stack_input_free);
    }
    out.add("OPL2 %lu Hz -> %lu Hz out, ring %lux%lu", (unsigned long)kOplClockHz, (unsigned long)kSfxOutputRateHz,
            (unsigned long)kAudioRingBlocks, (unsigned long)kAudioBlockFrames);
    if (in.has_guard)
        out.add("A3-04 guard %lu ns/read = %s ms/8 ms blk", (unsigned long)in.guard_ns,
                Ms(in.guard_us_per_block, 1).s);
    return out.count();
}

} // namespace openu5
