#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace openu5;
struct Hash {
    uint32_t h = 2166136261u;
    void n(int64_t n) {
        for (int i = 0; i < 4; ++i)
            h = (h ^ ((uint32_t(n) >> (i * 8)) & 255)) * 16777619u;
    }
    void s(const std::string &s) {
        n(s.size());
        for (unsigned char c : s)
            h = (h ^ c) * 16777619u;
    }
};
struct Ev {
    std::string kind, text;
    int id = -1;
};
struct Harness {
    GameState g;
    TurnState t;
    TravelState travel;
    CommandState commands;
    WorldData world;
    DungeonState d;
    DungeonScratch scratch;
    DungeonContext dc{d, scratch};
    CommandContext c{g, t, travel, commands, world};
    std::vector<Ev> events;
    std::vector<int> fx;
    void effect(int n) {
        fx.push_back(n);
        fx.push_back(g.rng.get_seed());
    }
    uint32_t snap() {
        Hash h;
        for (int n : {int(g.position.map.location), int(g.position.map.floor), int(g.position.xy.x),
                      int(g.position.xy.y), int(g.rng.get_seed()), int(g.time.year),
                      int(g.time.month), int(g.time.day), int(g.time.hour), int(g.time.minute),
                      int(g.turns_since_start), int(g.food), int(g.torch_turns), t.prev_hour,
                      int(t.time_spell), t.spell_turns, int(g.party.characters[0].current_hp),
                      int(g.party.characters[0].status), int(d.active)})
            h.n(n);
        if (d.active) {
            auto &p = d.pos;
            auto &w = d.wanderer;
            for (int n : {int(p.dungeon), int(p.floor), int(p.x), int(p.y), int(p.facing),
                          int(w.bank), int(w.type), int(w.x), int(w.y), int(w.floor), int(w.attr),
                          int(w.hidden), int(w.prev_x), int(w.prev_y), int(d.quickness_toggle)})
                h.n(n);
            for (int n : d.cells)
                h.n(n);
        }
        for (auto &e : events) {
            h.s(e.kind);
            h.s(e.text);
            h.n(e.id);
        }
        for (int n : fx)
            h.n(n);
        events.clear();
        fx.clear();
        return h.h;
    }
};
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::string root = argv[1];
    std::ifstream mf(root + "/dungeon-maps.txt"), lf(root + "/dungeon-locations.txt"),
        in(root + "/dungeon-flow.txt");
    DungeonData maps[8];
    for (auto &m : maps) {
        int n;
        mf >> n;
        m.location = uint8_t(n);
        for (auto &t : m.cells) {
            mf >> n;
            t = uint8_t(n);
        }
    }
    std::vector<uint8_t> loc;
    int n;
    while (lf >> n)
        loc.push_back(uint8_t(n));
    size_t count = loc.size() / 2;
    int mi, v, a, cases = 0;
    while (in >> mi >> v >> a) {
        Harness h;
        auto &g = h.g;
        g.rng.seed(mi * 3011 + v * 97 + a);
        g.position = {{10, 20}, {0, int16_t(v & 1 ? 255 : 0)}};
        g.party.party_size = g.party.character_count = 1;
        g.party.active_character = 255;
        auto &r = g.party.characters[0];
        r.status = 'G';
        r.current_hp = r.max_hp = 100;
        r.dexterity = 20;
        r.party_status = 0;
        r.ring = 255;
        g.food = g.torch_turns = 100;
        g.time = {139, 1, 1, 5, 59};
        h.t.prev_hour = 5;
        h.t.time_spell = v & 2 ? 'Q' : 0;
        h.t.spell_turns = 255;
        h.c.dungeon_context = &h.dc;
        h.dc.data = maps;
        h.dc.count = 8;
        h.c.locations = {loc.data(), loc.data() + count, count, count};
        h.c.events = {
            &h, [](void *p, const GameEvent &e) {
                auto &h = *static_cast<Harness *>(p);
                const char *kind = e.kind == GameEventKind::Message          ? "message"
                                   : e.kind == GameEventKind::Sfx            ? "sfx"
                                   : e.kind == GameEventKind::DungeonEntered ? "dungeon-entered"
                                   : e.kind == GameEventKind::DungeonExited  ? "dungeon-exited"
                                   : e.kind == GameEventKind::CombatEnded    ? "combat-ended"
                                                                             : "map-changed";
                h.events.push_back({kind, e.text ? e.text : "",
                                    e.dungeon_id});
            }};
        h.c.services.context = &h;
        h.c.services.banner = [](void *, uint8_t) { return "BANNER"; };
        h.c.services.reload = [](void *p, ReloadEffect e, uint8_t, EventSink) {
            static_cast<Harness *>(p)->effect(e == ReloadEffect::ClearEnemies ? 0 : 1);
        };
        h.c.services.effect = [](void *p, CommandEffect, EventSink) {
            static_cast<Harness *>(p)->effect(3);
            return false;
        };
        h.dc.context = &h;
        h.dc.rescue_hook = [](void *p, EventSink) { static_cast<Harness *>(p)->effect(2); };
        h.dc.start_room = [](void *p, int32_t n, EventSink) {
            static_cast<Harness *>(p)->events.push_back(
                {"message", "room:" + std::to_string(n), -1});
        };
        h.dc.start_corridor = [](void *p, bool attack, EventSink) {
            static_cast<Harness *>(p)->events.push_back(
                {"message", attack ? "corridor:attack" : "corridor:ambush", -1});
        };
        Command enter;
        enter.kind = CommandKind::EnterDungeon;
        enter.member = maps[mi].location;
        enter.hours = g.position.map.floor;
        execute_command(h.c, enter);
        uint32_t expected;
        in >> expected;
        if (h.snap() != expected) {
            std::cerr << "entry mismatch " << mi << " " << v << " " << a << "\n";
            return 1;
        }
        ++cases;
        h.d.pos.floor = uint8_t(v % 8);
        h.d.pos.x = uint8_t((v >> 3) & 7);
        h.d.pos.y = uint8_t((v * 3) & 7);
        h.d.pos.facing = DungeonFacing(v % 4);
        Command cmd;
        cmd.kind = CommandKind::DungeonCommand;
        cmd.item = int16_t(a);
        cmd.hours = int16_t(v & 1 ? -1 : 1);
        execute_command(h.c, cmd);
        in >> expected;
        if (h.snap() != expected) {
            std::cerr << "command mismatch " << mi << " " << v << " " << a << "\n";
            return 1;
        }
        ++cases;
        uint32_t got = 0;
        if (h.d.active) {
            h.d.wanderer.type = 20;
            CombatMap map;
            uint8_t sprites[16];
            int max[] = {1, 3, 8, 16};
            build_corridor_map(g, h.d, max[v % 4], map, sprites);
            Hash hash;
            hash.n(g.rng.get_seed());
            for (int n : map.tiles)
                hash.n(n);
            for (auto &side : map.starts)
                for (auto p : side) {
                    hash.n(p.x);
                    hash.n(p.y);
                }
            hash.n(map.unit_count);
            for (int i = 0; i < map.unit_count; ++i) {
                hash.n(map.units[i].x);
                hash.n(map.units[i].y);
                hash.n(sprites[i]);
            }
            got = hash.h;
        }
        in >> expected;
        if (got != expected) {
            std::cerr << "corridor mismatch " << mi << " " << v << " " << a << "\n";
            return 1;
        }
        ++cases;
        CombatState s;
        s.initialized = true;
        s.victory = v & 4;
        s.rng.seed(g.rng.get_seed());
        s.escape_floor_delta = int8_t(v % 3 == 0 ? 0 : v % 3 == 1 ? -1 : 1);
        s.escape_border = int16_t(v % 4);
        h.c.combat = true;
        h.dc.corridor_cause = int8_t(v % 3 == 0 ? -1 : v % 3 == 1 ? 0 : 1);
        finish_encounter_combat(h.c, s);
        in >> expected;
        if (h.snap() != expected) {
            std::cerr << "aftermath mismatch " << mi << " " << v << " " << a << "\n";
            return 1;
        }
        ++cases;
    }
    std::cout << cases << " dungeon orchestration/corridor/aftermath snapshots passed\n";
    return cases ? 0 : 2;
}


