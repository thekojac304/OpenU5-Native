#pragma once
#include <cstdint>
namespace openu5 {
// Only reference-owned progression fields. Karma, box, crown-wearing and
// inventory remain in GameState; physical Shadowlord placement is TravelState.
enum class QuestFlag : uint8_t {
    FalsehoodDead, HatredDead, CowardiceDead, InDoom, GameWon,
    Word33, Word34, Word35, Word36, Word37, Word38, Word39, Word40, Count
};
const char *quest_flag_name(QuestFlag);
struct QuestState {
    uint16_t flags = 0, flag_present = 0;
    bool shards[3]{}, artifacts[3]{}; // falsehood/hatred/cowardice; amulet/crown/sceptre
    int32_t shrine_quest = 0, shrine_visited = 0, doom_bits = 0;
    int32_t summoned = -1;
    uint8_t shrine_destroyed[8]{};
    uint8_t destroyed_count = 0, optional_present = 0;
    uint8_t search_found[15]{}, search_present[15]{};
};
inline bool quest_flag(const QuestState &q, QuestFlag f) {
    return (q.flags & (uint16_t(1) << unsigned(f))) != 0;
}
inline void set_quest_flag(QuestState &q, QuestFlag f, bool value = true) {
    const auto bit = uint16_t(uint16_t(1) << unsigned(f));
    q.flag_present |= bit;
    q.flags = uint16_t(value ? q.flags | bit : q.flags & ~bit);
}
} // namespace openu5
