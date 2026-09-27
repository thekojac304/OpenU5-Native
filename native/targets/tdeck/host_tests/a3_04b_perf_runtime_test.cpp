// Alpha 3 A3-04B -- the Developer perf report through the REAL AlphaRuntime
// (ALPHA3_AUDIO.md section 19): raw keys in, the Developer screen the runtime
// hands Board::show_alpha out (batch37_board_capture_stub), time on the
// virtual clock.
//
//   D  Developer > Diagnostics > "Audio/render performance" ends in a report
//      ON THE DEVELOPER SCREEN that stays until dismissed, scrolls, closes
//      with Enter/Back; finished with the menu closed it waits for Alt+D;
//      "Audio/render stats (live)" shows the window since the last read.
//      A3-04A printed the results to the gameplay transcript, which that
//      screen covers (section 19.2) -- the defect the user reported.
//   R  the render counters count gameplay frames and inputs, not the
//      Developer screen
//   T  none of it touches the game
//
//   a3_04b_perf_runtime <openu5-alpha1-resources.bin>
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/audio_stream.h"
#include "openu5/perf_report.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

using namespace openu5;
using tdeck::RawInputKind;

void batch37_reset_screen();
const tdeck::DeviceDebugScreen *batch37_last_debug_screen();
int batch37_debug_draw_count();

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr int64_t kClockStartUs = 5'000'000;

// The audio task's published window, as the device backend would give it.
struct FakeAudioPerf final : AudioPerfSource {
    AudioPerfSnapshot snap{};
    bool valid = true;
    int resets = 0;
    FakeAudioPerf() {
        snap.music_active = true;
        snap.song = MusicSong::Theme;
        snap.window_us = 30000000;
        snap.blocks = 3750;
        snap.render_avg_us = 3210;
        snap.render_p99_us = 4100;
        snap.render_max_us = 5020;
        snap.cpu_permille = 401;
        snap.fill_min = 7;
        snap.fill_max = 8;
        snap.voices_max = 9;
        snap.stack_free_min = 3120;
    }
    bool perf_snapshot(AudioPerfSnapshot &out) const override {
        out = snap;
        return valid;
    }
    void perf_reset() override { ++resets; }
};

struct FakeSystemPerf final : SystemPerfSource {
    int resets = 0;
    void system_perf_reset() override { ++resets; }
    bool system_perf_snapshot(SystemPerfSnapshot &out) override {
        out = SystemPerfSnapshot{};
        out.valid = true;
        out.core_busy_permille[0] = 120;
        out.core_busy_permille[1] = 340;
        out.audio_permille = 330;
        out.main_permille = 110;
        out.heap_internal_free = 77777;
        out.heap_psram_free = 5555555;
        return true;
    }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    explicit Run(int seed) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        batch37_reset_screen();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.render_pixels = true;
        f.indexed_test_tiles = true;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.position.xy = {80, 80};
        g.time.hour = 12;
        g.food = 80;
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.character_class = 'F';
        m.level = 1;
        m.current_hp = 30;
        m.max_hp = 30;
        m.strength = m.dexterity = m.intelligence = 10;
        g.rng.seed(uint32_t(seed));
        rt->render(board, true);
    }
    // main.cpp's loop: every input is followed by a render.
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        rt->render(board);
    }
    void key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = code;
        e.modifiers.alt = alt;
        raw(e);
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000; // past the trackball debounce
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    void up() { ball(RawInputKind::TrackballUp); }
    void down() { ball(RawInputKind::TrackballDown); }
    void left() { ball(RawInputKind::TrackballLeft); }
    void right() { ball(RawInputKind::TrackballRight); }
    /** The frame loop for `ms` of virtual time, 5 ms per pass (main.cpp's delay). */
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
        }
    }
    /** Alt+D, then Diagnostics (two rows above the root's first). */
    void open_diagnostics() {
        key('d', true);
        up();
        up();
        key('\r');
    }
    std::string transcript() const {
        std::string out;
        for (size_t i = 0; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i)) out += std::string(b->text) + "\n";
        return out;
    }
    std::string state() {
        auto &g = rt->game();
        std::ostringstream s;
        s << int(g.position.map.location) << ',' << int(g.position.xy.x) << ',' << int(g.position.xy.y) << ','
          << int(g.time.hour) << ':' << int(g.time.minute) << ',' << g.food << ",rng" << g.rng.get_seed();
        return s.str();
    }
};

const tdeck::DeviceDebugScreen *screen() { return batch37_last_debug_screen(); }
std::string breadcrumb() { return screen() ? screen()->breadcrumb : "(gameplay HUD)"; }
std::string rows() {
    std::string out;
    if (const auto *s = screen())
        for (size_t i = 0; i < s->row_count; ++i) out += std::string(s->rows[i]) + "\n";
    return out;
}
bool rows_have(const char *needle) { return rows().find(needle) != std::string::npos; }
/** Every line of the report the runtime holds (all pages). */
std::string report_text(const tdeck::AlphaRuntime &rt) {
    std::string out;
    for (size_t i = 0; i < rt.perf_report_line_count(); ++i) out += std::string(rt.perf_report_line(i)) + "\n";
    return out;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;

    // ---- D: the benchmark's results on the Developer screen -------------
    {
        Run h(1);
        FakeAudioPerf audio;
        FakeSystemPerf system;
        h.rt->attach_audio_perf(&audio);
        h.rt->attach_system_perf(&system);
        h.open_diagnostics();
        check(breadcrumb().find("Diagnostics") != std::string::npos, "D0 Alt+D > Diagnostics is on the Developer screen");
        h.up();
        h.up();
        h.up();
        check(screen() && std::string(screen()->rows[screen()->selected_row]).find("Audio/render performance") == 0,
              "D0 the row is \"Audio/render performance\" (three above the first; the test tone stays last)");
        h.key('\r');
        h.run(10000);
        const std::string status = screen() ? screen()->status : "";
        check(h.rt->audio_benchmark_running() && status.find("Audio perf 10/47s music alone") == 0,
              "D1 while it runs the Developer screen shows its progress: \"" + status + "\"");
        h.run(38000);
        check(!h.rt->audio_benchmark_running() && h.rt->perf_report_open() && breadcrumb() == "Developer > Perf report",
              "D2 when it ends, the report REPLACES the Developer screen's rows (breadcrumb \"" + breadcrumb() + "\")");
        check(rows_have("AUDIO/RENDER PERF  benchmark") && rows_have("-- Music alone (idle): Ultima V Theme --") &&
                  rows_have("render avg 3.21 p99 4.10 max 5.02 ms"),
              "D2 ... with the measurements themselves on the first page:\n" + rows());
        check(screen() && std::string(screen()->status) == "Up/Down scroll  Enter/Back close" &&
                  std::string(screen()->position).find("1-9/") == 0,
              "D2 ... and says how to scroll and dismiss it (" + std::string(screen() ? screen()->position : "") + ")");
        check(h.transcript().find("render avg") == std::string::npos,
              "D2 nothing is dumped into the gameplay transcript any more (A3-04A's invisible 21 lines)");
        h.run(20000);
        check(h.rt->perf_report_open() && breadcrumb() == "Developer > Perf report",
              "D3 twenty more seconds of frames and it is still there: it stays until dismissed");
        const size_t n = h.rt->perf_report_line_count();
        h.down();
        check(screen() && std::string(screen()->position).find("2-10/") == 0 &&
                  std::string(screen()->rows[0]) == h.rt->perf_report_line(1),
              "D4 Down scrolls one line (" + std::string(screen() ? screen()->position : "") + ")");
        std::string seen = rows();
        for (size_t i = 0; i < n + 5; ++i) {
            h.down();
            seen += rows();
        }
        check(seen.find("-- Render:") != std::string::npos && seen.find("-- System --") != std::string::npos &&
                  seen.find("CPU0 12%  CPU1 34%") != std::string::npos && rows_have("A3-04 guard") &&
                  std::string(screen()->position).find(std::to_string(n) + "/" + std::to_string(n)) != std::string::npos,
              "D4 ... page by page to the last line: the render and system sections, then the guard price (" +
                  std::to_string(n) + " lines, stops at " + std::string(screen()->position) + ")");
        h.key('\r');
        check(!h.rt->perf_report_open() && breadcrumb().find("Diagnostics") != std::string::npos,
              "D5 Enter dismisses it: the Diagnostics menu is back (\"" + breadcrumb() + "\")");
        check(audio.resets >= 3 && system.resets >= 2,
              "D6 the benchmark started fresh windows (audio " + std::to_string(audio.resets) + ", system " +
                  std::to_string(system.resets) + " resets)");
    }

    // ---- D: finished with the menu closed -> waits for Alt+D --------------
    {
        Run h(2);
        FakeAudioPerf audio;
        h.rt->attach_audio_perf(&audio);
        h.open_diagnostics();
        h.up();
        h.up();
        h.up();
        h.key('\r');
        h.key('\b'); // Diagnostics -> root
        h.key('\b'); // root -> the game
        h.run(49000);
        check(!h.rt->perf_report_open() && h.rt->perf_report_pending() &&
                  h.transcript().find("Perf report ready: Alt+D shows it") != std::string::npos,
              "D7 a benchmark that ends while the player walks leaves ONE transcript line and waits");
        h.key('d', true);
        check(h.rt->perf_report_open() && breadcrumb() == "Developer > Perf report" &&
                  rows_have("AUDIO/RENDER PERF  benchmark"),
              "D7 ... Alt+D shows it first");
        h.key('\b');
        h.key('\b');
        h.key('\b');
        h.key('d', true);
        check(!h.rt->perf_report_open() && breadcrumb() == "Developer",
              "D8 once dismissed it does not come back: Alt+D opens the Developer menu as usual");
    }

    // ---- D + R: the live window, with real gameplay frames ---------------
    {
        Run h(3);
        FakeAudioPerf audio;
        FakeSystemPerf system;
        h.rt->attach_audio_perf(&audio);
        h.rt->attach_system_perf(&system);
        const auto before = h.rt->render_perf();
        (void)before;
        for (int i = 0; i < 6; ++i) {
            h.left();
            h.run(300);
            h.right();
            h.run(300);
        }
        RenderPerfSnapshot r{};
        h.rt->render_perf().snapshot(uint64_t(openu5_host_virtual_clock_us()), r);
        check(r.frames >= 12 && r.inputs >= 12 && r.shown >= 12 && r.cadence_avg_us > 0,
              "R1 twelve steps: the game thread counted " + std::to_string(r.frames) + " frames and " +
                  std::to_string(r.inputs) + " inputs, each shown by a later frame (" + std::to_string(r.shown) + ")");
        h.open_diagnostics();
        h.up();
        h.up();
        RenderPerfSnapshot menu{};
        h.rt->render_perf().snapshot(uint64_t(openu5_host_virtual_clock_us()), menu);
        check(menu.frames == r.frames, "R2 Developer screen redraws are not gameplay frames (still " +
                                           std::to_string(menu.frames) + ")");
        h.key('\r'); // "Audio/render stats (live)"
        char frames_line[64];
        std::snprintf(frames_line, sizeof frames_line, "-- Render: %u gameplay frames", unsigned(r.frames));
        const std::string text = report_text(*h.rt);
        check(h.rt->perf_report_open() && rows_have("AUDIO/RENDER PERF  live window") &&
                  rows_have("-- Since last read: Ultima V Theme --") && text.find(frames_line) != std::string::npos,
              "D9 \"Audio/render stats (live)\" opens the report at once, with the frames just played (" +
                  std::string(frames_line) + ")");
        RenderPerfSnapshot after{};
        h.rt->render_perf().snapshot(uint64_t(openu5_host_virtual_clock_us()), after);
        check(after.frames == 0 && audio.resets >= 1 && system.resets >= 1,
              "D9 ... and a new window starts: frames, audio and system all reset");
    }

    // ---- D: no audio output: said on the Developer screen too ------------
    {
        Run h(4);
        h.open_diagnostics();
        h.up();
        h.up();
        h.up();
        h.key('\r');
        check(h.rt->perf_report_open() && rows_have("Audio perf: no audio output on this device"),
              "D10 with no audio output the benchmark says so on the Developer screen, not in the hidden transcript");
    }

    // ---- T: none of it touches the game -----------------------------------
    {
        auto walk = [](bool instrumented) {
            Run h(5);
            FakeAudioPerf audio;
            FakeSystemPerf system;
            if (instrumented) {
                h.rt->attach_audio_perf(&audio);
                h.rt->attach_system_perf(&system);
            }
            for (int i = 0; i < 5; ++i) {
                h.left();
                h.run(200);
            }
            if (instrumented) {
                h.open_diagnostics();
                h.up();
                h.up();
                h.key('\r');
                h.key('\r');
                h.key('\b');
                h.key('\b');
            }
            for (int i = 0; i < 5; ++i) {
                h.right();
                h.run(200);
            }
            return h.state();
        };
        const std::string plain = walk(false), measured = walk(true);
        check(plain == measured, "T1 walking with the counters, a live read and the report in between leaves the game "
                                 "exactly as without (" + measured + ")");
    }

    std::printf("A3-04B perf runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
