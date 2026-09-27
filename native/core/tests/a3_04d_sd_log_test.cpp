// Alpha 3 A3-04D -- the SD diagnostic log's switch (ALPHA3_AUDIO.md section 21).
// Host side:
//
//   D  the policy: OFF at boot (kSdDiagLoggingDefault), switchable only when a
//      card and writer exist, never "on" after a card error
//   C  the ESP-IDF log hook's body (SdDiagLog::mirror, the device's own code):
//      serial gets EVERY call in every state; the card copy is queued only
//      while logging is on
//   W  the writer's wakes (SdDiagLog::wake, the device's own code) against a
//      fake card that records every operation: while off, the card is never
//      touched (no open, write, flush or close, whatever the wakes); switching
//      on opens and writes exactly as A3-04C did; switching off writes what
//      was captured and closes once; rotation, drops, card errors
//   K  the contention counters' new SD attribution (slow TFT transactions with
//      an SD-log burst in progress, and the longest)
//   F  the scenario label ("sdlog ON/OFF/n/a"), the report and the A3C_PERF
//      line say the log's state; A3-04C's text is unchanged when no logger
//      reports one; the longest label and line still fit
//   S  the device wiring (source scans): the writer sleeps without a timeout
//      while idle, serial always forwarded, the default not overridden, the
//      Board attributes slow transactions, and nothing unrelated on the card
//      (saves, settings, smoke tests, the packs) depends on the log
//
//   a3_04d_sd_log <native/core dir>
#include "openu5/perf_report.h"
#include "openu5/sd_diag_log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
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

// ---- the fakes -------------------------------------------------------------
/** A card that records every operation; `content` is what reached the file. */
struct FakeCard final : SdDiagFile {
    std::vector<std::string> ops;
    std::string content;       // bytes written since the last open/rotate (the stdio side)
    size_t existing = 0;       // the log's size on the card when opened
    bool fail_open = false, fail_write = false, fail_flush = false;
    int opens = 0, rotates = 0, writes = 0, flushes = 0, closes = 0;
    bool open(size_t &bytes) override {
        ops.push_back("open");
        ++opens;
        if (fail_open) return false;
        bytes = existing;
        return true;
    }
    bool rotate() override {
        ops.push_back("rotate");
        ++rotates;
        content.clear();
        return true;
    }
    bool write(const void *data, size_t size) override {
        ops.push_back("write");
        ++writes;
        if (fail_write) return false;
        content.append(static_cast<const char *>(data), size);
        return true;
    }
    bool flush() override {
        ops.push_back("flush");
        ++flushes;
        return !fail_flush;
    }
    void close() override {
        ops.push_back("close");
        ++closes;
    }
    size_t card_ops() const { return ops.size(); }
};

struct FakeQueue final : SdDiagQueue {
    std::deque<SdDiagLine> lines;
    size_t capacity = 48;
    bool push(const SdDiagLine &l) override {
        if (lines.size() >= capacity) return false;
        lines.push_back(l);
        return true;
    }
    bool pop(SdDiagLine &l) override {
        if (lines.empty()) return false;
        l = lines.front();
        lines.pop_front();
        return true;
    }
};

// The serial sink: every line it was handed, formatted.
std::vector<std::string> g_serial;
int serial_sink(const char *format, va_list args) {
    char buf[1024];
    const int n = std::vsnprintf(buf, sizeof buf, format, args);
    g_serial.emplace_back(buf);
    return n;
}
uint64_t g_now_ms = 0;
uint64_t test_clock() { return g_now_ms; }

int log_call(SdDiagLog &log, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int r = log.mirror(&serial_sink, &test_clock, format, args);
    va_end(args);
    return r;
}

/**
 * The device writer's loop (sd_diagnostic_logger.cpp writer_task) for `ms`
 * of virtual time: while idle it sleeps without a timeout (no wake at all);
 * otherwise each wake takes a queued line or times out after 250 ms.
 */
struct WriterRun {
    int wakes = 0, bursts = 0, idle_sleeps = 0;
};
WriterRun run_writer(SdDiagLog &log, FakeQueue &queue, uint64_t ms) {
    WriterRun r{};
    const uint64_t end = g_now_ms + ms;
    SdDiagLine line{};
    while (g_now_ms < end) {
        if (log.idle()) {
            ++r.idle_sleeps;
            g_now_ms = end; // asleep until switched on
            break;
        }
        const bool received = queue.pop(line);
        if (!received) g_now_ms += 250;
        ++r.wakes;
        if (log.wake(received ? &line : nullptr, g_now_ms).burst) ++r.bursts;
    }
    return r;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: a3_04d_sd_log <native/core dir>\n");
        return 2;
    }
    const std::string core_dir = argv[1];
    const std::string device_dir = argc > 2 ? argv[2] : core_dir + "/../targets/tdeck";

    // ======================================================================
    // D -- the policy
    // ======================================================================
    {
        FakeCard card;
        FakeQueue queue;
        SdDiagLog log(card, queue);
        check(!kSdDiagLoggingDefault && !log.enabled() && log.state() == SdLogState::Unavailable &&
                  !log.set_enabled(true) && !log.enabled(),
              "D1 before the card is mounted and the writer runs: unavailable, and switching on is refused");
        log.set_available(true);
        check(!log.enabled() && !log.capturing() && log.idle() && log.state() == SdLogState::Off,
              "D2 at boot (the writer running, the card mounted) SD diagnostic logging is OFF, capturing nothing, "
              "the writer idle");
        check(log.set_enabled(true) && log.enabled() && log.capturing() && !log.idle() &&
                  log.state() == SdLogState::On,
              "D3 the Developer switch turns it on: capturing, the writer awake");
        check(log.set_enabled(false) && !log.enabled() && !log.capturing() && log.state() == SdLogState::Off,
              "D4 ... and off again");
        check(card.card_ops() == 0, "D5 none of that touched the card by itself (the writer does, at its wake)");
    }

    // ======================================================================
    // C -- the ESP-IDF log hook's body: serial always, the card copy only when on
    // ======================================================================
    {
        FakeCard card;
        FakeQueue queue;
        SdDiagLog log(card, queue);
        log.set_available(true);
        g_serial.clear();
        g_now_ms = 1234;
        int serial_bytes = 0;
        for (int i = 0; i < 200; ++i)
            serial_bytes += log_call(log, "I (%d) AlphaRuntime: PRESENTATION_DISPATCH ui=%s\n", i, "Exploration");
        check(g_serial.size() == 200 && g_serial[7] == "I (7) AlphaRuntime: PRESENTATION_DISPATCH ui=Exploration\n" &&
                  serial_bytes > 200 * 40 && queue.lines.empty() && log.dropped() == 0,
              "C1 logging OFF: all 200 log calls reach serial, formatted exactly; nothing is queued for the card");
        log.set_enabled(true);
        log_call(log, "I (%d) AlphaRuntime: %s\n", 300, "A3C_PERF scen=[x]");
        check(g_serial.size() == 201 && queue.lines.size() == 1 && queue.lines[0].timestamp_ms == 1234 &&
                  std::string(queue.lines[0].text, queue.lines[0].length) == "I (300) AlphaRuntime: A3C_PERF scen=[x]\n",
              "C2 logging ON: serial still gets the call, and the card copy is queued with its timestamp");
        queue.capacity = 1;
        log_call(log, "second\n");
        log_call(log, "third\n");
        check(g_serial.size() == 203 && queue.lines.size() == 1 && log.dropped() == 2,
              "C3 a full queue drops the card copy (counted), never the serial one, never waits");
        log.set_enabled(false);
        log_call(log, "after off\n");
        check(g_serial.size() == 204 && g_serial.back() == "after off\n" && queue.lines.size() == 1 &&
                  log.dropped() == 2,
              "C4 switched off again: serial continues, the card copy stops at once");
    }

    // ======================================================================
    // W -- the writer against a fake card
    // ======================================================================
    {
        // W1: OFF from boot through a minute of gameplay logging.
        FakeCard card;
        FakeQueue queue;
        SdDiagLog log(card, queue);
        log.set_available(true);
        g_now_ms = 10'000;
        g_serial.clear();
        int bursts = 0;
        for (int i = 0; i < 600; ++i) { // 60 s: a frame line every 100 ms, the heartbeat every 5 s
            log_call(log, "I (%d) AlphaRuntime: PRESENTATION_DISPATCH ui=Exploration\n", i);
            if (i % 50 == 0) log_call(log, "I (%d) AlphaRuntime: A3C_PERF scen=[music 80%% sfx 80%% sdlog OFF]\n", i);
            if (i % 50 == 0) log_call(log, "I (%d) OpenU5-Input: KEYBOARD_METRICS polls=1\n", i);
            const auto r = run_writer(log, queue, 100);
            bursts += r.bursts;
        }
        // Even a stray wake (a timeout, or a line somehow queued) must not touch the card.
        SdDiagLine stray{};
        stray.length = 5;
        std::memcpy(stray.text, "stray", 5);
        const auto w1 = log.wake(&stray, g_now_ms);
        const auto w2 = log.wake(nullptr, g_now_ms + 5000);
        check(card.card_ops() == 0 && bursts == 0 && !w1.burst && !w2.burst && log.idle() && queue.lines.empty() &&
                  g_serial.size() == 624,
              "W1 logging OFF for 60 s of gameplay logging: the card is never touched (0 opens, 0 writes, 0 "
              "flushes, 0 closes, 0 bursts), the writer stays idle, serial got all " +
                  std::to_string(g_serial.size()) + " lines");

        // W2: switched on -- the writer opens the log, then writes exactly as A3-04C did.
        card.existing = 1000;
        log.set_enabled(true);
        g_now_ms = 100'000;
        const auto open_wake = run_writer(log, queue, 250);
        check(card.opens == 1 && card.writes == 0 && card.flushes == 0 && log.file_open() && open_wake.bursts == 1,
              "W2 switched on: the next wake opens the log for append (one burst), nothing else");
        g_now_ms = 100'300;
        log_call(log, "I (1) T: first line\n");
        g_now_ms = 100'350;
        log_call(log, "I (2) T: two\nphysical lines\n");
        run_writer(log, queue, 1);
        check(card.content == "[0000100300] I (1) T: first line\n[0000100350] I (2) T: two\n[0000100350] physical lines\n" &&
                  card.flushes == 0,
              "W3 every physical line gets the [%010llu] prefix; ordinary lines are only buffered (no flush yet)");
        g_now_ms = 100'400;
        log_call(log, "I (3) Alpha: INPUT_EDGE raw=01\n");
        run_writer(log, queue, 1);
        check(card.flushes == 1, "W4 an important line (INPUT_EDGE) flushes at once, as in A3-04C");
        // Periodic flush: 2 s after the last one, even with nothing new (A3-04C counted those wakes too).
        const int before = card.flushes;
        const auto quiet = run_writer(log, queue, 1500);
        const int after_quiet = card.flushes;
        const auto due = run_writer(log, queue, 1000);
        check(after_quiet == before && quiet.bursts == 0 && card.flushes == before + 1 && due.bursts == 1,
              "W5 while on: no flush before 2 s have passed, then the periodic flush (one burst), as in A3-04C");

        // W6: switched off -- what was captured is written, the log closed once, then nothing.
        log_call(log, "I (4) T: captured while on\n");
        log.set_enabled(false);
        log_call(log, "I (5) T: after off\n");
        const size_t ops_before_close = card.card_ops();
        const auto off_run = run_writer(log, queue, 60'000);
        check(card.content.find("captured while on") != std::string::npos &&
                  card.content.find("after off") == std::string::npos && card.closes == 1 && !log.file_open() &&
                  log.idle() && off_run.idle_sleeps == 1 && off_run.bursts == 1 &&
                  card.ops.back() == "close" && card.card_ops() == ops_before_close + 4,
              "W6 switched off: the next wake writes what was captured while on, closes the log once (the "
              "directory entry gets the size), then the writer sleeps without a timeout for the next 60 s");
        const size_t ops_after = card.card_ops();
        for (int i = 0; i < 100; ++i) log.wake(nullptr, g_now_ms + uint64_t(i) * 250);
        check(card.card_ops() == ops_after, "W7 ... and further wakes touch nothing");

        // W8: on again -- reopened (append), not recreated.
        log.set_enabled(true);
        run_writer(log, queue, 250);
        check(card.opens == 2 && card.rotates == 0 && log.file_open(), "W8 switched on again: reopened for append");
    }
    {
        // W9: rotation -- a full log on open becomes the archive; so does one that fills.
        FakeCard card;
        FakeQueue queue;
        SdDiagLog log(card, queue);
        log.set_available(true);
        card.existing = kSdDiagMaximumBytes;
        log.set_enabled(true);
        g_now_ms = 0;
        run_writer(log, queue, 250);
        const int rot_at_open = card.rotates;
        const std::string big(700, 'x');
        for (int round = 0; round < 30; ++round) { // 1,200 lines of 714 B with their prefix: 857 KB
            for (int i = 0; i < 40; ++i) log_call(log, "%s\n", big.c_str());
            run_writer(log, queue, 1);
        }
        check(rot_at_open == 1 && card.rotates == 2 && card.content.size() == 466u * 714u && log.dropped() == 0,
              "W9 a full log on open becomes the archive at once; one that fills rotates at the 512 KiB cap "
              "(line 735 of 1,200), as in A3-04C");
    }
    {
        // W10: drops -- the notice is written and flushed.
        FakeCard card;
        FakeQueue queue;
        queue.capacity = 2;
        SdDiagLog log(card, queue);
        log.set_available(true);
        log.set_enabled(true);
        g_now_ms = 50'000;
        run_writer(log, queue, 250);
        for (int i = 0; i < 5; ++i) log_call(log, "line %d\n", i);
        const int flushes = card.flushes;
        run_writer(log, queue, 1);
        check(card.content.find("SD_LOG_QUEUE_DROPPED count=3\n") != std::string::npos && card.flushes == flushes + 1,
              "W10 dropped lines are reported in the log (SD_LOG_QUEUE_DROPPED count=3) and flushed");
    }
    {
        // W11: a card error switches the log off for good; serial continues.
        FakeCard card;
        FakeQueue queue;
        SdDiagLog log(card, queue);
        log.set_available(true);
        log.set_enabled(true);
        g_now_ms = 0;
        run_writer(log, queue, 250);
        card.fail_write = true;
        log_call(log, "doomed\n");
        const auto w = log.wake(nullptr, g_now_ms);
        SdDiagLine l{};
        queue.pop(l);
        const auto fw = log.wake(&l, g_now_ms);
        g_serial.clear();
        log_call(log, "still on serial\n");
        check(!w.failed && fw.failed && log.failed() && !log.enabled() && !log.capturing() && !log.file_open() &&
                  card.closes == 1 && log.idle() && log.state() == SdLogState::Unavailable &&
                  !log.set_enabled(true) && g_serial.size() == 1 && queue.lines.empty(),
              "W11 a card write error closes the log, switches it off for good (n/a, cannot be switched on), "
              "and serial logging continues");
        FakeCard bad;
        FakeQueue q2;
        SdDiagLog log2(bad, q2);
        log2.set_available(true);
        bad.fail_open = true;
        log2.set_enabled(true);
        const auto ow = log2.wake(nullptr, 0);
        check(ow.failed && log2.failed() && log2.idle() && bad.opens == 1 && bad.closes == 0,
              "W12 a log that cannot be opened (no directory, write-protected card) fails the same way");
    }

    // ======================================================================
    // K -- the SD attribution in the contention counters
    // ======================================================================
    ContentionCounters cc;
    {
        cc.reset(0);
        TftTiming t{};
        t.cpu_mhz = 240;
        t.transactions = 160;
        t.rows = 158;
        t.xfer_cycles = 240ull * 900000;
        t.xfer_max_cycles = 240u * 812300;
        t.slow_xfers = 3;
        t.slow_xfers_busy = 1;
        t.slow_xfers_sd = 2;
        t.xfer_max_sd_cycles = 240u * 812300;
        t.viewport_full = 1;
        cc.on_frame(500, 950000, t);
        TftTiming u = t;
        u.slow_xfers_sd = 1;
        u.xfer_max_sd_cycles = 240u * 40000;
        cc.on_frame(500, 60000, u);
        TftTiming v = t;
        v.slow_xfers = v.slow_xfers_sd = 0;
        v.xfer_max_sd_cycles = 0;
        cc.on_frame(500, 30000, v);
        ContentionSnapshot c{};
        cc.snapshot(20'000'000, c);
        check(c.slow_xfers == 6 && c.slow_xfers_sd == 3 && c.xfer_max_sd_us == 812300 && c.xfer_max_us == 812300,
              "K1 slow TFT transactions during SD-log bursts add up across frames (3 of 6); the longest is kept "
              "(812.3 ms)");
        ContentionCounters fresh;
        fresh.reset(0);
        ContentionSnapshot z{};
        fresh.snapshot(1, z);
        check(z.slow_xfers_sd == 0 && z.xfer_max_sd_us == 0, "K2 a new window starts at zero");
    }

    // ======================================================================
    // F -- the scenario label, the report and the A3C_PERF line
    // ======================================================================
    {
        ContentionSnapshot contention{};
        cc.snapshot(20'000'000, contention);
        RenderPerfSnapshot render{};
        render.window_us = 20'000'000;
        render.frames = 250;
        SdLogPerf sd{};
        sd.valid = true;
        sd.bursts = 3;
        sd.max_us = 1'159'000;
        sd.busy_us = 1'500'000;
        AudioPerfSnapshot audio{};
        audio.music_active = true;
        audio.song = MusicSong::Theme;
        PerfScenario sc{};
        sc.music_available = true;
        sc.music_volume = 80;
        sc.sfx_volume = 80;
        PerfReportInput in{};
        in.title = "AUDIO/RENDER PERF  live window";
        in.audio = &audio;
        in.render = &render;
        in.scenario = &sc;
        in.contention = &contention;
        in.sdlog = &sd;
        char lines[kPerfReportMaxLines][kPerfReportLineBytes]{};
        auto report = [&]() { return join(lines, format_perf_report(in, lines, kPerfReportMaxLines)); };
        char line[kContentionLineBytes];
        auto one_line = [&]() {
            format_contention_line(in, line, sizeof line);
            return std::string(line);
        };

        const std::string a3c = report();
        const std::string a3c_line = one_line();
        check(a3c.find("\nmusic 80% Ultima V Theme sfx 80%\n") != std::string::npos &&
                  a3c_line.find("scen=[music 80% Ultima V Theme sfx 80%]") == 0 &&
                  a3c.find("sd log 3 bursts max 1159.0 total 1500.0 ms\n") != std::string::npos &&
                  a3c_line.find(" | sd=3:1159.0/1500.0") != std::string::npos,
              "F1 no logger reporting a state (NotReported): A3-04C's label and SD-log text, unchanged");

        sc.sd_log = SdLogState::On;
        const std::string on = report();
        const std::string on_line = one_line();
        check(on.find("\nmusic 80% Ultima V Theme sfx 80% sdlog ON\n") != std::string::npos &&
                  on_line.find("scen=[music 80% Ultima V Theme sfx 80% sdlog ON]") == 0 &&
                  on.find("sd log 3 bursts max 1159.0 total 1500.0 ms\n") != std::string::npos,
              "F2 logging ON: the report and the A3C_PERF line say \"sdlog ON\"");

        sc.sd_log = SdLogState::Off;
        sd.off = true;
        sd.bursts = 0;
        sd.max_us = sd.busy_us = 0;
        const std::string off = report();
        const std::string off_line = one_line();
        check(off.find("\nmusic 80% Ultima V Theme sfx 80% sdlog OFF\n") != std::string::npos &&
                  off.find("sd log OFF: 0 bursts max 0.0 total 0.0 ms\n") != std::string::npos &&
                  off_line.find("scen=[music 80% Ultima V Theme sfx 80% sdlog OFF]") == 0 &&
                  off_line.find(" | sd=OFF:0:0.0/0.0") != std::string::npos,
              "F3 logging OFF: \"sdlog OFF\" in the label, and the SD-log counters say OFF instead of looking "
              "stale:\n    " + off_line.substr(off_line.find(" | sd=")));

        sc.sd_log = SdLogState::Unavailable;
        sc.music_available = false;
        check(report().find("\nmusic n/a  sfx 80% sdlog n/a\n") != std::string::npos,
              "F4 no card / no writer: \"sdlog n/a\"");
        sc.music_available = true;

        check(on.find("\nslow in sd-log burst 3 max 812.3 ms\n") != std::string::npos &&
                  on.find("\nxfer max 812.30 ms slow 6 (3 w/audio)\n") != std::string::npos &&
                  on_line.find(" | xfer max=812.30 slow=6/3 insd=3:812.3 | rows=") != std::string::npos,
              "F5 the SD attribution is on the report (its own line; A3-04C's lines unchanged) and in the "
              "A3C_PERF line (insd=count:max)");

        // The longest label: 100 % volumes, the Theme, BYPASS and sdlog OFF -- one whole report row.
        sc.music_volume = sc.sfx_volume = 100;
        sc.synth_bypass = true;
        sc.sd_log = SdLogState::Off;
        const std::string longest = report();
        check(longest.find("\nmusic 100% Ultima V Theme sfx 100% BYPASS sdlog OFF\n") != std::string::npos,
              "F6 the longest label (51 characters) is one whole report row");

        // The worst realistic line (as A3-04C's L2) with every A3-04D addition still fits.
        RenderPerfSnapshot r9 = render;
        r9.frames = 999999;
        r9.frame_avg_us = r9.frame_p95_us = r9.frame_max_us = r9.compose_avg_us = r9.compose_max_us = 9999900;
        r9.tiles_max_us = r9.tft_avg_us = r9.tft_max_us = r9.input_max_us = 9999900;
        r9.late_frames = r9.shown = 999999;
        ContentionSnapshot c9 = contention;
        c9.frames = c9.viewport_frames = c9.panel_frames = c9.yields = c9.late_yields = c9.slow_xfers = 999999;
        c9.slow_xfers_busy = c9.loops = c9.slow_xfers_sd = 999999;
        c9.rows = c9.rows_busy = c9.rows_idle = 99999999;
        c9.logic_avg_us = c9.logic_max_us = c9.viewport_tft_avg_us = c9.viewport_tft_max_us = 9999900;
        c9.panel_tft_avg_us = c9.panel_tft_max_us = c9.tft_fill_avg_us = c9.tft_xfer_avg_us = 9999900;
        c9.tft_yield_avg_us = c9.yield_max_us = c9.xfer_max_us = c9.loop_avg_us = c9.loop_max_us = 9999900;
        c9.xfer_max_sd_us = 9999900;
        c9.row_fill_idle_x10 = c9.row_fill_busy_x10 = c9.row_xfer_idle_x10 = c9.row_xfer_busy_x10 = 9999999;
        AudioPerfSnapshot a9 = audio;
        a9.blocks = a9.underruns = a9.hw_underruns = a9.missed_deadlines = a9.mix_clipped = 999999;
        a9.render_avg_us = a9.render_max_us = a9.music_avg_us = a9.music_max_us = a9.task_busy_max_us = 9999990;
        SdLogPerf s9 = sd;
        s9.off = true;
        s9.bursts = 999999;
        s9.max_us = s9.busy_us = 99999900;
        SystemPerfSnapshot y9{};
        y9.valid = true;
        y9.core_busy_permille[0] = y9.core_busy_permille[1] = y9.main_permille = y9.audio_permille = 1000;
        y9.input_permille = y9.sdlog_permille = 1000;
        PerfReportInput worst{};
        worst.audio = &a9;
        worst.render = &r9;
        worst.system = &y9;
        worst.scenario = &sc;
        worst.contention = &c9;
        worst.sdlog = &s9;
        const size_t nw = format_contention_line(worst, line, sizeof line);
        const std::string w(line);
        check(nw < kContentionLineBytes - 1 && w.find(" sdl=100") != std::string::npos &&
                  w.find("sdlog OFF]") != std::string::npos && w.find(" insd=999999:9999.9") != std::string::npos &&
                  nw + std::strlen("I (4294967295) AlphaRuntime: A3C_PERF \n") < kSdDiagLineBytes,
              "F7 the worst realistic A3C_PERF line with the A3-04D fields is whole (" + std::to_string(nw) +
                  " chars, ends with sdl=100) and still fits one SD-log record with its prefix");

        // The longest report (the benchmark's) still fits the Developer report buffer.
        in.audio2 = &audio;
        in.audio2_heading = "Music + cue/100 ms";
        in.has_guard = true;
        in.guard_ns = 900;
        in.guard_us_per_block = 10;
        SystemPerfSnapshot sys{};
        sys.valid = true;
        in.system = &sys;
        const size_t nl = format_perf_report(in, lines, kPerfReportMaxLines);
        const std::string all = join(lines, nl);
        check(nl < kPerfReportMaxLines && std::string(lines[nl - 1]).find("A3-04 guard") == 0 &&
                  all.find("\nslow in sd-log burst ") != std::string::npos &&
                  all.find("\nsd log OFF: 0 bursts max 0.0 total 0.0 ms\n") != std::string::npos,
              "F8 the longest report (benchmark + contention + the A3-04D lines: " + std::to_string(nl) + " of " +
                  std::to_string(kPerfReportMaxLines) + " lines) still fits; the last line is the guard's (nothing cut)");
    }

    // ======================================================================
    // S -- the device wiring (source scans; the device code does not run here)
    // ======================================================================
    {
        const std::string sdlog = strip_comments(slurp(device_dir + "/main/sd_diagnostic_logger.cpp"));
        const std::string writer = function_body(sdlog, "void writer_task(");
        const std::string hook = function_body(sdlog, "int mirror_vprintf(");
        const std::string init = function_body(sdlog, "bool initialize_storage(");
        const std::string enable = function_body(sdlog, "bool set_enabled(");
        check(std::regex_search(writer, std::regex(R"(if \(g_log\.idle\(\)\) \{\s*ulTaskNotifyTake\(pdTRUE, portMAX_DELAY\);\s*continue;\s*\}[\s\S]*xQueueReceive[\s\S]*xSemaphoreTake\(g_storage_mutex)")),
              "S1 while the log is idle (off and closed) the writer sleeps without a timeout, before its queue "
              "wait and before it takes the storage mutex: no periodic wake, no flush, no card access");
        check(std::regex_search(writer, std::regex(R"(xSemaphoreTake\(g_storage_mutex[\s\S]*g_burst_active = 1;[\s\S]*g_log\.wake\([\s\S]*g_burst_active = 0;[\s\S]*xSemaphoreGive)")) &&
                  count_of(sdlog, "fopen(") == 1 && count_of(sdlog, "fwrite(") == 1 && count_of(sdlog, "fflush(") == 2,
              "S2 every card access is the core writer's (g_log.wake, bracketed by the storage mutex and the "
              "burst flag); the file has one fopen / fwrite and the two flushes of CardLogFile");
        check(hook.find("g_log.mirror(g_serial_writer") != std::string::npos && count_of(sdlog, "xQueueSend(") == 1,
              "S3 the ESP-IDF log hook is SdDiagLog::mirror with the serial writer: serial always, the queue only "
              "through it");
        check(!init.empty() && init.find("open") == std::string::npos && init.find("rotate") == std::string::npos &&
                  sdlog.find("set_enabled(true)") == std::string::npos &&
                  sdlog.find("openu5::SdDiagLog g_log(g_card, g_lines);") != std::string::npos,
              "S4 the boot path no longer opens or rotates the log, and nothing overrides the default: the log "
              "starts as kSdDiagLoggingDefault (OFF)");
        check(std::regex_search(enable, std::regex(R"(g_log\.set_enabled\(on\)[\s\S]*if \(on && g_writer\) xTaskNotifyGive\(g_writer\);)")),
              "S5 switching on wakes the sleeping writer");

        const std::string board = strip_comments(slurp(device_dir + "/main/tdeck_board.cpp"));
        const std::string transmit = function_body(board, "esp_err_t Board::tft_transmit(");
        check(std::regex_search(transmit, std::regex(R"(sd_log_burst\(\)[\s\S]*spi_device_transmit[\s\S]*sd_log_burst\(\)[\s\S]*if \(sd0 \|\| sd1\) \{\s*\+\+t\.slow_xfers_sd;)")),
              "S6 every TFT transaction reads the writer's burst flag at both ends; a slow one during a burst is "
              "counted and its length kept");

        const std::string main_cpp = strip_comments(slurp(device_dir + "/main/main.cpp"));
        check(main_cpp.find("&tdeck::sdlog::state,&tdeck::sdlog::set_enabled") != std::string::npos &&
                  main_cpp.find("board.set_sd_activity_flag(tdeck::sdlog::burst_flag())") != std::string::npos &&
                  main_cpp.find("sdlog::set_enabled(") == std::string::npos,
              "S7 main.cpp hands the runtime the switch and the Board the burst flag, and switches nothing on");

        const std::string runtime = strip_comments(slurp(device_dir + "/main/alpha_runtime.cpp"));
        const std::string probe = function_body(runtime, "openu5::SdLogState AlphaRuntime::sd_log_probe(");
        check(probe.find("sd_log_perf_.set_enabled(on)") != std::string::npos &&
                  runtime.find("s.sd_log=sd_log_state();") != std::string::npos &&
                  runtime.find("services.sd_log=sd_log_probe;") != std::string::npos,
              "S8 the Developer row reaches the device switch, and every report and A3C_PERF line is labelled");

        // Nothing unrelated on the card depends on the diagnostic log.
        const std::string begin = function_body(sdlog, "bool begin_storage_transaction(");
        const std::string save = strip_comments(slurp(device_dir + "/main/alpha_save.cpp"));
        const std::string smoke = strip_comments(slurp(device_dir + "/main/device_smoke_tests.cpp"));
        bool packs_clean = true;
        for (const char *f : {"/main/asset_pack.cpp", "/main/alpha_resources.cpp", "/main/alpha_audio.cpp",
                              "/main/dungeon_art_cache.cpp", "/main/alpha_dialogue.cpp"})
            packs_clean = packs_clean && slurp(device_dir + f).find("sd_diagnostic_logger") == std::string::npos;
        check(!begin.empty() && begin.find("g_log") == std::string::npos &&
                  begin.find("xSemaphoreTake(g_storage_mutex") != std::string::npos &&
                  save.find("mkdir(\"/sd/ultima5\",0777)") != std::string::npos &&
                  smoke.find("mkdir(\"/sd/ultima5/logs\",0777)") != std::string::npos && packs_clean,
              "S9 unrelated card use is untouched: a save's storage transaction does not depend on the switch "
              "(and never waits for an idle writer), saves/settings and the smoke tests create their own "
              "directories, and the packs never include the logger");
    }

    std::printf("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
