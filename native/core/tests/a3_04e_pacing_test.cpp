// Alpha 3 A3-04E -- the game thread's pacing (ALPHA3_AUDIO.md section 22).
// Host side:
//
//   P  the policy: A3-04E yields where Alpha 2.0 slept, at the same cadence
//      (every 16 rows / 32 fill chunks); the loop's idle wait is a tick COUNT,
//      never 0, where pdMS_TO_TICKS(5) was 0 at 100 Hz; the gate
//   C  the counters: full-screen repaints apart from walking viewport frames,
//      pauses per viewport frame, the loop's idle waits
//   F  the report's pacing section, the A3E_PACE line, A3C_PERF unchanged,
//      everything fits
//   U  the two Developer rows, above the SD log; the older rows keep their
//      places counted from the end
//   S  the device wiring (source scans): the Board's pauses, main.cpp's wait,
//      the input peek, the runtime's gate and probes; task placement and the
//      tick rate untouched; nothing saved
//
//   a3_04e_pacing <native/core dir> [device dir]
#include "openu5/debug_labels.h"
#include "openu5/perf_report.h"
#include "openu5/render_pacing.h"
#include "openu5/ui_debug_menu.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
std::string slurp(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}
std::string strip_comments(const std::string &src) {
    std::string out;
    std::istringstream in(src);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto c = line.find("//");
        out += (c == std::string::npos ? line : line.substr(0, c)) + "\n";
    }
    return out;
}
size_t count_of(const std::string &hay, const std::string &needle) {
    size_t n = 0;
    for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + needle.size())) ++n;
    return n;
}
/** The body of the first function whose definition line contains `signature`. */
std::string function_body(const std::string &src, const std::string &signature) {
    const auto at = src.find(signature);
    if (at == std::string::npos) return {};
    const auto open = src.find('{', at);
    if (open == std::string::npos) return {};
    int depth = 0;
    for (size_t i = open; i < src.size(); ++i) {
        if (src[i] == '{') ++depth;
        else if (src[i] == '}' && --depth == 0) return src.substr(open, i - open + 1);
    }
    return {};
}
std::string join(const char (*lines)[kPerfReportLineBytes], size_t n) {
    std::string out;
    for (size_t i = 0; i < n; ++i) out += std::string(lines[i]) + "\n";
    return out;
}

/** Pauses in a row loop of `rows` rows (draw_rgb565_strided, draw_text_box, ...). */
int row_loop_pauses(int rows) {
    int n = 0;
    for (int row = 0; row < rows; ++row)
        if (tft_row_yield_due(row)) ++n;
    return n;
}
/** Pauses in fill_rect(w x h): chunks of 320 pixels (A3-04F; min(w, 320) up to A3-HF2.1). */
int fill_pauses(int w, int h) {
    (void)w;
    const int chunk = 320;
    int n = 0, chunks = 0;
    for (int remaining = w * h; remaining > 0; remaining -= chunk)
        if (tft_chunk_yield_due(++chunks)) ++n;
    return n;
}

// The Developer rows' services, recorded.
struct Probes {
    bool tft = false, loop = false;
    std::vector<std::string> calls;
    static bool tft_probe(void *p, bool toggle) {
        auto &s = *static_cast<Probes *>(p);
        s.calls.push_back(toggle ? "tft!" : "tft?");
        if (toggle) s.tft = !s.tft;
        return s.tft;
    }
    static bool loop_probe(void *p, bool toggle) {
        auto &s = *static_cast<Probes *>(p);
        s.calls.push_back(toggle ? "loop!" : "loop?");
        if (toggle) s.loop = !s.loop;
        return s.loop;
    }
    static SdLogState sd(void *p, bool toggle) {
        static_cast<Probes *>(p)->calls.push_back(toggle ? "sd!" : "sd?");
        return SdLogState::Off;
    }
    static bool bypass(void *p, bool toggle) {
        static_cast<Probes *>(p)->calls.push_back(toggle ? "bypass!" : "bypass?");
        return false;
    }
    static void perf(void *p) { static_cast<Probes *>(p)->calls.push_back("perf"); }
    static void stats(void *p) { static_cast<Probes *>(p)->calls.push_back("stats"); }
    static void tone(void *p) { static_cast<Probes *>(p)->calls.push_back("tone"); }
};
UiAction pick(int i) {
    UiAction a;
    a.kind = UiActionKind::SelectIndex;
    a.index = i;
    return a;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: a3_04e_pacing <native/core dir> [device dir]\n");
        return 2;
    }
    const std::string core_dir = argv[1];
    const std::string device_dir = argc > 2 ? argv[2] : core_dir + "/../targets/tdeck";

    // ======================================================================
    // P -- the policy
    // ======================================================================
    {
        check(kPacingDefault.tft == TftPacing::Yield && kPacingDefault.loop == LoopPacing::IdleWait &&
                  kPacingLegacy.tft == TftPacing::TickSleep && kPacingLegacy.loop == LoopPacing::Spin,
              "P1 production is A3-04E (yield, idle wait); the legacy policy is what every image up to A3-04D did");
        bool same = true;
        for (int row = 0; row < 400; ++row) same = same && tft_row_yield_due(row) == (row > 0 && (row & 15) == 0);
        for (int chunks = 1; chunks < 800; ++chunks) same = same && tft_chunk_yield_due(chunks) == ((chunks & 31) == 0);
        check(same, "P2 the cooperative points are Alpha 2.0's exactly: after rows 16, 32, ... and after every 32nd "
                    "fill chunk (row>0&&(row&15)==0, (++chunks&31)==0)");
        check(tft_pause_ticks(TftPacing::TickSleep) == 1 && tft_pause_ticks(TftPacing::Yield) == 0,
              "P3 the pause: legacy sleeps one tick (vTaskDelay(1): until the next 10 ms tick), A3-04E none (taskYIELD)");
        check(legacy_loop_delay_ticks(100) == 0 && legacy_loop_delay_ticks(200) == 1 &&
                  legacy_loop_delay_ticks(1000) == 5,
              "P4 the defect, pinned: the old loop's pdMS_TO_TICKS(5) is 0 ticks at the device's 100 Hz -- not a "
              "sleep, a reschedule (it would be 1 tick at 200 Hz, 5 at 1000 Hz)");
        check(kLoopIdleWaitTicks >= 1 && loop_wait_ticks(LoopPacing::IdleWait, true) == kLoopIdleWaitTicks,
              "P5 no zero-tick regression: the A3-04E idle wait is a tick count (1), not milliseconds, so no tick rate "
              "rounds it to 0");
        check(loop_wait_ticks(LoopPacing::IdleWait, false) == 0 && loop_wait_ticks(LoopPacing::Spin, true) == 0 &&
                  loop_wait_ticks(LoopPacing::Spin, false) == 0,
              "P6 the gate: no wait while the runtime has something paced relative to now, and none at all with the "
              "legacy spin");
        check(std::strcmp(tft_pacing_name(TftPacing::Yield), "yield") == 0 &&
                  std::strcmp(tft_pacing_name(TftPacing::TickSleep), "TICK") == 0 &&
                  std::strcmp(loop_pacing_name(LoopPacing::IdleWait), "idle-wait") == 0 &&
                  std::strcmp(loop_pacing_name(LoopPacing::Spin), "SPIN") == 0,
              "P7 names: the legacy behaviour in upper case");
        // The pauses of the two frame kinds, from the Board's geometry (the
        // runtime test counts them in the real Board code: Y1/Y2).
        const int step = row_loop_pauses(176 - 9 - 9);
        const int full = fill_pauses(320, 240) + row_loop_pauses(158) + fill_pauses(138, 240) + 2 * fill_pauses(180, 2) +
                         2 * fill_pauses(2, 180) + 2 * fill_pauses(137, 1) + 2 * fill_pauses(1, 52) +
                         2 * fill_pauses(137, 1) + 2 * fill_pauses(1, 32) + fill_pauses(137, 1);
        check(step == 9 && full == 19 && fill_pauses(2, 180) == 0 && fill_pauses(320, 240) == 7,
              "P8 derived from the geometry: a walking step's viewport (158 rows) pauses 9 times; the menu-exit repaint "
              "19 (clear 7 + viewport 9 + panel reflow 3; since A3-04F a 2 px x 180 frame side is two 320 px chunks, "
              "not 180 rows, and never pauses -- A3-04D: 37) -- at one 10 ms tick each, ~90 ms and ~190 ms of sleep");
    }

    // ======================================================================
    // G -- the idle-service guard (A3-04E.1, section 23)
    // ======================================================================
    {
        check(kIdleServiceBudgetUs * 25 <= 5000000u && kIdleServiceBudgetUs >= 10000u && kIdleServiceMaxSleeps >= 2,
              "G1 the budget: 200 ms, 25x under the 5 s task watchdog and above one 10 ms tick; up to 3 sleeps per "
              "enforcement (the first can end a microsecond later, at the next tick)");
        IdleServiceGuard g;
        g.reset(5, 1000000);
        bool quiet = true;
        uint32_t count = 5;
        for (uint64_t t = 1000000; t < 3000000; t += 50000) quiet = quiet && !g.due(++count, t); // it keeps running
        check(quiet && g.stats().services == 40 && g.stats().max_gap_us == 0,
              "G2 while the idle loop keeps running, the guard never asks for a sleep");
        const uint64_t last = 2950000;
        check(!g.due(count, last + kIdleServiceBudgetUs - 1) && g.due(count, last + kIdleServiceBudgetUs) &&
                  g.stats().max_gap_us == kIdleServiceBudgetUs,
              "G3 no pass for the budget: due exactly at 200 ms since the last seen pass, not a microsecond before");
        check(!g.due(count + 1, last + kIdleServiceBudgetUs + 10000),
              "G4 one idle pass (the counter moves) ends it at once");
        g.note_enforcement(1, 7300, true);
        g.note_enforcement(3, 30000, false);
        check(g.stats().enforcements == 2 && g.stats().forced_sleeps == 4 && g.stats().forced_us == 37300 &&
                  g.stats().unserviced == 1,
              "G5 the enforcements, their ticks and time, and the ones that gave up");
        g.reset_stats();
        check(g.stats().enforcements == 0 && g.stats().checks == 0 &&
                  !g.due(count + 1, last + kIdleServiceBudgetUs + 20000) && g.stats().services == 0,
              "G6 a new window clears the counters but keeps the service state (no false alarm after a read)");
    }

    // ======================================================================
    // C -- the counters
    // ======================================================================
    {
        ContentionCounters k;
        k.reset(1000);
        TftTiming full{}, step{}, cell{};
        full.cpu_mhz = step.cpu_mhz = cell.cpu_mhz = 240;
        full.viewport_full = 1;
        full.full_screen = 1;
        full.yields = 37;
        full.yield_cycles = 370000ull * 240;
        step.viewport_full = 1;
        step.yields = 9;
        step.yield_cycles = 88000ull * 240;
        cell.rows = 16;
        k.on_frame(1000, 404000, full);
        for (int i = 0; i < 4; ++i) k.on_frame(1000, 120000 + i * 1000, step);
        k.on_frame(1000, 5000, cell);
        ContentionSnapshot c{};
        k.snapshot(61000, c);
        check(c.full_frames == 1 && c.full_tft_max_us == 404000 && c.vp_only_frames == 4 &&
                  c.vp_only_tft_avg_us == 121500 && c.vp_only_tft_max_us == 123000 && c.viewport_frames == 5 &&
                  c.viewport_tft_max_us == 404000 && c.panel_frames == 1,
              "C1 the full-screen repaint is counted apart from the walking viewport frames (and still, as in "
              "A3-04C, as a viewport frame)");
        check(c.viewport_yields_x10 == (37 + 4 * 9) * 10 / 5 && c.viewport_yield_avg_us == (370000 + 4 * 88000) / 5 &&
                  c.yield_total_us == 370000 + 4 * 88000,
              "C2 pauses per viewport frame (14.6) and their time, and the window's total");
        for (int i = 0; i < 100; ++i) k.on_loop_wait(9000 + uint32_t(i % 3) * 500, i % 10 == 0);
        k.snapshot(61000, c);
        check(c.loop_waits == 100 && c.loop_wait_max_us == 10000 && c.loop_input_wakes == 10 &&
                  c.loop_wait_total_us == 100 * 9000 + 33 * 500 + 33 * 1000 && c.loop_wait_avg_us == c.loop_wait_total_us / 100,
              "C3 the loop's idle waits: count, avg, max, total, and how many an input ended");
        k.reset(62000);
        k.snapshot(62000, c);
        check(c.loop_waits == 0 && c.full_frames == 0 && c.vp_only_frames == 0 && c.yield_total_us == 0,
              "C4 a new window starts empty");
    }

    // ======================================================================
    // F -- the report's pacing section, A3E_PACE, A3C_PERF unchanged
    // ======================================================================
    {
        RenderPerfSnapshot render{};
        render.window_us = 60000000;
        render.frames = 400;
        render.frame_avg_us = 79200;
        render.frame_max_us = 443000;
        render.tft_avg_us = 41100;
        render.tft_max_us = 404400;
        render.shown = 120;
        render.input_avg_us = 150000;
        render.input_max_us = 450000;
        ContentionSnapshot c{};
        c.window_us = 60000000;
        c.frames = 400;
        c.timed = true;
        c.viewport_frames = 41;
        c.full_frames = 1;
        c.full_tft_avg_us = c.full_tft_max_us = 404400;
        c.vp_only_frames = 40;
        c.vp_only_tft_avg_us = 122000;
        c.vp_only_tft_max_us = 130000;
        c.viewport_yields_x10 = 90;
        c.viewport_yield_avg_us = 88000;
        c.yield_total_us = 3520000;
        c.yields = 400;
        c.loops = 5000;
        c.loop_max_us = 450000;
        c.loop_waits = 4800;
        c.loop_wait_avg_us = 9800;
        c.loop_wait_max_us = 10200;
        c.loop_wait_total_us = 47000000;
        c.loop_input_wakes = 12;
        PerfScenario sc{};
        sc.sfx_volume = 80;
        sc.sd_log = SdLogState::Off;
        sc.pacing_reported = true;
        PerfReportInput in{};
        in.title = "AUDIO/RENDER PERF  live window";
        in.render = &render;
        in.scenario = &sc;
        in.contention = &c;
        char lines[kPerfReportMaxLines][kPerfReportLineBytes]{};
        std::string text = join(lines, format_perf_report(in, lines, kPerfReportMaxLines));
        check(text.find("-- Pacing (A3-04E) --\ntft yield  loop idle-wait\n"
                        "full-screen 1 frm tft avg 404.4 max 404.4\n"
                        "viewport w/o full 40 frm avg 122.0 max 130.0\n"
                        "pauses/vp frm 9.0 = 88.0 ms  all 3520.0 ms\n"
                        "loop 83/s  waits 4800 avg 9.8 max 10.2 ms\n"
                        "loop asleep 47.0 of 60.0 s  input wakes 12\n") != std::string::npos,
              "F1 the report's pacing section:\n" + text);
        check(text.find("idle0 gap") == std::string::npos, "F1b no idle-service window attached (the host): no line");
        IdleServiceStats idle{};
        idle.max_gap_us = 200900;
        idle.enforcements = idle.forced_sleeps = 30;
        idle.forced_us = 274200;
        in.idle = &idle;
        text = join(lines, format_perf_report(in, lines, kPerfReportMaxLines));
        check(text.find("loop asleep 47.0 of 60.0 s  input wakes 12\nidle0 gap max 200.9 ms forced 30/274.2 ms miss 0\n") !=
                  std::string::npos,
              "F1c A3-04E.1: the section ends with core 0's longest idle gap and what the guard slept for it");
        sc.pacing = kPacingLegacy;
        text = join(lines, format_perf_report(in, lines, kPerfReportMaxLines));
        check(text.find("-- Pacing (A3-04E) --\ntft TICK  loop SPIN\n") != std::string::npos,
              "F2 the legacy pacing is named as such");
        sc.pacing_reported = false;
        text = join(lines, format_perf_report(in, lines, kPerfReportMaxLines));
        check(text.find("-- Pacing (A3-04E) --\npacing not reported\n") != std::string::npos,
              "F3 no pacing reported: said, not guessed");
        sc.pacing_reported = true;
        sc.pacing = kPacingDefault;

        // The longest report the device makes: the benchmark (two audio phases, the guard) with everything.
        AudioPerfSnapshot a{};
        a.music_active = true;
        a.song = MusicSong::Theme;
        a.window_us = 4000000000u;
        a.blocks = 4000000000u;
        a.render_avg_us = a.render_p99_us = a.render_max_us = a.music_avg_us = a.music_max_us = 4000000000u;
        a.missed_deadlines = a.underruns = a.hw_underruns = a.mix_clipped = 4000000000u;
        SystemPerfSnapshot sys{};
        sys.valid = true;
        SdLogPerf sd{};
        sd.valid = true;
        c.full_tft_max_us = c.vp_only_tft_max_us = c.loop_wait_max_us = 4000000000u;
        c.loops = c.loop_waits = c.loop_input_wakes = 4000000000u;
        c.loop_wait_total_us = c.yield_total_us = 4000000000u;
        in.audio = &a;
        in.audio_heading = "Music alone (idle)";
        in.audio2 = &a;
        in.audio2_heading = "Music + render";
        in.system = &sys;
        in.sdlog = &sd;
        in.has_guard = true;
        in.guard_ns = 4000000000u;
        in.guard_us_per_block = 4000000000u;
        const size_t n = format_perf_report(in, lines, kPerfReportMaxLines);
        bool fit = true;
        for (size_t i = 0; i < n; ++i) fit = fit && std::strlen(lines[i]) <= 51;
        text = join(lines, n);
        check(fit && n < kPerfReportMaxLines && text.find("A3-04 guard ") != std::string::npos &&
                  text.find("-- Pacing (A3-04E) --") != std::string::npos,
              "F4 the longest report (the benchmark: both phases, the guard, maximal values) keeps every row on the "
              "51-column screen and its last line in the buffer (" + std::to_string(n) + " of " +
              std::to_string(kPerfReportMaxLines) + " lines)");

        char pace[kPacingLineBytes];
        size_t pn = format_pacing_line(in, pace, sizeof pace);
        std::string pl = pace;
        check(pn == pl.size() && pl.find("pace=[tft yield  loop idle-wait] win=60.0s") == 0 &&
                  pl.find(" | frame n=400 ") != std::string::npos && pl.find(" | full=1:") != std::string::npos &&
                  pl.find(" | yld n=400 /vp=9.0 ") != std::string::npos && pl.find(" | loop n=") != std::string::npos &&
                  pl.find(" wait=") != std::string::npos && pl.find(" | und=") != std::string::npos &&
                  pl.find(" | cpu0=") != std::string::npos,
              "F5 the A3E_PACE line: the pacing, frames, the repaint / viewport split, the pauses, the loop's waits, "
              "audio health, CPU:\n    " + pl);
        // Every numeric field at its maximum: the line still ends in the buffer.
        render.frames = render.frame_avg_us = render.frame_max_us = render.tft_avg_us = render.tft_max_us = 4000000000u;
        render.shown = render.input_avg_us = render.input_max_us = render.window_us = 4000000000u;
        c.window_us = c.full_frames = c.full_tft_avg_us = c.vp_only_frames = c.vp_only_tft_avg_us = 4000000000u;
        c.yields = c.viewport_yields_x10 = c.viewport_yield_avg_us = c.yield_max_us = c.late_yields = 4000000000u;
        c.loop_max_us = c.loop_wait_avg_us = 4000000000u;
        sys.core_busy_permille[0] = sys.core_busy_permille[1] = sys.main_permille = sys.audio_permille = 4000000000u;
        idle.max_gap_us = idle.enforcements = idle.forced_sleeps = idle.unserviced = 4000000000u;
        idle.forced_us = 4000000000ull * 1000;
        in.heartbeat = 4000000000u;
        sc.pacing = kPacingLegacy;
        char roomy[2048];
        pn = format_pacing_line(in, roomy, sizeof roomy); // the whole line, whatever the buffer
        pl = roomy;
        check(pn == pl.size() && pn < kPacingLineBytes - 1 && pl.find(" aud=") != std::string::npos &&
                  pl.find("hb=4000000000 pace=") == 0 && pl.find(" | idle0 gap=") != std::string::npos &&
                  pn + 64 < kSdDiagLineBytes,
              "F5b worst case " + std::to_string(pn) + " characters: whole, inside the buffer (" +
                  std::to_string(kPacingLineBytes) + ") and inside one SD-log record with its prefix (" +
                  std::to_string(kSdDiagLineBytes) + ")");
        char tiny[32];
        std::memset(tiny, 0x5a, sizeof tiny);
        const size_t tn = format_pacing_line(in, tiny, 24);
        check(tn == 23 && tiny[23] == 0 && tiny[24] == 0x5a, "F6 a short buffer is filled, NUL-terminated, never overrun");

        char with[kContentionLineBytes], without[kContentionLineBytes];
        format_contention_line(in, with, sizeof with);
        PerfScenario plain = sc;
        plain.pacing_reported = false;
        plain.pacing = PacingPolicy{};
        in.scenario = &plain;
        format_contention_line(in, without, sizeof without);
        check(std::strcmp(with, without) == 0,
              "F7 A3C_PERF is byte-identical whatever the pacing: its format stays A3-04C/D's, so old and new runs "
              "diff field by field (the pacing has its own line)");
    }

    // ======================================================================
    // U -- the Developer rows
    // ======================================================================
    {
        GameState g;
        TurnState t;
        TravelState tr;
        CommandState cs;
        static uint8_t large[65536]{};
        WorldData w{large, large, sizeof(large), sizeof(large)};
        CommandContext ctx{g, t, tr, cs, w};
        UiDebugMenu menu(ctx);
        Probes probes;
        UiDiagnosticsServices s{};
        s.context = &probes;
        s.legacy_tft_pacing = Probes::tft_probe;
        s.legacy_loop_spin = Probes::loop_probe;
        s.sd_log = Probes::sd;
        s.music_bypass = Probes::bypass;
        s.audio_perf = Probes::perf;
        s.audio_stats = Probes::stats;
        s.audio_test = Probes::tone;
        menu.attach_diagnostics(s);
        menu.open();
        menu.handle_input(pick(int(UiDebugCategory::Diagnostics)));
        const size_t gcount = debug_diagnostic_group_count(), rows = menu.view().count;
        check(rows == gcount + 8 && std::string(menu.row_label(gcount + 1)) == "Probe: legacy TFT pacing: off" &&
                  std::string(menu.row_label(gcount + 2)) == "Probe: legacy loop spin: off",
              "U1 Diagnostics has two more rows, directly above the SD log: \"" + std::string(menu.row_label(gcount + 1)) +
                  "\", \"" + std::string(menu.row_label(gcount + 2)) + "\"");
        check(rows - (gcount + 1) == 7 && std::string(menu.row_label(rows - 5)) == "Probe: SD diag logging: off" &&
                  std::string(menu.row_label(rows - 4)) == "Probe: synth bypass: off" &&
                  std::string(menu.row_label(rows - 3)) == "Audio/render performance" &&
                  std::string(menu.row_label(rows - 2)) == "Audio/render stats (live)" &&
                  std::string(menu.row_label(rows - 1)) == "Audio test tone (SFX)",
              "U2 counted from the end the older rows are where they were: tone 1, stats 2, benchmark 3, bypass 4, "
              "SD log 5; the new rows are 6 (loop) and 7 (TFT) up");
        probes.calls.clear();
        menu.handle_input(pick(int(gcount + 1)));
        const bool tft_on = probes.tft && std::string(menu.row_label(gcount + 1)) == "Probe: legacy TFT pacing: ON";
        menu.handle_input(pick(int(gcount + 2)));
        const bool loop_on = probes.loop && std::string(menu.row_label(gcount + 2)) == "Probe: legacy loop spin: ON";
        size_t tft_toggles = 0, loop_toggles = 0;
        for (const auto &call : probes.calls) {
            tft_toggles += call == "tft!";
            loop_toggles += call == "loop!";
        }
        check(tft_on && loop_on && tft_toggles == 1 && loop_toggles == 1,
              "U3 Enter toggles each through its own service, and the row reads ON");
        probes.calls.clear();
        for (size_t r = gcount + 3; r < rows; ++r) menu.handle_input(pick(int(r)));
        std::string order;
        for (const auto &call : probes.calls)
            if (call.back() == '!' || call == "perf" || call == "stats" || call == "tone") order += call + " ";
        check(order == "sd! bypass! perf stats tone ",
              "U4 the older rows still do what they did (\"" + order + "\")");
        UiDebugMenu bare(ctx);
        bare.open();
        bare.handle_input(pick(int(UiDebugCategory::Diagnostics)));
        bare.handle_input(pick(int(gcount + 1)));
        check(std::string(bare.row_label(gcount + 1)) == "Probe: legacy TFT pacing: off",
              "U5 no service attached: the row reads off and Enter does nothing");
    }

    // ======================================================================
    // S -- the device wiring (source scans; the device code does not run here)
    // ======================================================================
    {
        const std::string board = strip_comments(slurp(device_dir + "/main/tdeck_board.cpp"));
        const std::string board_h = strip_comments(slurp(device_dir + "/main/tdeck_board.h"));
        const std::string yield = function_body(board, "void Board::tft_yield(");
        check(!board.empty() && yield.find("tft_pause_ticks(tft_pacing_)") != std::string::npos &&
                  std::regex_search(yield, std::regex(R"(vTaskDelay\(\s*ticks\s*\)[\s\S]*taskYIELD\(\))")) &&
                  board_h.find("TftPacing tft_pacing_ = openu5::kPacingDefault.tft") != std::string::npos,
              "S1 Board::tft_yield sleeps only when the policy says TickSleep (legacy) and otherwise yields; the Board "
              "starts at A3-04E's pacing");
        // A4-END1 adds the fifth loop: the ending's full-screen page (show_endgame_page).
        check(count_of(board, "if(openu5::tft_row_yield_due(row))tft_yield();") == 4 &&
                  count_of(board, "if(openu5::tft_chunk_yield_due(++chunks))tft_yield();") == 1 &&
                  board.find("(row&15)") == std::string::npos && board.find("&31)") == std::string::npos &&
                  count_of(board, "tft_yield();") == 5,
              "S2 all five draw loops pause through the policy's cadence and nowhere else");
        const std::string show = function_body(board, "esp_err_t Board::show_alpha(");
        check(std::regex_search(show, std::regex(R"(\+\+tft_timing_\.full_screen;ESP_RETURN_ON_ERROR\(fill_rect\(0,0,kDisplayWidth,kDisplayHeight,kChromeBand\),kTag,"initialize Alpha 2\.0 game screen"\))")) &&
                  std::regex_search(show, std::regex(R"(\+\+tft_timing_\.full_screen;ESP_RETURN_ON_ERROR\(fill_rect\(0,0,kDisplayWidth,kDisplayHeight,kChromeBand\),kTag,"leave developer screen"\))")) &&
                  count_of(show, "full_screen") == 2,
              "S3 both gameplay full-screen repaints are marked (the first game frame, leaving the Developer screen); "
              "entering the Developer screen is not a gameplay frame (Alpha 4 UI Batch 1: they paint the frame band)");

        const std::string main_cpp = strip_comments(slurp(device_dir + "/main/main.cpp"));
        const auto loop_at = main_cpp.find("for(;;)");
        const std::string loop = loop_at == std::string::npos ? std::string() : function_body(main_cpp.substr(loop_at), "for(;;)");
        check(!loop.empty() && loop.find("pdMS_TO_TICKS") == std::string::npos &&
                  std::regex_search(loop, std::regex(R"(runtime\.loop_wait_ticks\(\))")) &&
                  std::regex_search(loop, std::regex(R"(input\.wait_for_event\(wait_ticks\))")) &&
                  loop.find("note_loop_wait(") != std::string::npos && loop.find("vTaskDelay(0)") != std::string::npos,
              "S4 main.cpp's loop: no milliseconds-to-ticks conversion left to round to 0; it waits the runtime's tick "
              "count on the input queue, records the wait, and reschedules (vTaskDelay(0), as before) only when told 0");
        check(loop.find("esp_rom_delay_us") == std::string::npos &&
                  !std::regex_search(loop, std::regex(R"(while\s*\([^)]*esp_timer_get_time)")),
              "S5 no busy-wait in the loop");
        const std::string input = strip_comments(slurp(device_dir + "/main/tdeck_input.cpp"));
        const std::string wait = function_body(input, "bool InputHardware::wait_for_event(");
        check(std::regex_search(wait, std::regex(R"(xQueuePeek\(event_queue_,\s*&event,\s*ticks\))")) &&
                  wait.find("xQueueReceive") == std::string::npos,
              "S6 the input wait PEEKS: the event stays queued for poll(), which counts and logs it");

        const std::string rt = strip_comments(slurp(device_dir + "/main/alpha_runtime.cpp"));
        const std::string rt_h = strip_comments(slurp(device_dir + "/main/alpha_runtime.h"));
        const std::string render = function_body(rt, "esp_err_t AlphaRuntime::render(");
        const auto set_at = render.find("board.set_tft_pacing(pacing_.tft);");
        check(set_at != std::string::npos && set_at < render.find("show_frontend") && set_at < render.find("show_alpha") &&
                  set_at < render.find("service_audio_benchmark"),
              "S7 render() hands the Board the pacing before anything can draw, in every mode");
        const std::string gate = function_body(rt, "bool AlphaRuntime::loop_may_sleep(");
        bool gate_complete = !gate.empty();
        for (const char *term : {"dirty_", "dungeon_presentation_pending_", "next_enemy_step_us_", "blackthorn_pacer_.active()",
                                 "blackthorn_pacer_.mounted()", "narrative_pacer_.active()", "narrative_pacer_.mounted()",
                                 "poison_.active()", "camp_scene_active_", "audio_bench_.running()", "smoke_.view().running"})
            gate_complete = gate_complete && gate.find(term) != std::string::npos;
        check(gate_complete,
              "S8 the gate consults every service scheduled relative to 'now' (the enemy beat, both scene pacers, the "
              "poison flash, Camp, the benchmark, the smoke tests) and a frame still owed");
        const std::string bind = function_body(rt, "void AlphaRuntime::bind_developer_diagnostics(");
        check(bind.find("services.legacy_tft_pacing=legacy_tft_probe") != std::string::npos &&
                  bind.find("services.legacy_loop_spin=legacy_loop_probe") != std::string::npos &&
                  rt_h.find("PacingPolicy pacing_ = openu5::kPacingDefault") != std::string::npos,
              "S9 the probes are bound by the one production binder; the runtime starts at A3-04E");
        const std::string metrics = function_body(rt, "void AlphaRuntime::log_metrics(");
        check(std::regex_search(metrics, std::regex(R"(A3C_PERF[\s\S]*A3E_PACE)")),
              "S10 the heartbeat logs A3E_PACE after A3C_PERF");

        const std::string audio = strip_comments(slurp(device_dir + "/main/tdeck_audio.cpp"));
        const std::string sdk = slurp(device_dir + "/sdkconfig");
        check(audio.find("xTaskCreatePinnedToCore(task_entry, \"openu5-audio\", kTaskStackBytes, this, 3, &task_, 1)") !=
                      std::string::npos &&
                  std::regex_search(input, std::regex(R"(xTaskCreatePinnedToCore\(capture_task_entry, "openu5-input", 4096, this, 4,\s*&capture_task_, 0\))")) &&
                  std::regex_search(sdk, std::regex("CONFIG_FREERTOS_HZ=100\r?\n")) &&
                  sdk.find("CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0=y") != std::string::npos,
              "S11 preserved: the audio task (prio 3, core 1), the input task (prio 4, core 0), the game thread on "
              "core 0, the 100 Hz tick -- A3-04E changes no task, core or tick rate");
        bool unsaved = true;
        for (const char *f : {"/main/alpha_save.cpp", "/main/alpha_save_generation.cpp"})
            unsaved = unsaved && strip_comments(slurp(device_dir + f)).find("pacing") == std::string::npos;
        for (const char *f : {"/src/persistence.cpp", "/src/frontend_settings.cpp", "/src/save_json.cpp"})
            unsaved = unsaved && strip_comments(slurp(core_dir + f)).find("pacing") == std::string::npos;
        check(unsaved, "S12 the pacing is never saved: no save, settings or persistence code names it");

        // A3-04E.1 (section 23): the idle-service guarantee, wired.
        const std::string idle_cpp = strip_comments(slurp(device_dir + "/main/idle_service.cpp"));
        const std::string enforce = function_body(idle_cpp, "bool IdleService::enforce(");
        const std::string hook = function_body(idle_cpp, "bool IdleService::core0_hook(");
        check(std::regex_search(enforce, std::regex(R"(guard_\.due\([\s\S]*while \(sleeps < openu5::kIdleServiceMaxSleeps\)[\s\S]*vTaskDelay\(1\);)")) &&
                  enforce.find("taskYIELD") == std::string::npos && enforce.find("vTaskDelay(0)") == std::string::npos &&
                  hook.find("return true;") != std::string::npos,
              "S13 enforce() BLOCKS (vTaskDelay(1), bounded) only when the guard says so -- never a yield, which "
              "cannot hand core 0 to the idle task; the hook counts and lets the core idle");
        check(std::regex_search(main_cpp, std::regex(R"(esp_register_freertos_idle_hook_for_cpu\(&tdeck::IdleService::core0_hook,0\))")) &&
                  main_cpp.find("idle_service.attach(tdeck::IdleService::core0_count());") != std::string::npos &&
                  main_cpp.find("board.set_idle_service(&idle_service);") != std::string::npos &&
                  main_cpp.find("runtime.attach_idle_service(&idle_service);") != std::string::npos,
              "S14 main.cpp registers the counting hook on core 0 (the core the watchdog's IDLE0 check and the game "
              "thread share) and hands the guard to the Board and the report");
        check(std::regex_search(loop, std::regex(R"(else vTaskDelay\(0\);[^\n]*\n\s*idle_service\.enforce\(\);\s*\}\s*$)")),
              "S15 every loop pass ends with the guard, after the idle wait or the reschedule, whichever ran");
        check(yield.find("else if (!(idle_ && idle_->enforce())) taskYIELD();") != std::string::npos,
              "S16 the draw loops' yield asks the guard first (a tick sleep only when the idle loop is overdue)");
        check(std::regex_search(sdk, std::regex("CONFIG_ESP_TASK_WDT_TIMEOUT_S=5\r?\n")) &&
                  std::regex_search(sdk, std::regex("CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y\r?\n")) &&
                  std::regex_search(sdk, std::regex("CONFIG_ESP_TASK_WDT_EN=y\r?\n")),
              "S17 the watchdog is neither disabled nor extended: 5 s, IDLE0 checked, as before");
    }

    std::printf("A3-04E pacing: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
