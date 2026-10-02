#pragma once

#include <cstddef>
#include <cstdint>

// Alpha 4 A4-ENH1 (targets/tdeck/ALPHA4_UI.md section 10): Enhanced-mode state
// and the player-facing cheats. Nothing here is 1988 behaviour. With every
// field at its default -- no cheat ever used -- the game is exactly the
// recreated original, and the save carries no trace of this file: the
// "enhanced" save key is written only when something differs from the default
// (save_core.cpp), it lives in the sidecar under its own key, and the PC save
// bridge never reads or writes it (section 5's rule for enhanced-only state).
namespace openu5 {

struct GameState;

// The player cheats, in Cheats-page order. APPEND new kinds -- restore MP,
// revive, food, keys, torches, gems, reagents, equipment, teleport, no
// encounters / hunger / poison, quest items -- the value is also the bit in
// EnhancedState::cheats_used, so no kind may ever be renumbered.
enum class CheatKind : uint8_t { GodMode, HealParty, CureParty, AddGold, MaxGold, Count };
constexpr uint32_t cheat_bit(CheatKind k) { return uint32_t(1) << unsigned(k); }

// The difficulty presets, in Difficulty-page order. Original is the default
// and the identity: every rules hook returns the 1988 value unchanged.
enum class Difficulty : uint8_t { Original, Relaxed, Easy, Count };

struct EnhancedState {
    Difficulty difficulty = Difficulty::Original;
    // Persistent toggle: no party member loses HP, from any source.
    bool god_mode = false;
    // Support metadata only: every cheat ever applied to this journey, one
    // cheat_bit() each. It changes nothing in play.
    uint32_t cheats_used = 0;
};
bool enhanced_is_default(const EnhancedState &);

// The difficulty layer: what a preset changes, as plain numbers, layered on top
// of the recreated rules at a few central hooks -- no enemy table, no item
// table, no original constant is edited. PROVISIONAL values (section 10),
// tuned on hardware and in play: change kGameplayRules in enhanced.cpp.
struct GameplayRules {
    uint16_t incoming_damage_pct; // an enemy's hit on a party member, in combat
    uint16_t outgoing_damage_pct; // a party member's hit on an enemy, in combat
    uint16_t xp_pct;              // experience per kill (combat kill(), the one award site)
    uint8_t poison_interval;      // a poisoned member loses its 1 HP on every Nth turn (1: every turn)
    uint16_t hunger_pct;          // share of the 06 / 12 / 18 meals that eat food
    uint16_t encounter_pct;       // share of passed overworld spawn rolls that spawn a monster
};
const GameplayRules &gameplay_rules(Difficulty);
const char *difficulty_name(Difficulty);
// The hooks. Each takes the value the 1988 rule computed and returns what is
// applied; at Original each returns its input unchanged. Percentages round
// half up, and a positive value never scales to 0 (a hit stays a hit, a kill
// is worth at least 1 XP).
int32_t rules_incoming_damage(const GameState &, int32_t damage); // 99 (COMBAT's kill-outright value) is kept
int32_t rules_outgoing_damage(const GameState &, int32_t damage);
int32_t rules_xp_award(const GameState &, int32_t xp);
// Poison and meals are thinned by existing, saved counters -- the turn count
// (turns_since_start) and the calendar -- so no new state can drift across
// save / load, rest, map changes or combat.
bool rules_poison_due(const GameState &);
bool rules_meal_due(const GameState &);
bool rules_encounter_allowed(const GameState &);

constexpr int32_t kGoldCap = 9999; // every gold writer in the port caps here (loot, shops, TLK)
constexpr int32_t kAddGoldAmounts[] = {10, 100, 1000};
constexpr size_t kAddGoldAmountCount = sizeof(kAddGoldAmounts) / sizeof(kAddGoldAmounts[0]);

struct CheatResult {
    bool applied = false; // false: refused or nothing to change (`text` says which)
    char text[40]{};      // the menu's footer line and the transcript's
};
// The ONE entry point for the player cheats: each changes the game here and
// only here, through the same fields the game's own writers use. Heal and Cure
// are refused in combat (the arena holds its own copy of every member's HP).
CheatResult apply_cheat(GameState &, CheatKind, int32_t amount = 0, bool in_combat = false);
const char *cheat_name(CheatKind);
// God Mode's one question, asked by every party HP-loss site: combat damage(),
// apply_damage() (poison, starvation, fire and lava, quakes, traps, the Look
// sun), the chest trap, the ladder fall and the waterfall. RNG draws still
// happen as in the original; only the HP write is skipped.
bool party_damage_blocked(const GameState &);

} // namespace openu5
