#pragma once
#include "time.h"
#include "rng.h"
namespace openu5 {
constexpr uint8_t kMaxParty = 6, kRosterCapacity = 16;
struct CharacterState {
    char name[10]{}; // saveNative.ts: nine byte name field plus terminator.
    uint8_t gender = 0;
    char character_class = 0, status = 0;
    uint8_t strength = 0, dexterity = 0, intelligence = 0, current_mp = 0;
    uint16_t current_hp = 0, max_hp = 0, exp = 0;
    uint8_t level = 0, months_at_inn = 0;
    uint8_t helmet = 0, armor = 0, weapon = 0, shield = 0, ring = 0, amulet = 0;
    uint8_t party_status = 0xff;
};
struct PartyState {
    CharacterState characters[kRosterCapacity]{};
    uint8_t character_count = 0; // Short synthetic rosters differ from empty records.
    int32_t party_size = 0;
    uint8_t active_character = 0xff;
};
// Foundation projection, NOT a complete save or complete TS GameState.
struct InitialState {
    PartyState party{};
    GameTime time{};
    WorldPosition position{};
    int64_t turns_since_start = 0; // Unsaturated TS counter; integer domain <= 2^53-1.
    uint16_t food = 0, gold = 0, torch_turns = 0;
    // Shared party pack. Counts are not bytes: unequipSlot has no 99/255 cap.
    // 0..47 are canonical; the byte-ID tail preserves malformed equipped IDs,
    // including the reference's Ready(255) empty-slot toggle behavior.
    int32_t equipment_quantities[256]{};
    int32_t scroll_quantities[8]{}, potion_quantities[8]{}, reagent_quantities[8]{};
    int32_t spell_quantities[48]{};
    bool worn_crown = false;
    int32_t keys = 0, gems = 0, torches = 0, skull_keys = 0, magic_carpets = 0;
    int32_t ship_hull = 99; // undefined shipHull defaults to 99 in campRepairShip.
    bool grapple = false;
    uint8_t dungeon_rooms_cleared[14]{};
    int32_t ship_skiffs = 0;
    bool wooden_box = false;
    uint8_t karma = 0;
    uint16_t equipment_count = 48; // Logical TS array length, distinct from reserved storage.
    bool sextant = false, spyglass = false, black_badge = false;
    uint32_t npc_met[32]{}, npc_dead[32]{}; // TS npcMet/npcDead rows, 32 slots/location.
};
struct GameState : InitialState {
    uint8_t version = 1;
    TransportMode transport = TransportMode::Foot;
    OriginalRng rng{}; // Game.liveRng lives beside state in TS; explicit here for replay.
};
inline void extend_equipment(GameState &g, int32_t index) {
    if (index >= 0 && index < 256 && index >= g.equipment_count)
        g.equipment_count = uint16_t(index+1);
}
GameState create_foundation_state(const InitialState &initial);
struct PartyMembers { uint8_t indices[kMaxParty]{}; uint8_t count = 0; };
PartyMembers party_members(const PartyState &party);
int32_t first_conscious_index(const PartyState &party);
}
