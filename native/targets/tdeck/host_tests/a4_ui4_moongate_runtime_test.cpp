// Alpha 4 A4-UI4/PRES1 (targets/tdeck/ALPHA4_UI.md section 8) -- the moongates
// as the original draws them (H-191 / D-48), on the REAL AlphaRuntime and the
// REAL tdeck_board.cpp over the fake ST7789 (the A4-UI2 harness).
//
//   A  the rise and the fall (kernel_moongate_render 0x475a, the cosmetic
//      counter [0x5887]): at night each compositor pass raises it by one to 16,
//      by day it sinks to 0 and the cell is grass again; a gate at 1..15 is
//      the partial composite (0x56e6 -> 0x1112 -> EGA.DRV 0x24d6): grass whose
//      bottom n rows are the TOP n rows of 0xdc. The idle getkey redraws
//      outdoors every tick (0x266c -> 0x5910), so a rise is 16 ticks.
//   T  the transit (kernel_moongate_enter 0x48a8): run_n_frames(1), the
//      activation sweep, the gate fizzled over the party, run_n_frames(1), the
//      gate closing 15 -> 1 with delay(2) each (1,648 ms), then the destination
//      -- whose gate rises from 0. No key does anything meanwhile.
//   N  midnight (hour 0, minute < 10): the same animation, and no teleport.
//
// The oracle is the panel itself: the cell by day (grass) and the cell at full
// night (the gate) are captured first; every partial stage must be exactly
// their composite. Positions are Batch 53B's 7E-C route (stone 1, 96,102).
//
//   a4_ui4_moongate_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> [--dump <dir>]
#include "a4_ui2_harness.h"
#include "openu5/debug_developer.h"
#include "openu5/debug_map_picker.h"
#include "openu5/presentation.h"
#include "openu5/quest_world.h"
#include "openu5/world.h"

using namespace a4_ui2;

namespace {
AudioPackInfo g_audio{};
constexpr int kStoneX = 96, kStoneY = 102; // INIT.GAM stone 1 (Britain's gate)

CommandContext &ctx(Run &h) { return h.rt->command_context_for_test(); }
bool teleport(Run &h, int x, int y) {
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::Britannia;
    r.x = x;
    r.y = y;
    if (apply_debug_teleport(ctx(h), r).status != DebugTeleportStatus::Applied) return false;
    h.render(true);
    return true;
}
void clock(Run &h, int hour, int minute) {
    debug_set_clock(h.rt->game(), DebugClockPart::Hour, hour);
    debug_set_clock(h.rt->game(), DebugClockPart::Minute, minute);
}
/** A composed viewport cell (col,row of the 11x11 window), 16x16 pixels. */
std::vector<uint16_t> cell(Run &h, int col, int row) {
    const uint16_t *vp = h.rt->composed_viewport();
    std::vector<uint16_t> out;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) out.push_back(vp[(row * 16 + y) * 176 + col * 16 + x]);
    return out;
}
/** The stage a cell shows: 0 = the ground, 16 = the whole gate, n = ground with the gate's top n rows below. */
int stage_of(const std::vector<uint16_t> &c, const std::vector<uint16_t> &ground, const std::vector<uint16_t> &gate) {
    for (int n = 0; n <= 16; ++n) {
        bool ok = true;
        for (int y = 0; y < 16 && ok; ++y)
            for (int x = 0; x < 16 && ok; ++x)
                ok = c[size_t(y * 16 + x)] == (y < 16 - n ? ground[size_t(y * 16 + x)] : gate[size_t((y - (16 - n)) * 16 + x)]);
        if (ok) return n;
    }
    return -1;
}
/** The GRAM's view of the same cell (viewport at (4,4) on the panel). */
bool panel_matches(Run &h, int col, int row) {
    const auto c = cell(h, col, row);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (px(4 + col * 16 + x, 4 + row * 16 + y) != c[size_t(y * 16 + x)]) return false;
    return true;
}

struct Fixture {
    Run h{{{"Avatar", 'G', 900}}, false, &g_audio, true};
    std::vector<uint16_t> ground, gate;
    bool ok = false;
    Fixture() {
        auto &g = h.rt->game();
        g.party.active_character = 0;
        h.rt->turn().transport_tile = 0x1c;
        // By day, two cells south of the stone: the stone's cell is plain ground.
        clock(h, 12, 0);
        const bool moved = teleport(h, kStoneX, kStoneY + 2);
        h.run(200);
        ground = cell(h, 5, 3);
        // Full night, settled: a load at night shows the gate at once (the counter is not saved).
        clock(h, 21, 0);
        h.run(1500);
        gate = cell(h, 5, 3);
        ok = moved && ground != gate;
    }
};

void test_rise_and_fall() {
    std::printf("A  the gates rise at night and sink by day (0x475a)\n");
    Fixture f;
    auto &h = f.h;
    const auto map = get_active_map(pack->world, MapId{LocationId(0), 0});
    check(f.ok && map.error == Error::None && map.value.tile_at(kStoneX, kStoneY) == 5,
          "A0 precondition: stone 1's cell is grass (tile 5) by day and the gate by night");
    // By day (noon, so the grass is lit when the gate is gone): the gate sinks
    // one stage a tick to grass. (At 05:00 the same happens, but the bare cell
    // is then dark: only the gate lights it.)
    clock(h, 12, 0);
    std::vector<int> stages;
    int64_t t0 = Run::now(), zero_at = -1;
    for (int t = 0; t < 1500 && zero_at < 0; t += 5) {
        h.run(5);
        const int s = stage_of(cell(h, 5, 3), f.ground, f.gate);
        if (stages.empty() || stages.back() != s) stages.push_back(s);
        if (s == 0) zero_at = (Run::now() - t0) / 1000;
    }
    bool down = stages.size() == 17;
    for (size_t i = 0; i < stages.size(); ++i) down = down && stages[i] == 16 - int(i);
    check(down && zero_at >= 16 * 55 - 60 && zero_at <= 16 * 55 + 10,
          "A1 by day the gate sinks 16 -> 0, one stage a tick, and the cell is grass again (" + n(long(stages.size())) + " stages, grass after " +
              n(zero_at) + " ms)");
    // Dusk: hour 20 -- it rises again, 0 -> 16.
    clock(h, 20, 0);
    stages.clear();
    t0 = Run::now();
    int64_t full_at = -1;
    bool panel = true;
    for (int t = 0; t < 1500 && full_at < 0; t += 5) {
        h.run(5);
        const int s = stage_of(cell(h, 5, 3), f.ground, f.gate);
        if (stages.empty() || stages.back() != s) {
            stages.push_back(s);
            panel = panel && panel_matches(h, 5, 3);
            if (s == 8) dump("a-gate-stage-8");
        }
        if (s == 16) full_at = (Run::now() - t0) / 1000;
    }
    // The rise starts from the bare cell (still the noon frame, 0, or -- once
    // the dusk light is worked out -- dark, -1: only the gate lights it); from
    // there every stage in turn.
    while (!stages.empty() && stages.front() <= 0) stages.erase(stages.begin());
    bool up = stages.size() == 16 && stages.front() == 1 && stages.back() == 16;
    for (size_t i = 1; i < stages.size(); ++i) up = up && stages[i] == stages[i - 1] + 1;
    check(up && full_at >= 15 * 55 - 60 && full_at <= 16 * 55 + 10,
          "A2 at dusk the gate rises 1 -> 16, one stage a tick, lighting its own cell (" + n(long(stages.size())) + " stages, full after " +
              n(full_at) + " ms)");
    check(panel, "A3 every stage reaches the panel (the GRAM holds the composed cell each time it changes)");
}

void test_transit() {
    std::printf("T  the transit (0x48a8)\n");
    Fixture f;
    auto &h = f.h;
    teleport(h, kStoneX, kStoneY + 1);
    h.run(200);
    const auto before = h.rt->game().position;
    const size_t sfx0 = h.audio.sfx(SfxId::Moongate);
    h.up(); // onto the gate
    const int64_t t0 = Run::now();
    const auto after = h.rt->game().position;
    const bool moved = after.xy.x != before.xy.x || after.xy.y != before.xy.y;
    // The origin stays on screen: its centre cell is the party, then the gate, then the closing gate.
    std::vector<std::pair<int64_t, int>> seen; // (ms, centre stage; -1 = the party)
    bool keys_dead = true;
    const auto avatar = cell(h, 5, 5);
    size_t routed = h.rt->routed_command_count();
    for (int t = 0; t < 4000; t += 5) {
        h.run(5);
        const int64_t ms = (Run::now() - t0) / 1000;
        if (ms == 600) {
            // A step: without the transit's hold it would move the party on.
            const auto here = h.rt->game().position;
            h.up();
            keys_dead = h.rt->routed_command_count() == routed && h.rt->game().position.xy.x == here.xy.x &&
                        h.rt->game().position.xy.y == here.xy.y;
        }
        const int s = stage_of(cell(h, 5, 5), f.ground, f.gate);
        const int v = cell(h, 5, 5) == avatar ? -1 : s;
        if (seen.empty() || seen.back().second != v) seen.push_back({ms, v});
        if (v == 8 && seen.back().first == ms) dump("t-closing-stage-8");
    }
    // Expected: party until 55 + sweep, the whole gate for two ticks, then 15..1, then the destination.
    const int64_t sweep = int64_t(tone_sweep_ms(0x7530));
    int64_t gate_at = -1, first_close = -1, last_close = -1, end_at = -1;
    std::vector<int> closing;
    for (const auto &e : seen) {
        if (e.second == 16 && gate_at < 0) gate_at = e.first;
        if (e.second >= 1 && e.second <= 15) {
            if (first_close < 0) first_close = e.first;
            last_close = e.first;
            closing.push_back(e.second);
        }
    }
    for (const auto &e : seen)
        if (e.first > last_close && last_close >= 0 && end_at < 0) end_at = e.first;
    bool order = closing.size() == 15;
    for (size_t i = 0; i < closing.size(); ++i) order = order && closing[i] == 15 - int(i);
    check(moved && h.audio.sfx(SfxId::Moongate) == sfx0 + 1, "T1 control: the step fires the gate once and the party is moved");
    check(gate_at >= 55 + sweep - 5 && gate_at <= 55 + sweep + 10,
          "T2 the party stays on the origin's gate through run_n_frames(1) and the sweep, then the gate stands over it (" +
              n(gate_at) + " ms; 55 + " + n(sweep) + ")");
    check(order && first_close >= gate_at + 105 && first_close <= gate_at + 120,
          "T3 two ticks later the gate closes over the cell 15 -> 1, grass beneath (" + n(long(closing.size())) +
              " stages from " + n(first_close) + " ms)");
    check(last_close - first_close >= 14 * 110 - 10 && last_close - first_close <= 14 * 110 + 10 && end_at - last_close >= 105 &&
              end_at - last_close <= 120,
          "T4 one stage every delay(2) = 110 ms: 1,648 ms of closing, then the destination (" + n(last_close - first_close) +
              " ms, end " + n(end_at) + " ms)");
    check(keys_dead, "T5 a step during the transit does nothing (0x48a8 reads no key)");
    h.run(200);
    const auto settled = cell(h, 5, 5);
    check(settled == avatar,
          "T6 after the transit the destination is drawn with the party at its centre");
}

void test_midnight() {
    std::printf("N  midnight\n");
    Fixture f;
    auto &h = f.h;
    teleport(h, kStoneX, kStoneY + 1);
    clock(h, 0, 5);
    h.run(1500);
    const auto before = h.rt->game().position;
    h.up();
    const auto at = h.rt->game().position;
    const int64_t t0 = Run::now();
    int closing = 0, last = -2;
    const auto avatar = cell(h, 5, 5);
    for (int t = 0; t < 3500; t += 5) {
        h.run(5);
        const int s = cell(h, 5, 5) == avatar ? -1 : stage_of(cell(h, 5, 5), f.ground, f.gate);
        if (s != last && s >= 1 && s <= 15) ++closing;
        last = s;
    }
    (void)t0;
    check(at.xy.x == kStoneX && at.xy.y == kStoneY && before.xy.y == kStoneY + 1 && closing == 15,
          "N1 at 00:05 the gate closes over the party all the same, and the party stays on it (0x494d-0x495b)");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a4_ui4_moongate_runtime <pack> <openu5-audio.bin> [--dump <dir>]\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load\n");
        return 1;
    }
    g_audio = tdeck::load_audio_pack_info(argv[2]);
    if (g_audio.state != AudioPackState::Valid) {
        std::printf("RED the audio pack is not valid -- run `npm run pack:audio`\n");
        return 1;
    }
    g_dump = arg_after(argc, argv, "--dump");
    test_rise_and_fall();
    test_transit();
    test_midnight();
    std::printf("A4-UI4 moongate runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
