#include "openu5/inventory.h"
namespace openu5 {
namespace {
constexpr uint8_t types[48] = {128, 128, 128, 128, 32, 32, 32, 32, 32, 64, 64, 64, 64, 64, 64, 64,
                               32,  48,  32,  48,  32, 32, 32, 32, 32, 32, 48, 0,  48, 0,  32, 48,
                               48,  48,  48,  48,  48, 32, 32, 32, 32, 48, 2,  2,  2,  4,  4,  4};
constexpr uint8_t weights[48] = {0,  1,  2,  3, 2, 3, 4, 0, 0, 0, 2, 4, 6, 10, 12, 0,
                                 1,  2,  3,  2, 3, 4, 6, 5, 7, 8, 8, 0, 6, 0,  9,  16,
                                 15, 13, 18, 0, 0, 8, 0, 5, 0, 0, 0, 0, 0, 0,  0,  0};
uint8_t CharacterState::*const slots[] = {&CharacterState::helmet, &CharacterState::armor,
                                          &CharacterState::weapon, &CharacterState::shield,
                                          &CharacterState::ring,   &CharacterState::amulet};
bool member_valid(const GameState &g, int32_t n) {
    return n >= 0 && n < g.party.character_count && n < 16;
}
int32_t weight(int32_t id) { return id >= 0 && id < 48 ? weights[id] : 0; }
ItemResult fail(const char *s) { return {false, false, false, s}; }
} // namespace
uint8_t equip_type_of(int32_t id) { return id >= 0 && id < 48 ? types[id] : 0; }
EquipSlot slot_for_equip(int32_t id) {
    switch (equip_type_of(id)) {
    case 128:
        return EquipSlot::Helmet;
    case 64:
        return EquipSlot::Armor;
    case 2:
        return EquipSlot::Ring;
    case 4:
        return EquipSlot::Amulet;
    case 32:
    case 48:
        return id >= 16 ? EquipSlot::Weapon : EquipSlot::Shield;
    default:
        return EquipSlot::None;
    }
}
uint8_t hand_state(const CharacterState &c) {
    if (c.weapon == 255 && c.shield == 255)
        return 2;
    if (c.weapon == 255)
        return 0;
    if (c.shield == 255 && equip_type_of(c.weapon) != 48)
        return 1;
    return 255;
}
int32_t total_equipped_weight(const CharacterState &c) {
    int32_t n = 0;
    for (auto s : slots)
        n += weight(c.*s);
    return n;
}
bool is_item_equipped(const CharacterState &c, int32_t id) {
    for (auto s : slots)
        if (c.*s == id)
            return true;
    return false;
}
int32_t ammo_item_for(int32_t id) { return id == 26 || id == 36 ? 27 : id == 28 ? 29 : -1; }
bool is_thrown_weapon(int32_t id) { return id == 16 || id == 21 || id == 22; }
EquipSlot unequip_item_by_id(GameState &g, int32_t n, int32_t id, bool attack) {
    if (!member_valid(g, n))
        return EquipSlot::None;
    auto &c = g.party.characters[n];
    for (uint8_t i = 0; i < 6; ++i) {
        if (attack && i != 0 && i != 2 && i != 3)
            continue;
        if (c.*slots[i] == id) {
            c.*slots[i] = 255;
            return EquipSlot(i);
        }
    }
    return EquipSlot::None;
}
ItemResult unequip_slot(GameState &g, int32_t n, EquipSlot s) {
    if (!member_valid(g, n))
        return fail("No such character.");
    if (static_cast<uint8_t>(s) >= 6)
        return fail("Invalid equipment slot.");
    auto &v = g.party.characters[n].*slots[static_cast<uint8_t>(s)];
    if (v == 255)
        return fail("Nothing equipped.");
    ++g.equipment_quantities[v];
    v = 255;
    return {true, false, false, ""};
}
ItemResult equip_item(GameState &g, int32_t n, int32_t id, const Rand *rand, bool battle) {
    if (!member_valid(g, n))
        return fail("No such character.");
    auto &c = g.party.characters[n];
    if (id == 27 || id == 29)
        return fail("");
    if (id >= 9 && id <= 15 && battle)
        return fail("Thou canst not change armour in heated battle!");
    if (is_item_equipped(c, id)) {
        unequip_item_by_id(g, n, id);
        if (g.equipment_quantities[id] < 99)
            ++g.equipment_quantities[id];
        return {true, false, true, ""};
    }
    if (id < 0 || id > 255 || g.equipment_quantities[id] <= 0)
        return fail("Thou hast none!");
    const auto ammo = ammo_item_for(id);
    if (ammo >= 0 && g.equipment_quantities[ammo] == 0)
        return fail("Thou hast no ammunition for that weapon!");
    EquipSlot s = slot_for_equip(id);
    switch (equip_type_of(id)) {
    case 2:
        if (c.ring != 255)
            return fail("Only one magic ring may be worn at a time!");
        break;
    case 4:
        if (c.amulet != 255)
            return fail("Thou must remove thine other amulet!");
        break;
    case 128:
        if (c.helmet != 255)
            return fail("Remove first thy present helm!");
        break;
    case 64:
        if (c.armor != 255)
            return fail("Thou must first remove thine other armour!");
        break;
    case 32:
        if (hand_state(c) == 255)
            return fail("Thou must free one of thy hands first!");
        s = hand_state(c) == 1 ? EquipSlot::Shield : EquipSlot::Weapon;
        break;
    case 48:
        if (hand_state(c) != 2)
            return fail("Both hands must be free before thou canst wield that!");
        s = EquipSlot::Weapon;
        break;
    default:
        return fail("");
    }
    if (total_equipped_weight(c) + weight(id) > c.strength)
        return fail("Thou art not strong enough!");
    c.*slots[static_cast<uint8_t>(s)] = static_cast<uint8_t>(id);
    --g.equipment_quantities[id];
    if ((id == 42 || id == 44) && rand && (*rand)(0, 15) == 0) {
        c.ring = 255;
        return {true, true, false, "\n\nRing vanishes!\n"};
    }
    return {true, false, false, ""};
}
ReadyItems ready_items(const GameState &g, int32_t n) {
    ReadyItems r;
    for (uint8_t id = 0; id < 48; ++id)
        if (g.equipment_quantities[id] > 0 ||
            (member_valid(g, n) && is_item_equipped(g.party.characters[n], id)))
            r.ids[r.count++] = id;
    return r;
}
RingExpiries roll_ring_expiry(const GameState &g, Rand rand) {
    RingExpiries r;
    for (int32_t i = 0; i < g.party.party_size && i < g.party.character_count && i < 16; ++i) {
        const auto &c = g.party.characters[i];
        if (c.status == 'D' || (c.ring != 42 && c.ring != 44))
            continue;
        if (rand(0, 15) == 11) {
            r.members[r.count] = static_cast<uint8_t>(i);
            r.ids[r.count++] = c.ring;
        }
    }
    return r;
}
ItemResult ignite_torch(GameState &g, Rand rand, int32_t loc) {
    if (g.torches == 0)
        return fail("None owned!");
    --g.torches;
    if (loc >= 33 && loc <= 40) {
        const int32_t n = g.torch_turns + rand(0, 15) + 112;
        g.torch_turns = static_cast<uint16_t>(n > 255 ? 255 : n);
    } else
        g.torch_turns = 240;
    return {true, false, false, ""};
}
int32_t add_capped(int32_t v, int32_t a, int32_t cap) {
    const int64_t sum = int64_t(v) + a;
    return static_cast<int32_t>(sum > cap ? cap : sum);
}
bool consume_potion(GameState &g, int32_t color) {
    if (color < 0 || color >= 8 || g.potion_quantities[color] <= 0)
        return false;
    --g.potion_quantities[color];
    return true;
}
int32_t reroll_potion_color(int32_t color, Rand rand) {
    const auto r = rand(0, 15);
    return r == 0 ? 4 : r == 1 ? rand(0, 7) : color;
}
PotionResult apply_potion_effect(CharacterState &c, int32_t color, Rand rand, int32_t loc) {
    PotionResult out;
    out.effective_color = color;
    auto &r = out.result;
    switch (color) {
    case 0:
        r.ok = c.status == 'S';
        if (r.ok)
            c.status = 'G';
        break;
    case 1:
        if (c.status != 'D') {
            auto heal = rand(0, 60) >> 1;
            if (heal == 0)
                heal = 1;
            const auto before = c.current_hp;
            const auto hp = c.current_hp + heal;
            c.current_hp = uint16_t(hp > c.max_hp ? c.max_hp : hp);
            r.ok = c.current_hp > before;
            if (r.ok)
                r.message = "Healed!";
        }
        break;
    case 2:
        r.ok = c.status == 'P';
        if (r.ok) {
            c.status = 'G';
            r.message = "Poison cured!";
        }
        break;
    case 3:
        r.ok = c.status == 'G';
        if (r.ok) {
            c.status = 'P';
            r.message = "POISONED!";
        }
        break;
    case 4:
        r.ok = c.status == 'G';
        if (r.ok) {
            c.status = 'S';
            r.message = "Slept!";
        }
        break;
    case 5:
    case 6:
        r.ok = loc > 127;
        r.message = r.ok ? (color == 5 ? "Poof!" : "Invisible!") : "\nNo noticeable effect now!";
        break;
    case 7:
        r.ok = loc < 33;
        out.reveal = r.ok;
        if (!r.ok)
            r.message = "\nNo noticeable effect now!";
        break;
    default:
        break;
    }
    return out;
}
} // namespace openu5
