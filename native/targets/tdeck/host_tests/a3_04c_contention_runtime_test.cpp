// Alpha 3 A3-04C -- the contention map through the REAL AlphaRuntime
// (ALPHA3_AUDIO.md section 20): raw keys in, the Developer screen the runtime
// hands Board::show_alpha out (batch37_board_capture_stub, which also plays
// the device Board's TFT timing for every gameplay frame), time on the
// virtual clock.
//
//   R  every gameplay frame's TFT timing reaches the live report and the
//      A3C_PERF line (the split, the row classes, viewport frames), the
//      Developer screen is not counted, a live read starts a new window
//   P  Developer > Diagnostics > "Probe: synth bypass" toggles the audio
//      task's probe, shows its state on the row, labels the report
//      (BYPASS), and says so when there is no audio output
//   T  none of it touches the game
//
//   a3_04c_contention_runtime <openu5-alpha1-resources.bin>
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
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;

void batch37_reset_screen();
const tdeck::DeviceDebugScreen *batch37_last_debug_screen();
void batch37_set_tft_feed(const openu5::TftTiming &timing, int64_t write_us);

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr int64_t kClockStartUs = 5'000'000;

// The audio task's published window and its probe, as the device backend gives them.
struct FakeAudioPerf final : AudioPerfSource {
    AudioPerfSnapshot snap{};
    std::vector<bool> bypass_calls;
    FakeAudioPerf() {
        snap.music_active = true;
        snap.song = MusicSong::Theme;
        snap.window_us = 20000000;
        snap.blocks = 2500;
        snap.render_avg_us = 2900;
        snap.music_avg_us = 2700;
        snap.task_busy_avg_us = 3000;
        snap.task_busy_max_us = 4300;
        snap.fill_min = 7;
    }
    bool perf_snapshot(AudioPerfSnapshot &out) const override {
        out = snap;
        return true;
    }
    void perf_reset() override {}
    bool set_music_bypass(bool on) override {
        bypass_calls.push_back(on);
        snap.music_bypass = on;
        return true;
    }
};

struct SdHooks {
    static inline int resets = 0;
    static bool snapshot(SdLogPerf &out) {
        out = SdLogPerf{};
        out.valid = true;
        out.bursts = 4;
        out.max_us = 8800;
        out.busy_us = 21000;
        return true;
    }
    static void reset() { ++resets; }
};

// One gameplay frame's TFT write as the device Board measures it (240 MHz cycles):
// 10 ms in SPI transactions, 20 ms in tick yields, 35 ms in all (so 5 ms building rows).
TftTiming device_frame() {
    TftTiming t{};
    t.cpu_mhz = 240;
    t.transactions = 170;
    t.rows = 160;
    t.xfer_cycles = 10000ull * 240;
    t.xfer_max_cycles = 300 * 240;
    t.yields = 9;
    t.yield_cycles = 20000ull * 240;
    t.yield_max_cycles = 9900 * 240;
    t.rows_idle = 100;
    t.rows_busy = 50;
    t.fill_cycles_idle = 100ull * 240 * 20;
    t.fill_cycles_busy = 50ull * 240 * 25;
    t.xfer_cycles_idle = 100ull * 240 * 60;
    t.xfer_cycles_busy = 50ull * 240 * 61;
    t.viewport_full = 1;
    return t;
}

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
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
        }
    }
    void open_diagnostics() {
        key('d', true);
        up();
        up();
        key('\r');
    }
    std::string state() {
        auto &g = rt->game();
        std::ostringstream s;
        s << int(g.position.map.location) << ',' << int(g.position.xy.x) << ',' << int(g.position.xy.y) << ','
          << int(g.time.hour) << ':' << int(g.time.minute) << ',' << g.food << ",rng" << g.rng.get_seed();
        return s.str();
    }
    std::string line() {
        char buf[kContentionLineBytes];
        rt->contention_line(buf, sizeof buf);
        return buf;
    }
};

const tdeck::DeviceDebugScreen *screen() { return batch37_last_debug_screen(); }
std::string selected_row() {
    const auto *s = screen();
    return s && s->selected_row < s->row_count ? std::string(s->rows[s->selected_row]) : std::string();
}
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

    // ---- R: the TFT split reaches the report and the line ----------------
    {
        Run h(1);
        FakeAudioPerf audio;
        h.rt->attach_audio_perf(&audio);
        h.rt->attach_sd_log_perf({&SdHooks::snapshot, &SdHooks::reset});
        batch37_set_tft_feed(device_frame(), 35000);
        for (int i = 0; i < 6; ++i) {
            h.left();
            h.run(300);
            h.right();
            h.run(300);
        }
        for (int i = 0; i < 40; ++i) h.rt->note_loop_pass(uint32_t(4000 + i * 100));
        ContentionSnapshot c{};
        h.rt->contention().snapshot(uint64_t(openu5_host_virtual_clock_us()), c);
        RenderPerfSnapshot r{};
        h.rt->render_perf().snapshot(uint64_t(openu5_host_virtual_clock_us()), r);
        // The fixture's first frame (drawn in Run's constructor) came before the feed: untimed.
        check(c.frames >= 13 && c.frames == r.frames && c.timed && c.viewport_frames == c.frames - 1,
              "R1 every gameplay frame carried the Board's TFT timing into the contention window (" +
                  std::to_string(c.frames) + " frames = the render counters' " + std::to_string(r.frames) + ")");
        check(c.tft_xfer_avg_us == 10000 && c.tft_yield_avg_us == 20000 && c.tft_fill_avg_us == 5000 &&
                  c.viewport_tft_avg_us == 35000 && c.row_fill_idle_x10 == 200 && c.row_fill_busy_x10 == 250 &&
                  c.rows == (c.frames - 1) * 160 && c.loops == 40 && c.loop_max_us == 7900,
              "R2 ... split exactly as measured: 5 ms building rows, 10 ms SPI, 20 ms yields; rows 20.0 / 25.0 us "
              "with audio idle / busy; the loop passes");
        const std::string line = h.line();
        check(line.find("scen=[music n/a  sfx 80%]") == 0 && line.find(" | split fill=5.0 xfer=10.0 yld=20.0") !=
                                                                  std::string::npos &&
                  line.find(" | rows=") != std::string::npos && line.find(" | sd=4:8.8/21.0") !=
                                                                        std::string::npos &&
                  line.find(" | audio blk=2500") != std::string::npos,
              "R3 the A3C_PERF line: the scenario (no music capability on this host), the split, the rows, the SD "
              "log and the audio window:\n    " + line);

        h.open_diagnostics();
        ContentionSnapshot menu{};
        h.rt->contention().snapshot(uint64_t(openu5_host_virtual_clock_us()), menu);
        check(menu.frames == c.frames, "R4 Developer screen redraws are not gameplay frames (still " +
                                           std::to_string(menu.frames) + ")");
        h.up();
        h.up();
        h.key('\r'); // "Audio/render stats (live)"
        const std::string text = report_text(*h.rt);
        check(h.rt->perf_report_open() && text.find("-- Contention map (A3-04C) --\nmusic n/a  sfx 80%\n") !=
                                               std::string::npos &&
                  text.find("tft/frame fill 5.0 xfer 10.0 yield 20.0\n") != std::string::npos &&
                  text.find("row fill us: audio idle 20.0 busy 25.0\n") != std::string::npos &&
                  text.find("sd log 4 bursts max 8.8 total 21.0 ms\n") != std::string::npos &&
                  text.find("audio task/blk avg 3.00 max 4.30 ms\n") != std::string::npos,
              "R5 \"Audio/render stats (live)\" shows the contention section on the Developer screen:\n" + text);
        ContentionSnapshot after{};
        h.rt->contention().snapshot(uint64_t(openu5_host_virtual_clock_us()), after);
        check(after.frames == 0 && after.loops == 0 && SdHooks::resets >= 1,
              "R6 ... and the read starts a new window for the contention counters and the SD-log bursts too");
    }

    // ---- P: the synth bypass probe ---------------------------------------
    {
        Run h(2);
        FakeAudioPerf audio;
        h.rt->attach_audio_perf(&audio);
        h.open_diagnostics();
        h.up();
        h.up();
        h.up();
        h.up();
        check(selected_row().find("Probe: synth bypass: off") == 0,
              "P1 four rows above the first is \"Probe: synth bypass: off\" (\"" + selected_row() + "\")");
        h.key('\r');
        check(audio.bypass_calls.size() == 1 && audio.bypass_calls[0] && h.rt->music_bypass() &&
                  selected_row().find("Probe: synth bypass: ON") == 0,
              "P2 Enter switches the probe on in the audio task, and the row says ON (\"" + selected_row() + "\")");
        h.down();
        h.down();
        h.key('\r'); // "Audio/render stats (live)"
        check(report_text(*h.rt).find("sfx 80% BYPASS\n") != std::string::npos && h.line().find("BYPASS]") !=
                                                                                      std::string::npos,
              "P3 while it is on, the report and the A3C_PERF line are labelled BYPASS");
        h.key('\r'); // dismiss
        h.up();
        h.up();
        h.key('\r'); // the probe again
        check(audio.bypass_calls.size() == 2 && !audio.bypass_calls[1] && !h.rt->music_bypass() &&
                  selected_row().find("Probe: synth bypass: off") == 0,
              "P4 Enter again switches it off");
        h.key('\b');
        h.key('\b');
        h.key('d', true);
        check(!h.rt->music_bypass() && audio.bypass_calls.size() == 2,
              "P5 leaving and re-entering the Developer menu changes nothing by itself");
    }
    {
        Run h(3);
        h.open_diagnostics();
        h.up();
        h.up();
        h.up();
        h.up();
        h.key('\r');
        check(!h.rt->music_bypass() && h.rt->perf_report_open() &&
                  report_text(*h.rt).find("Synth bypass: no audio output on this device") != std::string::npos,
              "P6 with no audio output the probe stays off and says so on the Developer screen");
    }

    // ---- T: none of it touches the game -----------------------------------
    {
        auto walk = [](bool probe) {
            Run h(5);
            FakeAudioPerf audio;
            h.rt->attach_audio_perf(&audio);
            batch37_set_tft_feed(device_frame(), 35000);
            if (probe) {
                h.open_diagnostics();
                h.up();
                h.up();
                h.up();
                h.up();
                h.key('\r');
                h.key('\b');
                h.key('\b');
            }
            for (int i = 0; i < 5; ++i) {
                h.left();
                h.run(200);
            }
            for (int i = 0; i < 5; ++i) {
                h.right();
                h.run(200);
            }
            return h.state();
        };
        const std::string plain = walk(false), probed = walk(true);
        check(plain == probed, "T1 walking with the probe on and every frame timed leaves the game exactly as without (" +
                                   probed + ")");
    }

    std::printf("A3-04C contention runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
