#include "openu5/state.h"
namespace openu5 {
GameState create_foundation_state(const InitialState &initial) {
    GameState result{};
    static_cast<InitialState &>(result) = initial;
    return result;
}
PartyMembers party_members(const PartyState &p) {
    PartyMembers result{};
    for (uint8_t i = 0; i < p.character_count && i < kRosterCapacity; ++i) {
        if (p.characters[i].party_status == 0) {
            result.indices[result.count++] = i;
            if (result.count == kMaxParty) break;
        }
    }
    return result;
}
int32_t first_conscious_index(const PartyState &p) {
    for (int32_t i = 0; i < p.party_size && i < kMaxParty && i < p.character_count; ++i) {
        if (p.characters[i].status == 'G' || p.characters[i].status == 'P') return i;
    }
    return -1;
}
}
