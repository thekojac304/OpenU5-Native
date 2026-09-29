// Alpha 3 A3-04F -- render / TFT efficiency (ALPHA3_AUDIO.md section 26),
// through the REAL AlphaRuntime driving the REAL tdeck_board.cpp over the fake
// ST7789 (host_tests/board_shims, A3-04E's harness).
//
//   B  the census: for each scenario the brief names -- standing still with and
//      without animation, a step, a transcript line, a status change, a modal,
//      the Developer menu, combat entry / turns / exit -- the TFT transactions
//      (window commands and pixel payloads), the thin ones (fewer bytes than
//      the transaction's fixed cost), the pixels, the windows by screen region,
//      the draw-primitive calls and the modelled transfer time
//   G  the panel golden: over one fixed, untimed script the panel (all
//      320x240 pixels) after EVERY render call is hashed and compared with the
//      recorded sequence (a3_04f_panel_goldens.h), so a change to how the Board
//      draws cannot change what the panel shows. Recorded from the A3-04F
//      baseline; re-recorded ONCE, in its own commit, from the Alpha 4 UI
//      Batch 1 Board, whose restyle is the one intended change (ALPHA4_UI.md)
//
//   a3_04f_render_runtime <openu5-alpha1-resources.bin> [--record <goldens.h>]
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/native_renderer.h"
#include "../main/tdeck_board.h"
#include "a3_04f_panel_goldens.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/outdoor.h"
#include "openu5/perf_report.h"
#include "openu5/presentation.h"
#include "openu5/render_pacing.h"
#include "openu5/world.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;
namespace bus = openu5_host_bus;

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
std::string n(uint64_t v) { return std::to_string(v); }

uint64_t fnv(const uint16_t *p, size_t count) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < count; ++i) {
        h = (h ^ (p[i] & 0xff)) * 1099511628211ull;
        h = (h ^ (p[i] >> 8)) * 1099511628211ull;
    }
    return h;
}

// ---------------------------------------------------------------------------
// The runtime on the real Board.
// ---------------------------------------------------------------------------
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    size_t renders = 0, viewport_mismatches = 0, gameplay_checks = 0;
    // G: the panel after every render call (consecutive repeats folded).
    std::vector<uint64_t> *panel = nullptr;
    // A3-HF3: per panel state, whether it shows a D-63 hit cue (see hit_cue_visible).
    std::vector<uint8_t> *cues = nullptr;
    uint64_t last_windows = ~0ull;
    Run(uint32_t seed, bool timed, std::vector<uint64_t> *panel_log = nullptr, std::vector<uint8_t> *cue_log = nullptr)
        : panel(panel_log), cues(cue_log) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        bus::install();
        bus::model() = bus::Model{};
        bus::model().timed = timed;
        board.initialize_display();
        openu5_host_virtual_clock_us() = kClockStartUs;
        idle.attach(bus::idle_passes());
        board.set_idle_service(&idle);
        rt->attach_idle_service(&idle);
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
        f.patterned_test_tiles = true; // an animation frame changes pixels (A3-04F fixture option)
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
        render(true);
    }
    int64_t now() const { return openu5_host_virtual_clock_us(); }
    uint32_t frames() const {
        RenderPerfSnapshot s{};
        rt->render_perf().snapshot(uint64_t(now()), s);
        return s.frames;
    }
    void render(bool force = false) {
        rt->render(board, force);
        ++renders;
        if (panel && bus::stats().windows != last_windows) {
            last_windows = bus::stats().windows;
            const uint64_t h = fnv(bus::gram(), 320 * 240);
            if (panel->empty() || panel->back() != h) {
                panel->push_back(h);
                if (cues) cues->push_back(hit_cue_visible() ? 1 : 0);
            }
        }
        if (!rt->ui() || rt->ui()->mode() == UiMode::DebugMenu || !rt->composed_viewport()) return;
        ++gameplay_checks;
        const uint16_t *vp = rt->composed_viewport(), *p = bus::gram();
        for (int y = 9; y < 167; ++y)
            if (!std::equal(vp + y * 176, vp + y * 176 + 176, p + (4 + y) * 320 + 4)) {
                ++viewport_mismatches;
                break;
            }
    }
    /**
     * A3-HF3 (D-63): the frame shows a combat hit cue -- a viewport cell holding
     * tile 0 (the 0x359f marker; the fixture's patterned tiles have a closed
     * form, byte b of tile 0 = b*11 + (b>>3)*7) or a party row in reverse video
     * (most of its rectangle the text colour, not black).
     */
    bool hit_cue_visible() const {
        const uint16_t *vp = rt->composed_viewport();
        if (vp && rt->ui() && rt->ui()->mode() != UiMode::DebugMenu)
            for (int cy = 0; cy < 11; ++cy)
                for (int cx = 0; cx < 11; ++cx) {
                    bool tile0 = true;
                    for (int b = 0; b < 128 && tile0; ++b) {
                        const uint8_t v = uint8_t(b * 11 + (b >> 3) * 7);
                        const uint16_t *px = vp + (cy * 16 + b / 8) * 176 + cx * 16 + (b % 8) * 2;
                        tile0 = px[0] == uint16_t((v >> 4) * 0x1111) && px[1] == uint16_t((v & 15) * 0x1111);
                    }
                    if (tile0) return true;
                }
        const uint16_t *p = bus::gram();
        for (int row = 0; row < 6; ++row) {
            int black = 0, total = 0;
            for (int y = 4 + row * 8; y < 12 + row * 8; ++y)
                for (int x = kHudRightX; x < kHudRightX + kHudRightW; ++x, ++total) black += p[y * 320 + x] == 0;
            if (black * 2 < total) return true;
        }
        return false;
    }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = now();
        rt->handle(e);
    }
    void key(uint8_t code, bool alt = false) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = code;
        e.modifiers.alt = alt;
        raw(e);
        render();
    }
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
        render();
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
        render();
    }
    void dir(int dx, int dy) {
        ball(dx > 0 ? RawInputKind::TrackballRight
             : dx < 0 ? RawInputKind::TrackballLeft
             : dy > 0 ? RawInputKind::TrackballDown
                      : RawInputKind::TrackballUp);
    }
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            render();
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
        render(true);
        return ok;
    }
    std::vector<std::string> visible() const {
        std::vector<std::string> out;
        for (const auto &row : rows()) out.push_back(row.first);
        return out;
    }
    /** The transcript as the Board draws it: every geometry row's text and colour class (blank below the page). */
    std::vector<std::pair<std::string, int>> rows() const {
        UiRenderedLine lines[kHudTranscriptLines]{};
        const size_t count = rt->ui()->visible_lines(lines, kHudTranscriptLines, size_t(kHudTranscriptColumns));
        std::vector<std::pair<std::string, int>> out;
        for (size_t i = 0; i < count; ++i)
            out.emplace_back(lines[i].text, lines[i].channel == UiTextChannel::Prompt   ? 1
                                            : lines[i].channel == UiTextChannel::Combat ? 2
                                                                                         : 0);
        return out;
    }
    /** How many of the transcript's 19 geometry rows differ in text or colour between two snapshots. */
    static size_t changed_rows(const std::vector<std::pair<std::string, int>> &a,
                               const std::vector<std::pair<std::string, int>> &b) {
        size_t k = 0;
        for (size_t i = 0; i < size_t(kHudTranscriptLines); ++i) {
            const auto x = i < a.size() ? a[i] : std::pair<std::string, int>{"", 0};
            const auto y = i < b.size() ? b[i] : std::pair<std::string, int>{"", 0};
            k += x != y;
        }
        return k;
    }
};

// --- A3-HF1's fight helpers (a3_hf1_arena_loot_test.cpp), unchanged ---------
int enemies_alive(const CombatState &s) {
    int k = 0;
    for (int i = 0; i < s.count; ++i)
        if (s.actors[i].enemy && (s.actors[i].status == CombatStatus::Active || s.actors[i].status == CombatStatus::Sleeping)) ++k;
    return k;
}
bool arena_free(const CombatState &s, int x, int y) {
    if (x < 0 || y < 0 || x >= kCombatGrid || y >= kCombatGrid) return false;
    const int k = y * kCombatGrid + x;
    if (s.loot[k] == 1 || s.loot[k] == 129) return false;
    for (int i = 0; i < s.count; ++i)
        if (s.actors[i].position.x == x && s.actors[i].position.y == y &&
            (s.actors[i].status == CombatStatus::Active || s.actors[i].status == CombatStatus::Sleeping))
            return false;
    return true;
}
CombatActor *player_turn(Run &h) {
    auto &s = h.cs();
    if (!h.combat() || s.current < 0 || s.current >= s.count) return nullptr;
    auto &a = s.actors[s.current];
    if (a.enemy || a.member == 255 || a.status != CombatStatus::Active) return nullptr;
    return h.mode() == UiMode::Combat ? &a : nullptr;
}
CombatActor *settle(Run &h, int budget_ms = 20000) {
    for (int t = 0; t < budget_ms && h.combat(); t += 50) {
        if (auto *a = player_turn(h)) return a;
        openu5_host_virtual_clock_us() += 50000;
        h.render();
    }
    return player_turn(h);
}
void step_toward(Run &h, const CombatActor &a, int cx, int cy) {
    int best = 1 << 30, gx = -1, gy = -1;
    const int nx[] = {cx - 1, cx + 1, cx, cx}, ny[] = {cy, cy, cy - 1, cy + 1};
    for (int k = 0; k < 4; ++k) {
        if (!arena_free(h.cs(), nx[k], ny[k])) continue;
        const int d = std::abs(nx[k] - a.position.x) + std::abs(ny[k] - a.position.y);
        if (d < best) { best = d; gx = nx[k]; gy = ny[k]; }
    }
    if (gx < 0) { h.key(' '); return; }
    const int dx = gx - a.position.x, dy = gy - a.position.y;
    if (dx && arena_free(h.cs(), a.position.x + (dx > 0 ? 1 : -1), a.position.y)) h.dir(dx, 0);
    else if (dy && arena_free(h.cs(), a.position.x, a.position.y + (dy > 0 ? 1 : -1))) h.dir(0, dy);
    else if (dx) h.dir(dx, 0);
    else h.key(' ');
}
bool fight(Run &h) {
    for (int turn = 0; turn < 400 && h.combat() && enemies_alive(h.cs()); ++turn) {
        auto *a = settle(h);
        if (!a || !enemies_alive(h.cs())) break;
        const CombatActor *target = nullptr;
        int best = 1 << 30;
        for (int i = 0; i < h.cs().count; ++i) {
            const auto &e = h.cs().actors[i];
            if (!e.enemy || e.status != CombatStatus::Active) continue;
            const int d = std::max(std::abs(e.position.x - a->position.x), std::abs(e.position.y - a->position.y));
            if (d < best) { best = d; target = &e; }
        }
        if (!target) break;
        if (best <= std::max<int>(1, a->range)) {
            const int tx = target->position.x, ty = target->position.y;
            h.key('a');
            if (h.mode() != UiMode::TargetSelection) return false;
            int rx = a->position.x, ry = a->position.y;
            while (rx != tx) { h.dir(tx > rx ? 1 : -1, 0); rx += tx > rx ? 1 : -1; }
            while (ry != ty) { h.dir(0, ty > ry ? 1 : -1); ry += ty > ry ? 1 : -1; }
            h.key('\r');
        } else
            step_toward(h, *a, target->position.x, target->position.y);
    }
    return h.combat() && !enemies_alive(h.cs());
}
int troll_tile(CommandContext &ctx) {
    for (size_t i = 0; i < ctx.outdoor->resources->enemy_count; ++i)
        if (const auto *d = ctx.outdoor->resources->enemies[i]; d && d->index == 41) return d->tile;
    return -1;
}
bool troll_attack(Run &h) {
    auto &ctx = h.ctx();
    ctx.outdoor->enemies.clear();
    OutdoorEnemy troll{};
    troll.definition = 41;
    troll.tile = troll_tile(ctx);
    troll.x = h.rt->game().position.xy.x + 1;
    troll.y = h.rt->game().position.xy.y;
    ctx.outdoor->enemies.push_back(troll);
    h.key(' ');
    h.run(1500);
    return h.combat();
}

// --- Places on Britannia (A3-HF2.1's search, and its inverse) ---------------
struct Spot {
    int x = -1, y = -1;
};
/** A 5x3 block of grass (tile 5); `water` = animated water within the 9x9 around it, or none within 11x11. */
Spot find_grass(bool water) {
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    if (m.error != Error::None) return {};
    for (int y = 60; y < 200; ++y)
        for (int x = 60; x < 200; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 1 && ok; ++dy)
                for (int dx = -2; dx <= 2 && ok; ++dx) ok = m.value.tile_at(x + dx, y + dy) == 5;
            if (!ok) continue;
            bool animated_near = false;
            const int r = water ? 4 : 6;
            for (int dy = -r; dy <= r && !animated_near; ++dy)
                for (int dx = -r; dx <= r && !animated_near; ++dx)
                    animated_near = tile_animation_kind(m.value.tile_at(x + dx, y + dy)) != TileAnimationKind::Static;
            if (animated_near == water) return {x, y};
        }
    return {};
}
/** The 5x3 grass block with the most animated cells in its 11x11 view: a coast. */
Spot find_coast(int &animated) {
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    animated = 0;
    Spot best{};
    if (m.error != Error::None) return best;
    for (int y = 20; y < 236; ++y)
        for (int x = 20; x < 236; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 1 && ok; ++dy)
                for (int dx = -2; dx <= 2 && ok; ++dx) ok = m.value.tile_at(x + dx, y + dy) == 5;
            if (!ok) continue;
            int k = 0;
            for (int dy = -5; dy <= 5; ++dy)
                for (int dx = -5; dx <= 5; ++dx)
                    k += tile_animation_kind(m.value.tile_at(x + dx, y + dy)) != TileAnimationKind::Static;
            if (k > animated) {
                animated = k;
                best = {x, y};
            }
        }
    return best;
}

// ---------------------------------------------------------------------------
// B -- the census.
// ---------------------------------------------------------------------------
struct Region {
    uint32_t windows = 0;
    uint64_t pixels = 0;
};
struct Census {
    std::string name;
    uint32_t frames = 0;
    bus::Stats s{};
    tdeck::BoardDrawCalls d{};
    std::map<std::string, Region> regions;
    uint32_t solid = 0, transcript_rows = 0, party_rows = 0, status_rows = 0;
    std::vector<bus::WindowWrite> windows;
};
const char *region_of(const bus::WindowWrite &w) {
    if (w.width() == 320 && w.height() == 240) return "full-clear";
    if (w.x0 == kHudPartyFrameX && w.width() == 320 - kHudPartyFrameX && w.height() >= 200) return "panel-clear";
    if (w.width() <= 2 || w.height() <= 2) return "lines";
    if (w.x1 < kHudPartyFrameX) {
        if (w.y1 < kHudSkyBarY + kHudSkyBarH) return "sky";
        if (w.y0 >= kHudWindBarY) return "wind";
        return "viewport";
    }
    if (w.y1 < kHudWorldFrameY) return "party";
    if (w.y1 < kHudTranscriptSeparatorY) return "status";
    return "transcript";
}
template <class F> Census measure(Run &h, const char *name, F action) {
    bus::reset_stats();
    h.board.reset_draw_calls();
    const uint32_t f0 = h.frames();
    action();
    Census c;
    c.name = name;
    c.frames = h.frames() - f0;
    c.s = bus::stats();
    c.d = h.board.draw_calls();
    c.windows = bus::windows();
    for (const auto &w : bus::windows()) {
        const char *r = region_of(w);
        auto &reg = c.regions[r];
        ++reg.windows;
        reg.pixels += w.pixels;
        c.solid += w.solid;
        if (!std::strcmp(r, "transcript")) ++c.transcript_rows;
        if (!std::strcmp(r, "party")) ++c.party_rows;
        if (!std::strcmp(r, "status")) ++c.status_rows;
    }
    return c;
}
std::string fixed(double v, int places = 1) {
    char b[32];
    std::snprintf(b, sizeof b, "%.*f", places, v);
    return b;
}
void print(const Census &c) {
    const double f = c.frames ? double(c.frames) : 1.0;
    const uint64_t cmds = c.s.transactions - c.s.pixel_transactions;
    std::printf("  %-34s frames %4u | per frame: txns %7s (cmd %6s pix %6s thin %6s tiny %6s) windows %6s px %8s "
                "xfer %6s ms | fills %s text %s mtext %s rgb %s sky %s | max txn %u B\n",
                c.name.c_str(), c.frames, fixed(double(c.s.transactions) / f).c_str(), fixed(double(cmds) / f).c_str(),
                fixed(double(c.s.pixel_transactions) / f).c_str(), fixed(double(c.s.thin_transactions) / f).c_str(),
                fixed(double(c.s.tiny_transactions) / f).c_str(), fixed(double(c.s.windows) / f).c_str(),
                fixed(double(c.s.pixel_bytes) / 2.0 / f, 0).c_str(), fixed(double(c.s.xfer_ns) / 1e6 / f, 2).c_str(),
                fixed(c.d.fill_rects / f).c_str(), fixed(c.d.text_boxes / f).c_str(),
                fixed(c.d.metric_text_boxes / f).c_str(), fixed(c.d.rgb565 / f).c_str(), fixed(c.d.sky_strips / f).c_str(),
                unsigned(c.s.max_transaction_bytes));
    std::string regions;
    for (const auto &[r, reg] : c.regions)
        regions += " " + r + " " + fixed(reg.windows / f) + "w/" + fixed(double(reg.pixels) / f, 0) + "px";
    std::printf("  %-34s   regions per frame:%s | solid windows %s\n", "", regions.c_str(), fixed(c.solid / f).c_str());
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    const char *record = argc >= 4 && !std::strcmp(argv[2], "--record") ? argv[3] : nullptr;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report_out{};
    if (source.open(argv[1], report_out) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report_out) != ESP_OK) return 2;
    pack = &owners;
    g_dungeon_count = report_out.dungeon_count;
    const Spot shore = find_grass(true), inland = find_grass(false);
    int coast_cells = 0;
    const Spot coast = find_coast(coast_cells);
    std::printf("grass with water in view at (%d,%d); grass with nothing animated near at (%d,%d); the coast with the "
                "most animated cells in view (%d) at (%d,%d)\n",
                shore.x, shore.y, inland.x, inland.y, coast_cells, coast.x, coast.y);

    // ---- B: the census (timed: the modelled transfer time moves the clock) --
    std::printf("\nB  census (modelled bus: 40 MHz, 26 us per DMA / 24 us per CPU transaction; row building not modelled)\n");
    std::vector<Census> census;
    {
        Run h(kSeed, /*timed=*/true);
        const bool at_inland = inland.x >= 0 && h.teleport(inland.x, inland.y);
        h.run(500);
        census.push_back(measure(h, "B1 stand 2 s, nothing animated", [&] { h.run(2000); }));
        const bool at_shore = shore.x >= 0 && h.teleport(shore.x, shore.y);
        h.run(500);
        census.push_back(measure(h, "B2 stand 2 s, water in view", [&] { h.run(2000); }));
        // For the transcript rule: each scenario's rows before and after, and how many changed.
        std::map<std::string, size_t> changed;
        auto tracked = [&](const char *name, auto action) {
            const auto before = h.rows();
            census.push_back(measure(h, name, action));
            changed[name] = Run::changed_rows(before, h.rows());
        };
        tracked("B3 one step (the step frame)", [&] { h.dir(1, 0); });
        h.run(300);
        // Fill the transcript (each step adds a line) so every later line scrolls it.
        for (int i = 0; i < 24; ++i) {
            h.dir(i % 2 ? 1 : -1, 0);
            h.run(200);
        }
        const auto lines = h.visible();
        std::printf("  transcript after 24 steps: %zu visible rows, the last: \"%s\" / \"%s\"\n", lines.size(),
                    lines.size() > 1 ? lines[lines.size() - 2].c_str() : "", lines.empty() ? "" : lines.back().c_str());
        tracked("B4 step, full transcript, zig-zag", [&] { h.dir(-1, 0); });
        h.run(300);
        tracked("B5 step, full transcript, zig-zag", [&] { h.dir(1, 0); });
        h.run(300);
        tracked("B6 Pass (one line + clock)", [&] { h.key(' '); });
        h.run(300);
        for (int i = 0; i < 12; ++i) {
            h.key(' ');
            h.run(100);
        }
        const auto passes = h.visible();
        std::printf("  transcript after 12 more Passes: \"%s\" / \"%s\"\n",
                    passes.size() > 1 ? passes[passes.size() - 2].c_str() : "", passes.empty() ? "" : passes.back().c_str());
        tracked("B6b Pass, 12 Passes above it", [&] { h.key(' '); });
        h.run(300);
        // The same text on another channel: the row keeps its text and changes colour.
        tracked("B6d \"Pass\" as a prompt line", [&] {
            const_cast<UiSession *>(h.rt->ui())->append(UiTextChannel::Prompt, "Pass");
            h.render(true);
        });
        h.run(300);
        // T3: a message long enough to wrap over three rows (the 22-column transcript).
        tracked("B6c a wrapped 3-row message", [&] {
            const_cast<UiSession *>(h.rt->ui())->append(UiTextChannel::Message,
                "The quick brown fox jumps over the lazy dog near the old ferry at dawn");
            h.render(true);
        });
        h.run(300);
        census.push_back(measure(h, "B7 status only (HP -1, redraw)", [&] {
            --h.rt->game().party.characters[0].current_hp;
            h.render(true);
        }));
        h.run(300);
        census.push_back(measure(h, "B8 nothing changed (forced redraw)", [&] { h.render(true); }));
        h.run(300);
        census.push_back(measure(h, "B9 Z modal open", [&] { h.key('z'); }));
        census.push_back(measure(h, "B10 Z modal close", [&] { h.cancel(); }));
        h.run(300);
        census.push_back(measure(h, "B11 Developer menu open", [&] { h.key('d', true); }));
        census.push_back(measure(h, "B12 Developer menu close (repaint)", [&] { h.key('\b'); }));
        h.run(300);
        apply_debug_preset(h.ctx(), DebugPreset::Combat);
        h.render(true);
        census.push_back(measure(h, "B13 combat entry (+1.5 s)", [&] { troll_attack(h); }));
        bool won = false;
        census.push_back(measure(h, "B14 the fight (all frames)", [&] { won = fight(h); }));
        census.push_back(measure(h, "B15 leave the victory arena", [&] { h.key('\b'); }));
        census.push_back(measure(h, "B16 back on the map 1 s", [&] { h.run(1000); }));
        const bool at_coast = coast.x >= 0 && h.teleport(coast.x, coast.y);
        h.run(500);
        census.push_back(measure(h, "B17 stand 2 s on the coast", [&] { h.run(2000); }));
        census.push_back(measure(h, "B18 step on the coast", [&] { h.dir(1, 0); }));
        h.run(300);
        census.push_back(measure(h, "B19 one animation tick, coast", [&] {
            openu5_host_virtual_clock_us() += 55000;
            h.render();
        }));
        for (const auto &c : census) print(c);
        check(at_inland && at_shore && at_coast && won && !h.combat() && h.viewport_mismatches == 0 && bus::stats().malformed == 0,
              "B0 the census ran every scenario (inland, shore, 24 steps, Pass, status, Z, Developer, a won fight and "
              "back) with the panel's viewport equal to the composed one after all " + n(h.gameplay_checks) +
              " gameplay renders and no malformed transaction");

        auto by = [&](const char *prefix) -> const Census & {
            for (const auto &c : census)
                if (c.name.rfind(prefix, 0) == 0 && c.name[std::strlen(prefix)] == ' ') return c;
            std::printf("no scenario %s\n", prefix);
            std::exit(4);
        };
        // ---- P: how the pixels travel ---------------------------------------
        // P1: the panel's window wraps, so whole rows can share one transaction
        // up to the bus's max_transfer_sz (640 B = one 320 px row). A window
        // needs ceil(rows / floor(320 / width)) of them; anything more is
        // per-transaction overhead (26 us each) spent on nothing.
        {
            size_t over = 0, seen = 0;
            std::string example;
            for (const auto &c : census)
                for (const auto &w : c.windows) {
                    ++seen;
                    const uint32_t per = std::max(1, 320 / w.width());
                    const uint32_t bound = (uint32_t(w.height()) + per - 1) / per;
                    if (w.transactions > bound) {
                        if (example.empty())
                            example = n(w.width()) + "x" + n(w.height()) + " at (" + n(w.x0) + "," + n(w.y0) + ") in " +
                                      n(w.transactions) + " transactions, " + n(bound) + " needed (" + c.name + ")";
                        ++over;
                    }
                }
            check(over == 0 && bus::max_transfer_bytes() == 640,
                  "P1 every window's pixels travel as whole rows packed up to the bus's 640 B: " + n(over) + " of " +
                      n(seen) + " windows use more transactions than their rows need" +
                      (example.empty() ? std::string() : "; e.g. " + example));
        }
        // P2: adjacent animated cells of one tile row are one window, and none of
        // an animation tick's transactions is tiny (a 16 px row = 32 B).
        {
            const auto &tick = by("B19");
            size_t adjacent = 0;
            for (const auto &a : tick.windows)
                for (const auto &b : tick.windows) adjacent += a.y0 == b.y0 && a.y1 == b.y1 && a.x1 + 1 == b.x0;
            check(tick.frames == 1 && tick.windows.size() > 0 && adjacent == 0 && tick.s.tiny_transactions == 0,
                  "P2 one animation tick on the coast: " + n(tick.windows.size()) + " windows, " + n(adjacent) +
                      " of them side by side in one tile row, " + n(tick.s.tiny_transactions) + " tiny transactions of " +
                      n(tick.s.transactions) + " (" + fixed(double(tick.s.xfer_ns) / 1e6, 2) + " ms modelled)");
        }
        // ---- R: retained regions redraw what changed -----------------------
        {
            const auto &same = by("B8"), &hp = by("B7");
            check(same.party_rows == 0 && same.status_rows == 0,
                  "R1 a redraw with nothing changed draws no party or status row (" + n(same.party_rows) + " party, " +
                      n(same.status_rows) + " status)");
            check(hp.party_rows == 1 && hp.status_rows == 0,
                  "R2 one member's HP changes: exactly that party row is drawn (" + n(hp.party_rows) + " party, " +
                      n(hp.status_rows) + " status)");
            // R3 / R4: the bed's status refresh (CMDS 0x060a / 0x0674) draws the same
            // rows outside show_alpha. What it leaves must be what the rows' cache
            // believes, so the next frame puts back exactly what differs.
            auto block = [] { // the party and status block, x 182-319, y 0-85
                uint64_t v = 1469598103934665603ull;
                for (int y = 0; y < kHudTranscriptSeparatorY; ++y) v ^= fnv(bus::gram() + y * 320 + kHudPartyFrameX, 320 - kHudPartyFrameX) + uint64_t(y);
                return v;
            };
            h.run(300);
            const uint64_t shown = block();
            tdeck::DevicePartyHighlight flash{};
            flash.damage_flash = 0;
            h.board.refresh_bed_status_panel(h.rt->game(), flash); // row 0 in reverse video, same text
            const bool flashed = block() != shown;
            const Census back = measure(h, "R3 after a bed refresh in reverse video", [&] { h.render(true); });
            check(flashed && back.party_rows == 1 && back.status_rows == 0 && block() == shown,
                  "R3 a row the bed refresh drew in reverse video (same text) is redrawn by the next frame, and only it (" +
                      n(back.party_rows) + " party, " + n(back.status_rows) + " status): the panel is as before");
            GameState other = h.rt->game();
            other.party.characters[0].current_hp = uint16_t(other.party.characters[0].current_hp / 2);
            h.board.refresh_bed_status_panel(other, {});
            const bool halved = block() != shown;
            const Census back2 = measure(h, "R4 after a bed refresh with other HP", [&] { h.render(true); });
            check(halved && back2.party_rows == 1 && back2.status_rows == 0 && block() == shown,
                  "R4 a row the bed refresh drew with other text is redrawn by the next frame, and only it (" +
                      n(back2.party_rows) + " party, " + n(back2.status_rows) + " status): the panel is as before");
        }
        // P3: a narrow, tall window still pauses after rows 16, 32 and 48 --
        // the batches end at a pause row -- and arrives intact. Board::show_view
        // (the legacy boot view) is the one public path to an arbitrary window.
        {
            std::vector<uint16_t> art(16 * 64);
            for (size_t i = 0; i < art.size(); ++i) art[i] = uint16_t(i * 2654435761u >> 16);
            bus::reset_stats();
            h.board.show_view(art.data(), 16, 64, "x", "y", false);
            size_t tall = 0;
            for (const auto &p : bus::pauses()) tall += p.x1 - p.x0 == 15 && p.y1 - p.y0 == 63;
            bool intact = true;
            for (int y = 0; y < 64; ++y)
                intact = intact && std::equal(art.begin() + y * 16, art.begin() + y * 16 + 16, bus::gram() + (8 + y) * 320 + 8);
            size_t window_txns = 0;
            for (const auto &w : bus::windows())
                if (w.width() == 16 && w.height() == 64) window_txns = w.transactions;
            check(tall == 3 && intact && window_txns == 4,
                  "P3 a 16 x 64 window pauses after rows 16, 32 and 48 (" + n(tall) + " pauses) and its 64 rows arrive "
                  "intact in " + n(window_txns) + " transactions (17 + 16 + 16 + 15 rows: a batch ends at each pause row)");
        }
        // ---- T: the transcript --------------------------------------------
        {
            std::string rule;
            bool exact = true;
            for (const char *name : {"B3", "B4", "B5", "B6", "B6b", "B6c", "B6d"}) {
                const auto &c = by(name);
                const size_t want = changed.at(c.name);
                exact = exact && c.transcript_rows == want;
                rule += " " + std::string(name) + " " + n(c.transcript_rows) + "/" + n(want);
            }
            check(by("B2").transcript_rows == 0 && by("B8").transcript_rows == 0 && by("B17").transcript_rows == 0,
                  "T1 animation ticks and a redraw with nothing changed draw no transcript row (control)");
            check(by("B3").transcript_rows == 1,
                  "T2 a new short line while the transcript is not yet full: one row (control, " +
                      n(by("B3").transcript_rows) + ")");
            check(exact, "T3 on a line, a scroll and a wrapped message the rows drawn are exactly the rows whose text or "
                         "colour changed (drawn/changed:" + rule + ")");
            check(by("B10").transcript_rows == size_t(kHudTranscriptLines) &&
                      by("B12").transcript_rows == size_t(kHudTranscriptLines),
                  "T5 closing the Z modal and leaving the Developer screen still redraw all " + n(kHudTranscriptLines) +
                      " transcript rows (the retained rows are invalidated, control)");
        }
    }
    // ---- C: the viewport CRC is the same function -------------------------
    {
        auto reference = [](const uint16_t *p, size_t count) { // native_renderer.cpp up to A3-HF2.1
            uint32_t crc = 0xffffffffU;
            for (size_t i = 0; i < count; ++i) {
                const uint8_t bytes[] = {uint8_t(p[i]), uint8_t(p[i] >> 8)};
                for (uint8_t value : bytes) {
                    crc ^= value;
                    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
                }
            }
            return crc ^ 0xffffffffU;
        };
        std::vector<uint16_t> buf(kViewportPixelCount);
        uint32_t x = 0x2545f491u;
        size_t same = 0, total = 0;
        for (int trial = 0; trial < 64; ++trial) {
            for (auto &v : buf) {
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                v = trial == 0 ? 0 : trial == 1 ? 0xffff : uint16_t(x);
            }
            const size_t count = trial < 8 ? size_t(trial * 7) : buf.size() - size_t(trial % 5);
            ++total;
            same += recompute_viewport_crc32(buf.data(), count) == reference(buf.data(), count);
        }
        const uint16_t check_vector[] = {0x3231, 0x3433, 0x3635, 0x3837, 0x0039}; // "123456789" + 0x00
        // "12345678" as four little-endian pixels: zlib's crc32 of it is 0x9ae0daaf.
        const bool standard = recompute_viewport_crc32(check_vector, 4) == 0x9ae0daafU &&
                              reference(check_vector, 4) == 0x9ae0daafU;
        check(same == total && standard,
              "C1 the viewport CRC equals the bit-by-bit CRC-32 it replaces on " + n(same) + "/" + n(total) +
                  " buffers (empty, all 0, all 1, random, odd lengths): the Board's change detector and every logged "
                  "crc= are unchanged");
    }
    // ---- I: the idle-service guarantee holds (A3-04E.1) ---------------------
    {
        Run h(kSeed, /*timed=*/true);
        bus::model().rows_feed_idle = false; // the hardware's worst case: only tick-long blocks feed IDLE0
        h.teleport(coast.x, coast.y);
        uint32_t seen = *bus::idle_passes();
        int64_t seen_at = h.now(), gap = 0;
        auto observe = [&] {
            if (*bus::idle_passes() != seen) {
                seen = *bus::idle_passes();
                seen_at = h.now();
            } else
                gap = std::max(gap, h.now() - seen_at);
        };
        for (int step = 0, pass = 0; step < 40; ++pass) { // input always queued: main.cpp's peek never waits
            openu5_host_virtual_clock_us() += 1000;
            if (pass % 100 == 0) {
                tdeck::RawInputEvent e{};
                e.kind = step++ % 2 ? RawInputKind::TrackballRight : RawInputKind::TrackballLeft;
                h.raw(e);
            }
            h.rt->render(h.board);
            observe();
            h.idle.enforce();
            observe();
        }
        check(gap <= int64_t(kIdleServiceBudgetUs) + 50000 && h.idle.stats().unserviced == 0,
              "I1 walking the coast with input always queued and rows that never let the idle task finish a pass: the "
              "longest idle gap is " + fixed(double(gap) / 1000.0) + " ms (budget " +
              fixed(double(kIdleServiceBudgetUs) / 1000.0) + " ms), no enforcement in vain (control)");
    }

    // ---- G: the panel golden (untimed: the script alone moves the clock) ----
    std::vector<uint64_t> panel;
    std::vector<uint8_t> cues;
    std::vector<size_t> phase_start;
    size_t renders = 0;
    {
        Run h(kSeed, /*timed=*/false, &panel, &cues);
        auto phase = [&] { phase_start.push_back(panel.size()); };
        phase(); // 0: boot, the first gameplay frame
        h.teleport(shore.x, shore.y);
        h.run(1000);
        phase(); // 1: 30 steps (the transcript fills and scrolls), animation between them
        for (int i = 0; i < 30; ++i) {
            h.dir(i % 4 < 2 ? 1 : -1, 0);
            h.run(150);
        }
        phase(); // 2: Pass, a status-only change, a forced redraw
        h.key(' ');
        h.run(300);
        --h.rt->game().party.characters[0].current_hp;
        h.render(true);
        h.render(true);
        h.run(300);
        phase(); // 3: the Z modal, the Developer menu (its repaint on leaving)
        h.key('z');
        h.run(300);
        h.cancel();
        h.run(300);
        h.key('d', true);
        h.run(200);
        h.key('\b');
        h.run(300);
        phase(); // 4: combat -- entry, the fight, leaving the arena
        apply_debug_preset(h.ctx(), DebugPreset::Combat);
        h.render(true);
        troll_attack(h);
        fight(h);
        h.key('\b');
        h.run(1000);
        phase(); // 5: walking inland again
        for (int i = 0; i < 6; ++i) {
            h.dir(i % 2 ? 1 : -1, 0);
            h.run(150);
        }
        phase(); // 6: the coast -- many animated cells, standing and stepping
        h.teleport(coast.x, coast.y);
        h.run(600);
        for (int i = 0; i < 4; ++i) {
            h.dir(i % 2 ? -1 : 1, 0);
            h.run(200);
        }
        phase(); // 7: 14 Passes -- identical lines scroll the transcript
        for (int i = 0; i < 14; ++i) {
            h.key(' ');
            h.run(100);
        }
        renders = h.renders;
        check(h.viewport_mismatches == 0 && bus::stats().malformed == 0 && bus::stats().oversize == 0,
              "G0 the golden script: " + n(h.renders) + " render calls, " + n(panel.size()) +
                  " distinct panel states, the viewport always the composed one, no malformed or oversize transaction");
    }
    if (record) {
        FILE *out = std::fopen(record, "wb");
        if (!out) return 3;
        std::fprintf(out, "#pragma once\n// A3-04F (ALPHA3_AUDIO.md section 26): the panel after every render call of\n"
                          "// a3_04f_render_runtime's golden script (consecutive repeats folded), FNV-1a\n"
                          "// over all 320x240 pixels, with `a3_04f_render_runtime <pack> --record <this file>`.\n"
                          "// Re-recorded ONCE from the Alpha 4 UI Batch 1 Board (ALPHA4_UI.md): its restyle is\n"
                          "// the one intended change; the census (R/T/P) that guards A3-04F's efficiency is\n"
                          "// unchanged. Never regenerate it from a Board that changes how, not what, it draws.\n"
                          "#include <cstddef>\n#include <cstdint>\nnamespace a3_04f_goldens {\n");
        std::fprintf(out, "constexpr const char *kSource = \"Alpha 4 UI Batch 1 restyle (the reviewed A4-UI1 Board)\";\n");
        std::fprintf(out, "constexpr size_t kRenders = %zu;\n", renders);
        std::fprintf(out, "constexpr size_t kPhaseCount = %zu;\nconstexpr size_t kPhaseStart[%zu] = {", phase_start.size(),
                     phase_start.size());
        for (size_t i = 0; i < phase_start.size(); ++i) std::fprintf(out, "%s%zu", i ? ", " : "", phase_start[i]);
        std::fprintf(out, "};\nconstexpr size_t kCount = %zu;\nconstexpr uint64_t kPanel[%zu] = {\n", panel.size(),
                     panel.size());
        for (size_t i = 0; i < panel.size(); ++i)
            std::fprintf(out, "%s0x%016llxull,%s", i % 4 ? " " : "    ", (unsigned long long)panel[i], i % 4 == 3 ? "\n" : "");
        std::fprintf(out, "\n};\n} // namespace a3_04f_goldens\n");
        std::fclose(out);
        std::printf("recorded %zu panel states over %zu render calls to %s\n", panel.size(), renders, record);
    }
    {
        // A3-HF3 (D-63, ALPHA3_AUDIO.md section 27.8): the fight (phase 4) shows the
        // combat hit cue. Alpha 4 UI Batch 1 re-recorded the golden from its
        // restyled Board, cues included, so G1 is plain equality and G2 reads the
        // cue off the pixels (hit_cue_visible()): it shows, and only in the fight.
        size_t first_diff = std::min(panel.size(), a3_04f_goldens::kCount);
        size_t cue_states = 0, cue_outside_fight = 0;
        const size_t fight_begin = phase_start.size() > 5 ? phase_start[4] : 0;
        const size_t fight_end = phase_start.size() > 5 ? phase_start[5] : 0;
        for (size_t i = 0; i < cues.size(); ++i)
            if (cues[i]) {
                ++cue_states;
                if (i < fight_begin || i >= fight_end) ++cue_outside_fight;
            }
        for (size_t i = 0; i < first_diff; ++i)
            if (panel[i] != a3_04f_goldens::kPanel[i]) {
                first_diff = i;
                break;
            }
        check(cue_states > 0 && cue_outside_fight == 0,
              "G2 the D-63 hit cue shows (reverse video / tile 0 read off the panel) in " + n(cue_states) +
                  " panel states, all of them in the fight; none outside it");
        size_t phase = 0;
        for (size_t p = 0; p < a3_04f_goldens::kPhaseCount; ++p)
            if (first_diff >= a3_04f_goldens::kPhaseStart[p]) phase = p;
        const bool same = a3_04f_goldens::kCount > 0 && panel.size() == a3_04f_goldens::kCount &&
                          first_diff == panel.size() && renders == a3_04f_goldens::kRenders;
        check(same, "G1 the panel after every render call equals the recorded golden (" + n(panel.size()) + " states vs " +
                        n(a3_04f_goldens::kCount) + " recorded, " + n(renders) + " vs " + n(a3_04f_goldens::kRenders) +
                        " render calls" + (same ? "" : "; first difference at state " + n(first_diff) + ", phase " + n(phase)) +
                        "; source: " + a3_04f_goldens::kSource + ")");
    }

    std::printf("A3-04F render runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
