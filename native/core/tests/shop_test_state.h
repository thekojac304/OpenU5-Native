#pragma once
#include "openu5/shops.h"
#include <fstream>
#include <iostream>
#include <string>
using namespace openu5;
struct Hash {
    uint32_t h = 2166136261;
    void n(int64_t v) {
        for (int i = 0; i < 4; ++i)
            h = (h ^ uint8_t(uint64_t(v) >> (i * 8))) * 16777619u;
    }
    void s(const char *s) {
        std::string text = s ? s : "";
        n(text.size());
        for (unsigned char ch : text)
            h = (h ^ ch) * 16777619u;
    }
};
void init(GameState &g, TurnState &t, int v) {
    g.party.character_count = 16;
    g.party.party_size = 1 + v % 6;
    g.party.active_character = uint8_t(v % 8);
    const int intel[] = {0, 15, 33, 34, 50}, gold[] = {0, 1, 10, 100, 9998, 9999},
              food[] = {0, 2, 99, 9980, 9999}, qty[] = {0, 1, 98, 99, 100};
    for (int i = 0; i < 16; ++i) {
        auto &c = g.party.characters[i];
        snprintf(c.name, sizeof(c.name), "P%d", i);
        c.gender = uint8_t(i % 2 ? 12 : 11);
        c.character_class = "ABMF"[i % 4];
        c.status = "GPSD"[(i + v) % 4];
        c.strength = c.dexterity = 20;
        c.intelligence = uint8_t(intel[v % 5]);
        c.current_mp = uint8_t(i);
        c.current_hp = uint16_t(10 + i);
        c.max_hp = uint16_t(30 + i);
        c.exp = uint16_t(i);
        c.level = 2;
        c.months_at_inn = uint8_t((i + v) % 5);
        c.helmet = c.shield = c.amulet = 255;
        c.armor = 1;
        c.weapon = 17;
        c.ring = uint8_t(i % 3 ? 255 : 44);
        c.party_status = uint8_t(i < g.party.party_size ? 0 : 2 + i % 6);
    }
    g.position = {{10, 10}, {2, 0}};
    g.time = {139, 1, 1, v % 24, v % 60};
    g.turns_since_start = 42;
    g.gold = uint16_t(gold[v % 6]);
    g.food = uint16_t(food[v % 5]);
    for (int i = 0; i < 48; ++i)
        g.equipment_quantities[i] = qty[(i + v) % 5];
    for (int i = 0; i < 8; ++i)
        g.reagent_quantities[i] = qty[(i + v) % 5];
    g.keys = g.gems = g.torches = v % 101;
    g.karma = 50;
    g.torch_turns = 120;
    t.prev_hour = v % 24;
    t.light_spell_minutes = 50;
    t.time_spell = v % 3 == 1 ? 'Q' : v % 3 == 2 ? 'T' : 0;
    t.spell_turns = 255;
    g.rng.seed(uint16_t(v * 97 + 13));
}
uint32_t digest(const GameState &g, const TurnState &t, const ShopResult &r,
                const TavernResult &bar, int calls) {
    Hash h;
    for (auto n :
         {int64_t(g.gold), int64_t(g.food), int64_t(g.keys), int64_t(g.gems), int64_t(g.torches),
          int64_t(g.party.party_size), int64_t(g.party.active_character),
          int64_t(g.equipment_count), int64_t(g.time.year), int64_t(g.time.month),
          int64_t(g.time.day), int64_t(g.time.hour), int64_t(g.time.minute), int64_t(g.torch_turns),
          int64_t(t.prev_hour), int64_t(t.light_spell_minutes), g.turns_since_start,
          int64_t(g.rng.get_seed()), int64_t(calls)})
        h.n(n);
    for (int i = 0; i < g.equipment_count; ++i)
        h.n(g.equipment_quantities[i]);
    for (auto n : g.reagent_quantities)
        h.n(n);
    for (const auto &c : g.party.characters) {
        h.s(c.name);
        for (int n : std::initializer_list<int>{
                 c.gender, c.character_class, c.status, c.strength, c.dexterity, c.intelligence,
                 c.current_mp, c.current_hp, c.max_hp, c.exp, c.level, c.months_at_inn, c.helmet,
                 c.armor, c.weapon, c.shield, c.ring, c.amulet, c.party_status})
            h.n(n);
    }
    h.n(r.ok);
    h.s(r.message);
    h.n(int(r.reason));
    for (int n : {r.bought, int(r.full), int(r.died), r.x, r.y})
        h.n(n);
    h.s(r.subject);
    h.s(r.place);
    for (int n : {bar.cost, int(bar.sir), bar.plate_dy, bar.plate_tile, int(bar.counts_as_service)})
        h.n(n);
    h.s(bar.message == nullptr ? "" : tavern_alive_word(bar.living));
    return h.h;
}
