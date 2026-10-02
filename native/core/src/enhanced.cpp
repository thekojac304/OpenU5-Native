#include "openu5/enhanced.h"

#include "openu5/state.h"

#include <algorithm>
#include <cstdio>

// Alpha 4 A4-ENH1 (targets/tdeck/ALPHA4_UI.md section 10). See enhanced.h.
namespace openu5 {

bool enhanced_is_default(const EnhancedState &e) { return !e.god_mode && !e.cheats_used; }

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
