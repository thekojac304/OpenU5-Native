#pragma once
#include "turn.h"
#include "dialogue.h"
#include <array>
namespace openu5 {
enum class PlotItem : uint8_t { Amulet, Crown, Sceptre, WoodenBox, Carpet, Falsehood, Hatred, Cowardice, None };
PlotItem plot_item_for_npc_type(int32_t);
struct DestroyShadowlordResult { bool ok = false; std::u16string message; };
DestroyShadowlordResult destroy_shadowlord(GameState &, int32_t);
const char *grant_plot_item(GameState &, PlotItem);
bool can_reach_doom(const GameState &);
bool endgame_ready(const GameState &);
enum class RescueEnding : uint8_t { Incomplete, Victory, Stranded };
// State/gating portion of rescueLordBritish; narrative/scroll is a separate caller concern.
RescueEnding rescue_lord_british(GameState &, bool via_absorption = false);
struct QuestPlaytime { int32_t years, months, days; };
QuestPlaytime endgame_playtime(const GameState &);
struct QuestLines { const char *lines[3]{}; uint8_t count = 0; };
struct RitualResult { QuestLines text{}; bool destroyed = false; uint8_t doom_bit = 0; };
int32_t match_yell_name(TalkText);
bool quest_text_equal(TalkText,TalkText);
bool quest_text_contains(TalkText,TalkText);
int32_t summon_shadowlord(TalkText, int32_t party_y, const bool (&alive)[3], bool present);
RitualResult cast_shard_into_flame(int32_t shard, int32_t x, int32_t y, int32_t location,
                                 int32_t floor, int32_t tile_above, int32_t summoned);
void apply_shard_destruction(GameState &, TurnState &, int32_t shard);
struct YellWordResult { bool uttered = false, opened = false; int32_t location = -1; QuestLines text{}; };
YellWordResult yell_word_of_power(TalkView<TalkText> words, TalkText said, TalkView<int32_t> adjacent);
struct ShrineData { TalkText virtues[8]{}, mantras[8]{}; int32_t x[8]{}, y[8]{}; uint8_t count = 0; };
enum class ShrineMode : uint8_t { ShowMantra, QuestComplete, Donation };
ShrineMode shrine_mode(const GameState &, uint8_t virtue);
void shrine_show_mantra(GameState &, uint8_t virtue);
bool shrine_visit_check(uint8_t virtue, TalkText, TalkView<TalkText> mantras, const ShrineData &);
struct CodexLesson { int32_t virtue = -1; bool ceremony = false; };
CodexLesson shrine_codex_lesson(GameState &);
struct DonationResult { bool accepted = false; int64_t cost = 0; };
DonationResult shrine_donate(GameState &, int32_t cycles);
// Attribute bits in reference output order: strength, dexterity, intelligence.
uint8_t shrine_complete_quest(GameState &, uint8_t virtue, CharacterState &avatar);
bool shrine_restore(GameState &, uint8_t virtue, TalkText, TalkView<TalkText>, int32_t x, int32_t y, const ShrineData &);
int32_t shrine_index_at(const ShrineData &, int32_t x, int32_t y);
struct MelodyStep { int32_t progress = 0; bool complete = false; };
MelodyStep advance_melody(int32_t progress, int32_t digit);
enum class TheftKind : uint8_t { None, Keys, Gems, Torches, Equipment, Potion, Scroll, Gold };
struct TheftResult { TheftKind kind = TheftKind::None; int32_t index = -1, amount = 0; };
TheftResult apply_faulinei_theft(GameState &, int32_t shadowlord_here, Rand);
} // namespace openu5
