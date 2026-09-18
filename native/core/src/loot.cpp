#include "openu5/loot.h"
#include <algorithm>
#include <cstdio>
namespace openu5 {
#include "loot_names.inc"
ChestTrap chest_trap(GameState &g, int loc, int opener, Rand rand) {
    static constexpr int types[] = {0, 0, 0, 1, 1, 2, 2, 3};
    static const char *names[] = {"ACID!", "POISON!", "BOMB!", "GAS!"};
    int type = loc > 127 ? rand(0, 1) : types[rand(0, 7)];
    ChestTrap r{names[type], 0};
    int count = std::min<int>(6, std::min<int>(g.party.party_size, g.party.character_count));
    auto damage = [&](int i, int v) {
        auto &c = g.party.characters[i];
        c.current_hp = uint16_t(std::max<int>(0, c.current_hp - v));
        if (!c.current_hp)
            c.status = 'D';
    };
    if (type == 0) {
        int v = rand(0, 60) >> 1;
        if (!v)
            v = 1;
        if (opener >= 0 && opener < g.party.character_count) {
            damage(opener, v);
            if (opener < 6)
                r.damage_mask = uint8_t(1 << opener);
        }
    }
    if (type == 1 && opener >= 0 && opener < count && g.party.characters[opener].status != 'D')
        g.party.characters[opener].status = 'P';
    if (type >= 2)
        for (int i = 0; i < count; ++i)
            if (g.party.characters[i].status != 'D') {
                if (type == 2) {
                    r.damage_mask |= uint8_t(1 << i);
                    damage(i, rand(1, 8));
                } else
                    g.party.characters[i].status = 'P';
            }
    return r;
}
void chest_loot(int contents, Rand rand, LootSink sink) {
    static constexpr int items[] = {1, 2, 3, 4, 7, 8, 13, 15},
                         guards[] = {25, 3, 17, 17, 9, 15, 7, 7},
                         maxqty[] = {10, 90, 8, 8, 2, 2, 2, 2};
    static constexpr int ri[] = {9, 9, 9, 9, 6, 6, 6, 6, 6, 11, 11, 11, 11, 11, 11, 11,
                                 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,  5,  5,  5,  5,  5,  5,
                                 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,  10, 10, 10, 12, 12, 12};
    static constexpr int rg[] = {10, 10, 15, 20,  10,  15,  20, 28, 255, 15, 15, 20,
                                 20, 20, 24, 255, 5,   10,  10, 10, 10,  10, 10, 10,
                                 15, 15, 15, 10,  15,  10,  20, 20, 20,  20, 20, 255,
                                 23, 23, 23, 255, 255, 255, 23, 23, 23,  23, 15, 255};
    auto grant = [&](int id, int base) {
        int qty = id == 1              ? rand(1, contents)
                  : id == 2            ? rand(1, 3 * contents)
                  : id == 3 || id == 4 ? base - 1
                                       : base;
        if (sink.grant)
            sink.grant(sink.context, {id, qty});
    };
    for (int i = 7; i >= 0; --i) {
        if (guards[i] > contents || guards[i] > rand(1, 30))
            continue;
        int base = maxqty[i] == 1 ? 1 : rand(1, maxqty[i]);
        grant(items[i], base);
    }
    for (int i = 0; i < (contents >> 1) + 1; ++i) {
        int n = rand(0, 47);
        if (rg[n] > contents || rg[n] > rand(1, 30))
            continue;
        grant(ri[n], n);
    }
}
Error dungeon_chest_loot(int floor, Rand rand, LootGrant (&out)[7], uint8_t &count) {
    static constexpr int guards[] = {2, 4, 5, 10, 20, 25, 25}, maxqty[] = {31, 0, 3, 3, 3, 7, 7},
                         items[] = {15, 2, 7, 8, 13, 255, 255};
    count = 0;
    for (int i = 0; i < 7; ++i) {
        if (guards[i] > rand(1, floor * 4 + 4))
            continue;
        if (i == 5 || i == 6)
            out[count++] = {i == 5 ? 3 : 4, rand(0, 7)};
        else if (i == 1) {
            if (floor == 0)
                return Error::InvalidRange;
            out[count++] = {2, rand(1, floor * 8)};
        } else
            out[count++] = {items[i], rand(1, maxqty[i])};
    }
    return Error::None;
}
DecodedLootItem decode_loot(LootGrant v) {
    DecodedLootItem out{}; out.quantity=v.quantity;
    switch(v.id){
    case 1: out.category=LootCategory::Chest; break;
    case 2: out.category=LootCategory::Gold; break;
    case 3: out.category=LootCategory::Potion; out.item_index=v.quantity; out.quantity=1; break;
    case 4: out.category=LootCategory::Scroll; out.item_index=v.quantity&7; out.quantity=1; break;
    case 5: case 6: case 9: case 10: case 11: case 12:
        out.category=LootCategory::Equipment; out.item_index=v.quantity; out.quantity=v.quantity==27||v.quantity==29?5:1; break;
    case 7: out.category=LootCategory::Keys; out.quantity=v.quantity&127; break;
    case 8: out.category=LootCategory::Gems; break;
    case 13: out.category=LootCategory::Torches; break;
    case 14: out.category=LootCategory::QuestItem; out.item_index=14; out.quantity=1; break;
    case 15: out.category=LootCategory::Food; break;
    default: break;
    }
    return out;
}
const char *loot_category_name(LootCategory c){switch(c){
case LootCategory::Chest:return "chest";case LootCategory::Gold:return "gold";
case LootCategory::Potion:return "potion";case LootCategory::Scroll:return "scroll";
case LootCategory::Equipment:return "equipment";case LootCategory::Keys:return "keys";
case LootCategory::Gems:return "gems";case LootCategory::Torches:return "torches";
case LootCategory::Food:return "food";case LootCategory::QuestItem:return "quest";
default:return "none";}}
bool apply_loot_grant(GameState &g, LootGrant v) {
    auto add = [&](int32_t &n, int a) {if(a<=0||n>=99)return false;const auto before=n;n=std::min<int32_t>(99,n+a);return n!=before;};
    int q = v.quantity;
    switch (v.id) {
    case 2:
        if(q<=0||g.gold>=9999)return false;
        g.gold = uint16_t(std::min(9999, int(g.gold) + q));
        return true;
    case 7:
        return add(g.keys, q&127);
    case 8:
        return add(g.gems, q);
    case 13:
        return add(g.torches, q);
    case 15:
        if(q<=0||g.food>=9999)return false;
        g.food = uint16_t(std::min(9999, int(g.food) + q));
        return true;
    case 3:
        if (q < 0 || q >= 8) return false;
        return add(g.potion_quantities[q], 1);
    case 4:
        if (q < 0 || q > 255) return false;
        return add(g.scroll_quantities[q & 7], 1);
    case 5:
    case 6:
    case 9:
    case 10:
    case 11:
    case 12:
        if (q < 0 || q >= 256) return false;
        if(!add(g.equipment_quantities[q], q == 27 || q == 29 ? 5 : 1))return false;
        extend_equipment(g, q); return true;
    case 14:
        if(g.wooden_box)return false;
        g.wooden_box = true; return true;
    default:
        return false;
    }
    return false;
}
const char *loot_open_line(int id) {
    static const char *names[] = {
        "Nothing of note.", "a chest!",      "a sack of gold!",  "a potion!",
        "a scroll!",        "a weapon!",     "a shield!",        "a ring of keys!",
        "a gem!",           "a helm!",       "a ring!",          "some armour!",
        "an amulet!",       "some torches!", "Nothing of note.", "some food!"};
    return id >= 0 && id < 16 ? names[id]
           : id == 25         ? "a strange rock!"
           : id == 30         ? "a rotting body!"
           : id == 31         ? "a moldy corpse!"
                              : "Nothing of note.";
}
void loot_item_name(LootGrant v, char *b, size_t size) {
    int q = v.quantity;
    const char *s = "An item!";
    static const char *colors[] = {"blue",   "yellow", "red",   "green",
                                   "orange", "purple", "black", "white"},
                      *scrolls[] = {"VL", "RH", "IS", "IA", "IQW", "KXC", "IMC", "AT"};
    switch (v.id) {
    case 2:
        std::snprintf(b, size, "%d gold!", q);
        return;
    case 7:
        std::snprintf(b, size, "%d%s %s!", q & 127, q > 127 ? " odd" : "",
                      (q & 127) == 1 ? "key" : "keys");
        return;
    case 8:
        std::snprintf(b, size, "%d %s!", q, q == 1 ? "gem" : "gems");
        return;
    case 13:
        std::snprintf(b, size, "%d %s!", q, q == 1 ? "torch" : "torches");
        return;
    case 15:
        std::snprintf(b, size, "%d food!", q);
        return;
    case 14:
        s = "A sandalwood box!";
        break;
    case 25:
        s = "A moonstone!";
        break;
    case 1:
        s = "Open it first!";
        break;
    case 3:
        std::snprintf(b, size, "A %s potion!", colors[q & 7]);
        return;
    case 4:
        if (q == 255)
            s = "The plans for the HMS Cape!";
        else {
            std::snprintf(b, size, "A scroll: %s!", scrolls[q & 7]);
            return;
        }
        break;
    case 5:
    case 6:
    case 9:
    case 10:
    case 11:
    case 12:
        if (q >= 0 && q < int(sizeof(kLootNames) / sizeof(*kLootNames))) {
            std::snprintf(b, size, "%s!", kLootNames[q]);
            return;
        }
        break;
    default:
        break;
    }
    std::snprintf(b, size, "%s", s);
}
} // namespace openu5

