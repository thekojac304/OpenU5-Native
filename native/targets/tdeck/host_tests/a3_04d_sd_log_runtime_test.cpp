// Alpha 3 A3-04D -- the SD diagnostic log's switch through the REAL
// AlphaRuntime (ALPHA3_AUDIO.md section 21): raw keys in, the Developer
// screen the runtime hands Board::show_alpha out (batch37_board_capture_stub),
// time on the virtual clock. The device logger's hooks are backed by the
// core openu5::SdDiagLog the device uses, over a fake card.
//
//   R  Developer > Diagnostics > "Probe: SD diag logging" (five rows above
//      the first): off at boot, Enter switches the log on and off, the row,
//      the report and the A3C_PERF line say so; every older Diagnostics row
//      keeps its place counted from the end
//   N  no logger (the host fixture's default): the row says n/a, Enter says
//      so on the Developer screen, and the A3-04C label is unchanged
//   T  none of it touches the game
//
//   a3_04d_sd_log_runtime <openu5-alpha1-resources.bin>
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/perf_report.h"
#include "openu5/sd_diag_log.h"

#include <cstdio>
#include <deque>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;

void batch37_reset_screen();
const tdeck::DeviceDebugScreen *batch37_last_debug_screen();

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr int64_t kClockStartUs = 5'000'000;

struct CountingCard final : SdDiagFile {
    int ops = 0;
    bool open(size_t &bytes) override {
        ++ops;
        bytes = 0;
        return true;
    }
    bool rotate() override { return ++ops, true; }
    bool write(const void *, size_t) override { return ++ops, true; }
    bool flush() override { return ++ops, true; }
    void close() override { ++ops; }
};
struct LineQueue final : SdDiagQueue {
    std::deque<SdDiagLine> lines;
    bool push(const SdDiagLine &l) override { return lines.push_back(l), true; }
    bool pop(SdDiagLine &l) override {
        if (lines.empty()) return false;
        l = lines.front();
        lines.pop_front();
        return true;
    }
};

// tdeck::sdlog's hooks over the core log, as the device's are.
struct Logger {
    static inline CountingCard card{};
    static inline LineQueue queue{};
    static inline SdDiagLog log{card, queue};
    static inline std::vector<bool> set_calls{};
    static SdLogState state() { return log.state(); }
    static bool set_enabled(bool on) {
        set_calls.push_back(on);
        return log.set_enabled(on);
    }
    static bool snapshot(SdLogPerf &out) {
        out = SdLogPerf{};
        out.valid = log.state() != SdLogState::Unavailable;
        out.off = !log.enabled();
        return out.valid;
    }
    static void reset() {}
    static tdeck::AlphaRuntime::SdLogPerfHooks hooks() {
        return {&Logger::snapshot, &Logger::reset, &Logger::state, &Logger::set_enabled};
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
    void up(int n = 1) {
        for (int i = 0; i < n; ++i) ball(RawInputKind::TrackballUp);
    }
    void down(int n = 1) {
        for (int i = 0; i < n; ++i) ball(RawInputKind::TrackballDown);
    }
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
    Logger::log.set_available(true); // the device: the card mounted, the writer running

    // ---- R: the switch ------------------------------------------------------
    {
        Run h(1);
        h.rt->attach_sd_log_perf(Logger::hooks());
        h.open_diagnostics();
        h.up(5);
        check(selected_row().find("Probe: SD diag logging: off") == 0 && !Logger::log.enabled() &&
                  h.rt->sd_log_state() == SdLogState::Off,
              "R1 at boot, five rows above the first is \"Probe: SD diag logging: off\" and the log is off (\"" +
                  selected_row() + "\")");
        h.down();
        const std::string bypass = selected_row();
        h.down();
        const std::string perf = selected_row();
        h.down();
        const std::string stats = selected_row();
        h.down();
        const std::string tone = selected_row();
        check(bypass.find("Probe: synth bypass: off") == 0 && perf.find("Audio/render performance") == 0 &&
                  stats.find("Audio/render stats (live)") == 0 && tone.find("Audio test tone (SFX)") == 0,
              "R2 the older rows keep their places counted from the end: bypass, performance, stats, test tone");
        h.up(4);
        h.key('\r');
        check(Logger::set_calls.size() == 1 && Logger::set_calls[0] && Logger::log.enabled() &&
                  selected_row().find("Probe: SD diag logging: ON") == 0,
              "R3 Enter switches the device log on, and the row says ON (\"" + selected_row() + "\")");
        h.down(3);
        h.key('\r'); // "Audio/render stats (live)"
        const std::string on_report = report_text(*h.rt);
        check(h.rt->perf_report_open() && on_report.find("\nmusic n/a  sfx 80% sdlog ON\n") != std::string::npos &&
                  h.line().find("scen=[music n/a  sfx 80% sdlog ON]") == 0,
              "R4 while it is on, the report and the A3C_PERF line are labelled \"sdlog ON\"");
        h.key('\r'); // dismiss
        h.up(3);
        h.key('\r'); // the switch again
        check(Logger::set_calls.size() == 2 && !Logger::set_calls[1] && !Logger::log.enabled() &&
                  selected_row().find("Probe: SD diag logging: off") == 0,
              "R5 Enter again switches it off");
        h.down(3);
        h.key('\r');
        const std::string off_report = report_text(*h.rt);
        check(off_report.find("\nmusic n/a  sfx 80% sdlog OFF\n") != std::string::npos &&
                  off_report.find("sd log OFF: 0 bursts max 0.0 total 0.0 ms\n") != std::string::npos &&
                  h.line().find("scen=[music n/a  sfx 80% sdlog OFF]") == 0 &&
                  h.line().find(" | sd=OFF:0:0.0/0.0") != std::string::npos,
              "R6 while it is off, the report and the line say \"sdlog OFF\", and the SD-log counters say OFF");
        h.key('\r');
        h.key('\b');
        h.key('\b');
        h.key('d', true);
        check(Logger::set_calls.size() == 2 && !Logger::log.enabled(),
              "R7 leaving and re-entering the Developer menu changes nothing by itself");
        h.key('\b');
        h.key('\b');
    }

    // ---- N: no SD logger on this device -----------------------------------
    {
        Run h(2);
        h.open_diagnostics();
        h.up(5);
        const std::string row = selected_row();
        h.key('\r');
        check(row.find("Probe: SD diag logging: n/a") == 0 && h.rt->perf_report_open() &&
                  report_text(*h.rt).find("SD diag logging: no SD card logger on this device") != std::string::npos &&
                  h.rt->sd_log_state() == SdLogState::NotReported &&
                  h.line().find("scen=[music n/a  sfx 80%]") == 0,
              "N1 with no logger the row says n/a, Enter says so on the Developer screen, and the label keeps its "
              "A3-04C form (\"" + row + "\")");
    }

    // ---- T: none of it touches the game -----------------------------------
    {
        auto walk = [](bool toggle) {
            Run h(5);
            h.rt->attach_sd_log_perf(Logger::hooks());
            if (toggle) { // on, a walk, off, a walk
                h.open_diagnostics();
                h.up(5);
                h.key('\r');
                h.key('\b');
                h.key('\b');
            }
            for (int i = 0; i < 5; ++i) {
                h.left();
                h.run(200);
            }
            if (toggle) {
                h.open_diagnostics();
                h.up(5);
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
        const size_t calls = Logger::set_calls.size();
        const std::string plain = walk(false), toggled = walk(true);
        check(plain == toggled && Logger::set_calls.size() == calls + 2 && !Logger::log.enabled(),
              "T1 walking with SD diagnostic logging switched on and off leaves the game exactly as without (" +
                  toggled + ")");
    }

    std::printf("A3-04D SD log runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
