#pragma once
#include "turn.h"
namespace openu5 {
constexpr uint8_t kEquipmentNothing = 255;
using ItemId = uint8_t;
using ItemQuantity = int32_t; // Finite integer domain; validate before codecs.
struct InventoryEntry {
    ItemQuantity quantity = 0;
    ItemId id = 0;
};
enum class EquipSlot : uint8_t { Helmet, Armor, Weapon, Shield, Ring, Amulet, None };
struct ItemResult {
    bool ok = false, vanished = false, removed = false;
    const char *message = "";
};
uint8_t equip_type_of(int32_t id);
EquipSlot slot_for_equip(int32_t id);
uint8_t hand_state(const CharacterState &);
int32_t total_equipped_weight(const CharacterState &);
bool is_item_equipped(const CharacterState &, int32_t id);
int32_t ammo_item_for(int32_t id); // -1 = no dedicated ammunition
bool is_thrown_weapon(int32_t id);
ItemResult equip_item(GameState &, int32_t member, int32_t id, const Rand *rand = nullptr,
                      bool battle = false);
ItemResult unequip_slot(GameState &, int32_t member, EquipSlot);
EquipSlot unequip_item_by_id(GameState &, int32_t member, int32_t id,
                             bool attack_slots_only = false);
struct ReadyItems {
    uint8_t ids[48]{};
    uint8_t count = 0;
};
ReadyItems ready_items(const GameState &, int32_t member);
struct RingExpiries {
    uint8_t members[16]{}, ids[16]{};
    uint8_t count = 0;
};
RingExpiries roll_ring_expiry(const GameState &, Rand);
ItemResult ignite_torch(GameState &, Rand, int32_t effective_location);
struct PotionResult {
    ItemResult result{};
    int32_t effective_color = -1;
    bool reveal = false;
};
int32_t reroll_potion_color(int32_t color, Rand);
PotionResult apply_potion_effect(CharacterState &, int32_t color, Rand, int32_t location);
// Consumption is a separate step: canceling target selection still loses it.
bool consume_potion(GameState &, int32_t color);
int32_t add_capped(int32_t value, int32_t amount, int32_t cap = 99);
} // namespace openu5
