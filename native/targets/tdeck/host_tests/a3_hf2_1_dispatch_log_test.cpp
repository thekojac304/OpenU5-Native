// Alpha 3 A3-HF2.1 -- PRESENTATION_DISPATCH is logged when the presentation
// decision changes, not on every gameplay frame (ALPHA3_AUDIO.md section 25).
//
// The A3-04E.1 hardware captures had it as 24-31 % of every line (section
// 23.10.5): render() logged it unconditionally, so standing still beside an
// animated tile emitted one line per 55 ms tick, and every step several. The
// line exists to show WHICH source (world / combat / dungeon3d / scenes) a
// frame was composed from under WHICH UI mode; a repeat of the previous line
// says nothing. Through the REAL AlphaRuntime (A3-HF1's harness), stdout is
// captured per phase and the dispatch lines are counted against the
// runtime's own frame counter (RenderPerfCounters).
//
//   a3_hf2_1_dispatch_log <openu5-alpha1-resources.bin>
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/outdoor.h"
#include "openu5/perf_report.h"
#include "openu5/presentation.h"
#include "openu5/world.h"

#include <io.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;

void batch37_reset_screen();

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
const tdeck::AlphaResourceOwners *pack = nullptr;
size_t g_dungeon_count = 0;
constexpr int64_t kClockStartUs = 5'000'000;
constexpr uint32_t kSeed = 2; // A3-HF1's roaming-troll seed

/** Everything `fn` printed to stdout (the host ESP_LOG sink). */
std::string capture(const std::function<void()> &fn) {
    const char *path = "a3_hf2_1_dispatch_capture.txt";
    std::fflush(stdout);
    const int saved = _dup(_fileno(stdout));
    FILE *file = std::fopen(path, "w+");
    _dup2(_fileno(file), _fileno(stdout));
    fn();
    std::fflush(stdout);
    _dup2(saved, _fileno(stdout));
    _close(saved);
    std::fclose(file);
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    in.close();
    std::remove(path);
    return s.str();
}
/** The PRESENTATION_DISPATCH lines in `out`, each from "ui=" to the end of the line. */
std::vector<std::string> dispatch_lines(const std::string &out) {
    std::vector<std::string> lines;
    const std::string tag = "PRESENTATION_DISPATCH ";
    for (size_t at = out.find(tag); at != std::string::npos; at = out.find(tag, at + 1)) {
        const size_t from = at + tag.size();
        size_t end = out.find_first_of("\r\n", from);
        if (end == std::string::npos) end = out.size();
        lines.push_back(out.substr(from, end - from));
    }
    return lines;
}
size_t repeats(const std::vector<std::string> &lines) {
    size_t n = 0;
    for (size_t i = 1; i < lines.size(); ++i) n += lines[i] == lines[i - 1];
    return n;
}
std::string n(size_t v) { return std::to_string(v); }

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    explicit Run(uint32_t seed) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        batch37_reset_screen();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.dungeons = pack->dungeons;
        f.dungeon_count = g_dungeon_count;
        f.enemy_defs = pack->combat_enemy_views;
        f.enemy_def_count = pack->combat_enemy_count;
        f.render_pixels = true;
        f.indexed_test_tiles = true;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.time.hour = 12;
        g.time.minute = 0;
        g.food = 900;
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.character_class = 'A';
        m.level = 1;
        m.current_hp = m.max_hp = 100;
        m.strength = m.dexterity = m.intelligence = 20;
        g.rng.seed(seed);
        rt->render(board, true);
    }
    int64_t now() const { return openu5_host_virtual_clock_us(); }
    uint32_t frames() const {
        RenderPerfSnapshot s{};
        rt->render_perf().snapshot(uint64_t(now()), s);
        return s.frames;
    }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = now();
        rt->handle(e);
    }
    void key(uint8_t code) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = code;
        raw(e);
        rt->render(board);
    }
    /** The physical Mic key's short press is the device's Cancel / ESC. */
    void cancel() {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent down{};
        down.kind = RawInputKind::Keyboard;
        down.column = tdeck::kMicrophoneKeyColumn;
        down.row = tdeck::kMicrophoneKeyRow;
        down.transition = tdeck::KeyTransition::Pressed;
        raw(down);
        openu5_host_virtual_clock_us() += 50000;
        tdeck::RawInputEvent up = down;
        up.transition = tdeck::KeyTransition::Released;
        raw(up);
        rt->render(board);
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
        rt->render(board);
    }
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
        }
    }
    CombatState &cs() { return rt->combat_state_for_test(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    bool combat() { return ctx().combat && cs().initialized; }
    UiMode mode() const { return rt->ui()->mode(); }
    bool teleport(int x, int y) {
        DebugTeleportRequest r{};
        r.kind = DebugDestinationKind::Britannia;
        r.x = x;
        r.y = y;
        const bool ok = apply_debug_teleport(ctx(), r).status == DebugTeleportStatus::Applied;
        rt->render(board, true);
        return ok;
    }
};

/**
 * Open grassland on Britannia (a 5 x 3 block of grass, so a step east and back
 * stays on it) with animated water in view, so standing still keeps drawing
 * animation frames: the frames that each logged the line.
 */
struct Land {
    int x = -1, y = -1;
};
Land find_land() {
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    if (m.error != Error::None) return {};
    for (int y = 60; y < 200; ++y)
        for (int x = 60; x < 200; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 1 && ok; ++dy)
                for (int dx = -2; dx <= 2 && ok; ++dx) ok = m.value.tile_at(x + dx, y + dy) == 5;
            bool water = false;
            for (int dy = -4; dy <= 4 && ok && !water; ++dy)
                for (int dx = -4; dx <= 4 && !water; ++dx) {
                    const auto k = tile_animation_kind(m.value.tile_at(x + dx, y + dy));
                    water = k == TileAnimationKind::WaterScroll || k == TileAnimationKind::WaterComposite;
                }
            if (ok && water) return {x, y};
        }
    return {};
}
int troll_tile(CommandContext &ctx) {
    for (size_t i = 0; i < ctx.outdoor->resources->enemy_count; ++i)
        if (const auto *d = ctx.outdoor->resources->enemies[i]; d && d->index == 41) return d->tile;
    return -1;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report_out{};
    if (source.open(argv[1], report_out) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report_out) != ESP_OK) return 2;
    pack = &owners;
    g_dungeon_count = report_out.dungeon_count;

    const Land land = find_land();
    std::printf("  grassland at (%d,%d)\n", land.x, land.y);
    std::vector<std::string> all; // every dispatch line of the one session, in order

    // P0 (control): the first gameplay frame names its source.
    std::unique_ptr<Run> hp;
    {
        const auto lines = dispatch_lines(capture([&] { hp = std::make_unique<Run>(kSeed); }));
        all.insert(all.end(), lines.begin(), lines.end());
        check(lines.size() == 1 && lines[0] == "ui=explore combat=0 dungeon=0 source=world",
              "P0 the first gameplay frame logs its presentation source once (" + n(lines.size()) + " line(s)" +
                  (lines.empty() ? std::string() : ": " + lines[0]) + ")");
    }
    Run &h = *hp;
    const bool at = land.x >= 0 && h.teleport(land.x, land.y);

    // P1: standing still. Before A3-HF2.1 every animation frame logged the line.
    {
        const uint32_t f0 = h.frames();
        const auto lines = dispatch_lines(capture([&] { h.run(5000); }));
        all.insert(all.end(), lines.begin(), lines.end());
        const uint32_t drawn = h.frames() - f0;
        check(at && drawn >= 50 && lines.empty(),
              "P1 standing still 5 s on open land: " + n(drawn) + " frames drawn, " + n(lines.size()) +
                  " PRESENTATION_DISPATCH lines (nothing changed, nothing logged)");
    }

    // P2: walking. Each step draws frames; the source and the mode never change.
    {
        const uint32_t f0 = h.frames();
        const auto lines = dispatch_lines(capture([&] {
            for (int i = 0; i < 3; ++i) {
                h.ball(RawInputKind::TrackballRight);
                h.run(300);
                h.ball(RawInputKind::TrackballLeft);
                h.run(300);
            }
        }));
        all.insert(all.end(), lines.begin(), lines.end());
        const uint32_t drawn = h.frames() - f0;
        check(drawn >= 6 && lines.empty() && h.mode() == UiMode::Exploration,
              "P2 six steps: " + n(drawn) + " frames drawn, " + n(lines.size()) + " PRESENTATION_DISPATCH lines");
    }

    // P3 (control): a UI-mode change is still logged, once each way.
    {
        UiMode opened = UiMode::Exploration;
        const auto lines = dispatch_lines(capture([&] {
            h.key('z');
            opened = h.mode();
            h.run(1000);
            h.cancel();
            h.run(1000);
        }));
        all.insert(all.end(), lines.begin(), lines.end());
        const bool shape = lines.size() == 2 && lines[0].rfind("ui=explore ", 0) != 0 &&
                           lines[1] == "ui=explore combat=0 dungeon=0 source=world";
        check(opened != UiMode::Exploration && h.mode() == UiMode::Exploration && shape,
              "P3 Z opens a modal and Cancel closes it: one line each way (" + n(lines.size()) + ": " +
                  (lines.empty() ? std::string("none") : lines.front() + " / " + lines.back()) + ")");
    }

    // P4: a source change (world -> combat) is logged once; the fight's own frames are not.
    {
        auto &ctx = h.ctx();
        apply_debug_preset(ctx, DebugPreset::Combat);
        h.rt->render(h.board, true);
        const uint32_t f0 = h.frames();
        const auto lines = dispatch_lines(capture([&] {
            ctx.outdoor->enemies.clear();
            OutdoorEnemy troll{};
            troll.definition = 41;
            troll.tile = troll_tile(ctx);
            troll.x = h.rt->game().position.xy.x + 1;
            troll.y = h.rt->game().position.xy.y;
            ctx.outdoor->enemies.push_back(troll);
            h.key(' ');
            h.run(4000);
        }));
        all.insert(all.end(), lines.begin(), lines.end());
        const uint32_t drawn = h.frames() - f0;
        size_t combat_lines = 0;
        for (const auto &l : lines) combat_lines += l.find("source=combat") != std::string::npos;
        std::printf("  combat phase: %u frames, %zu dispatch lines\n", drawn, lines.size());
        for (const auto &l : lines) std::printf("    %s\n", l.c_str());
        check(h.combat() && drawn >= 40 && combat_lines >= 1 && lines.size() <= 4 && repeats(lines) == 0,
              "P4 a roaming troll's attack moves the source to the arena: " + n(combat_lines) +
                  " line(s) name source=combat, " + n(lines.size()) + " in all over " + n(drawn) + " frames");
    }

    // P6: the camp scene -- a source of its own (camp-scene) that comes and
    // goes while the UI mode may stay put, so the source alone must be compared.
    {
        Run c(kSeed);
        const bool on_land = land.x >= 0 && c.teleport(land.x + 1, land.y);
        const uint32_t f0 = c.frames();
        const auto lines = dispatch_lines(capture([&] {
            c.key('h');
            c.key('1');
            c.key('\r');
            c.key('n');
            c.run(30000);
        }));
        const uint32_t drawn = c.frames() - f0;
        std::printf("  camp phase: %u frames, %zu dispatch lines\n", drawn, lines.size());
        for (const auto &l : lines) std::printf("    %s\n", l.c_str());
        size_t camp = 0, source_only = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            camp += lines[i].find("source=camp-scene") != std::string::npos;
            if (i && lines[i].substr(0, lines[i].find(' ')) == lines[i - 1].substr(0, lines[i - 1].find(' ')))
                ++source_only;
        }
        check(on_land && camp >= 1 && source_only >= 1 && repeats(lines) == 0 && lines.size() <= 8,
              "P6 camping moves the source to the camp scene and back: " + n(camp) + " camp-scene line(s), " +
                  n(source_only) + " source-only change(s), " + n(lines.size()) + " lines over " + n(drawn) + " frames");
    }

    // P5: over the whole session no line repeats the one before it.
    check(!all.empty() && repeats(all) == 0,
          "P5 the whole session's " + n(all.size()) + " PRESENTATION_DISPATCH lines hold no repeat of the previous one (" +
              n(repeats(all)) + " repeats)");

    std::printf("A3-HF2.1 dispatch log: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
