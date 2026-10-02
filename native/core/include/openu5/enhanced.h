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

struct EnhancedState {
    // Persistent toggle: no party member loses HP, from any source.
    bool god_mode = false;
    // Support metadata only: every cheat ever applied to this journey, one
    // cheat_bit() each. It changes nothing in play.
    uint32_t cheats_used = 0;
};
bool enhanced_is_default(const EnhancedState &);

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
