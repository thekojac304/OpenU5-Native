#include "openu5/enhanced.h"

#include "openu5/state.h"

#include <algorithm>
#include <cstdio>

// Alpha 4 A4-ENH1 / A4-ENH2 (targets/tdeck/ALPHA4_UI.md sections 10 and 11). See enhanced.h.
namespace openu5 {

bool enhanced_is_default(const EnhancedState &e) {
    return e.difficulty == Difficulty::Original && !e.god_mode && !e.cheats_used &&
           same_rules(e.custom, kOriginalRules);
}

// A4-ENH2: the Custom choices (enhanced.h). Every preset row below is a point
// of this space (a4_enh2_rules K2), so Custom can reproduce any of them.
namespace {
constexpr const char *kPoisonNames[] = {"Off", "Light", "Reduced", "Original"};
constexpr const char *kHungerNames[] = {"Off", "25%", "50%", "75%", "Original"};
} // namespace
const RuleChoice kRuleChoices[] = {
    {"Enemy damage", &GameplayRules::incoming_damage_pct, nullptr, false, 5, {50, 65, 75, 85, 100}},
    {"Player damage", &GameplayRules::outgoing_damage_pct, nullptr, false, 5, {100, 110, 120, 135, 150}},
    {"XP rate", &GameplayRules::xp_pct, nullptr, true, 5, {100, 150, 200, 250, 300}},
    {"Overworld encounters", &GameplayRules::encounter_pct, nullptr, false, 6, {25, 50, 65, 75, 90, 100}},
    {"Poison", &GameplayRules::poison_interval, kPoisonNames, false, 4, {0, 10, 4, 1}},
    {"Hunger", &GameplayRules::hunger_pct, kHungerNames, false, 5, {0, 25, 50, 75, 100}},
};
static_assert(sizeof(kRuleChoices) / sizeof(kRuleChoices[0]) == size_t(RuleField::Count), "one choice per field");

bool same_rules(const GameplayRules &a, const GameplayRules &b) {
    for (const auto &c : kRuleChoices)
        if (a.*c.field != b.*c.field) return false;
    return true;
}

int rule_choice(RuleField f, uint16_t v) {
    if (unsigned(f) >= unsigned(RuleField::Count)) return -1;
    const auto &c = kRuleChoices[unsigned(f)];
    for (int i = 0; i < c.count; ++i)
        if (c.values[i] == v) return i;
    return -1;
}

uint16_t rule_step(RuleField f, uint16_t v, int dir) {
    const int i = rule_choice(f, v);
    if (i < 0) return v;
    const auto &c = kRuleChoices[unsigned(f)];
    return c.values[std::clamp(i + (dir < 0 ? -1 : 1), 0, c.count - 1)];
}

void format_rule(char *out, size_t size, RuleField f, const GameplayRules &r) {
    const int i = unsigned(f) < unsigned(RuleField::Count) ? rule_choice(f, r.*kRuleChoices[unsigned(f)].field) : -1;
    if (i < 0) {
        std::snprintf(out, size, "%s", "?");
        return;
    }
    const auto &c = kRuleChoices[unsigned(f)];
    const unsigned v = c.values[i];
    if (c.names)
        std::snprintf(out, size, "%s: %s", c.label, c.names[i]);
    else if (c.multiplier)
        std::snprintf(out, size, "%s: %u.%ux", c.label, v / 100, v / 10 % 10);
    else
        std::snprintf(out, size, "%s: %u%%", c.label, v);
}

namespace {
// PROVISIONAL (sections 10 and 11). One row per preset; Original MUST stay
// kOriginalRules, the identity (a4_enh1_rules R1, the a4_enh1/a4_enh2
// preservation goldens).
constexpr GameplayRules kGameplayRules[] = {
    // incoming, outgoing, xp, overworld encounters, poison every N turns, meals
    kOriginalRules,               // Original: the recreated 1988 rules
    {85, 100, 150, 90, 4, 75},    // Relaxed
    {65, 100, 200, 75, 10, 50},   // Easy
};
static_assert(sizeof(kGameplayRules) / sizeof(kGameplayRules[0]) == size_t(Difficulty::Custom), "one row per preset");

int32_t scale(int32_t v, uint16_t pct) {
    if (pct == 100 || v <= 0) return v;
    const int64_t s = (int64_t(v) * pct + 50) / 100;
    return int32_t(std::clamp<int64_t>(s, 1, INT32_MAX));
}
uint16_t rule(const GameState &g, uint16_t GameplayRules::*field) { return effective_rule(g.enhanced, field); }
} // namespace

const GameplayRules &gameplay_rules(Difficulty d) {
    return kGameplayRules[unsigned(d) < unsigned(Difficulty::Custom) ? unsigned(d) : 0U];
}

uint16_t effective_rule(const EnhancedState &e, uint16_t GameplayRules::*field) {
    return (e.difficulty == Difficulty::Custom ? e.custom : gameplay_rules(e.difficulty)).*field;
}

GameplayRules effective_rules(const EnhancedState &e) {
    GameplayRules r = kOriginalRules;
    for (const auto &c : kRuleChoices)
        r.*c.field = effective_rule(e, c.field);
    return r;
}

const char *difficulty_name(Difficulty d) {
    switch (d) {
    case Difficulty::Relaxed: return "Relaxed";
    case Difficulty::Easy: return "Easy";
    case Difficulty::Custom: return "Custom";
    case Difficulty::Original:
    case Difficulty::Count: break;
    }
    return "Original";
}

int32_t rules_incoming_damage(const GameState &g, int32_t d) {
    if (d == 99) return d; // COMBAT's kill-outright value keeps its meaning
    const int32_t s = scale(d, rule(g, &GameplayRules::incoming_damage_pct));
    return s == 99 ? 98 : s; // and is never manufactured by scaling
}

int32_t rules_outgoing_damage(const GameState &g, int32_t d) {
    if (d == 99) return d;
    const int32_t s = scale(d, rule(g, &GameplayRules::outgoing_damage_pct));
    return s == 99 ? 98 : s;
}

int32_t rules_xp_award(const GameState &g, int32_t xp) { return scale(xp, rule(g, &GameplayRules::xp_pct)); }

bool rules_poison_due(const GameState &g) {
    const int64_t n = rule(g, &GameplayRules::poison_interval);
    if (!n) return false; // A4-ENH2: poison takes nothing
    return n == 1 || g.turns_since_start % n == 0;
}

bool rules_meal_due(const GameState &g) {
    // The calendar's meal number (three a day, 28-day months, 13 months a
    // year): eat on meal m when floor((m + 1) p) > floor(m p) -- exactly p of
    // the meals, evenly spread, the same ones after any save / load.
    const int64_t p = rule(g, &GameplayRules::hunger_pct);
    if (p >= 100) return true;
    if (!p) return false; // A4-ENH2: no meal eats
    const int64_t day = (int64_t(g.time.year) * 13 + (g.time.month - 1)) * 28 + (g.time.day - 1);
    const int64_t meal = day * 3 + g.time.hour / 6 - 1;
    if (meal < 0) return true;
    return (meal + 1) * p / 100 > meal * p / 100;
}

bool rules_encounter_allowed(const GameState &g) {
    // A fixed hash of the turn number: no draw from the game's RNG, the same
    // answer for the same turn after a load.
    const uint16_t p = rule(g, &GameplayRules::encounter_pct);
    if (p >= 100) return true;
    uint32_t x = uint32_t(uint64_t(g.turns_since_start));
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x % 100U < p;
}

bool party_damage_blocked(const GameState &g) { return g.enhanced.god_mode; }

const char *cheat_name(CheatKind k) {
    switch (k) {
    case CheatKind::GodMode: return "God Mode";
    case CheatKind::HealParty: return "Heal Party";
    case CheatKind::CureParty: return "Cure Party";
    case CheatKind::AddGold: return "Add Gold";
    case CheatKind::MaxGold: return "Max Gold";
    case CheatKind::Count: break;
    }
    return "";
}

CheatResult apply_cheat(GameState &g, CheatKind kind, int32_t amount, bool in_combat) {
    CheatResult r;
    const int32_t members = std::min<int32_t>({g.party.party_size, int32_t(g.party.character_count), int32_t(kMaxParty)});
    switch (kind) {
    case CheatKind::GodMode:
        g.enhanced.god_mode = !g.enhanced.god_mode;
        std::snprintf(r.text, sizeof(r.text), "God Mode: %s", g.enhanced.god_mode ? "ON" : "OFF");
        r.applied = true;
        break;
    case CheatKind::HealParty:
    case CheatKind::CureParty: {
        const bool heal = kind == CheatKind::HealParty;
        if (in_combat) {
            std::snprintf(r.text, sizeof(r.text), "%s", "Not during combat");
            return r;
        }
        int n = 0;
        for (int32_t i = 0; i < members; ++i) {
            auto &c = g.party.characters[i];
            if (c.status == 'D') continue; // reviving is a cheat of its own (not yet)
            if (heal && c.current_hp < c.max_hp) {
                c.current_hp = c.max_hp;
                ++n;
            } else if (!heal && (c.status == 'P' || c.status == 'S')) {
                c.status = 'G';
                ++n;
            }
        }
        if (!n) {
            std::snprintf(r.text, sizeof(r.text), "%s", heal ? "No one needs healing" : "No one needs curing");
            return r;
        }
        std::snprintf(r.text, sizeof(r.text), "%s %d", heal ? "Healed:" : "Cured:", n);
        r.applied = true;
        break;
    }
    case CheatKind::AddGold:
    case CheatKind::MaxGold: {
        // Never lowers gold (a Developer preset can leave more than 9999) and
        // never passes the cap every gold writer in the game keeps.
        const int32_t before = g.gold;
        if (before >= kGoldCap) {
            std::snprintf(r.text, sizeof(r.text), "Gold is full: %d", int(before));
            return r;
        }
        const int32_t add = kind == CheatKind::MaxGold ? kGoldCap : std::clamp<int32_t>(amount, 0, kGoldCap);
        const int32_t after = std::min<int32_t>(kGoldCap, before + add);
        if (after == before) {
            std::snprintf(r.text, sizeof(r.text), "Gold: %d", int(before));
            return r;
        }
        g.gold = uint16_t(after);
        std::snprintf(r.text, sizeof(r.text), "Gold: %d (+%d)", int(after), int(after - before));
        r.applied = true;
        break;
    }
    case CheatKind::Count:
        return r;
    }
    g.enhanced.cheats_used |= cheat_bit(kind);
    return r;
}

} // namespace openu5
