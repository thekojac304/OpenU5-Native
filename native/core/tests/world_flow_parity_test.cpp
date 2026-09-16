#include "openu5/combat.h"
#include "openu5/transport.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace openu5;
struct Hash {
    uint32_t h = 2166136261u;
    void n(int64_t v) {
        for (int i = 0; i < 4; i++)
            h = (h ^ ((uint32_t(v) >> (i * 8)) & 255)) * 16777619u;
    }
    void s(const char *s) {
        std::string text = s ? s : "";
        n(text.size());
        for (unsigned char c : text)
            h = (h ^ c) * 16777619u;
    }
};
struct Events {
    std::vector<CombatEvent> values;
    std::vector<std::string> text;
    void clear() {
        values.clear();
        text.clear();
    }
};
uint32_t snap(CombatState &c, GameState &g, Events &ev) {
    Hash h;
    for (int n : {int(c.rng.get_seed()), int(c.action_count), c.scan,
                  c.current < 0 ? -1 : c.actors[c.current].id, int(c.ended), int(c.victory),
                  int(combat_over(c)), c.spoil_chests, int(c.escape_border),
                  int(c.escape_floor_delta), c.count})
        h.n(n);
    for (int i = 0; i < c.count; ++i) {
        auto &a = c.actors[i];
        for (int n : {a.id,
                      int(a.member),
                      a.enemy ? a.enemy->index : -1,
                      int(a.position.x),
                      int(a.position.y),
                      a.hp,
                      a.max_hp,
                      a.strength,
                      a.dexterity,
                      a.intelligence,
                      a.defense,
                      a.attack,
                      a.range,
                      int(a.status),
                      int(a.fleeing),
                      int(a.speed),
                      int(a.counter),
                      int(a.sleeping),
                      int(a.dragged),
                      int(a.charmed),
                      int(a.invisible)})
            h.n(n);
    }
    for (int n : c.map.tiles)
        h.n(n);
    for (int n : c.loot)
        h.n(n);
    for (int n : c.chest_contents)
        h.n(n);
    h.n(c.field_count);
    for (int i = 0; i < c.field_count; ++i) {
        auto &f = c.fields[i];
        h.n(f.position.x);
        h.n(f.position.y);
        h.n(f.tile);
    }
    h.n(c.pile_count);
    for (int i = 0; i < c.pile_count; ++i) {
        auto &p = c.piles[i];
        h.n(p.position.x);
        h.n(p.position.y);
        h.n(p.id);
        h.n(p.quantity);
    }
    for (int n :
         {int(g.gold), int(g.food), g.keys, g.gems, g.torches, int(g.torch_turns), c.spoil_gold})
        h.n(n);
    for (int n : g.equipment_quantities)
        h.n(n);
    for (int n : g.potion_quantities)
        h.n(n);
    for (int n : g.scroll_quantities)
        h.n(n);
    const char *kinds[] = {"message", "echo",  "moved",      "attacked", "died",
                           "turn",    "ended", "projectile", "quake",    "line-spray"};
    for (size_t i = 0; i < ev.values.size(); ++i) {
        auto &e = ev.values[i];
        h.s(kinds[int(e.kind)]);
        h.s(ev.text[i].c_str());
        for (int n : {e.actor, e.target, e.x, e.y, e.damage})
            h.n(n);
    }
    return h.h;
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::string root = argv[1];
    std::ifstream mf(root + "/fixed-maps.txt"), df(root + "/fixed-enemies.txt"),
        in(root + "/fixed-combat.txt"), tf(root + "/transport.txt");
    struct Map {
        CombatMap m;
        uint8_t sprites[16]{};
    };
    std::vector<Map> maps;
    int index;
    while (mf >> index) {
        Map x;
        x.m.index = index;
        for (auto &t : x.m.tiles)
            mf >> t;
        for (int d = 0; d < 4; d++) {
            int n;
            mf >> n;
            x.m.start_count[d] = uint8_t(n);
            for (int i = 0; i < n; i++)
                mf >> x.m.starts[d][i].x >> x.m.starts[d][i].y;
        }
        int n;
        mf >> n;
        x.m.unit_count = uint8_t(n);
        for (int i = 0; i < n; i++) {
            int s;
            mf >> x.m.units[i].x >> x.m.units[i].y >> s;
            x.sprites[i] = uint8_t(s);
        }
        mf >> n;
        x.m.trigger_count = uint8_t(n);
        for (int i = 0; i < n; i++) {
            auto &t = x.m.triggers[i];
            mf >> t.tile >> t.at.x >> t.at.y >> t.first.x >> t.first.y >> t.second.x >> t.second.y;
        }
        maps.push_back(x);
    }
    CombatEnemy defs[48];
    const CombatEnemy *ptrs[48];
    for (int i = 0; i < 48; ++i) {
        auto &d = defs[i];
        int move, stationary;
        df >> d.index >> d.strength >> d.dexterity >> d.intelligence >> d.armor >> d.damage >>
            d.hp >> d.range >> d.treasure >> d.max_per_map >> d.abilities >> move >> stationary;
        d.move_class = uint8_t(move);
        d.stationary = stationary;
        ptrs[i] = &d;
    }
    if (maps.size() != 128 || !df)
        return 2;
    int mi, f, v, seed, entry, cases = 0;
    while (in >> mi >> f >> v >> seed >> entry) {
        GameState g;
        TurnState t;
        CombatState s;
        CombatLootPile loot_overflow[512];
        s.piles.overflow = loot_overflow;
        s.piles.overflow_capacity = 512;
        CombatActor overflow[512];
        s.actors.overflow = overflow;
        s.actors.overflow_capacity = 512;
        CombatField fields[16];
        g.rng.seed(seed);
        g.party.party_size = 2;
        g.party.character_count = 2;
        g.party.active_character = 255;
        g.food = 100;
        g.karma = 50;
        for (int i = 0; i < 2; ++i) {
            auto &r = g.party.characters[i];
            r.status = 'G';
            r.current_hp = r.max_hp = 100;
            r.strength = 20;
            r.dexterity = uint8_t(30 - i * 5);
            r.intelligence = 20;
            r.level = 8;
            r.helmet = r.armor = r.weapon = r.shield = r.ring = r.amulet = 255;
            r.party_status = 0;
        }
        Events ev;
        CombatContext c{g, t, s};
        c.enemy_defs = ptrs;
        c.enemy_def_count = 48;
        c.events = {&ev, [](void *p, const CombatEvent &e) {
                        auto &ev = *static_cast<Events *>(p);
                        ev.values.push_back(e);
                        ev.text.emplace_back(e.text ? e.text : "");
                    }};
        auto &map = maps[mi];
        FixedCombatSetup fixed{map.sprites, v, fields, 16};
        auto dir = dungeon_room_entry(map.m, uint8_t(f));
        if (int(dir) != entry)
            return 3;
        if (initialize_combat(c, map.m, dir, nullptr, 0, true, &fixed) != CombatResult::Ok)
            return 4;
        for (int a = 0; a < 13; ++a) {
            ev.clear();
            if (a == 1)
                combat_action(c, CombatAction::Klimb);
            if (a == 2)
                combat_action(c, CombatAction::Escape, f);
            if (a == 3)
                combat_action(c, CombatAction::Pass);
            if (a >= 4) {
                int x = 5, y = 5;
                for (int i = 0; i < map.m.unit_count; ++i)
                    if (map.sprites[i] > 0 && map.sprites[i] < 16) {
                        x = map.m.units[i].x;
                        y = map.m.units[i].y;
                        break;
                    }
                s.actors[0].position = {int16_t(x), int16_t(y)};
                s.actors[0].status = CombatStatus::Active;
                s.current = 0;
                s.ended = false;
                combat_action(c, a == 4 ? CombatAction::Open : CombatAction::Get, -1);
            }
            uint32_t expected;
            in >> expected;
            auto got = snap(s, g, ev);
            if (got != expected) {
                std::cerr << "fixed mismatch map=" << mi << " facing=" << f << " floor=" << v
                          << " action=" << a << " expected=" << expected << " got=" << got << "\n";
                return 1;
            }
            ++cases;
        }
    }
    int tile, from, tc = 0;
    uint32_t expected;
    while (tf >> tile >> from >> v >> expected) {
        GameState g;
        g.position.map.location = uint8_t(v == 7 ? 33 : 0);
        g.ship_hull = v == 0 ? 1 : 99;
        g.ship_skiffs = v % 3;
        g.magic_carpets = v % 2;
        auto b = board_transport(g, tile, from, v == 6);
        auto e = disembark_transport(g, b.tile < 0 ? from : b.tile, v & 1, v & 2, v & 4);
        Hash h;
        for (auto r : {b, e}) {
            h.n(r.ok);
            h.s(r.message);
            h.n(r.tile);
            h.n(r.drop_tile);
            h.n(r.parked_ship_tile);
            h.n(r.damaged_warning);
            h.n(r.skiff_warning);
        }
        h.n(g.ship_hull);
        h.n(g.ship_skiffs);
        h.n(g.magic_carpets);
        if (h.h != expected) {
            std::cerr << "transport mismatch " << tile << " " << from << " " << v << "\n";
            return 1;
        }
        ++tc;
    }
    std::cout << cases << " fixed combat snapshots; " << tc << " transport sequences passed\n";
    return cases && tc ? 0 : 2;
}

