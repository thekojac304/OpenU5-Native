// Alpha 4 A4-PARITY2 D-87 (targets/tdeck/ALPHA4_UI.md section 16) -- picking crops (and the table plates)
// caps food at 9999.
//
// Derivation (native/core/a4-parity2-findings/D87-FINAL.md; re-disassembled): every food-adding branch of
// SJOG.OVL's Get (0x18CE) ends in kernel counter_add(&food, 1, 9999) (ULTIMA.EXE 0x3F14, `call 0x7f94`): the
// crop 0x2D and the plate 0x9A at 0x1A50, the plates 0x9B and 0x9C at 0x1ABE. counter_add has NO failure path: it
// computes s = int16(old + 1) and stores 9999 when s >= 9999 (signed), else old + 1 -- so 9998 -> 9999,
// 9999 -> 9999 and 10000..32766 -> 9999 (it clamps DOWN; it does not leave a larger value alone and does not
// refuse). Only old >= 32767 leaves the plain-min family (unreachable in play). At the cap NOTHING else changes:
// the tile rewrite (0x2D -> 0x2C), the redraw mark, "Crops picked!" and the karma decrement all run before or
// independently of counter_add, and no instruction after the call reads its result. There is NO separate arena
// routine in COMBAT.OVL: the arena G key runs the same SJOG routine through thunk 0x7E06; the ports' arena twin
// (combat.cpp) is a duplicate and already capped.
//
// Native's overworld/town/dungeon-room Get did `++c.game.food` (quest_search.cpp), which also serves all three
// plates: 9999 -> 10000, 65535 -> 0.
//
//   C1 a crop at food 0 / 9997 / 9998 / 9999 / 10000 / 12345 / 32766: 1 / 9998 / 9999 / 9999 / 9999 / 9999 / 9999
//   C2 the three plates, every valid direction, at the cap: the same food, the same tile / message / karma / turn
//   C3 the refusals (a plate from the wrong side) change nothing at 9999 and at 10000
//   C4 at the cap the crop is still consumed, still costs 1 karma (karma 0 stays 0), still passes the turn
//   C5 the arena twin: crop and plates at 9999 / 10000 (it caps; this pins it, there was no native test)
#include "openu5/combat.h"
#include "openu5/commands.h"
#include "openu5/quest_world.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *id, const std::string &what) {
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s %s\n", ok ? "GREEN" : "RED", id, what.c_str());
}

struct Pool {
    int writes = 0, last_x = -1, last_y = -1, last_tile = -1;
};
size_t pool_count(void *) { return 0; }
QuestObject pool_read(void *, size_t) { return QuestObject{}; }
bool pool_reserve(void *, size_t) { return true; }
void pool_append(void *, const QuestObject &) {}
void pool_erase(void *, size_t) {}
void volatile_tile(void *p, int32_t x, int32_t y, int32_t tile) {
    auto &o = *static_cast<Pool *>(p);
    ++o.writes;
    o.last_x = x;
    o.last_y = y;
    o.last_tile = tile;
}

struct Got {
    int food = 0, karma = 0;
    std::vector<std::string> messages;
    int writes = 0, tile = -1;
    CommandStatus status = CommandStatus::Success;
    bool turn = false;
};
Got get(int food, int karma, int tile, Direction dir) {
    GameState g;
    TurnState t;
    TravelState travel;
    CommandState commands;
    std::vector<uint8_t> terrain(65536, 5);
    const auto v = direction_delta(dir);
    terrain[size_t((20 + v.dy) * 256 + 20 + v.dx)] = uint8_t(tile);
    WorldData world;
    world.overworld = terrain.data();
    world.overworld_size = terrain.size();
    CommandContext c{g, t, travel, commands, world};
    g.position.xy = {20, 20};
    g.party.party_size = g.party.character_count = 1;
    g.party.characters[0].status = 'G';
    g.food = uint16_t(food);
    g.karma = uint8_t(karma);
    Pool pool;
    QuestWorldServices q;
    q.context = &pool;
    q.count = pool_count;
    q.read = pool_read;
    q.reserve = pool_reserve;
    q.append = pool_append;
    q.erase = pool_erase;
    q.volatile_tile = volatile_tile;
    c.quest_world = &q;
    Got r;
    const auto res = get_quest_object(c, dir, EventSink{&r, [](void *p, const GameEvent &e) {
        if (e.kind == GameEventKind::Message) static_cast<Got *>(p)->messages.push_back(e.text ? e.text : "");
    }});
    r.food = int(g.food);
    r.karma = int(g.karma);
    r.writes = pool.writes;
    r.tile = pool.last_tile;
    r.status = res.status;
    r.turn = res.turn;
    return r;
}
bool said(const Got &r, const char *m) { return r.messages.size() == 1 && r.messages[0] == m; }

// The arena twin: an actor at (5,4); the crop / plate one cell away in the given direction.
struct Arena {
    int food, karma, tile_after, status_ok;
    std::vector<std::string> messages;
};
// `dir` is the arena Get's own direction index (combat.cpp: 0 east, 1 west, 2 south, 3 north).
Arena arena_get(int food, int karma, int tile, int dir) {
    static constexpr int xs[] = {1, -1, 0, 0}, ys[] = {0, 0, 1, -1};
    const int dx = xs[dir], dy = ys[dir];
    GameState game{};
    game.party.party_size = game.party.character_count = 1;
    auto &m = game.party.characters[0];
    m.party_status = 0;
    m.status = 'G';
    m.current_hp = m.max_hp = 100;
    m.dexterity = 20;
    game.food = uint16_t(food);
    game.karma = uint8_t(karma);
    TurnState turn{};
    CombatState battle{};
    battle.initialized = true;
    battle.victory = true;
    battle.count = 1;
    battle.current = 0;
    battle.actors[0].id = 1;
    battle.actors[0].member = 0;
    battle.actors[0].status = CombatStatus::Active;
    battle.actors[0].position = {5, 4};
    const int key = (4 + dy) * kCombatGrid + 5 + dx;
    battle.map.tiles[key] = int16_t(tile);
    CombatContext ctx{game, turn, battle};
    Arena a{};
    ctx.events = {&a, [](void *p, const CombatEvent &e) {
        if (e.text) static_cast<Arena *>(p)->messages.push_back(e.text);
    }};
    const auto res = combat_action(ctx, CombatAction::Get, dir);
    a.status_ok = res == CombatResult::Ok;
    a.food = int(game.food);
    a.karma = int(game.karma);
    a.tile_after = battle.map.tiles[key];
    return a;
}

void test_all() {
    // C1
    {
        const int in[] = {0, 9997, 9998, 9999, 10000, 12345, 32766};
        const int want[] = {1, 9998, 9999, 9999, 9999, 9999, 9999};
        std::string bad;
        for (size_t i = 0; i < 7; ++i) {
            const auto r = get(in[i], 50, 45, Direction::East);
            if (r.food != want[i] || !said(r, "Crops picked!") || r.tile != 44 || r.karma != 49 || r.status != CommandStatus::Success || !r.turn)
                bad += " " + std::to_string(in[i]) + "->" + std::to_string(r.food);
        }
        check(bad.empty(), "C1", "a crop at food 0 / 9997 / 9998 / 9999 / 10000 / 12345 / 32766 gives 1 / 9998 / 9999 / 9999 / 9999 / 9999 / 9999, tile 44, "
                                 "\"Crops picked!\", karma 49, Success, a turn" + (bad.empty() ? "" : " -- wrong:" + bad));
    }
    // C2
    {
        struct Case { int tile; Direction dir; int next; };
        const Case cases[] = {{154, Direction::South, 149}, {155, Direction::North, 149}, {156, Direction::South, 155}, {156, Direction::North, 154}};
        std::string bad;
        for (const auto &c : cases)
            for (int food : {9998, 9999, 10000}) {
                const auto r = get(food, 50, c.tile, c.dir);
                if (r.food != 9999 || !said(r, "Mmmmm...!") || r.tile != c.next || r.karma != 49 || !r.turn)
                    bad += " tile " + std::to_string(c.tile) + " food " + std::to_string(food) + "->" + std::to_string(r.food);
            }
        check(bad.empty(), "C2", "the plates 0x9A / 0x9B / 0x9C (every valid direction) at food 9998 / 9999 / 10000 all end at 9999 with the same tile, "
                                 "\"Mmmmm...!\", karma 49 and a turn" + (bad.empty() ? "" : " -- wrong:" + bad));
    }
    // C3
    {
        struct Case { int tile; Direction dir; };
        const Case cases[] = {{154, Direction::North}, {154, Direction::East}, {155, Direction::South}, {155, Direction::West}, {156, Direction::East}, {156, Direction::West}};
        std::string bad;
        for (const auto &c : cases)
            for (int food : {9999, 10000}) {
                const auto r = get(food, 50, c.tile, c.dir);
                if (r.food != food || !said(r, "Can't reach plate!") || r.writes != 0 || r.karma != 50 || r.turn)
                    bad += " tile " + std::to_string(c.tile) + " food " + std::to_string(food) + "->" + std::to_string(r.food);
            }
        check(bad.empty(), "C3", "a plate from the wrong side: \"Can't reach plate!\", food, tile, karma and the turn untouched at 9999 and at 10000" +
                                     (bad.empty() ? "" : " -- wrong:" + bad));
    }
    // C4
    {
        const auto r = get(9999, 0, 45, Direction::East);
        check(r.food == 9999 && r.tile == 44 && r.karma == 0 && r.turn && said(r, "Crops picked!"), "C4",
              "at the cap the crop is still consumed (tile 44), still says \"Crops picked!\", karma 0 stays 0, the turn passes");
    }
    // C5
    {
        std::string bad;
        for (int food : {9998, 9999, 10000}) {
            auto c = arena_get(food, 50, 0x2d, 3);
            if (c.food != 9999 || c.tile_after != 0x2c || c.karma != 49) bad += " crop " + std::to_string(food) + "->" + std::to_string(c.food);
            auto p = arena_get(food, 50, 0x9a, 2);
            if (p.food != 9999 || p.tile_after != 0x95 || p.karma != 49) bad += " plate " + std::to_string(food) + "->" + std::to_string(p.food);
        }
        check(bad.empty(), "C5", "the arena twin: a crop and a plate at 9998 / 9999 / 10000 end at 9999, the tile is consumed, karma 49" +
                                     (bad.empty() ? "" : " -- wrong:" + bad));
    }
}
} // namespace

int main() {
    test_all();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
