#include "openu5/dungeon.h"
#include "openu5/loot.h"
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
uint32_t snapshot(DungeonState &d, GameState &g, const std::vector<DungeonEvent> &events) {
    Hash h;
    auto &p = d.pos;
    auto &w = d.wanderer;
    for (int n :
         {int(p.dungeon), int(p.floor), int(p.x), int(p.y), int(p.facing), int(w.bank), int(w.type),
          int(w.x), int(w.y), int(w.floor), int(w.attr), int(w.hidden), int(w.prev_x),
          int(w.prev_y), int(g.rng.get_seed()), int(g.party.active_character)})
        h.n(n);
    for (int i = 0; i < 6; i++) {
        h.n(g.party.characters[i].current_hp);
        h.n(g.party.characters[i].status);
    }
    for (int n : g.dungeon_rooms_cleared)
        h.n(n);
    h.n(d.quickness_toggle);
    for (int n = 0; n < 512; ++n)
        h.n((d.revealed[n >> 3] >> (n & 7)) & 1);
    for (int n : {int(g.gold), int(g.food), g.keys, g.gems, g.torches})
        h.n(n);
    for (int n : g.equipment_quantities)
        h.n(n);
    for (int n : g.potion_quantities)
        h.n(n);
    for (int n : g.scroll_quantities)
        h.n(n);
    for (int n : d.cells)
        h.n(n);
    for (auto &e : events) {
        if (e.kind == DungeonEventKind::Loot) {
            char text[128];
            loot_item_name({e.value, e.member}, text, sizeof(text));
            h.n(0);
            h.s(text);
            h.n(-1);
            h.n(-1);
        } else {
            h.n(int(e.kind));
            h.s(e.text);
            h.n(e.value);
            h.n(e.member);
        }
    }
    return h.h;
}
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    std::ifstream mf(argv[2]), in(argv[1]);
    std::vector<DungeonData> maps;
    int loc;
    while (mf >> loc) {
        DungeonData m;
        m.location = uint8_t(loc);
        for (auto &c : m.cells) {
            int n;
            mf >> n;
            c = uint8_t(n);
        }
        maps.push_back(m);
    }
    if (maps.size() != 8)
        return 2;
    int mi, cell, v, seed, cases = 0;
    while (in >> mi >> cell >> v >> seed) {
        GameState g;
        TurnState t;
        DungeonState d;
        g.rng.seed(seed);
        g.torch_turns = v < 2 ? 30 : 0;
        g.gold = g.food = 20;
        g.keys = g.gems = g.torches = 20;
        g.party.character_count = 6;
        g.party.party_size = 6;
        g.party.active_character = 4;
        g.grapple = v % 2 == 1;
        t.time_spell = v == 1 ? 'Q' : v == 2 ? 'T' : 0;
        for (auto &b : g.dungeon_rooms_cleared)
            b = uint8_t(v == 3 ? 0x55 : 0);
        for (int i = 0; i < 6; i++) {
            auto &c = g.party.characters[i];
            c.status = "GSPDGS"[i];
            c.current_hp = uint16_t(20 + i);
            c.max_hp = 50;
            c.dexterity = uint8_t(5 + i * 5);
            c.party_status = 0;
        }
        d.pos = {maps[mi].location, uint8_t(cell >> 6), uint8_t(cell & 7), uint8_t((cell >> 3) & 7),
                 DungeonFacing(v)};
        for (int i = 0; i < 512; i++) {
            int c = maps[mi].cells[i];
            d.cells[i] = uint8_t((c >> 4) == 15 && dungeon_room_cleared(g, d.pos.dungeon, c & 15)
                                     ? 0xa0 | (c & 15)
                                     : c);
        }
        dungeon_respawn(g, d);
        std::vector<DungeonEvent> events;
        DungeonSink sink{&events, [](void *p, const DungeonEvent &e) {
                             static_cast<std::vector<DungeonEvent> *>(p)->push_back(e);
                         }};
        for (int a = -1; a < 17; a++) {
            events.clear();
            if (a == 11) {
                d.pos.floor = uint8_t(cell >> 6);
                d.pos.x = uint8_t(cell & 7);
                d.pos.y = uint8_t((cell >> 3) & 7);
            }
            if (a >= 0 && a < 16)
                dungeon_action(g, t, d, DungeonAction(a), sink,
                               a == 5 ? (v == 0   ? -1
                                         : v == 1 ? 1
                                         : v == 2 ? 2
                                                  : 0)
                                      : 0);
            if (a == 16)
                dungeon_mark_room(g, d, d.pos.floor, d.pos.x, d.pos.y);
            uint32_t expected;
            if (!(in >> expected))
                return 2;
            uint32_t got = snapshot(d, g, events);
            if (got != expected) {
                std::cerr << "Mismatch map " << mi << " cell " << cell << " variant " << v
                          << " action " << a << " expected " << expected << " got " << got << "\n";
                return 1;
            }
            ++cases;
        }
    }
    std::cout << cases << " dungeon rule snapshots passed; DungeonState=" << sizeof(DungeonState)
              << " GameState=" << sizeof(GameState) << "\n";
    return cases ? 0 : 2;
}
