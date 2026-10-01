// Alpha 4 A4-ENH1 (targets/tdeck/ALPHA4_UI.md section 10) -- the Original-mode
// preservation golden.
//
// This file deliberately uses ONLY interfaces that existed before A4-ENH1
// (turn housekeeping, the spawn gate, the combat engine, the save codec). Its
// four golden hashes were recorded by building and running this very file
// against the unmodified pre-A4-ENH1 tree (commit aae348ac, log
// native/core/a4-enh1-golden-head.log). After the difficulty / cheat framework
// landed, a default GameState -- Difficulty Original, no cheat ever used --
// must reproduce every hash bit for bit: poison cadence, food and starvation,
// regeneration-ring draws, the spawn gate, every combat hit, kill and XP
// award, and the saved document's exact bytes (no new key in it).
//
// The new modifiers themselves are proven in a4_enh1_rules (the other half).
#include "openu5/combat.h"
#include "openu5/persistence.h"
#include "openu5/save_json.h"
#include "openu5/turn.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace openu5;

namespace {
int failures = 0;
void check(bool ok, const char *what) {
    std::printf("%s %s\n", ok ? "GREEN" : "RED", what);
    if (!ok) ++failures;
}

struct Fnv {
    uint64_t h = 1469598103934665603ULL;
    void byte(uint8_t b) { h = (h ^ b) * 1099511628211ULL; }
    void add(int64_t v) {
        for (int i = 0; i < 8; ++i) byte(uint8_t(uint64_t(v) >> (8 * i)));
    }
};

void party(GameState &g, int members) {
    g.party.character_count = uint8_t(members);
    g.party.party_size = members;
    for (int i = 0; i < members; ++i) {
        auto &c = g.party.characters[i];
        std::snprintf(c.name, sizeof(c.name), "Hero%d", i);
        c.character_class = i == 0 ? 'A' : 'F';
        c.status = 'G';
        c.party_status = 0;
        c.strength = uint8_t(18 + i);
        c.dexterity = uint8_t(14 + 3 * i);
        c.intelligence = 12;
        c.current_hp = c.max_hp = 150;
        c.level = 3;
        c.helmet = c.armor = c.shield = c.ring = c.amulet = 255;
        c.weapon = 1;
    }
}

// A1. 3,000 world steps of kernel_turn_housekeeping (0x2AE8) through the real
// advance_turn(): poison ticks on two members, food at 06/12/18, starvation
// once the food is gone, and the Ring of Regeneration's rand(0,7) on one.
uint64_t housekeeping_golden(int &poison_hp, int &meals) {
    GameState g{};
    party(g, 4);
    g.party.characters[1].status = 'P';
    g.party.characters[2].status = 'P';
    g.party.characters[3].ring = 44;
    g.food = 40;
    g.time = {};
    g.time.year = 139; g.time.month = 1; g.time.day = 1; g.time.hour = 5; g.time.minute = 0;
    TurnState s{};
    s.prev_hour = g.time.hour;
    OriginalRng rng(0x1234);
    const Rand rand = rng_source(rng);
    Fnv f;
    poison_hp = meals = 0;
    for (int i = 0; i < 3000; ++i) {
        const int hp_before = g.party.characters[1].current_hp + g.party.characters[2].current_hp;
        const int food_before = g.food;
        const auto r = advance_turn(g, s, 7, rand, nullptr);
        poison_hp += r.poison_ticks.count;
        if (g.food < food_before) ++meals;
        (void)hp_before;
        if (i % 400 == 399) {
            for (int m = 0; m < 4; ++m) {
                auto &c = g.party.characters[m];
                if (c.status == 'D') c.status = m == 1 || m == 2 ? 'P' : 'G';
                c.current_hp = c.max_hp;
            }
            g.food = uint16_t(g.food + 30);
        }
        f.add(i);
        for (int m = 0; m < 4; ++m) {
            f.add(g.party.characters[m].current_hp);
            f.add(g.party.characters[m].status);
        }
        f.add(g.food);
        f.add(g.time.day);
        f.add(g.time.hour);
        f.add(g.turns_since_start);
        f.add(r.message_count);
        f.add(rng.get_seed());
    }
    return f.h;
}

// A2. The overworld spawn gate (roll_spawn_gate): 4,000 rolls over the
// terrain classes, both surface and underworld, every hour.
uint64_t spawn_golden(int &spawns) {
    OriginalRng rng(0x0bad);
    const Rand rand = rng_source(rng);
    static constexpr int tiles[] = {5, 4, 10, 32, 1, 12, 35, 6};
    Fnv f;
    spawns = 0;
    for (int i = 0; i < 4000; ++i) {
        const auto s = roll_spawn_gate(rand, tiles[i % 8], (i / 8) % 2 ? 255 : 0, i % 24);
        spawns += s.spawn;
        f.add(s.roll);
        f.add(s.threshold);
        f.add(s.spawn);
    }
    return f.h;
}

// A3. A whole fight on the real combat engine: two fighters walk up to three
// enemies and hit them; the enemies hit back (combat damage(), the 0x99 rule,
// wounds), die (kill(): the (hp >> 2) + 1 XP award and its 9999 cap) and drop
// their chests. Every actor's HP, status and cell is hashed after every action.
uint64_t combat_golden(int &party_hp_lost, int &xp, int &kills) {
    GameState g{};
    party(g, 2);
    g.rng.seed(0x4242);
    TurnState t{};
    CombatState s{};
    static int32_t attack[256]{}, range[256]{}, defense[256]{};
    attack[1] = 12; range[1] = 1;
    CombatEnemy orc{};
    orc.index = 12; orc.name = "orc"; orc.group_name = "orcs";
    orc.hp = 40; orc.strength = 16; orc.dexterity = 12; orc.intelligence = 4;
    orc.armor = 2; orc.damage = 14; orc.range = 1; orc.treasure = 10; orc.max_per_map = 4;
    orc.tile = 0x70;
    const CombatEnemy *enemies[] = {&orc, &orc, &orc};
    CombatMap map{};
    for (auto &tile : map.tiles) tile = 5;
    map.start_count[int(CombatDirection::South)] = 2;
    map.starts[int(CombatDirection::South)][0] = {4, 9};
    map.starts[int(CombatDirection::South)][1] = {6, 9};
    map.unit_count = 3;
    map.units[0] = {3, 2}; map.units[1] = {5, 2}; map.units[2] = {7, 2};
    CombatContext c{g, t, s};
    c.tables.attack = attack; c.tables.range = range; c.tables.defense = defense; c.tables.count = 256;
    Fnv f;
    party_hp_lost = xp = kills = 0;
    if (initialize_combat(c, map, CombatDirection::South, enemies, 3) != CombatResult::Ok) return 0;
    const int hp_start = g.party.characters[0].current_hp + g.party.characters[1].current_hp;
    for (int step = 0; step < 4000 && !combat_over(s) && !s.victory; ++step) {
        CombatActor *a = current_combat_actor(c);
        if (!a) break;
        if (a->member == 255 || a->charmed) {
            combat_action(c, CombatAction::EnemyStep);
        } else {
            int best = -1, best_d = 1 << 30;
            for (int i = 0; i < s.count; ++i) {
                const auto &o = s.actors[i];
                if (o.member != 255 || o.status != CombatStatus::Active) continue;
                const int d = std::abs(o.position.x - a->position.x) + std::abs(o.position.y - a->position.y);
                if (d < best_d) { best_d = d; best = i; }
            }
            if (best < 0) { combat_action(c, CombatAction::Pass); }
            else {
                const auto &o = s.actors[best];
                const int dx = o.position.x - a->position.x, dy = o.position.y - a->position.y;
                if (std::abs(dx) + std::abs(dy) == 1)
                    combat_action(c, CombatAction::Attack, o.position.x, o.position.y);
                else {
                    // Orthogonal steps only (ordinals 0 E, 1 W, 2 S, 3 N), the
                    // longer axis first; a blocked step costs no turn, so the
                    // other axis is tried and then Pass.
                    const int horizontal = dx > 0 ? 0 : 1, vertical = dy > 0 ? 2 : 3;
                    const bool wide = std::abs(dx) >= std::abs(dy);
                    const int first = wide && dx ? horizontal : dy ? vertical : horizontal;
                    const int second = first == horizontal ? (dy ? vertical : -1) : (dx ? horizontal : -1);
                    const int id = a->id;
                    const auto at = a->position;
                    combat_action(c, CombatAction::Move, first);
                    const CombatActor *now = current_combat_actor(c);
                    if (now && now->id == id && now->position.x == at.x && now->position.y == at.y) {
                        if (second >= 0) combat_action(c, CombatAction::Move, second);
                        now = current_combat_actor(c);
                        if (now && now->id == id && now->position.x == at.x && now->position.y == at.y)
                            combat_action(c, CombatAction::Pass);
                    }
                }
            }
        }
        f.add(step);
        f.add(s.current);
        for (int i = 0; i < s.count; ++i) {
            const auto &o = s.actors[i];
            f.add(o.hp); f.add(int(o.status)); f.add(o.position.x); f.add(o.position.y);
        }
        for (int m = 0; m < 2; ++m) {
            f.add(g.party.characters[m].current_hp);
            f.add(g.party.characters[m].exp);
            f.add(g.party.characters[m].status);
        }
        f.add(s.rng.get_seed());
    }
    for (int i = 0; i < s.count; ++i)
        if (s.actors[i].member == 255 && s.actors[i].status == CombatStatus::Dead) ++kills;
    party_hp_lost = hp_start - (g.party.characters[0].current_hp + g.party.characters[1].current_hp);
    xp = g.party.characters[0].exp + g.party.characters[1].exp;
    f.add(kills); f.add(party_hp_lost); f.add(xp);
    return f.h;
}

// A4. The saved state document of a default journey, byte for byte, then the
// same journey loaded and saved again (both documents are hashed): Original
// with no cheat must add no key to either save.
uint64_t save_golden(bool &round_trip) {
    GameState g{};
    party(g, 3);
    g.food = 321; g.gold = 1234; g.karma = 50; g.turns_since_start = 777;
    g.time.year = 139; g.time.month = 4; g.time.day = 12; g.time.hour = 9; g.time.minute = 30;
    g.position.map = {0, 0};
    g.position.xy = {82, 108};
    TurnState t{};
    save::Json doc = save::Json::object();
    save::capture_core(g, t, doc);
    std::string text;
    const bool encoded = save::encode_json(doc, text) == save::JsonError::None;
    GameState back{};
    TurnState back_turn{};
    save::Json retained;
    round_trip = encoded && save::load_state(text, back, back_turn, retained) == save::Error::None &&
                 back.food == 321 && back.gold == 1234 && back.turns_since_start == 777 &&
                 back.party.character_count == 3;
    std::string again;
    round_trip = round_trip && save::save_state(back, back_turn, retained, again) == save::Error::None;
    Fnv f;
    for (unsigned char ch : text) f.byte(ch);
    f.add(int64_t(text.size()));
    for (unsigned char ch : again) f.byte(ch);
    f.add(int64_t(again.size()));
    return f.h;
}
} // namespace

int main() {
    // Recorded on the unmodified tree (aae348ac), see the header comment.
    constexpr uint64_t kHousekeeping = 0xb2556b13961ce5d6ULL;
    constexpr uint64_t kSpawn = 0xea46657ab3b6be1cULL;
    constexpr uint64_t kCombat = 0x28bf13fcc51cab0dULL;
    constexpr uint64_t kSave = 0xbf9b4e479f50410fULL;

    int poison_hp = 0, meals = 0, spawns = 0, hp_lost = 0, xp = 0, kills = 0;
    bool round_trip = false;
    const uint64_t housekeeping = housekeeping_golden(poison_hp, meals);
    const uint64_t spawn = spawn_golden(spawns);
    const uint64_t combat = combat_golden(hp_lost, xp, kills);
    const uint64_t saved = save_golden(round_trip);
    std::printf("golden housekeeping=0x%016llx poison_hp=%d meals=%d\n", (unsigned long long)housekeeping, poison_hp, meals);
    std::printf("golden spawn=0x%016llx spawns=%d\n", (unsigned long long)spawn, spawns);
    std::printf("golden combat=0x%016llx party_hp_lost=%d xp=%d kills=%d\n", (unsigned long long)combat, hp_lost, xp, kills);
    std::printf("golden save=0x%016llx round_trip=%d\n", (unsigned long long)saved, round_trip);

    check(poison_hp > 500 && meals > 30, "A1 the scenario exercises poison ticks and meals");
    check(kills >= 1 && hp_lost > 0 && xp > 0, "A3 the fight exercises enemy damage, kills and XP");
    check(round_trip, "A4 the default save loads back and saves again");
    check(housekeeping == kHousekeeping, "A1 Original: housekeeping (poison, food, starvation, ring) == pre-A4-ENH1");
    check(spawn == kSpawn, "A2 Original: spawn gate == pre-A4-ENH1");
    check(combat == kCombat, "A3 Original: combat damage, kills and XP == pre-A4-ENH1");
    check(saved == kSave, "A4 Original: the saved document == pre-A4-ENH1 (no new key)");
    std::printf("%s a4_enh1_preservation failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
