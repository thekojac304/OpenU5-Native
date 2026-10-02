#include "openu5/enhanced.h"

#include "openu5/state.h"

#include <algorithm>
#include <cstdio>

// Alpha 4 A4-ENH1 (targets/tdeck/ALPHA4_UI.md section 10). See enhanced.h.
namespace openu5 {

bool enhanced_is_default(const EnhancedState &e) {
    return e.difficulty == Difficulty::Original && !e.god_mode && !e.cheats_used;
}

namespace {
// PROVISIONAL (section 10). One row per Difficulty; Original MUST stay the
// identity row (a4_enh1_rules R1 and the a4_enh1_preservation goldens).
constexpr GameplayRules kGameplayRules[] = {
    // incoming, outgoing, xp, poison every N turns, meals, encounters
    {100, 100, 100, 1, 100, 100}, // Original: the recreated 1988 rules
    {85, 100, 150, 4, 75, 90},    // Relaxed
    {65, 100, 200, 10, 50, 75},   // Easy
};
static_assert(sizeof(kGameplayRules) / sizeof(kGameplayRules[0]) == size_t(Difficulty::Count), "one row per preset");

int32_t scale(int32_t v, uint16_t pct) {
    if (pct == 100 || v <= 0) return v;
    const int64_t s = (int64_t(v) * pct + 50) / 100;
    return int32_t(std::clamp<int64_t>(s, 1, INT32_MAX));
}
const GameplayRules &rules(const GameState &g) { return gameplay_rules(g.enhanced.difficulty); }
} // namespace

const GameplayRules &gameplay_rules(Difficulty d) {
    return kGameplayRules[unsigned(d) < unsigned(Difficulty::Count) ? unsigned(d) : 0U];
}

const char *difficulty_name(Difficulty d) {
    switch (d) {
    case Difficulty::Relaxed: return "Relaxed";
    case Difficulty::Easy: return "Easy";
    case Difficulty::Original:
    case Difficulty::Count: break;
    }
    return "Original";
}

int32_t rules_incoming_damage(const GameState &g, int32_t d) {
    if (d == 99) return d; // COMBAT's kill-outright value keeps its meaning
    const int32_t s = scale(d, rules(g).incoming_damage_pct);
    return s == 99 ? 98 : s; // and is never manufactured by scaling
}

int32_t rules_outgoing_damage(const GameState &g, int32_t d) {
    if (d == 99) return d;
    const int32_t s = scale(d, rules(g).outgoing_damage_pct);
    return s == 99 ? 98 : s;
}

int32_t rules_xp_award(const GameState &g, int32_t xp) { return scale(xp, rules(g).xp_pct); }

bool rules_poison_due(const GameState &g) {
    const int64_t n = rules(g).poison_interval;
    return n <= 1 || g.turns_since_start % n == 0;
}

bool rules_meal_due(const GameState &g) {
    // The calendar's meal number (three a day, 28-day months, 13 months a
    // year): eat on meal m when floor((m + 1) p) > floor(m p) -- exactly p of
    // the meals, evenly spread, the same ones after any save / load.
    const int64_t p = rules(g).hunger_pct;
    if (p >= 100) return true;
    const int64_t day = (int64_t(g.time.year) * 13 + (g.time.month - 1)) * 28 + (g.time.day - 1);
    const int64_t meal = day * 3 + g.time.hour / 6 - 1;
    if (meal < 0) return true;
    return (meal + 1) * p / 100 > meal * p / 100;
}

bool rules_encounter_allowed(const GameState &g) {
    // A fixed hash of the turn number: no draw from the game's RNG, the same
    // answer for the same turn after a load.
    const uint16_t p = rules(g).encounter_pct;
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
