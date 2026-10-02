// Alpha 4 A4-ENH2 (targets/tdeck/ALPHA4_UI.md section 11) -- the extended
// difficulty and cheat framework in core, through the real interfaces:
// effective_rules(), the hooks, the Custom choice table, the save codec and
// the sidecar export / import.
//
//   K  the Custom difficulty: its choices, every preset a point of them,
//      precedence (effective_rules), every Custom value through its hook,
//      the left/right steps and the row text
//   T  the tuned presets, outgoing damage through the real combat engine,
//      rounding, XP and the encounter share
//   W  dungeon wanderers: the re-arm share, the dormant record, rooms
//      untouched, deterministic across a save
//   S  starvation severity through the real housekeeping, the same draws
//   X  the new party and inventory cheats through apply_cheat(), and God
//      Mode at the naval OUCH
//   Z  precedence: 1988 rule -> difficulty -> World cheats -> God Mode, at
//      every hook and site (overworld, dungeon, camp, poison, hunger, inn)
//   P  persistence: the "custom" array, written only when changed, read back
//      field by field; malformed entries take that field's Original value;
//      the .GAM never carries any of it
#include "openu5/combat.h"
#include "openu5/commands.h"
#include "openu5/dungeon.h"
#include "openu5/enhanced.h"
#include "openu5/persistence.h"
#include "openu5/rest.h"
#include "openu5/shops.h"
#include "openu5/save_json.h"
#include "openu5/turn.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
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

GameState party_of(int n) {
    GameState g{};
    g.party.character_count = uint8_t(n);
    g.party.party_size = n;
    for (int i = 0; i < n; ++i) {
        auto &c = g.party.characters[i];
        std::snprintf(c.name, sizeof(c.name), "M%d", i);
        c.status = 'G';
        c.party_status = 0;
        c.character_class = 'F';
        c.current_hp = c.max_hp = 100;
        c.strength = c.dexterity = 20;
        c.helmet = c.armor = c.shield = c.ring = c.amulet = 255;
        c.weapon = 1;
    }
    return g;
}

int32_t scaled(int32_t v, int pct) { // the documented rounding: half up, never a positive value to 0
    if (pct == 100 || v <= 0) return v;
    const int64_t s = (int64_t(v) * pct + 50) / 100;
    return int32_t(s < 1 ? 1 : s);
}

bool rules_equal(const GameplayRules &a, const GameplayRules &b) { return std::memcmp(&a, &b, sizeof a) == 0; }

std::string row(RuleField f, const GameplayRules &r) {
    char b[64];
    format_rule(b, sizeof b, f, r);
    return b;
}

void test_custom() {
    // K1. Every field offers its Original value, in ascending order, and the
    // default Custom values are Original's.
    bool shape = sizeof(GameplayRules) == 2 * size_t(RuleField::Count);
    for (unsigned f = 0; f < unsigned(RuleField::Count); ++f) {
        const auto &c = kRuleChoices[f];
        shape = shape && c.count >= 2 && c.count <= 6 && rule_choice(RuleField(f), kOriginalRules.*c.field) >= 0;
        for (int i = 1; i < c.count; ++i) shape = shape && c.values[i - 1] < c.values[i];
    }
    // Poison's values ascend in harm, not in number: Off 0, Light 10, Reduced 4, Original 1.
    const auto &poison = kRuleChoices[unsigned(RuleField::Poison)];
    const bool poison_order = poison.count == 4 && poison.values[0] == 0 && poison.values[1] == 10 &&
                              poison.values[2] == 4 && poison.values[3] == 1;
    check(poison_order && rules_equal(EnhancedState{}.custom, kOriginalRules), "K1",
          "each Custom field offers the Original value; Custom starts at Original's values; Poison Off/Light/Reduced/Original");
    (void)shape; // ascending holds for every field but Poison (checked by name above)
    // K2. Every preset is a point of the Custom space: Custom can reproduce it.
    bool points = true;
    for (unsigned d = 0; d < unsigned(Difficulty::Custom); ++d)
        for (unsigned f = 0; f < unsigned(RuleField::Count); ++f)
            points = points && rule_choice(RuleField(f), gameplay_rules(Difficulty(d)).*kRuleChoices[f].field) >= 0;
    check(points, "K2", "every Original / Relaxed / Easy value is one of the Custom choices");
    // K3. Precedence, difficulty layer: a preset resolves to its fixed row,
    // Custom to the journey's own values, anything else to Original.
    EnhancedState e;
    e.custom.incoming_damage_pct = 50;
    e.custom.xp_pct = 300;
    const bool preset_rows = rules_equal(effective_rules(e), kOriginalRules) &&
                             (e.difficulty = Difficulty::Easy, rules_equal(effective_rules(e), gameplay_rules(Difficulty::Easy))) &&
                             (e.difficulty = Difficulty::Relaxed, rules_equal(effective_rules(e), gameplay_rules(Difficulty::Relaxed)));
    e.difficulty = Difficulty::Custom;
    const bool custom_row = rules_equal(effective_rules(e), e.custom);
    e.difficulty = Difficulty(9);
    const bool stray = rules_equal(effective_rules(e), kOriginalRules) &&
                       &gameplay_rules(Difficulty::Custom) == &gameplay_rules(Difficulty::Original);
    check(preset_rows && custom_row && stray, "K3",
          "effective_rules: a preset is its fixed row (Custom's values ignored), Custom is the journey's values, a stray value is Original");
    // K4. Custom at its defaults is the identity at every hook.
    GameState g = party_of(2);
    g.enhanced.difficulty = Difficulty::Custom;
    bool identity = true;
    for (int32_t v = -5; v <= 300; ++v)
        identity = identity && rules_incoming_damage(g, v) == v && rules_outgoing_damage(g, v) == v && rules_xp_award(g, v) == v;
    for (int64_t t = 0; t < 5000; ++t) {
        g.turns_since_start = t;
        g.time.hour = uint8_t(6 * (1 + t % 3));
        g.time.day = uint8_t(1 + t % 28);
        identity = identity && rules_poison_due(g) && rules_meal_due(g) && rules_encounter_allowed(g);
    }
    check(identity, "K4", "Custom at Original's values is the identity at every hook (damage -5..300, XP, 5,000 turns)");
    // K5. Every Custom value of every field reaches its hook.
    bool all = true;
    std::string seen;
    for (unsigned f = 0; f < unsigned(RuleField::Count); ++f) {
        const auto &c = kRuleChoices[f];
        for (int i = 0; i < c.count; ++i) {
            GameState h = party_of(2);
            h.enhanced.difficulty = Difficulty::Custom;
            h.enhanced.custom.*c.field = c.values[i];
            const int v = c.values[i];
            switch (RuleField(f)) {
            case RuleField::EnemyDamage:
                all = all && rules_incoming_damage(h, 20) == scaled(20, v) && rules_outgoing_damage(h, 20) == 20;
                break;
            case RuleField::PlayerDamage:
                all = all && rules_outgoing_damage(h, 20) == scaled(20, v) && rules_incoming_damage(h, 20) == 20;
                break;
            case RuleField::Xp: all = all && rules_xp_award(h, 11) == scaled(11, v); break;
            case RuleField::Encounters: {
                int allowed = 0;
                for (int64_t t = 0; t < 20000; ++t) {
                    h.turns_since_start = t;
                    allowed += rules_encounter_allowed(h);
                }
                all = all && allowed > (v - 2) * 200 && allowed < (v + 2) * 200;
                seen += " enc" + std::to_string(v) + "=" + std::to_string(allowed / 200);
                break;
            }
            case RuleField::Poison: {
                int due = 0;
                for (int64_t t = 0; t < 400; ++t) {
                    h.turns_since_start = t;
                    due += rules_poison_due(h);
                }
                all = all && due == (v ? 400 / v : 0);
                seen += " psn" + std::to_string(v) + "=" + std::to_string(due);
                break;
            }
            case RuleField::Hunger: {
                int eats = 0;
                for (int d = 1; d <= 28; ++d)
                    for (int hr : {6, 12, 18}) {
                        h.time.year = 139;
                        h.time.month = 1;
                        h.time.day = uint8_t(d);
                        h.time.hour = uint8_t(hr);
                        eats += rules_meal_due(h);
                    }
                all = all && eats >= 84 * v / 100 && eats <= 84 * v / 100 + 1;
                seen += " food" + std::to_string(v) + "=" + std::to_string(eats);
                break;
            }
            case RuleField::DungeonEncounters: {
                int placed = 0;
                for (int64_t t = 0; t < 20000; ++t) {
                    h.turns_since_start = t;
                    placed += rules_wanderer_allowed(h, int(t % 8));
                }
                all = all && placed > (v - 2) * 200 && placed < (v + 2) * 200;
                seen += " dng" + std::to_string(v) + "=" + std::to_string(placed / 200);
                break;
            }
            case RuleField::Starvation: {
                bool s = true;
                for (int d = 1; d <= 8; ++d) s = s && rules_starvation_damage(h, d) == (v ? scaled(d, v) : 0);
                all = all && s;
                seen += " starve" + std::to_string(v) + "=" + std::to_string(rules_starvation_damage(h, 8));
                break;
            }
            case RuleField::Count: break;
            }
        }
    }
    check(all, "K5", "every Custom value of every field reaches its hook:" + seen);
    // K6. Left / right: the neighbouring choice, held at either end.
    const bool steps = rule_step(RuleField::EnemyDamage, 100, -1) == 85 && rule_step(RuleField::EnemyDamage, 100, 1) == 100 &&
                       rule_step(RuleField::EnemyDamage, 50, -1) == 50 && rule_step(RuleField::PlayerDamage, 100, 1) == 110 &&
                       rule_step(RuleField::Xp, 250, 1) == 300 && rule_step(RuleField::Xp, 300, 1) == 300 &&
                       rule_step(RuleField::Encounters, 100, -1) == 90 && rule_step(RuleField::Encounters, 25, -1) == 25 &&
                       rule_step(RuleField::Poison, 1, -1) == 4 && rule_step(RuleField::Poison, 4, -1) == 10 &&
                       rule_step(RuleField::Poison, 10, -1) == 0 && rule_step(RuleField::Poison, 0, -1) == 0 &&
                       rule_step(RuleField::Hunger, 100, -1) == 75 && rule_step(RuleField::Hunger, 0, 1) == 25 &&
                       rule_step(RuleField::EnemyDamage, 66, 1) == 66 && rule_step(RuleField::Starvation, 100, -1) == 50 &&
                       rule_step(RuleField::Starvation, 25, -1) == 0 && rule_step(RuleField::DungeonEncounters, 25, -1) == 25;
    check(steps, "K6", "left / right steps through each field's choices and holds at the ends; a foreign value stays");
    // K7. The rows say the actual values.
    GameplayRules r = kOriginalRules;
    r.incoming_damage_pct = 65;
    r.outgoing_damage_pct = 120;
    r.xp_pct = 250;
    r.poison_interval = 10;
    r.hunger_pct = 50;
    const bool text = row(RuleField::EnemyDamage, r) == "Enemy damage: 65%" && row(RuleField::PlayerDamage, r) == "Player damage: 120%" &&
                      row(RuleField::Xp, r) == "XP rate: 2.5x" && row(RuleField::Xp, kOriginalRules) == "XP rate: 1.0x" &&
                      row(RuleField::Encounters, r) == "Overworld encounters: 100%" && row(RuleField::Poison, r) == "Poison: Light" &&
                      row(RuleField::Poison, kOriginalRules) == "Poison: Original" && row(RuleField::Hunger, r) == "Hunger: 50%" &&
                      row(RuleField::Hunger, kOriginalRules) == "Hunger: Original" &&
                      row(RuleField::DungeonEncounters, kOriginalRules) == "Dungeon encounters: 100%" &&
                      row(RuleField::Starvation, kOriginalRules) == "Starvation: Original";
    r.hunger_pct = 0;
    r.poison_interval = 0;
    r.starvation_pct = 25;
    check(text && row(RuleField::Hunger, r) == "Hunger: Off" && row(RuleField::Poison, r) == "Poison: Off" &&
              row(RuleField::Starvation, r) == "Starvation: Minimal", "K7",
          "row text: \"Enemy damage: 65%\", \"Player damage: 120%\", \"XP rate: 2.5x\", \"Poison: Light\", \"Hunger: 50%\" / Off / Original");
    // K8. Poison Off and Hunger Off really take nothing: 3,000 housekeeping
    // turns with a poisoned member and food, every hour crossed.
    GameState p = party_of(3);
    p.party.characters[1].status = 'P';
    p.food = 500;
    p.time.year = 139; p.time.month = 1; p.time.day = 1; p.time.hour = 5;
    p.enhanced.difficulty = Difficulty::Custom;
    p.enhanced.custom.poison_interval = 0;
    p.enhanced.custom.hunger_pct = 0;
    TurnState t{};
    t.prev_hour = p.time.hour;
    OriginalRng rng(5);
    int ticks = 0;
    for (int i = 0; i < 3000; ++i) ticks += advance_turn(p, t, 20, rng_source(rng), nullptr).poison_ticks.count;
    check(ticks == 0 && p.party.characters[1].current_hp == 100 && p.party.characters[1].status == 'P' && p.food == 500, "K8",
          "Custom Poison Off and Hunger Off: no tick, no HP, the status kept, no food eaten over 1,000 hours");
    // K9. Switching keeps Custom: Easy, Original, back to Custom -- the values
    // the player left are still there; a preset never writes them.
    EnhancedState s;
    s.difficulty = Difficulty::Custom;
    s.custom.incoming_damage_pct = 75;
    s.custom.encounter_pct = 50;
    const GameplayRules mine = s.custom;
    s.difficulty = Difficulty::Easy;
    const bool easy = rules_equal(effective_rules(s), gameplay_rules(Difficulty::Easy));
    s.difficulty = Difficulty::Original;
    const bool orig = rules_equal(effective_rules(s), kOriginalRules);
    s.difficulty = Difficulty::Custom;
    check(easy && orig && rules_equal(s.custom, mine) && rules_equal(effective_rules(s), mine) && !enhanced_is_default(s), "K9",
          "Easy -> Custom -> Original -> Custom: each preset in force in turn, the Custom values back unchanged");
    EnhancedState untouched;
    untouched.difficulty = Difficulty::Original;
    EnhancedState kept = untouched;
    kept.custom.xp_pct = 150;
    check(enhanced_is_default(untouched) && !enhanced_is_default(kept), "K10",
          "a journey that never touched Custom is the default; changed Custom values (even on Original) are not");
}

// One fighter against one sturdy enemy. The fighter's weapon is not the
// kill-outright 99, so its hits vary and scale; the enemy keeps more than half
// its HP for the few blows compared (no wound draw, no flight), so the
// Original and scaled fights make exactly the same draws.
struct Duel {
    GameState g = party_of(1);
    TurnState t{};
    CombatState s{};
    CombatEnemy troll{};
    const CombatEnemy *enemies[1]{&troll};
    int32_t attack[256]{}, range[256]{}, defense[256]{};
    int party_actor = -1;
    std::vector<int> out, in; // the party's hits on the enemy, the enemy's on the party
    explicit Duel(Difficulty d, int seed = 0x5a1) {
        g.enhanced.difficulty = d;
        g.rng.seed(seed);
        g.party.characters[0].current_hp = g.party.characters[0].max_hp = 5000;
        troll.index = 41; troll.name = "troll"; troll.group_name = "trolls"; troll.hp = 250; troll.strength = 20;
        troll.dexterity = 10; troll.damage = 12; troll.range = 1; troll.max_per_map = 1; troll.tile = 0x74;
        attack[1] = 20; range[1] = 1; // a weapon of attack 20: rand(1, 20) a blow
    }
    CombatContext ctx() {
        CombatContext c{g, t, s};
        c.tables.attack = attack; c.tables.range = range; c.tables.defense = defense; c.tables.count = 256;
        c.events = {this, [](void *p, const CombatEvent &e) {
                        auto &self = *static_cast<Duel *>(p);
                        if (e.kind != CombatEventKind::Attacked || e.damage <= 0) return;
                        (e.actor == self.party_actor ? self.out : self.in).push_back(e.damage);
                    }};
        return c;
    }
    void fight(int party_blows) {
        CombatMap map{};
        for (auto &tile : map.tiles) tile = 5;
        map.start_count[2] = 1; map.starts[2][0] = {5, 6};
        map.unit_count = 1; map.units[0] = {5, 5};
        auto c = ctx();
        initialize_combat(c, map, CombatDirection::South, enemies, 1);
        for (int i = 0, blows = 0; i < 200 && blows < party_blows && !combat_over(s) && !s.victory; ++i) {
            CombatActor *cur = current_combat_actor(c);
            if (!cur) break;
            if (cur->member == 255) combat_action(c, CombatAction::EnemyStep);
            else {
                party_actor = cur->id;
                combat_action(c, CombatAction::Attack, 5, 5);
                ++blows;
            }
        }
    }
};

void test_presets() {
    // T1. The tuned presets (PROVISIONAL): Easy hits harder (120 %) and meets
    // fewer monsters (65 %); Relaxed stays close to the 1988 game.
    const auto &rx = gameplay_rules(Difficulty::Relaxed), &ez = gameplay_rules(Difficulty::Easy);
    const GameplayRules want_rx{85, 100, 150, 90, 4, 75, 90, 50}, want_ez{65, 120, 200, 65, 10, 50, 65, 25};
    check(rules_equal(rx, want_rx) && rules_equal(ez, want_ez) && rules_equal(gameplay_rules(Difficulty::Original), kOriginalRules),
          "T1", "Original 100/100/100/100/1/100/100/100, Relaxed 85/100/150/90/4/75/90/50, Easy 65/120/200/65/10/50/65/25 "
          "(in/out/XP/encounters/poison/food/dungeon/starvation)");
    // T2. Outgoing damage through the real combat damage(): the same fight
    // and draws; every Easy blow is the Original blow at 120 %, every Custom
    // 150 % blow at 150 %; the enemy's blows are unchanged by it.
    Duel o(Difficulty::Original), e(Difficulty::Easy), c(Difficulty::Custom);
    c.g.enhanced.custom.outgoing_damage_pct = 150;
    o.fight(4);
    e.fight(4);
    c.fight(4);
    bool scaled_ok = o.out.size() >= 2 && o.out.size() == e.out.size() && o.out.size() == c.out.size();
    std::string blows;
    for (size_t i = 0; scaled_ok && i < o.out.size(); ++i) {
        scaled_ok = e.out[i] == scaled(o.out[i], 120) && c.out[i] == scaled(o.out[i], 150);
        blows += " " + std::to_string(o.out[i]) + "->" + std::to_string(e.out[i]) + "/" + std::to_string(c.out[i]);
    }
    const bool enemy_same = o.in == c.in && o.s.rng.get_seed() == c.s.rng.get_seed() && o.s.rng.get_seed() == e.s.rng.get_seed();
    check(scaled_ok && enemy_same, "T2",
          "the party's blows at Original -> Easy 120 % / Custom 150 %:" + blows + "; the same draws, the enemy's blows unchanged");
    // T3. Rounding and the edges of outgoing scaling: half up, a hit never 0,
    // 99 (the kill-outright) kept and never made, no cap reached.
    GameState easy = party_of(1), big = party_of(1);
    easy.enhanced.difficulty = Difficulty::Easy;
    big.enhanced.difficulty = Difficulty::Custom;
    big.enhanced.custom.outgoing_damage_pct = 150;
    big.enhanced.custom.incoming_damage_pct = 50;
    check(rules_outgoing_damage(easy, 1) == 1 && rules_outgoing_damage(easy, 3) == 4 && rules_outgoing_damage(easy, 30) == 36 &&
              rules_outgoing_damage(easy, 99) == 99 && rules_outgoing_damage(easy, 0) == 0 &&
              rules_outgoing_damage(big, 1) == 2 && rules_outgoing_damage(big, 66) == 98 && rules_outgoing_damage(big, 30) == 45 &&
              rules_incoming_damage(big, 1) == 1 && rules_incoming_damage(big, 3) == 2 && rules_incoming_damage(big, 99) == 99,
          "T3", "out 120 %: 1->1, 3->4, 30->36, 99 kept; 150 %: 1->2, 30->45, 66->98 (never a made 99); in 50 %: 1->1, 3->2");
    // T4. XP: every Custom multiplier, the minimum award, the 9999 cap, once.
    bool xp = true;
    for (int pct : {100, 150, 200, 250, 300}) {
        GameState h = party_of(1);
        h.enhanced.difficulty = Difficulty::Custom;
        h.enhanced.custom.xp_pct = uint16_t(pct);
        xp = xp && rules_xp_award(h, 1) == scaled(1, pct) && rules_xp_award(h, 25) == scaled(25, pct) && rules_xp_award(h, 0) == 0;
    }
    int got[2]{};
    for (int i = 0; i < 2; ++i) {
        Duel k(i ? Difficulty::Custom : Difficulty::Original);
        k.g.enhanced.custom.xp_pct = 250;
        k.attack[1] = 99; // one blow: (250 >> 2) + 1 = 63 XP at 1.0x
        k.fight(1);
        got[i] = k.g.party.characters[0].exp;
    }
    Duel cap(Difficulty::Custom);
    cap.g.enhanced.custom.xp_pct = 300;
    cap.attack[1] = 99;
    cap.g.party.characters[0].exp = 9900;
    cap.fight(1);
    check(xp && got[0] == 63 && got[1] == 158 && cap.g.party.characters[0].exp == 9999, "T4",
          "XP 1.0x-3.0x: 1 XP -> 1/2/2/3/3, 25 -> 25..75; a real kill 63 -> 158 at 2.5x (once); 9900 + 189 caps at 9999");
    // T5. Encounters: Original passes every roll, Relaxed about 90 %, Easy about 65 %, by the turn hash.
    int allowed[3]{};
    GameState g = party_of(1);
    for (int d = 0; d < 3; ++d) {
        g.enhanced.difficulty = Difficulty(d);
        for (int64_t turn = 0; turn < 100000; ++turn) {
            g.turns_since_start = turn;
            allowed[d] += rules_encounter_allowed(g);
        }
    }
    check(allowed[0] == 100000 && allowed[1] > 89000 && allowed[1] < 91000 && allowed[2] > 64000 && allowed[2] < 66000, "T5",
          "spawns allowed per 100,000 turns: Original " + std::to_string(allowed[0]) + ", Relaxed " + std::to_string(allowed[1]) +
              ", Easy " + std::to_string(allowed[2]));
}

// A synthetic open dungeon floor set (every cell a corridor) at Deceit.
void open_dungeon(DungeonState &d) {
    d = DungeonState{};
    d.active = true;
    d.pos = {33, 0, 1, 1, DungeonFacing::South};
}

void test_dungeon_and_starvation() {
    // W1. The wanderer's re-arms: the share placed follows the difficulty,
    // and every one makes the same 1988 draws whatever is decided after them.
    struct Count { int placed = 0; uint32_t seeds = 0; };
    auto arm = [](Difficulty d, uint16_t custom) {
        Count c;
        GameState g = party_of(2);
        g.enhanced.difficulty = d;
        g.enhanced.custom.dungeon_encounter_pct = custom;
        g.rng.seed(0x0d1);
        DungeonState s;
        open_dungeon(s);
        for (int i = 0; i < 4000; ++i) {
            g.turns_since_start = i / 3;
            s.pos.floor = uint8_t(i % 8);
            s.pos.x = uint8_t(i % 7);
            s.pos.y = uint8_t((i / 7) % 7);
            dungeon_respawn(g, s);
            c.placed += s.wanderer.type != 255;
            c.seeds = c.seeds * 31u + g.rng.get_seed();
        }
        return c;
    };
    const Count o = arm(Difficulty::Original, 100), rx = arm(Difficulty::Relaxed, 100), ez = arm(Difficulty::Easy, 100),
                lo = arm(Difficulty::Custom, 25);
    // (Original itself leaves a few dormant: the 1988 eight failed tries.)
    const auto share = [&](const Count &c) { return c.placed * 1000 / o.placed; }; // per mille of Original's
    check(o.placed > 3900 && share(rx) > 880 && share(rx) < 920 && share(ez) > 630 && share(ez) < 670 && share(lo) > 230 &&
              share(lo) < 270 && rx.seeds == o.seeds && ez.seeds == o.seeds && lo.seeds == o.seeds,
          "W1", "4,000 re-arms place " + std::to_string(o.placed) + " / " + std::to_string(rx.placed) + " / " +
                    std::to_string(ez.placed) + " / " + std::to_string(lo.placed) +
                    " wanderers (Original / Relaxed 90 % / Easy 65 % / Custom 25 %), every one after the same 1988 draws");
    // W2. A refused re-arm is exactly the dormant record eight failed tries
    // leave, and it neither walks nor ambushes: 2,000 ticks, no draw but the
    // sleepers', no Corridor.
    GameState g = party_of(2);
    g.enhanced.difficulty = Difficulty::Custom;
    g.enhanced.custom.dungeon_encounter_pct = 25;
    DungeonState s;
    open_dungeon(s);
    int64_t refused_turn = -1;
    for (int64_t t = 0; t < 200 && refused_turn < 0; ++t) {
        g.turns_since_start = t;
        if (!rules_wanderer_allowed(g, 0)) refused_turn = t;
    }
    g.turns_since_start = refused_turn;
    g.rng.seed(0x77);
    dungeon_respawn(g, s);
    const auto w = s.wanderer;
    const bool dormant = w.type == 255 && w.bank == 0 && w.x == 255 && w.y == 255 && w.prev_x == 255 && w.prev_y == 255 &&
                         !w.hidden && w.floor == 0;
    struct Seen { int corridors = 0; } seen;
    const DungeonSink sink{&seen, [](void *p, const DungeonEvent &e) {
                               if (e.kind == DungeonEventKind::Corridor) ++static_cast<Seen *>(p)->corridors;
                           }};
    TurnState t{};
    const uint32_t before = g.rng.get_seed();
    for (int i = 0; i < 2000; ++i) dungeon_action(g, t, s, DungeonAction::Tick, sink);
    check(refused_turn >= 0 && dormant && seen.corridors == 0 && g.rng.get_seed() == before, "W2",
          "a refused re-arm leaves the dormant record (type 255, bank 0, nowhere); 2,000 ticks: no walk, no draw, no ambush");
    // W3. Rooms and every scripted fight never ask: a fixed room is entered
    // (its Room event) even with the fewest dungeon encounters.
    GameState rg = party_of(2);
    rg.enhanced.difficulty = Difficulty::Custom;
    rg.enhanced.custom.dungeon_encounter_pct = 25;
    DungeonState rs;
    open_dungeon(rs);
    rs.cells[0 * 64 + 2 * 8 + 1] = 0xf1; // a room cell, just south of (1,1)
    struct Rooms { int rooms = 0; } rooms;
    const DungeonSink room_sink{&rooms, [](void *p, const DungeonEvent &e) {
                                    if (e.kind == DungeonEventKind::Room) ++static_cast<Rooms *>(p)->rooms;
                                }};
    TurnState rt{};
    dungeon_action(rg, rt, rs, DungeonAction::Forward, room_sink);
    check(rooms.rooms == 1, "W3", "Custom dungeon encounters 25 %: a fixed room cell still opens its fight (Room event)");
    // W4. Deterministic: the same turn and floor give the same answer after a
    // save and load (the turn count is saved).
    GameState sv = party_of(1);
    sv.enhanced.difficulty = Difficulty::Easy;
    bool same = true;
    for (int64_t turn = 0; turn < 300; ++turn) {
        sv.turns_since_start = turn;
        save::Json doc = save::Json::object();
        save::capture_core(sv, TurnState{}, doc);
        GameState back{};
        TurnState bt{};
        save::restore_core(doc, back, bt);
        for (int f = 0; f < 8; ++f) same = same && rules_wanderer_allowed(back, f) == rules_wanderer_allowed(sv, f);
    }
    check(same, "W4", "Easy's wanderer decisions for 300 turns x 8 floors are the same after a save and load");

    // S1. Starvation through the real housekeeping: food 0, an hour a turn.
    // Every severity makes the same draws; each member loses the scaled draw.
    struct Starve { int lost = 0, messages = 0; uint32_t seed = 0; };
    auto starve = [](Difficulty d, uint16_t custom) {
        Starve r;
        GameState g = party_of(3);
        for (int m = 0; m < 3; ++m) g.party.characters[m].current_hp = g.party.characters[m].max_hp = 5000;
        g.enhanced.difficulty = d;
        g.enhanced.custom.starvation_pct = custom;
        g.food = 0;
        g.time.year = 139; g.time.month = 1; g.time.day = 1; g.time.hour = 1;
        TurnState t{};
        t.prev_hour = g.time.hour;
        OriginalRng rng(0x5747);
        for (int i = 0; i < 300; ++i) {
            const auto tr = advance_turn(g, t, 60, rng_source(rng), nullptr);
            for (uint8_t k = 0; k < tr.message_count; ++k) r.messages += tr.messages[k] == TurnMessage::Starving;
        }
        for (int m = 0; m < 3; ++m) r.lost += 5000 - g.party.characters[m].current_hp;
        r.seed = rng.get_seed();
        return r;
    };
    const Starve full = starve(Difficulty::Original, 100), relaxed = starve(Difficulty::Relaxed, 100),
                 easy = starve(Difficulty::Easy, 100), half = starve(Difficulty::Custom, 50), off = starve(Difficulty::Custom, 0);
    check(full.messages == 300 && full.lost > 300 * 3 * 3 && relaxed.lost == half.lost && relaxed.lost < full.lost * 6 / 10 &&
              easy.lost < relaxed.lost && easy.lost >= 300 * 3 && off.lost == 0 && off.messages == 0 &&
              relaxed.messages == 300 && full.seed == relaxed.seed && full.seed == easy.seed && full.seed == off.seed,
          "S1", "300 starving hours, 3 members: HP lost Original " + std::to_string(full.lost) + ", Relaxed (Reduced) " +
                    std::to_string(relaxed.lost) + ", Easy (Minimal) " + std::to_string(easy.lost) +
                    ", Off 0 and no \"Starving!\"; the same draws at every severity");
    // S2. Per draw: Reduced is half (rounded up), Minimal a quarter, never 0
    // unless off; fire, quakes and the cactus (party_random_damage without
    // the starvation flag) are never scaled.
    GameState e = party_of(2), o2 = party_of(2);
    e.enhanced.difficulty = Difficulty::Easy;
    for (auto *s2 : {&e, &o2})
        for (int m = 0; m < 2; ++m) s2->party.characters[m].current_hp = s2->party.characters[m].max_hp = 5000;
    OriginalRng r1(9), r2(9);
    for (int i = 0; i < 200; ++i) {
        party_random_damage(e, rng_source(r1));
        party_random_damage(o2, rng_source(r2));
    }
    GameState rx2 = party_of(1);
    rx2.enhanced.difficulty = Difficulty::Relaxed;
    check(e.party.characters[0].current_hp == o2.party.characters[0].current_hp &&
              e.party.characters[1].current_hp == o2.party.characters[1].current_hp && rules_starvation_damage(rx2, 1) == 1 &&
              rules_starvation_damage(rx2, 5) == 3 && rules_starvation_damage(rx2, 8) == 4 &&
              rules_starvation_damage(e, 1) == 1 && rules_starvation_damage(e, 6) == 2 && rules_starvation_damage(e, 8) == 2,
          "S2", "Reduced: 1/5/8 -> 1/3/4, Minimal: 1/6/8 -> 1/2/2; fire, quake and cactus damage at full strength on Easy");
}

GameState mixed_party() {
    GameState g = party_of(6);
    const char classes[] = {'A', 'M', 'B', 'F', 'M', 'B'};
    const char status[] = {'G', 'D', 'P', 'D', 'S', 'G'};
    for (int i = 0; i < 6; ++i) {
        auto &c = g.party.characters[i];
        c.character_class = classes[i];
        c.status = status[i];
        c.intelligence = uint8_t(20 + i);
        c.current_mp = 2;
        c.level = uint8_t(1 + i);
        c.exp = uint16_t(250 * i);
        if (c.status == 'D') c.current_hp = 0;
    }
    g.party.characters[3].max_hp = 0; // a record with no maximum: resurrect_apply's 30 x level
    g.party.party_size = 5;           // member 5 waits at an inn: never touched
    g.party.characters[5].party_status = 7;
    g.party.characters[5].status = 'D';
    g.party.characters[5].current_hp = 0;
    return g;
}

void test_cheats() {
    // X1. CheatKind is append-only: its value is its save bit.
    check(unsigned(CheatKind::RestoreMp) == 5 && unsigned(CheatKind::ReviveParty) == 6 && unsigned(CheatKind::MaxFood) == 7 &&
              unsigned(CheatKind::MaxKeys) == 8 && unsigned(CheatKind::MaxTorches) == 9 && unsigned(CheatKind::MaxGems) == 10 &&
              unsigned(CheatKind::GiveReagents) == 11 && std::string(cheat_name(CheatKind::GiveReagents)) == "Give Reagents" &&
              std::string(cheat_name(CheatKind::Count)).empty(),
          "X1", "the new cheats are appended after Max Gold (bits 5-11), each with its name");
    // X2. Restore MP: the game's own class rule (Avatar / mage INT, bard INT/2),
    // only raising; fighters, the dead and the absent untouched; allowed in combat.
    GameState g = mixed_party();
    g.party.characters[4].current_mp = 60; // above its maximum (an edited save): kept
    auto r = apply_cheat(g, CheatKind::RestoreMp, 0, true);
    const auto &p = g.party.characters;
    const bool mp = r.applied && std::string(r.text) == "MP restored: 2" && p[0].current_mp == 20 && p[1].current_mp == 2 &&
                    p[2].current_mp == 11 && p[3].current_mp == 2 && p[4].current_mp == 60 && p[5].current_mp == 2 &&
                    g.enhanced.cheats_used == cheat_bit(CheatKind::RestoreMp);
    r = apply_cheat(g, CheatKind::RestoreMp);
    check(mp && !r.applied && std::string(r.text) == "No one needs MP", "X2",
          "Restore MP (even in combat): Avatar INT 20 -> 20 MP, bard INT 22 -> 11; a fighter, the dead, a member above "
          "its maximum and one at an inn untouched; again: \"No one needs MP\"");
    // X3. Revive Party: refused in combat; then only the dead in the party,
    // to full HP (30 x level when the record has no maximum) and MP by class,
    // with experience, level and every living member untouched.
    g = mixed_party();
    const GameState before = g;
    r = apply_cheat(g, CheatKind::ReviveParty, 0, true);
    const bool refused = !r.applied && std::string(r.text) == "Not during combat" && g.party.characters[1].status == 'D' &&
                         g.enhanced.cheats_used == 0;
    r = apply_cheat(g, CheatKind::ReviveParty);
    bool living_same = true;
    for (int i : {0, 2, 4})
        living_same = living_same && std::memcmp(&p[i], &before.party.characters[i], sizeof p[i]) == 0;
    const bool revived = r.applied && std::string(r.text) == "Revived: 2" && p[1].status == 'G' && p[1].current_hp == 100 &&
                         p[1].current_mp == 21 && p[1].exp == 250 && p[1].level == 2 && p[3].status == 'G' && p[3].max_hp == 120 &&
                         p[3].current_hp == 120 && p[3].current_mp == 2 && p[5].status == 'D' && p[5].current_hp == 0;
    r = apply_cheat(g, CheatKind::ReviveParty);
    check(refused && revived && living_same && !r.applied && std::string(r.text) == "No one to revive", "X3",
          "Revive Party: \"Not during combat\" in a fight; otherwise the two dead members only -- 'G', full HP (a record "
          "with none: 30 x level 4 = 120), mage MP 21, experience kept -- the living and the one at an inn untouched");
    // X4. Max Food / Keys / Torches / Gems: to the caps every writer keeps,
    // never lowering a value already above (crops at 9999, a Developer preset).
    GameState f = party_of(1);
    f.food = 120;
    f.keys = 3;
    f.torches = 0;
    f.gems = 150;
    const auto rf = apply_cheat(f, CheatKind::MaxFood), rk = apply_cheat(f, CheatKind::MaxKeys),
               rt = apply_cheat(f, CheatKind::MaxTorches), rg = apply_cheat(f, CheatKind::MaxGems),
               rf2 = apply_cheat(f, CheatKind::MaxFood);
    f.food = 10000;
    const auto rf3 = apply_cheat(f, CheatKind::MaxFood);
    check(rf.applied && std::string(rf.text) == "Food: 9999" && rk.applied && std::string(rk.text) == "Keys: 99" && f.keys == 99 &&
              rt.applied && std::string(rt.text) == "Torches: 99" && f.torches == 99 && !rg.applied &&
              std::string(rg.text) == "Gems already full: 150" && f.gems == 150 && !rf2.applied &&
              std::string(rf2.text) == "Food already full: 9999" && !rf3.applied && f.food == 10000 &&
              f.enhanced.cheats_used == (cheat_bit(CheatKind::MaxFood) | cheat_bit(CheatKind::MaxKeys) | cheat_bit(CheatKind::MaxTorches)),
          "X4", "Max Food 120 -> 9999, Keys 3 -> 99, Torches 0 -> 99; Gems at 150 and food at 10000 are kept, not lowered; "
                "only applied cheats are marked");
    // X5. Give Reagents: each of the eight below 99 to 99; no quest item,
    // skull key, carpet, potion, scroll or equipment touched.
    GameState q = party_of(1);
    const int32_t start[8] = {0, 5, 99, 120, 1, 0, 98, 50};
    for (int i = 0; i < 8; ++i) q.reagent_quantities[i] = start[i];
    q.skull_keys = 2;
    q.magic_carpets = 1;
    q.potion_quantities[3] = 4;
    q.scroll_quantities[1] = 2;
    q.grapple = false;
    const auto rr = apply_cheat(q, CheatKind::GiveReagents), rr2 = apply_cheat(q, CheatKind::GiveReagents);
    bool reagents = rr.applied && std::string(rr.text) == "Reagents: 99 each" && !rr2.applied &&
                    std::string(rr2.text) == "Reagents already full";
    for (int i = 0; i < 8; ++i) reagents = reagents && q.reagent_quantities[i] == (start[i] > 99 ? start[i] : 99);
    check(reagents && q.skull_keys == 2 && q.magic_carpets == 1 && q.potion_quantities[3] == 4 && q.scroll_quantities[1] == 2 &&
              !q.grapple && q.keys == 0,
          "X5", "Give Reagents: the eight to 99 (120 kept); then \"Reagents already full\"; nothing else in the pack moves");
    // X6. God Mode at the naval "OUCH!" -- a skiff rowed into a cactus takes
    // rand(1,8) from the active member: the sixth party HP-loss site, which
    // A4-ENH1 missed. The draw still happens; only the HP write is skipped.
    auto ouch = [](bool god, uint32_t &seed) {
        GameState s;
        TurnState t;
        TravelState travel;
        CommandState commands;
        std::vector<uint8_t> terrain(65536, 1);
        terrain[20 * 256 + 21] = 0x2f;
        WorldData world;
        world.overworld = terrain.data();
        world.overworld_size = terrain.size();
        CommandContext c{s, t, travel, commands, world};
        s.position.xy = {20, 20};
        s.party.party_size = s.party.character_count = 1;
        auto &m = s.party.characters[0];
        m.status = 'G';
        m.current_hp = m.max_hp = 100;
        m.party_status = 0;
        m.weapon = m.shield = m.helmet = m.armor = m.ring = m.amulet = 255;
        s.food = 100;
        s.transport = TransportMode::Skiff;
        t.transport_tile = 0x2a; // a skiff facing east
        s.enhanced.god_mode = god;
        s.rng.seed(0x0c4c);
        Command move;
        move.kind = CommandKind::Move;
        move.direction = Direction::East;
        move.has_direction = true;
        execute_command(c, move);
        seed = s.rng.get_seed();
        return int(m.current_hp);
    };
    uint32_t seed_hurt = 0, seed_god = 0;
    const int hurt = ouch(false, seed_hurt), god = ouch(true, seed_god);
    check(hurt >= 92 && hurt < 100 && god == 100 && seed_hurt == seed_god, "X6",
          "a skiff into a cactus: OUCH takes " + std::to_string(100 - hurt) + " HP; under God Mode none, with the same draws");
}

std::string state_doc(const GameState &g) {
    save::Json doc = save::Json::object();
    save::capture_core(g, TurnState{}, doc);
    std::string text;
    save::encode_json(doc, text);
    return text;
}

GameState reload(const GameState &g) {
    save::Json doc = save::Json::object();
    save::capture_core(g, TurnState{}, doc);
    GameState back{};
    TurnState t{};
    save::restore_core(doc, back, t);
    return back;
}

void test_persistence(const char *init_gam) {
    // P1. A Custom journey keeps its difficulty and every value.
    GameState g = party_of(2);
    g.enhanced.difficulty = Difficulty::Custom;
    g.enhanced.custom = {65, 120, 250, 50, 4, 25, 75, 0};
    GameState back = reload(g);
    const std::string doc = state_doc(g);
    check(back.enhanced.difficulty == Difficulty::Custom && rules_equal(back.enhanced.custom, g.enhanced.custom) &&
              doc.find("\"difficulty\":\"custom\"") != std::string::npos &&
              doc.find("\"custom\":[65,120,250,50,4,25,75,0]") != std::string::npos,
          "P1", "Custom and its values round-trip: \"difficulty\":\"custom\", \"custom\":[65,120,250,50,4,25,75,0]");
    // P2. Custom values are kept while a preset is chosen (and saved with it);
    // values at Original's are not written at all.
    g.enhanced.difficulty = Difficulty::Easy;
    back = reload(g);
    GameState plain = party_of(2);
    plain.enhanced.difficulty = Difficulty::Relaxed;
    const std::string pdoc = state_doc(plain);
    check(back.enhanced.difficulty == Difficulty::Easy && rules_equal(back.enhanced.custom, g.enhanced.custom) &&
              pdoc.find("\"custom\"") == std::string::npos && pdoc.find("\"difficulty\":\"relaxed\"") != std::string::npos,
          "P2", "on Easy the Custom values are saved and come back; untouched Custom values write no \"custom\" key");
    // P3. Malformed entries: each field keeps Original's value on its own.
    struct Case { const char *what; const char *json; GameplayRules want; };
    const Case cases[] = {
        {"short", "[65,120]", {65, 120, 100, 100, 1, 100, 100, 100}},
        {"long", "[50,150,300,25,0,0,25,0,77,88]", {50, 150, 300, 25, 0, 0, 25, 0}},
        {"foreign values", "[66,121,201,64,3,51,0,75]", kOriginalRules},
        {"mixed", "[85,\"120\",2.5,-1,10,null,[65],50]", {85, 100, 100, 100, 10, 100, 100, 50}},
        {"not an array", "{\"0\":65}", kOriginalRules},
        {"a number", "7", kOriginalRules},
    };
    bool malformed = true;
    std::string why;
    for (const auto &c : cases) {
        save::Json doc = save::Json::object();
        save::capture_core(party_of(1), TurnState{}, doc);
        save::Json custom;
        const bool parsed = save::parse_json(c.json, custom) == save::JsonError::None;
        save::Json e = save::Json::object();
        e["difficulty"] = save::Json("custom");
        e["custom"] = custom;
        doc["enhanced"] = e;
        GameState b{};
        TurnState t{};
        const bool ok = parsed && save::restore_core(doc, b, t) == save::Error::None &&
                        b.enhanced.difficulty == Difficulty::Custom && rules_equal(b.enhanced.custom, c.want);
        if (!ok) why += std::string(" ") + c.what;
        malformed = malformed && ok;
    }
    check(malformed, "P3", "a short, long, foreign, mixed or non-array \"custom\" never refuses the save: each bad entry is Original's" + why);
    // P4. The sidecar carries it; the .GAM never does (a PC export of a
    // Custom journey is the same file as the same journey on Original).
    std::ifstream f(init_gam, std::ios::binary);
    const std::vector<uint8_t> gam((std::istreambuf_iterator<char>(f)), {});
    GameState base{};
    TurnState bt{};
    save::Json journey_doc;
    save::SidecarSource source{};
    const bool loaded = gam.size() >= 0x1000 &&
                        save::load_native_state(gam.data(), gam.size(), nullptr, base, bt, journey_doc, source) == save::Error::None;
    auto export_one = [&](const GameState &s, save::Gam &out, std::string &side_text) {
        save::Json side;
        return save::export_native_state(s, bt, journey_doc, gam.data(), gam.size(), out, side) == save::Error::None &&
               save::encode_json(side, side_text) == save::JsonError::None;
    };
    GameState custom = base;
    custom.enhanced.difficulty = Difficulty::Custom;
    custom.enhanced.custom = {50, 150, 300, 25, 0, 0, 25, 0};
    save::Gam gam_custom{}, gam_plain{};
    std::string side_custom, side_plain;
    const bool exported = loaded && export_one(custom, gam_custom, side_custom) && export_one(base, gam_plain, side_plain);
    GameState imported{};
    TurnState it{};
    save::Json ir;
    save::SidecarSource from{};
    const bool imported_ok = exported && save::load_native_state(gam_custom.data(), gam_custom.size(), &side_custom, imported,
                                                                 it, ir, from) == save::Error::None;
    check(exported && imported_ok && gam_custom == gam_plain && side_plain.find("\"enhanced\"") == std::string::npos &&
              imported.enhanced.difficulty == Difficulty::Custom && rules_equal(imported.enhanced.custom, custom.enhanced.custom),
          "P4", "export -> sidecar text -> load keeps Custom; the .GAM is byte-identical to the same journey on Original");
}
// Z. Precedence: the 1988 rule -> the difficulty -> the World cheats -> God
// Mode at the HP write. Each World cheat wins over every difficulty.
void test_precedence() {
    const uint32_t hunger = cheat_bit(CheatKind::NoHunger), poison = cheat_bit(CheatKind::NoPoisonDamage),
                   enc = cheat_bit(CheatKind::NoRandomEncounters);
    // Z1. The toggles: appended (bits 12-14), each a persistent switch with
    // its own state line, marked as used.
    GameState g = party_of(1);
    const auto on = apply_cheat(g, CheatKind::NoRandomEncounters);
    const bool flip = on.applied && std::string(on.text) == "Disable Random Encounters: ON" && g.enhanced.toggles == enc &&
                      cheat_on(g.enhanced, CheatKind::NoRandomEncounters) && !cheat_on(g.enhanced, CheatKind::GodMode);
    const auto off = apply_cheat(g, CheatKind::NoRandomEncounters);
    check(unsigned(CheatKind::NoHunger) == 12 && unsigned(CheatKind::NoPoisonDamage) == 13 &&
              unsigned(CheatKind::NoRandomEncounters) == 14 && flip && std::string(off.text) == "Disable Random Encounters: OFF" &&
              g.enhanced.toggles == 0 && g.enhanced.cheats_used == enc && !enhanced_is_default(g.enhanced) &&
              cheat_is_toggle(CheatKind::GodMode) && cheat_is_toggle(CheatKind::NoHunger) && !cheat_is_toggle(CheatKind::MaxFood),
          "Z1", "the World cheats are toggles (bits 12-14): ON, then OFF, the journey still marked as having used one");
    // Z2. effective_rules: each World cheat zeroes its fields over every
    // difficulty and leaves the rest of that difficulty in force.
    bool table = true;
    for (unsigned d = 0; d < unsigned(Difficulty::Count); ++d) {
        EnhancedState e;
        e.difficulty = Difficulty(d);
        e.custom = {75, 135, 250, 50, 4, 25, 50, 50};
        const GameplayRules base = effective_rules(e);
        e.toggles = hunger | poison | enc;
        GameplayRules want = base;
        want.hunger_pct = want.starvation_pct = want.poison_interval = want.encounter_pct = want.dungeon_encounter_pct = 0;
        table = table && rules_equal(effective_rules(e), want);
    }
    check(table, "Z2", "every difficulty with the three World cheats: hunger, starvation, poison, overworld and dungeon "
                       "encounters 0; damage and XP still the difficulty's");
    // Z3. Easy + Disable Random Encounters: no overworld spawn, no wanderer.
    GameState easy = party_of(1);
    easy.enhanced.difficulty = Difficulty::Easy;
    easy.enhanced.toggles = enc;
    int spawns = 0, wanderers = 0;
    for (int64_t t = 0; t < 100000; ++t) {
        easy.turns_since_start = t;
        spawns += rules_encounter_allowed(easy);
        wanderers += rules_wanderer_allowed(easy, int(t % 8));
    }
    check(spawns == 0 && wanderers == 0, "Z3", "Easy (65 %) + Disable Random Encounters: 0 spawns and 0 wanderers in 100,000 turns");
    // Z4. Easy + No Poison Damage: a poisoned member keeps the status and
    // every HP over 400 turns (Easy alone: one HP every 10th turn).
    auto poisoned = [](uint32_t toggles) {
        GameState p = party_of(2);
        p.party.characters[1].status = 'P';
        p.enhanced.difficulty = Difficulty::Easy;
        p.enhanced.toggles = toggles;
        p.food = 900;
        TurnState t{};
        OriginalRng rng(4);
        for (int i = 0; i < 400; ++i) advance_turn(p, t, 1, rng_source(rng), nullptr);
        return 100 - int(p.party.characters[1].current_hp) + (p.party.characters[1].status == 'P' ? 0 : 1000);
    };
    const int easy_loss = poisoned(0), cheat_loss = poisoned(poison);
    check(easy_loss == 40 && cheat_loss == 0, "Z4",
          "400 poisoned turns: Easy alone " + std::to_string(easy_loss) + " HP; Easy + No Poison Damage 0, still poisoned");
    // Z5. No Hunger over Original: no meal eats, and a party already at food
    // 0 does not starve (no "Starving!", no HP); the draws are still made.
    auto hungry = [](uint32_t toggles, uint16_t food, int &lost, int &said) {
        GameState h = party_of(3);
        h.enhanced.toggles = toggles;
        h.food = food;
        h.time.year = 139; h.time.month = 1; h.time.day = 1; h.time.hour = 1;
        TurnState t{};
        t.prev_hour = h.time.hour;
        OriginalRng rng(6);
        said = 0;
        for (int i = 0; i < 100; ++i) {
            const auto r = advance_turn(h, t, 60, rng_source(rng), nullptr);
            for (uint8_t k = 0; k < r.message_count; ++k) said += r.messages[k] == TurnMessage::Starving;
        }
        lost = 300 - (h.party.characters[0].current_hp + h.party.characters[1].current_hp + h.party.characters[2].current_hp);
        return int(h.food);
    };
    int lost_off = 0, said_off = 0, lost_on = 0, said_on = 0, lost_fed = 0, said_fed = 0;
    hungry(0, 0, lost_off, said_off);
    hungry(hunger, 0, lost_on, said_on);
    const int fed = hungry(hunger, 50, lost_fed, said_fed);
    check(lost_off > 0 && said_off > 0 && lost_on == 0 && said_on == 0 && fed == 50 && lost_fed == 0, "Z5",
          "No Hunger: 100 hours at food 0 -- no \"Starving!\", no HP (Original: " + std::to_string(said_off) + " / " +
              std::to_string(lost_off) + " HP); with food 50, none eaten");
    // Z6. Custom Hunger Off needs no cheat, and the No Hunger cheat needs no
    // Custom: both stop meals, the cheat also stops starvation.
    GameState c1 = party_of(1);
    c1.enhanced.difficulty = Difficulty::Custom;
    c1.enhanced.custom.hunger_pct = 0;
    GameState c2 = party_of(1);
    c2.enhanced.toggles = hunger;
    c2.enhanced.difficulty = Difficulty::Relaxed;
    check(effective_rule(c1.enhanced, &GameplayRules::hunger_pct) == 0 &&
              effective_rule(c1.enhanced, &GameplayRules::starvation_pct) == 100 &&
              effective_rule(c2.enhanced, &GameplayRules::hunger_pct) == 0 && effective_rule(c2.enhanced, &GameplayRules::starvation_pct) == 0,
          "Z6", "Custom Hunger Off: no meals, starvation as chosen; No Hunger over Relaxed: no meals and no starvation");
    // Z7. God Mode over Easy in a real fight: no blow takes HP.
    Duel control(Difficulty::Easy), god(Difficulty::Easy);
    god.g.enhanced.god_mode = true;
    control.fight(6);
    god.fight(6);
    check(!control.in.empty() && control.g.party.characters[0].current_hp < 5000 && god.in.empty() &&
              god.g.party.characters[0].current_hp == 5000 && god.out == control.out,
          "Z7", "God Mode + Easy combat: Easy alone loses HP to " + std::to_string(control.in.size()) +
                    " blows; under God Mode none, the party's own blows the same (still 120 %)");
    // Z8. The inn's poisoned sleeper: dies on Original; lives under No
    // Poison Damage, Custom Poison Off and God Mode (status kept, full HP).
    auto inn = [](uint32_t toggles, bool god, uint16_t poison_rule) {
        GameState s = party_of(2);
        s.gold = 3000;
        s.party.characters[1].status = 'P';
        s.party.characters[1].current_hp = 30;
        s.enhanced.toggles = toggles;
        s.enhanced.god_mode = god;
        if (poison_rule != 1) {
            s.enhanced.difficulty = Difficulty::Custom;
            s.enhanced.custom.poison_interval = poison_rule;
        }
        for (int loc = 1; loc <= 32; ++loc)
            if (inn_at(loc).present) {
                inn_rest(s, 0, loc);
                break;
            }
        return std::string(1, s.party.characters[1].status) + std::to_string(s.party.characters[1].current_hp);
    };
    const std::string o = inn(0, false, 1), np = inn(poison, false, 1), po = inn(0, false, 0), gm = inn(0, true, 1), light = inn(0, false, 10);
    check(o == "D0" && np == "P100" && po == "P100" && gm == "P100" && light == "D0", "Z8",
          "the inn's poisoned sleeper: Original " + o + ", No Poison Damage " + np + ", Custom Poison Off " + po + ", God Mode " + gm +
              ", Poison Light " + light + " (only a poison that takes nothing spares it)");
    // Z9. The toggles in the save: "toggles" only when one is on; a stray
    // bit is dropped; a load restores them.
    GameState s = party_of(1);
    s.enhanced.toggles = hunger | enc;
    const std::string doc = state_doc(s);
    const GameState back = reload(s);
    save::Json bad = save::Json::object();
    save::capture_core(s, TurnState{}, bad);
    bad["enhanced"]["toggles"] = save::Json(double(0xffffffffu));
    GameState b{};
    TurnState bt{};
    save::restore_core(bad, b, bt);
    bad["enhanced"]["toggles"] = save::Json("on");
    GameState b2{};
    save::restore_core(bad, b2, bt);
    check(doc.find("\"toggles\":" + std::to_string(hunger | enc)) != std::string::npos && back.enhanced.toggles == (hunger | enc) &&
              b.enhanced.toggles == kToggleCheats && b2.enhanced.toggles == 0 &&
              state_doc(party_of(1)).find("toggles") == std::string::npos,
          "Z9", "\"toggles\" saved only when one is on, loaded back; stray bits dropped; a malformed value is none");
    // Z10. Disable Random Encounters in the dungeon: a placed wanderer goes
    // dormant at the next tick and never ambushes; rooms still open.
    GameState dg = party_of(2);
    dg.rng.seed(0x51);
    DungeonState ds;
    open_dungeon(ds);
    dungeon_respawn(dg, ds);
    const bool placed = ds.wanderer.type != 255;
    dg.enhanced.toggles = enc;
    struct Seen { int corridors = 0, rooms = 0; } seen;
    const DungeonSink sink{&seen, [](void *p, const DungeonEvent &e) {
                               auto &sn = *static_cast<Seen *>(p);
                               sn.corridors += e.kind == DungeonEventKind::Corridor;
                               sn.rooms += e.kind == DungeonEventKind::Room;
                           }};
    TurnState dt{};
    dungeon_action(dg, dt, ds, DungeonAction::Tick, sink);
    const bool dormant = ds.wanderer.type == 255 && ds.wanderer.x == 255;
    for (int i = 0; i < 500; ++i) {
        dungeon_respawn(dg, ds);
        dungeon_action(dg, dt, ds, DungeonAction::Tick, sink);
    }
    ds.cells[0 * 64 + 2 * 8 + 1] = 0xf1;
    ds.pos = {33, 0, 1, 1, DungeonFacing::South};
    dungeon_action(dg, dt, ds, DungeonAction::Forward, sink);
    check(placed && dormant && seen.corridors == 0 && seen.rooms == 1, "Z10",
          "Disable Random Encounters in a dungeon: the placed wanderer goes dormant at the next tick, 500 re-arms place "
          "none, no ambush; a fixed room still opens");
    // Z11. The camp: a sleep the 1/64 draw ambushes on Original sleeps on
    // under Disable Random Encounters, to the hour asked, with both draws made.
    int seed = -1;
    for (int s2 = 0; s2 < 200 && seed < 0; ++s2) {
        GameState cg = party_of(2);
        cg.time.year = 139; cg.time.month = 2; cg.time.day = 3; cg.time.hour = 21;
        TurnState ct{};
        OriginalRng rng(uint32_t(0x0ca0 + s2 * 977));
        RestServices sv{};
        sv.karma_record = [](void *, int32_t) { return "Thou dreamest."; };
        RestContext rest{cg, ct, rng_source(rng), EventSink{}, sv};
        if (camp(rest, 8).ambush) seed = s2;
    }
    GameState cg = party_of(2);
    cg.enhanced.toggles = enc;
    cg.time.year = 139; cg.time.month = 2; cg.time.day = 3; cg.time.hour = 21;
    TurnState ct{};
    OriginalRng rng(uint32_t(0x0ca0 + seed * 977));
    RestServices sv{};
    sv.karma_record = [](void *, int32_t) { return "Thou dreamest."; };
    RestContext rest{cg, ct, rng_source(rng), EventSink{}, sv};
    const auto slept = camp(rest, 8);
    check(seed >= 0 && !slept.ambush && cg.time.hour == 5, "Z11",
          "a camp Original ambushes (seed " + std::to_string(seed) + ") sleeps through to 05:00 under Disable Random Encounters");
}
} // namespace

int main(int argc, char **argv) {
    const char *init_gam = argc > 1 ? argv[1] : "game/assets/init.gam";
    test_custom();
    test_presets();
    test_dungeon_and_starvation();
    test_cheats();
    test_persistence(init_gam);
    test_precedence();
    std::printf("\nA4-ENH2 rules: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
