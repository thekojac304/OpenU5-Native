// Screenshot capture tool (not a test; no ctest entry): the REAL AlphaRuntime on the REAL tdeck_board.cpp over the
// fake ST7789, drawing with the device's own tile art (openu5-assets.bin) and dungeon art. Each scene writes the
// 320x240 panel as <dir>/<scene>.png. Build with -DOPENU5_BUILD_CAPTURE=ON.
//
//   native_capture_runtime <resources.bin> <assets.bin> <audio.bin> --dump <dir> [--scene <name>]
#include "a4_ui2_harness.h"
#include "openu5/debug_map_picker.h"
#include "openu5/quest_state.h"
#include "openu5/movement.h"

using namespace a4_ui2;

namespace {
const std::vector<Member> kParty = {{"Avatar", 'G', 120}, {"Iolo", 'G', 110}, {"Shamino", 'G', 90}, {"Dupre", 'G', 130}};
CommandContext &ctx(Run &h) { return h.rt->command_context_for_test(); }
size_t ordinal(Run &h, DebugDestinationKind kind, int location) {
    for (size_t i = 0; i < debug_destination_count(ctx(h)); ++i) {
        const auto d = debug_destination_at(ctx(h), i);
        if (d.error != Error::None || d.value.kind != kind) continue;
        if (kind == DebugDestinationKind::Britannia || kind == DebugDestinationKind::Underworld || d.value.location == location)
            return i;
    }
    return size_t(-1);
}
void edit_field(Run &h, const std::string &digits) {
    h.key('\r');
    for (char c : digits) h.key(uint8_t(c));
    h.key('\r');
}
void dev_teleport(Run &h, DebugDestinationKind kind, int location) {
    h.key('d', true);
    h.key('\r');
    edit_field(h, std::to_string(ordinal(h, kind, location)));
    for (int i = 0; i < 5; ++i) h.down();
    h.key('\r');
    h.key('\b');
    h.key('\b');
    h.run(200);
}
void settle(Run &h, int ms = 400) { h.render(true); h.run(ms); }
void grass(Run &h, int x, int y) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {uint8_t(x), uint8_t(y)};
    settle(h);
}
bool enter_place(Run &h, int location) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {pack->location_x[location - 1], pack->location_y[location - 1]};
    h.key('e');
    h.run(100);
    return g.position.map.location == location;
}
// Stand beside the index-th NPC with a dialogue script and (T)alk to it.
bool talk_to_nth(Run &h, int nth) {
    auto &g = h.rt->game();
    int seen = 0;
    for (size_t i = 0; i < h.rt->actors().count; ++i) {
        const auto *a = &h.rt->actors().actors[i];
        if (a->location != g.position.map.location || !a->schedule.dialog || seen++ != nth) continue;
        g.position.map.floor = a->z;
        constexpr Direction dirs[] = {Direction::South, Direction::North, Direction::East, Direction::West};
        for (auto dir : dirs) {
            const auto dd = direction_delta(dir);
            const int x = a->x + dd.dx, y = a->y + dd.dy;
            if (!is_passable(ctx(h).terrain->effective(ctx(h).world, g.position.map, x, y), TransportMode::Foot).value) continue;
            bool occupied = false;
            for (size_t k = 0; k < h.rt->actors().count; ++k) {
                const auto &o = h.rt->actors().actors[k];
                if (o.location == g.position.map.location && o.z == a->z && o.x == x && o.y == y) occupied = true;
            }
            if (occupied) continue;
            g.position.xy = {uint8_t(x), uint8_t(y)};
            const RawInputKind toward = dir == Direction::South ? RawInputKind::TrackballUp : dir == Direction::North ? RawInputKind::TrackballDown
                                      : dir == Direction::East ? RawInputKind::TrackballLeft : RawInputKind::TrackballRight;
            h.key('t');
            h.ball(toward);
            h.run(300);
            return true;
        }
    }
    return false;
}
CombatState &cs(Run &h) { return h.rt->combat_state_for_test(); }
bool in_combat(Run &h) { return ctx(h).combat && cs(h).initialized; }
bool inland_grass(int &gx, int &gy) {
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    if (m.error != Error::None) return false;
    for (int y = 60; y < 200; ++y)
        for (int x = 60; x < 200; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 1 && ok; ++dy)
                for (int dx = -2; dx <= 2 && ok; ++dx) ok = m.value.tile_at(x + dx, y + dy) == 5;
            if (ok) { gx = x; gy = y; return true; }
        }
    return false;
}
// Surface fight: the party meets `n` enemies of definition `def` on open ground, so the HUD names Britannia.
bool surface_fight(Run &h, int def, int n, int gx, int gy) {
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::Britannia;
    r.x = gx; r.y = gy;
    if (apply_debug_teleport(ctx(h), r).status != DebugTeleportStatus::Applied) return false;
    h.render(true);
    auto &c = ctx(h);
    c.outdoor->enemies.clear();
    int tile = -1;
    for (size_t i = 0; i < c.outdoor->resources->enemy_count; ++i)
        if (const auto *e = c.outdoor->resources->enemies[i]; e && e->index == def) tile = e->tile;
    if (tile < 0) return false;
    const int dx[] = {1, 1, 1, 2, 2}, dy[] = {0, 1, -1, 0, 1};
    for (int k = 0; k < n && k < 5; ++k) {
        OutdoorEnemy e{};
        e.definition = def; e.tile = tile;
        e.x = h.rt->game().position.xy.x + dx[k];
        e.y = h.rt->game().position.xy.y + dy[k];
        c.outdoor->enemies.push_back(e);
    }
    h.key(' ');
    h.run(1500);
    for (int t = 0; t < 20000 && !(in_combat(h) && h.rt->ui()->mode() == UiMode::Combat); t += 5) h.run(5);
    return in_combat(h);
}
void type(Run &h, const char *s) { for (; *s; ++s) h.key(uint8_t(*s)); }
bool want(const char *only, const char *name) { return !only || !std::strcmp(only, name); }
} // namespace

int main(int argc, char **argv) {
    if (argc < 4 || !arg_after(argc, argv, "--dump")) {
        std::fprintf(stderr, "usage: native_capture_runtime <resources.bin> <assets.bin> <audio.bin> --dump <dir> [--scene <name>]\n");
        return 2;
    }
    g_dump = arg_after(argc, argv, "--dump");
    const char *only = arg_after(argc, argv, "--scene");
    if (!load_pack(argv[1])) { std::fprintf(stderr, "the resource pack does not load\n"); return 1; }
    if (!load_real_art(argv[2])) { std::fprintf(stderr, "the asset pack does not load\n"); return 1; }
    static std::vector<DungeonArena> arenas;
    for (size_t i = 0; i < pack->combat_map_count; ++i) arenas.push_back({pack->combat_map_views[i], pack->combat_sprites + i * 16});
    g_arenas = arenas.data();
    g_arena_count = arenas.size();
    int failed = 0;
    auto missing = [&](const char *scene) { std::fprintf(stderr, "scene %s could not be reached\n", scene); ++failed; };

    if (want(only, "title")) {
        Run h({{"Avatar", 'G', 100}});
        h.return_to_title();
        dump("title");
    }
    if (want(only, "main-menu")) {
        Run h({{"Avatar", 'G', 100}});
        h.return_to_title();
        h.key(' ');
        dump("main-menu");
    }
    if (want(only, "overworld-castle")) {   // Lord British's castle from the shore, midday
        Run h(kParty);
        h.rt->game().time.hour = 12;
        grass(h, pack->location_x[16] + 2, pack->location_y[16] + 2);
        h.run(300);
        dump("overworld-castle");
    }
    if (want(only, "overworld-coast")) {    // a coastal castle, midday
        Run h(kParty);
        h.rt->game().time.hour = 12;
        grass(h, pack->location_x[20] + 1, pack->location_y[20] + 1);
        h.run(200);
        dump("overworld-coast");
    }
    for (const auto &t : {std::pair<const char *, std::array<int, 3>>{"town-britain", {2, 5, 5}}, {"town-yew-cemetery", {4, 15, 5}}}) {
        if (!want(only, t.first)) continue;
        Run h(kParty);
        grass(h, 80, 80);
        if (!enter_place(h, t.second[0])) { missing(t.first); continue; }
        auto &g = h.rt->game();
        g.position.xy = {uint8_t(t.second[1]), uint8_t(t.second[2])};
        settle(h, 300);
        dump(t.first);
    }
    for (const auto &d : {std::pair<const char *, int>{"dungeon-deceit", 33}, {"dungeon-hythloth", 39}}) {
        if (!want(only, d.first)) continue;
        Run h(kParty);
        h.rt->game().torches = 9;
        dev_teleport(h, DebugDestinationKind::Dungeon, d.second);
        h.key('i');
        h.up();
        h.run(100);
        dump(d.first);
    }
    // Surface fights: the HUD names Britannia. A fight entered from a dungeon room keeps the stale surface return
    // context (the new-game hut), which is native behaviour, so none of those are captured.
    for (const auto &c : {std::pair<const char *, int>{"combat-skeletons", 33}, {"combat-orcs", 32}}) {
        if (!want(only, c.first)) continue;
        int gx = -1, gy = -1;
        Run h(kParty);
        if (!inland_grass(gx, gy) || !surface_fight(h, c.second, 4, gx, gy)) { missing(c.first); continue; }
        settle(h, 300);
        dump(c.first);
    }
    // (place, which NPC, what to say / how to leave the prompt)
    const struct { const char *name; int place, nth; const char *say; int backspaces; } talks[] = {
        {"dialogue-castle", 17, 3, "", 3}, {"dialogue-tavern", 2, 2, "job", 0}, {"shop-alchemist", 7, 0, "job", 0},
    };
    for (const auto &t : talks) {
        if (!want(only, t.name)) continue;
        Run h(kParty);
        grass(h, 80, 80);
        if (!enter_place(h, t.place) || !talk_to_nth(h, t.nth)) { missing(t.name); continue; }
        if (*t.say) { type(h, t.say); h.key('\r'); h.run(1500); }
        for (int i = 0; i < t.backspaces; ++i) h.key('\b');
        h.run(t.backspaces ? 300 : 0);
        settle(h, 200);
        dump(t.name);
    }
    if (want(only, "party-stats")) {
        Run h(kParty);
        dev_teleport(h, DebugDestinationKind::SmallMap, 2);
        settle(h, 300);
        h.key('z');
        h.run(300);
        dump("party-stats");
    }
    return failed ? 1 : 0;
}
