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
// ContentionCounters (A3-04C, ALPHA3_AUDIO.md section 20)
// ===========================================================================
void ContentionCounters::reset(uint64_t now_us) {
    *this = ContentionCounters{};
    start_us_ = now_us;
}

void ContentionCounters::on_frame(uint32_t logic_us, uint32_t tft_us, const TftTiming &t) {
    ++frames_;
    logic_sum_ += logic_us;
    if (logic_us > logic_max_) logic_max_ = logic_us;
    // Viewport frames against the rest: the whole 176-px viewport is ~28,000
    // pixels in 158 row transactions; a panel or animated-cell frame is a
    // fraction of that. Different workloads, so they are not averaged together.
    if (t.viewport_full) {
        ++viewport_frames_;
        viewport_tft_sum_ += tft_us;
        if (tft_us > viewport_tft_max_) viewport_tft_max_ = tft_us;
    } else {
        ++panel_frames_;
        panel_tft_sum_ += tft_us;
        if (tft_us > panel_tft_max_) panel_tft_max_ = tft_us;
    }
    // A3-04E: the full-screen repaint (leaving the Developer screen, the first
    // game frame) apart from the viewport frames a walk draws.
    if (t.full_screen) {
        ++full_frames_;
        full_tft_sum_ += tft_us;
        if (tft_us > full_tft_max_) full_tft_max_ = tft_us;
    } else if (t.viewport_full) {
        ++vp_only_frames_;
        vp_only_tft_sum_ += tft_us;
        if (tft_us > vp_only_tft_max_) vp_only_tft_max_ = tft_us;
    }
    if (!t.cpu_mhz) return;
    ++timed_frames_;
    cpu_mhz_ = t.cpu_mhz;
    const uint32_t xfer_us = uint32_t(t.xfer_cycles / t.cpu_mhz);
    const uint32_t yield_us = uint32_t(t.yield_cycles / t.cpu_mhz);
    // What is left of show_alpha: building rows, panel text, window setup.
    fill_sum_ += tft_us > xfer_us + yield_us ? tft_us - xfer_us - yield_us : 0;
    xfer_sum_ += xfer_us;
    yield_sum_ += yield_us;
    if (xfer_us > xfer_frame_max_) xfer_frame_max_ = xfer_us;
    if (yield_us > yield_frame_max_) yield_frame_max_ = yield_us;
    transactions_ += t.transactions;
    rows_ += t.rows;
    rows_idle_ += t.rows_idle;
    rows_busy_ += t.rows_busy;
    yields_ += t.yields;
    if (t.xfer_max_cycles > xfer_max_cycles_) xfer_max_cycles_ = t.xfer_max_cycles;
    if (t.yield_max_cycles > yield_max_cycles_) yield_max_cycles_ = t.yield_max_cycles;
    slow_ += t.slow_xfers;
    slow_busy_ += t.slow_xfers_busy;
    slow_sd_ += t.slow_xfers_sd;
    if (t.xfer_max_sd_cycles > xfer_max_sd_cycles_) xfer_max_sd_cycles_ = t.xfer_max_sd_cycles;
    late_yields_ += t.late_yields;
    fill_idle_cycles_ += t.fill_cycles_idle;
    fill_busy_cycles_ += t.fill_cycles_busy;
    xfer_idle_cycles_ += t.xfer_cycles_idle;
    xfer_busy_cycles_ += t.xfer_cycles_busy;
    if (t.viewport_full) {
        ++timed_viewport_frames_;
        viewport_yields_ += t.yields;
        viewport_yield_sum_ += yield_us;
    }
}

void ContentionCounters::on_loop(uint32_t loop_us) {
    ++loops_;
    loop_sum_ += loop_us;
    if (loop_us > loop_max_) loop_max_ = loop_us;
}

void ContentionCounters::on_loop_wait(uint32_t wait_us, bool input) {
    ++loop_waits_;
    loop_wait_sum_ += wait_us;
    if (wait_us > loop_wait_max_) loop_wait_max_ = wait_us;
    if (input) ++loop_input_wakes_;
}

void ContentionCounters::snapshot(uint64_t now_us, ContentionSnapshot &out) const {
    out = ContentionSnapshot{};
    out.window_us = now_us > start_us_ ? uint32_t(now_us - start_us_) : 0;
    out.frames = frames_;
    if (frames_) {
        out.logic_avg_us = uint32_t(logic_sum_ / frames_);
        out.logic_max_us = logic_max_;
    }
    out.viewport_frames = viewport_frames_;
    if (viewport_frames_) {
        out.viewport_tft_avg_us = uint32_t(viewport_tft_sum_ / viewport_frames_);
        out.viewport_tft_max_us = viewport_tft_max_;
    }
    out.panel_frames = panel_frames_;
    if (panel_frames_) {
        out.panel_tft_avg_us = uint32_t(panel_tft_sum_ / panel_frames_);
        out.panel_tft_max_us = panel_tft_max_;
    }
    out.timed = timed_frames_ > 0;
    if (timed_frames_) {
        const auto cap = [](uint64_t v) { return uint32_t(v > UINT32_MAX ? UINT32_MAX : v); };
        out.tft_fill_avg_us = uint32_t(fill_sum_ / timed_frames_);
        out.tft_xfer_avg_us = uint32_t(xfer_sum_ / timed_frames_);
        out.tft_xfer_max_us = xfer_frame_max_;
        out.tft_yield_avg_us = uint32_t(yield_sum_ / timed_frames_);
        out.tft_yield_max_us = yield_frame_max_;
        out.transactions = cap(transactions_);
        out.rows = cap(rows_);
        out.rows_idle = cap(rows_idle_);
        out.rows_busy = cap(rows_busy_);
        out.yields = cap(yields_);
        out.xfer_max_us = xfer_max_cycles_ / cpu_mhz_;
        out.yield_max_us = yield_max_cycles_ / cpu_mhz_;
        out.slow_xfers = slow_;
        out.slow_xfers_busy = slow_busy_;
        out.slow_xfers_sd = slow_sd_;
        out.xfer_max_sd_us = xfer_max_sd_cycles_ / cpu_mhz_;
        out.late_yields = late_yields_;
        // Per-row averages in 0.1 us: the same row code, split by what the
        // other core was doing -- the direct test of cross-core contention.
        const auto per_row_x10 = [&](uint64_t cycles, uint64_t rows) {
            return rows ? cap(cycles * 10 / cpu_mhz_ / rows) : 0u;
        };
        out.row_fill_idle_x10 = per_row_x10(fill_idle_cycles_, rows_idle_);
        out.row_fill_busy_x10 = per_row_x10(fill_busy_cycles_, rows_busy_);
        out.row_xfer_idle_x10 = per_row_x10(xfer_idle_cycles_, rows_idle_);
        out.row_xfer_busy_x10 = per_row_x10(xfer_busy_cycles_, rows_busy_);
    }
    out.loops = loops_;
    if (loops_) {
        out.loop_avg_us = uint32_t(loop_sum_ / loops_);
        out.loop_max_us = loop_max_;
    }
    // A3-04E
    const auto cap32 = [](uint64_t v) { return uint32_t(v > UINT32_MAX ? UINT32_MAX : v); };
    out.full_frames = full_frames_;
    if (full_frames_) {
        out.full_tft_avg_us = uint32_t(full_tft_sum_ / full_frames_);
        out.full_tft_max_us = full_tft_max_;
    }
    out.vp_only_frames = vp_only_frames_;
    if (vp_only_frames_) {
        out.vp_only_tft_avg_us = uint32_t(vp_only_tft_sum_ / vp_only_frames_);
        out.vp_only_tft_max_us = vp_only_tft_max_;
    }
    if (timed_viewport_frames_) {
        out.viewport_yields_x10 = cap32(viewport_yields_ * 10 / timed_viewport_frames_);
        out.viewport_yield_avg_us = cap32(viewport_yield_sum_ / timed_viewport_frames_);
    }
    out.yield_total_us = cap32(yield_sum_);
    out.loop_waits = loop_waits_;
    out.loop_input_wakes = loop_input_wakes_;
    out.loop_wait_total_us = cap32(loop_wait_sum_);
    if (loop_waits_) {
        out.loop_wait_avg_us = uint32_t(loop_wait_sum_ / loop_waits_);
        out.loop_wait_max_us = loop_wait_max_;
    }
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

/** A share as a whole percentage, rounded; 0 when there is no whole. */
unsigned long share(uint64_t part, uint64_t whole) {
    return whole ? (unsigned long)((part * 100 + whole / 2) / whole) : 0ul;
}

/** "12.3" from a value in tenths. */
struct Tenths {
    char s[16];
    explicit Tenths(uint32_t x10) {
        std::snprintf(s, sizeof s, "%lu.%lu", (unsigned long)(x10 / 10), (unsigned long)(x10 % 10));
    }
};

/** A3-04D: the SD diagnostic log's state; nothing when no logger reported one (the A3-04C label). */
const char *sd_log_suffix(SdLogState state) {
    switch (state) {
    case SdLogState::On: return " sdlog ON";
    case SdLogState::Off: return " sdlog OFF";
    case SdLogState::Unavailable: return " sdlog n/a";
    case SdLogState::NotReported: break;
    }
    return "";
}

/** What was playing and at which settings -- at most 51 characters (one report row). */
void scenario_text(char *out, size_t cap, const PerfScenario &sc, const AudioPerfSnapshot *audio) {
    if (!sc.music_available)
        std::snprintf(out, cap, "music n/a  sfx %u%%%s%s", unsigned(sc.sfx_volume), sc.synth_bypass ? " BYPASS" : "",
                      sd_log_suffix(sc.sd_log));
    else
        std::snprintf(out, cap, "music %u%% %.14s sfx %u%%%s%s", unsigned(sc.music_volume),
                      audio && audio->music_active ? music_song_title(audio->song) : "(silent)",
                      unsigned(sc.sfx_volume), sc.synth_bypass ? " BYPASS" : "", sd_log_suffix(sc.sd_log));
}

/** Passes per second over the window. */
unsigned long per_second(uint32_t count, uint32_t window_us) {
    return window_us ? (unsigned long)(uint64_t(count) * 1000000u / window_us) : 0ul;
}

/** A3-04E (section 22): which pacing ran -- legacy behaviour in upper case. */
void pacing_text(char *out, size_t cap, const PerfScenario *sc) {
    if (!sc || !sc->pacing_reported)
        std::snprintf(out, cap, "pacing not reported");
    else
        std::snprintf(out, cap, "tft %s  loop %s", tft_pacing_name(sc->pacing.tft), loop_pacing_name(sc->pacing.loop));
}

/** A3-04E: what the pacing cost -- the repaint classes, the pauses, the loop's waits. */
void pacing_section(Lines &out, const PerfReportInput &in) {
    out.add("-- Pacing (A3-04E) --");
    char pace[48];
    pacing_text(pace, sizeof pace, in.scenario);
    out.add("%.51s", pace);
    const ContentionSnapshot *c = in.contention;
    if (!c) return;
    out.add("full-screen %lu frm tft avg %s max %s", (unsigned long)c->full_frames, Ms(c->full_tft_avg_us, 1).s,
            Ms(c->full_tft_max_us, 1).s);
    out.add("viewport w/o full %lu frm avg %s max %s", (unsigned long)c->vp_only_frames,
            Ms(c->vp_only_tft_avg_us, 1).s, Ms(c->vp_only_tft_max_us, 1).s);
    out.add("pauses/vp frm %s = %s ms  all %s ms", Tenths(c->viewport_yields_x10).s, Ms(c->viewport_yield_avg_us, 1).s,
            Ms(c->yield_total_us, 1).s);
    out.add("loop %lu/s  waits %lu avg %s max %s ms", per_second(c->loops, c->window_us), (unsigned long)c->loop_waits,
            Ms(c->loop_wait_avg_us, 1).s, Ms(c->loop_wait_max_us, 1).s);
    // Seconds with one decimal: Ms() of a millisecond count.
    out.add("loop asleep %s of %s s  input wakes %lu", Ms(c->loop_wait_total_us / 1000, 1).s,
            Ms(c->window_us / 1000, 1).s, (unsigned long)c->loop_input_wakes);
}

void contention_section(Lines &out, const PerfReportInput &in) {
    out.add("-- Contention map (A3-04C) --");
    if (in.scenario) {
        char sc[64];
        scenario_text(sc, sizeof sc, *in.scenario, in.audio);
        out.add("%.51s", sc);
    }
    if (const ContentionSnapshot *c = in.contention) {
        out.add("logic avg %s max %s  loop max %s ms", Ms(c->logic_avg_us, 1).s, Ms(c->logic_max_us, 1).s,
                Ms(c->loop_max_us, 1).s);
        out.add("viewport %lu frm tft avg %s max %s", (unsigned long)c->viewport_frames,
                Ms(c->viewport_tft_avg_us, 1).s, Ms(c->viewport_tft_max_us, 1).s);
        out.add("other %lu frm tft avg %s max %s", (unsigned long)c->panel_frames, Ms(c->panel_tft_avg_us, 1).s,
                Ms(c->panel_tft_max_us, 1).s);
        if (c->timed) {
            out.add("tft/frame fill %s xfer %s yield %s", Ms(c->tft_fill_avg_us, 1).s, Ms(c->tft_xfer_avg_us, 1).s,
                    Ms(c->tft_yield_avg_us, 1).s);
            out.add("yield %lu max %s ms late %lu (tick 10)", (unsigned long)c->yields, Ms(c->yield_max_us, 1).s,
                    (unsigned long)c->late_yields);
            out.add("xfer max %s ms slow %lu (%lu w/audio)", Ms(c->xfer_max_us, 2).s, (unsigned long)c->slow_xfers,
                    (unsigned long)c->slow_xfers_busy);
            // A3-04D: the slow ones the SD-log writer was in a burst for (the card shares this bus).
            out.add("slow in sd-log burst %lu max %s ms", (unsigned long)c->slow_xfers_sd,
                    Ms(c->xfer_max_sd_us, 1).s);
            out.add("rows %lu  audio running at %lu%%", (unsigned long)c->rows,
                    share(c->rows_busy, uint64_t(c->rows_busy) + c->rows_idle));
            out.add("row fill us: audio idle %s busy %s", Tenths(c->row_fill_idle_x10).s,
                    Tenths(c->row_fill_busy_x10).s);
            out.add("row xfer us: audio idle %s busy %s", Tenths(c->row_xfer_idle_x10).s,
                    Tenths(c->row_xfer_busy_x10).s);
        } else {
            out.add("(no TFT timing in this window)");
        }
    }
    if (const AudioPerfSnapshot *a = in.audio) {
        const uint32_t sfx_mix = a->render_avg_us > a->music_avg_us ? a->render_avg_us - a->music_avg_us : 0;
        out.add("audio task/blk avg %s max %s ms", Ms(a->task_busy_avg_us, 2).s, Ms(a->task_busy_max_us, 2).s);
        out.add("sfx+mix avg %s write avg %s max %s", Ms(sfx_mix, 2).s, Ms(a->write_avg_us, 1).s,
                Ms(a->write_max_us, 1).s);
    }
    if (const SdLogPerf *sd = in.sdlog) {
        if (!sd->valid)
            out.add("sd log: not running");
        else if (sd->off) // A3-04D: switched off -- the counters are this window's, not stale
            out.add("sd log OFF: %lu bursts max %s total %s ms", (unsigned long)sd->bursts, Ms(sd->max_us, 1).s,
                    Ms(sd->busy_us, 1).s);
        else
            out.add("sd log %lu bursts max %s total %s ms", (unsigned long)sd->bursts, Ms(sd->max_us, 1).s,
                    Ms(sd->busy_us, 1).s);
    }
    if ((in.scenario && in.scenario->pacing_reported) || in.contention) pacing_section(out, in);
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
    if (in.scenario || in.contention || in.sdlog) contention_section(out, in);
    out.add("OPL2 %lu Hz -> %lu Hz out, ring %lux%lu", (unsigned long)kOplClockHz, (unsigned long)kSfxOutputRateHz,
            (unsigned long)kAudioRingBlocks, (unsigned long)kAudioBlockFrames);
    if (in.has_guard)
        out.add("A3-04 guard %lu ns/read = %s ms/8 ms blk", (unsigned long)in.guard_ns,
                Ms(in.guard_us_per_block, 1).s);
    return out.count();
}

// ===========================================================================
// format_contention_line (A3-04C) -- one heartbeat line, fixed field order
// ===========================================================================
namespace {
// Appends to a bounded line; a field that does not fit is cut, never overrun.
class LineOut {
  public:
    LineOut(char *out, size_t cap) : out_(out), cap_(cap) { out_[0] = 0; }
    __attribute__((format(printf, 2, 3))) void add(const char *format, ...) {
        if (n_ + 1 >= cap_) return;
        va_list args;
        va_start(args, format);
        const int w = std::vsnprintf(out_ + n_, cap_ - n_, format, args);
        va_end(args);
        if (w > 0) n_ = n_ + size_t(w) < cap_ ? n_ + size_t(w) : cap_ - 1;
    }
    size_t size() const { return n_; }

  private:
    char *out_;
    size_t cap_, n_ = 0;
};
} // namespace

size_t format_contention_line(const PerfReportInput &in, char *out, size_t cap) {
    if (!out || !cap) return 0;
    LineOut line(out, cap);
    // Field legend (ALPHA3_AUDIO.md section 20.5): a/b = avg/max ms; n:a/b =
    // count:avg/max; xfer/fill per row in us as audio-idle/audio-busy.
    // A3-04D (section 21): insd=n:max -- slow TFT transactions with an SD-log
    // burst in progress, and the longest; sd=OFF:... while the log is off.
    char sc[64] = "?";
    if (in.scenario) scenario_text(sc, sizeof sc, *in.scenario, in.audio);
    line.add("scen=[%s]", sc);
    const uint32_t window = in.render ? in.render->window_us : in.contention ? in.contention->window_us : 0;
    line.add(" win=%lu.%lus", (unsigned long)(window / 1000000), (unsigned long)((window / 100000) % 10));
    if (const RenderPerfSnapshot *r = in.render)
        line.add(" | frame n=%lu avg=%s p95=%s max=%s late=%lu | cmp=%s/%s tiles=%s tft=%s/%s in=%lu:%s",
                 (unsigned long)r->frames, Ms(r->frame_avg_us, 1).s, Ms(r->frame_p95_us, 1).s,
                 Ms(r->frame_max_us, 1).s, (unsigned long)r->late_frames, Ms(r->compose_avg_us, 1).s,
                 Ms(r->compose_max_us, 1).s, Ms(r->tiles_max_us, 1).s, Ms(r->tft_avg_us, 1).s,
                 Ms(r->tft_max_us, 1).s, (unsigned long)r->shown, Ms(r->input_max_us, 1).s);
    if (const ContentionSnapshot *c = in.contention) {
        line.add(" | logic=%s/%s vp=%lu:%s/%s oth=%lu:%s/%s", Ms(c->logic_avg_us, 1).s, Ms(c->logic_max_us, 1).s,
                 (unsigned long)c->viewport_frames, Ms(c->viewport_tft_avg_us, 1).s, Ms(c->viewport_tft_max_us, 1).s,
                 (unsigned long)c->panel_frames, Ms(c->panel_tft_avg_us, 1).s, Ms(c->panel_tft_max_us, 1).s);
        if (c->timed)
            line.add(" | split fill=%s xfer=%s yld=%s | yld n=%lu max=%s late=%lu | xfer max=%s slow=%lu/%lu"
                     " insd=%lu:%s | rows=%lu busy=%lu%% fill=%s/%s xfer=%s/%s",
                     Ms(c->tft_fill_avg_us, 1).s, Ms(c->tft_xfer_avg_us, 1).s, Ms(c->tft_yield_avg_us, 1).s,
                     (unsigned long)c->yields, Ms(c->yield_max_us, 1).s, (unsigned long)c->late_yields,
                     Ms(c->xfer_max_us, 2).s, (unsigned long)c->slow_xfers, (unsigned long)c->slow_xfers_busy,
                     (unsigned long)c->slow_xfers_sd, Ms(c->xfer_max_sd_us, 1).s,
                     (unsigned long)c->rows, share(c->rows_busy, uint64_t(c->rows_busy) + c->rows_idle),
                     Tenths(c->row_fill_idle_x10).s, Tenths(c->row_fill_busy_x10).s,
                     Tenths(c->row_xfer_idle_x10).s, Tenths(c->row_xfer_busy_x10).s);
        else
            line.add(" | split none");
        line.add(" | loop=%lu:%s/%s", (unsigned long)c->loops, Ms(c->loop_avg_us, 1).s, Ms(c->loop_max_us, 1).s);
    }
    if (const AudioPerfSnapshot *a = in.audio)
        line.add(" | audio blk=%lu rnd=%s/%s mus=%s/%s task=%s buf=%lu und=%lu hw=%lu miss=%lu clip=%lu",
                 (unsigned long)a->blocks, Ms(a->render_avg_us, 2).s, Ms(a->render_max_us, 2).s,
                 Ms(a->music_avg_us, 2).s, Ms(a->music_max_us, 2).s, Ms(a->task_busy_max_us, 2).s,
                 (unsigned long)(uint64_t(a->fill_min) * a->block_us / 1000), (unsigned long)a->underruns,
                 (unsigned long)a->hw_underruns, (unsigned long)a->missed_deadlines, (unsigned long)a->mix_clipped);
    else
        line.add(" | audio none");
    if (const SdLogPerf *sd = in.sdlog)
        if (sd->valid)
            line.add(" | sd=%s%lu:%s/%s", sd->off ? "OFF:" : "", (unsigned long)sd->bursts, Ms(sd->max_us, 1).s,
                     Ms(sd->busy_us, 1).s);
    if (const SystemPerfSnapshot *s = in.system)
        if (s->valid)
            line.add(" | cpu0=%lu cpu1=%lu main=%lu aud=%lu inp=%lu sdl=%lu", pct(s->core_busy_permille[0]),
                     pct(s->core_busy_permille[1]), pct(s->main_permille), pct(s->audio_permille),
                     pct(s->input_permille), pct(s->sdlog_permille));
    return line.size();
}

size_t format_pacing_line(const PerfReportInput &in, char *out, size_t cap) {
    if (!out || !cap) return 0;
    LineOut line(out, cap);
    // Legend (ALPHA3_AUDIO.md section 22): a/b = avg/max ms; n:a/b = count:avg/max;
    // full = full-screen repaints, vp = the viewport frames that were not; yld /vp =
    // draw-loop pauses per viewport frame; wait = the loop's idle waits, in = an input ended one.
    char pace[48];
    pacing_text(pace, sizeof pace, in.scenario);
    line.add("pace=[%s]", pace);
    const uint32_t window = in.render ? in.render->window_us : in.contention ? in.contention->window_us : 0;
    line.add(" win=%lu.%lus", (unsigned long)(window / 1000000), (unsigned long)((window / 100000) % 10));
    if (const RenderPerfSnapshot *r = in.render)
        line.add(" | frame n=%lu avg=%s max=%s | tft=%s/%s in=%lu:%s/%s", (unsigned long)r->frames,
                 Ms(r->frame_avg_us, 1).s, Ms(r->frame_max_us, 1).s, Ms(r->tft_avg_us, 1).s, Ms(r->tft_max_us, 1).s,
                 (unsigned long)r->shown, Ms(r->input_avg_us, 1).s, Ms(r->input_max_us, 1).s);
    if (const ContentionSnapshot *c = in.contention) {
        line.add(" | full=%lu:%s/%s vp=%lu:%s/%s", (unsigned long)c->full_frames, Ms(c->full_tft_avg_us, 1).s,
                 Ms(c->full_tft_max_us, 1).s, (unsigned long)c->vp_only_frames, Ms(c->vp_only_tft_avg_us, 1).s,
                 Ms(c->vp_only_tft_max_us, 1).s);
        line.add(" | yld n=%lu /vp=%s vpms=%s tot=%s max=%s late=%lu", (unsigned long)c->yields,
                 Tenths(c->viewport_yields_x10).s, Ms(c->viewport_yield_avg_us, 1).s, Ms(c->yield_total_us, 1).s,
                 Ms(c->yield_max_us, 2).s, (unsigned long)c->late_yields);
        line.add(" | loop n=%lu /s=%lu max=%s wait=%lu:%s/%s in=%lu asleep=%s", (unsigned long)c->loops,
                 per_second(c->loops, c->window_us), Ms(c->loop_max_us, 1).s, (unsigned long)c->loop_waits,
                 Ms(c->loop_wait_avg_us, 1).s, Ms(c->loop_wait_max_us, 1).s, (unsigned long)c->loop_input_wakes,
                 Ms(c->loop_wait_total_us, 1).s);
    }
    if (const AudioPerfSnapshot *a = in.audio)
        line.add(" | und=%lu hw=%lu miss=%lu", (unsigned long)a->underruns, (unsigned long)a->hw_underruns,
                 (unsigned long)a->missed_deadlines);
    if (const SystemPerfSnapshot *s = in.system)
        if (s->valid)
            line.add(" | cpu0=%lu cpu1=%lu main=%lu aud=%lu", pct(s->core_busy_permille[0]),
                     pct(s->core_busy_permille[1]), pct(s->main_permille), pct(s->audio_permille));
    return line.size();
}

} // namespace openu5
