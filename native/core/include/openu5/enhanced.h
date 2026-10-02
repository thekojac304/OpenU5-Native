#pragma once

#include <cstddef>
#include <cstdint>

// Alpha 4 A4-ENH1 / A4-ENH2 (targets/tdeck/ALPHA4_UI.md sections 10 and 11):
// Enhanced-mode state, the difficulty rules and the player-facing cheats.
// Nothing here is 1988 behaviour. With every field at its default -- Difficulty
// Original, no cheat ever used -- the game is exactly the recreated original,
// and the save carries no trace of this file: the "enhanced" save key is
// written only when something differs from the default (save_core.cpp), it
// lives in the sidecar under its own key, and the PC save bridge never reads or
// writes it (section 5's rule for enhanced-only state).
namespace openu5 {

struct GameState;

// The player cheats, in Cheats-page order. APPEND new kinds -- restore MP,
// revive, food, keys, torches, gems, reagents, equipment, teleport, no
// encounters / hunger / poison, quest items -- the value is also the bit in
// EnhancedState::cheats_used, so no kind may ever be renumbered.
enum class CheatKind : uint8_t { GodMode, HealParty, CureParty, AddGold, MaxGold, Count };
constexpr uint32_t cheat_bit(CheatKind k) { return uint32_t(1) << unsigned(k); }

// The difficulties, in Difficulty-page order. Original is the default and the
// identity: every rules hook returns the 1988 value unchanged. Relaxed and Easy
// are fixed rows; Custom (A4-ENH2) is the journey's own values.
enum class Difficulty : uint8_t { Original, Relaxed, Easy, Custom, Count };

// The difficulty layer: what a difficulty changes, as plain numbers, layered
// on top of the recreated rules at a few central hooks -- no enemy table, no
// item table, no original constant is edited. Every field is a uint16_t, so the
// Custom page reaches each one through the same member-pointer type.
struct GameplayRules {
    uint16_t incoming_damage_pct; // an enemy's hit on a party member, in combat
    uint16_t outgoing_damage_pct; // a party member's hit on an enemy, in combat
    uint16_t xp_pct;              // experience per kill (combat kill(), the one award site)
    uint16_t encounter_pct;       // share of passed overworld spawn rolls that spawn a monster
    uint16_t poison_interval;     // a poisoned member loses its 1 HP on every Nth turn (1: every turn; 0: never)
    uint16_t hunger_pct;          // share of the 06 / 12 / 18 meals that eat food
    // A4-ENH2 (appended):
    uint16_t dungeon_encounter_pct; // share of the dungeon wanderer's re-arms that place it
    uint16_t starvation_pct;        // share of starvation's rand(1,8) a starving member loses (0: none)
};
// The 1988 rules: at these values every hook returns its input unchanged.
inline constexpr GameplayRules kOriginalRules{100, 100, 100, 100, 1, 100, 100, 100};
bool same_rules(const GameplayRules &, const GameplayRules &);

struct EnhancedState {
    Difficulty difficulty = Difficulty::Original;
    // Persistent toggle: no party member loses HP, from any source.
    bool god_mode = false;
    // Support metadata only: every cheat ever applied to this journey, one
    // cheat_bit() each. It changes nothing in play.
    uint32_t cheats_used = 0;
    // A4-ENH2: the Custom difficulty's own values -- each one of its
    // kRuleChoices -- used while `difficulty` is Custom and kept, untouched,
    // while another difficulty is chosen, so Custom comes back as it was left.
    GameplayRules custom = kOriginalRules;
};
bool enhanced_is_default(const EnhancedState &);

// A preset's fixed row (PROVISIONAL values, section 11: change kGameplayRules
// in enhanced.cpp). Custom has no fixed row: it, and any value out of range,
// gets Original's.
const GameplayRules &gameplay_rules(Difficulty);
const char *difficulty_name(Difficulty);
// The rules in force for a journey -- THE one place their precedence lives:
// the 1988 rule, then the difficulty (a preset's row, or the Custom values).
// Every hook below reads its one field through effective_rule();
// effective_rules() is the same answer for every field at once.
uint16_t effective_rule(const EnhancedState &, uint16_t GameplayRules::*field);
GameplayRules effective_rules(const EnhancedState &);

// The Custom choices: one per GameplayRules field, in RuleField order -- the
// Custom page's rows and the save's "custom" array. APPEND only: a value's
// place in that array is its field. Each field offers a few discrete values,
// the Original one among them (the default).
enum class RuleField : uint8_t { EnemyDamage, PlayerDamage, Xp, Encounters, Poison, Hunger, DungeonEncounters, Starvation, Count };
struct RuleChoice {
    const char *label;
    uint16_t GameplayRules::*field;
    const char *const *names; // a name per value ("Light"); null: the number itself
    bool multiplier;          // shown as "2.5x" (XP) rather than "250%"
    uint8_t count;
    uint16_t values[6]; // ascending: left lowers, right raises
};
extern const RuleChoice kRuleChoices[size_t(RuleField::Count)];
int rule_choice(RuleField, uint16_t value);             // its index in the choices; -1: not one of them
uint16_t rule_step(RuleField, uint16_t value, int dir); // the neighbouring choice, held at either end
void format_rule(char *out, size_t size, RuleField, const GameplayRules &); // "Enemy damage: 65%"

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
// A4-ENH2: whether a dungeon re-arm (dungeon_respawn: entry, floor change,
// pit fall, after a corridor fight) places the wanderer it rolled -- asked
// after all of the 1988 placement draws, by a fixed hash of the turn and the
// floor (no draw); a refused one is the dormant record eight failed tries
// leave. Fixed rooms and every scripted fight never come here.
bool rules_wanderer_allowed(const GameState &, int floor);
// A4-ENH2: what a starving member loses of its 1988 rand(1,8) (the draw is
// always made); 0 when starvation is off. Only the starvation site asks:
// fire, quakes and the cactus share party_random_damage() but not this.
int32_t rules_starvation_damage(const GameState &, int32_t damage);

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
