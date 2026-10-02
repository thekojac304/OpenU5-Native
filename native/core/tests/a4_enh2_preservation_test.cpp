// Alpha 4 A4-ENH2 (targets/tdeck/ALPHA4_UI.md section 11) -- the Original-mode
// preservation golden for the sites A4-ENH2 may touch.
//
// Like a4_enh1_preservation, this file uses ONLY interfaces that existed before
// A4-ENH2 (commit 5e0a16eb), and its golden hashes were recorded by building
// and running it against that unmodified tree (log
// native/core/a4-enh2-golden-head.log). A4-ENH1's own goldens left four gaps
// that the new hooks sit in, so each gets its own hash here:
//
//   B1  starvation: 2,400 turns with no food through the real advance_turn()
//       (kernel 0x2AE8: "Starving!" and rand(1,8) per member at every hour
//       change), plus party_random_damage()'s other callers (fire, quake,
//       cactus) called directly. a4_enh1_preservation A1 never starves (its
//       food never reaches 0), although its comment said it did.
//   B2  dungeon wanderers: dungeon_respawn() swept over positions and
//       floors, and a 6,000-action walk through dungeon_action() in the
//       orchestration's order (advance_turn, then the action; a corridor
//       ambush re-arms the wanderer as dungeon_combat_return does).
//   B3  camp: camp() sleeps from 40 seeds, with the 1/64 hourly ambush.
//   B4  death and resurrection: In Mani Corp (apply_target_spell, the
//       resurrect_apply copy) at several karmas, the healer's Resurrect, and
//       the inn's poisoned sleeper.
//   B5  saves: an A4-ENH1-shaped "enhanced" object loads and saves back to
//       the very same bytes (an A4-ENH1 save opened by A4-ENH2).
//
// After A4-ENH2, a default GameState -- Difficulty Original, no cheat, no
// toggle -- must reproduce every hash bit for bit.
#include "openu5/dungeon.h"
#include "openu5/magic.h"
#include "openu5/persistence.h"
#include "openu5/rest.h"
#include "openu5/save_json.h"
#include "openu5/shops.h"
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
    void text(const std::string &s) {
        for (char c : s) byte(uint8_t(c));
        add(int64_t(s.size()));
    }
};

void party(GameState &g, int members) {
    g.party.character_count = uint8_t(members);
    g.party.party_size = members;
    for (int i = 0; i < members; ++i) {
        auto &c = g.party.characters[i];
        std::snprintf(c.name, sizeof(c.name), "Hero%d", i);
        c.character_class = "AMBF"[i % 4];
        c.status = 'G';
        c.party_status = 0;
        c.strength = uint8_t(16 + i);
        c.dexterity = uint8_t(12 + 4 * i);
        c.intelligence = uint8_t(14 + 3 * i);
        c.current_hp = c.max_hp = 240;
        c.current_mp = 3;
        c.level = 4;
        c.exp = uint16_t(700 + 150 * i);
        c.helmet = c.armor = c.shield = c.ring = c.amulet = 255;
        c.weapon = 1;
    }
}

void hash_party(Fnv &f, const GameState &g) {
    for (int m = 0; m < g.party.character_count; ++m) {
        const auto &c = g.party.characters[m];
        f.add(c.current_hp);
        f.add(c.max_hp);
        f.add(c.current_mp);
        f.add(c.status);
        f.add(c.exp);
        f.add(c.level);
    }
}

// B1. Starvation: food 0, 20 minutes a turn (an hour change every third
// turn), one poisoned, one asleep, one with the Ring of Regeneration. The test
// itself revives the dead every 90 turns and gives a little food every 500, so
// meals run out again and the party starves for most of the run.
uint64_t starvation_golden(int &starving, int &deaths) {
    GameState g{};
    party(g, 4);
    g.party.characters[1].status = 'P';
    g.party.characters[2].status = 'S';
    g.party.characters[3].ring = 44;
    g.food = 0;
    g.time = {};
    g.time.year = 139; g.time.month = 1; g.time.day = 1; g.time.hour = 5; g.time.minute = 0;
    TurnState s{};
    s.prev_hour = g.time.hour;
    OriginalRng rng(0x57a4);
    const Rand rand = rng_source(rng);
    Fnv f;
    starving = deaths = 0;
    for (int i = 0; i < 2400; ++i) {
        const auto r = advance_turn(g, s, 20, rand, nullptr);
        for (uint8_t k = 0; k < r.message_count; ++k) {
            starving += r.messages[k] == TurnMessage::Starving;
            f.add(int(r.messages[k]));
        }
        f.add(r.poison_ticks.count);
        if (i % 90 == 89)
            for (int m = 0; m < 4; ++m) {
                auto &c = g.party.characters[m];
                if (c.status == 'D') {
                    ++deaths;
                    c.status = m == 1 ? 'P' : m == 2 ? 'S' : 'G';
                    c.current_hp = c.max_hp;
                }
            }
        if (i % 500 == 499) g.food = uint16_t(g.food + 6);
        f.add(i);
        hash_party(f, g);
        f.add(g.food);
        f.add(g.time.day);
        f.add(g.time.hour);
        f.add(g.turns_since_start);
        f.add(rng.get_seed());
    }
    // party_random_damage()'s other callers -- fire, the quake, the cactus --
    // share it with starvation: called directly, it must stay the 1988 loop.
    for (int i = 0; i < 300; ++i) {
        for (int m = 0; m < 4; ++m) {
            auto &c = g.party.characters[m];
            if (c.status == 'D' || c.current_hp < 20) { c.status = 'G'; c.current_hp = c.max_hp; }
        }
        party_random_damage(g, rand);
        hash_party(f, g);
        f.add(rng.get_seed());
    }
    return f.h;
}

// B2. A synthetic 8-floor dungeon: open corridor with walls, a few sleep,
// poison and fire fields, chests, fountains and pit traps.
void build_dungeon(DungeonState &d) {
    d = DungeonState{};
    d.active = true;
    d.pos = {33, 0, 1, 1, DungeonFacing::South};
    for (int f = 0; f < 8; ++f)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                const int k = (x * 7 + y * 13 + f * 5) % 23;
                uint8_t c = 0x00;
                if (k == 0 || k == 9) c = 0xb0;          // wall
                else if (k == 4) c = 0x80;                // sleep field
                else if (k == 15) c = 0x81;               // poison field
                else if (k == 19) c = 0x82;               // fire field
                else if (k == 6) c = 0x40;                // chest
                else if (k == 11) c = 0x50;               // fountain
                else if (k == 21 && f < 6) c = 0x61;      // pit trap
                d.cells[f * 64 + y * 8 + x] = c;
            }
}

void hash_dungeon(Fnv &f, const DungeonState &d) {
    f.add(d.pos.floor);
    f.add(d.pos.x);
    f.add(d.pos.y);
    f.add(int(d.pos.facing));
    const auto &w = d.wanderer;
    f.add(w.bank);
    f.add(w.type);
    f.add(w.x);
    f.add(w.y);
    f.add(w.floor);
    f.add(w.attr);
    f.add(w.hidden);
    f.add(w.prev_x);
    f.add(w.prev_y);
}

uint64_t dungeon_golden(int &placed, int &ambushes) {
    Fnv f;
    placed = ambushes = 0;
    // The respawn alone, over every floor and many party cells.
    {
        GameState g{};
        party(g, 2);
        g.rng.seed(0x0d06);
        DungeonState d;
        build_dungeon(d);
        for (int i = 0; i < 2000; ++i) {
            d.pos.floor = uint8_t(i % 8);
            d.pos.x = uint8_t((i * 3) % 8);
            d.pos.y = uint8_t((i * 5 / 8) % 8);
            dungeon_respawn(g, d);
            placed += d.wanderer.type != 255;
            hash_dungeon(f, d);
            f.add(g.rng.get_seed());
        }
    }
    // A walk in the orchestration's order: the turn (clock and housekeeping),
    // then the action; a corridor ambush is "fought" and re-arms the wanderer.
    GameState g{};
    party(g, 3);
    g.food = 30;
    g.time.year = 139; g.time.month = 1; g.time.day = 1; g.time.hour = 9; g.time.minute = 0;
    g.rng.seed(0x77d1);
    TurnState t{};
    t.prev_hour = g.time.hour;
    DungeonState d;
    build_dungeon(d);
    dungeon_respawn(g, d);
    struct Seen { Fnv *f; int *ambushes; bool corridor; };
    Seen seen{&f, &ambushes, false};
    const DungeonSink sink{&seen, [](void *p, const DungeonEvent &e) {
                               auto &s = *static_cast<Seen *>(p);
                               s.f->add(int(e.kind));
                               s.f->add(e.value);
                               s.f->add(e.member);
                               if (e.kind == DungeonEventKind::Corridor) { ++*s.ambushes; s.corridor = true; }
                           }};
    static constexpr DungeonAction script[] = {
        DungeonAction::Forward, DungeonAction::Forward, DungeonAction::Right, DungeonAction::Forward,
        DungeonAction::Pass,    DungeonAction::Forward, DungeonAction::Left,  DungeonAction::Forward,
        DungeonAction::Tick,    DungeonAction::Forward, DungeonAction::TurnAround, DungeonAction::Forward,
        DungeonAction::Pass,    DungeonAction::Forward, DungeonAction::Forward, DungeonAction::Right};
    for (int i = 0; i < 6000; ++i) {
        advance_turn(g, t, 1, rng_source(g.rng), nullptr);
        seen.corridor = false;
        dungeon_action(g, t, d, script[i % 16], sink);
        if (seen.corridor) dungeon_respawn(g, d); // dungeon_combat_return, cause >= 0
        if (d.pos.floor >= 7 || i % 250 == 249) { // keep the walk inside, and keep it moving
            d.pos.floor = uint8_t(i % 6);
            dungeon_respawn(g, d);
        }
        for (int m = 0; m < 3; ++m) {
            auto &c = g.party.characters[m];
            if (c.status == 'D' || c.current_hp < 40) { c.status = 'G'; c.current_hp = c.max_hp; }
        }
        if (g.food < 5) g.food = 30;
        f.add(i);
        hash_dungeon(f, d);
        hash_party(f, g);
        f.add(g.food);
        f.add(g.rng.get_seed());
    }
    return f.h;
}

// B3. Camp: an 8-hour sleep from 40 seeds; the 1/64 hourly ambush draw.
uint64_t camp_golden(int &ambushes) {
    Fnv f;
    ambushes = 0;
    for (int seed = 0; seed < 40; ++seed) {
        GameState g{};
        party(g, 4);
        g.party.characters[2].ring = 44;
        g.food = 50;
        g.time.year = 139; g.time.month = 2; g.time.day = 3; g.time.hour = 21; g.time.minute = 0;
        TurnState t{};
        t.prev_hour = g.time.hour;
        OriginalRng rng(uint32_t(0x0ca0 + seed * 977));
        RestServices services{};
        services.karma_record = [](void *, int32_t) { return "Thou dreamest."; };
        RestContext rest{g, t, rng_source(rng), EventSink{}, services};
        const auto r = camp(rest, 8);
        ambushes += r.ambush;
        f.add(seed);
        f.add(r.ambush);
        f.add(r.enemy);
        f.add(r.invalid_context);
        hash_party(f, g);
        f.add(g.time.hour);
        f.add(g.time.minute);
        f.add(rng.get_seed());
    }
    return f.h;
}

// B4. Death and resurrection as the port has them today: In Mani Corp's
// resurrect_apply copy (karma below 98 cuts exp), the healer's Resurrect and
// the inn's poisoned sleeper.
uint64_t resurrection_golden(int &revived, int &inn_deaths) {
    Fnv f;
    revived = inn_deaths = 0;
    OriginalRng rng(0x0dea);
    for (int karma : {0, 25, 50, 75, 97, 98, 99}) {
        GameState g{};
        party(g, 4);
        for (int m = 0; m < 4; ++m) {
            auto &c = g.party.characters[m];
            c.status = m == 3 ? 'G' : 'D';
            c.current_hp = m == 3 ? c.max_hp : 0;
        }
        for (int m = 0; m < 4; ++m) {
            const bool ok = apply_target_spell(g.party.characters[m], MagicEffect::Resurrect, uint8_t(karma),
                                               rng_source(rng));
            revived += ok;
            f.add(ok);
        }
        f.add(karma);
        hash_party(f, g);
    }
    {
        GameState g{};
        party(g, 3);
        g.gold = 2000;
        g.party.characters[1].status = 'D';
        g.party.characters[1].current_hp = 0;
        const auto r = healer_heal(g, 1, HealerService::Resurrect, 400);
        const auto again = healer_heal(g, 1, HealerService::Resurrect, 400);
        f.add(r.ok);
        f.add(again.ok);
        f.add(g.gold);
        hash_party(f, g);
    }
    for (int loc = 1; loc <= 32; ++loc) {
        if (!inn_at(loc).present) continue;
        GameState g{};
        party(g, 4);
        g.gold = 3000;
        g.party.characters[1].status = 'P';
        g.party.characters[2].status = 'S';
        g.party.characters[3].current_hp = 9;
        const auto r = inn_rest(g, 0, loc);
        inn_deaths += g.party.characters[1].status == 'D';
        f.add(loc);
        f.add(r.ok);
        f.add(g.gold);
        hash_party(f, g);
    }
    return f.h;
}

// B5. An A4-ENH1 save: its "enhanced" object loads and saves back unchanged.
uint64_t enh1_save_golden(bool &round_trip) {
    Fnv f;
    round_trip = true;
    struct Shape { Difficulty d; bool god; uint32_t used; };
    static constexpr Shape shapes[] = {{Difficulty::Easy, true, 9}, {Difficulty::Relaxed, false, 0},
                                       {Difficulty::Original, false, 6}, {Difficulty::Original, true, 1}};
    for (const auto &s : shapes) {
        GameState g{};
        party(g, 2);
        g.gold = 432;
        g.food = 120;
        g.enhanced.difficulty = s.d;
        g.enhanced.god_mode = s.god;
        g.enhanced.cheats_used = s.used;
        save::Json doc = save::Json::object();
        save::capture_core(g, TurnState{}, doc);
        std::string first, second;
        const bool encoded = save::encode_json(doc, first) == save::JsonError::None;
        GameState back{};
        TurnState tb{};
        const bool loaded = save::restore_core(doc, back, tb) == save::Error::None;
        save::Json again = save::Json::object();
        save::capture_core(back, tb, again);
        const bool reencoded = save::encode_json(again, second) == save::JsonError::None;
        round_trip = round_trip && encoded && loaded && reencoded && first == second && back.enhanced.difficulty == s.d &&
                     back.enhanced.god_mode == s.god && back.enhanced.cheats_used == s.used;
        f.text(first);
        f.text(second);
    }
    return f.h;
}
} // namespace

int main() {
    int starving = 0, deaths = 0, placed = 0, ambushes = 0, camp_ambushes = 0, revived = 0, inn_deaths = 0;
    bool round_trip = false;
    const uint64_t starve = starvation_golden(starving, deaths);
    const uint64_t dungeon = dungeon_golden(placed, ambushes);
    const uint64_t campg = camp_golden(camp_ambushes);
    const uint64_t res = resurrection_golden(revived, inn_deaths);
    const uint64_t save = enh1_save_golden(round_trip);
    std::printf("golden starvation=0x%016llx starving=%d deaths=%d\n", (unsigned long long)starve, starving, deaths);
    std::printf("golden dungeon=0x%016llx placed=%d ambushes=%d\n", (unsigned long long)dungeon, placed, ambushes);
    std::printf("golden camp=0x%016llx ambushes=%d\n", (unsigned long long)campg, camp_ambushes);
    std::printf("golden resurrection=0x%016llx revived=%d inn_deaths=%d\n", (unsigned long long)res, revived,
                inn_deaths);
    std::printf("golden enh1_save=0x%016llx round_trip=%d\n", (unsigned long long)save, int(round_trip));

    // Recorded on the unmodified tree (5e0a16eb), see the header comment.
    constexpr uint64_t kStarvation = 0x99c9087bd26f1442ULL, kDungeon = 0x57250730265a465dULL,
                       kCamp = 0xb6f1b3a77ed80150ULL, kResurrection = 0xbe90d6bb09040045ULL,
                       kEnh1Save = 0xf126b030acad54a3ULL;

    check(starving > 300 && deaths > 0, "B1 the scenario starves the party (and starvation kills)");
    check(placed > 500 && ambushes > 0, "B2 the dungeon places wanderers and one ambushes the party");
    check(camp_ambushes > 0 && camp_ambushes < 40, "B3 some camps are ambushed, some are not");
    check(revived > 0 && inn_deaths > 0, "B4 resurrection revives and the inn's poisoned sleeper dies");
    check(round_trip, "B5 an A4-ENH1 \"enhanced\" object loads and saves back unchanged");
    check(starve == kStarvation, "B1 Original: starvation and party_random_damage == pre-A4-ENH2");
    check(dungeon == kDungeon, "B2 Original: dungeon wanderer respawn, movement and ambush == pre-A4-ENH2");
    check(campg == kCamp, "B3 Original: camp sleep and its ambush == pre-A4-ENH2");
    check(res == kResurrection, "B4 Original: In Mani Corp, the healer and the inn == pre-A4-ENH2");
    check(save == kEnh1Save, "B5 Original: the A4-ENH1 save documents == pre-A4-ENH2");
    std::printf("%s a4_enh2_preservation failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
