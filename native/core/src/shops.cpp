#include "openu5/shops.h"
#include <algorithm>
#include <iterator>
namespace openu5 {
namespace {
#include "shop_tables.inc"
template <size_t N> int32_t at(const int32_t (&a)[N], int32_t i) {
    return i >= 0 && size_t(i) < N ? a[i] : 0;
}
template <size_t N> int32_t index(const int32_t (&a)[N], int32_t v) {
    for (size_t i = 0; i < N; ++i)
        if (a[i] == v)
            return int32_t(i);
    return -1;
}
ShopResult invalid() { return {false, "", ShopFailure::InvalidInput}; }
ShopResult range() { return {false, "", ShopFailure::NumericRange}; }
bool gold_fits(const GameState &g, int32_t price) {
    const int64_t n = int64_t(g.gold) - price;
    return n >= 0 && n <= 65535;
}
void pay(GameState &g, int32_t price) { g.gold = uint16_t(int64_t(g.gold) - price); }
bool member(const GameState &g, int32_t i) { return i >= 0 && i < g.party.character_count; }
void mp(CharacterState &c) {
    if (c.character_class == 'A' || c.character_class == 'M')
        c.current_mp = c.intelligence;
    else if (c.character_class == 'B')
        c.current_mp = uint8_t(c.intelligence / 2);
}
} // namespace
int32_t shop_town_index(ShopType type, int32_t loc) {
    switch (type) {
#define T(n)                                                                                       \
    case ShopType::n:                                                                              \
        return index(towns_##n, loc)
        T(Blacksmith);
        T(Barkeeper);
        T(HorseSeller);
        T(Shipwright);
        T(MagicSeller);
        T(GuildMaster);
        T(Healer);
        T(InnKeeper);
#undef T
    }
    return -1;
}
const ShopRecord *shop_lookup(const ShopData &d, int32_t loc, ShopType type) {
    for (size_t i = 0; d.records && i < d.record_count; ++i)
        if (d.records[i].location == loc && d.records[i].type == type)
            return d.records + i;
    return nullptr;
}
bool shop_is_open(const uint8_t (&times)[4], uint8_t hour) {
    return (schedule_index(times, hour) & 1) != 0;
}
// int64 intermediate preserves JS exact integer products over the byte-stat/word-base domain.
int32_t shop_buy_price(int32_t base, uint8_t intel) {
    return int32_t(base + int64_t(base) * (100 - 3 * int32_t(intel)) / 100);
}
int32_t shop_sell_price(int32_t base, uint8_t intel) {
    return int32_t(int64_t(3) * intel * base / 100 + 1);
}
int32_t guild_price(int32_t town, int32_t item, uint8_t intel) {
    return shop_buy_price(
        town >= 0 && town < 4 && item >= 0 && item < 3 ? GUILD_PRICES[town][item] : 0, intel);
}
int32_t ration_price(int32_t town, uint8_t intel) {
    return shop_buy_price(at(RATION_BASE, town), intel);
}
int32_t horse_price(int32_t town, uint8_t intel) {
    return shop_buy_price(at(HORSE_PRICES, town), intel);
}
int32_t ship_price(int32_t town, bool frigate, uint8_t intel) {
    return shop_buy_price(frigate ? at(FRIGATE_PRICES, town) : at(SKIFF_PRICES, town), intel);
}
int32_t wine_price(int32_t item) { return at(WINE_PRICES, item); }
InnInfo inn_at(int32_t loc) {
    auto i = shop_town_index(ShopType::InnKeeper, loc);
    if (i < 0)
        return {};
    return {true, i, INN_RATE[i], INN_CAPACITY[i], INN_ROOM_X[i], INN_ROOM_Y[i]};
}
int32_t inn_guest_count(const GameState &g, int32_t loc) {
    int32_t n = 0;
    for (int i = 0; i < g.party.character_count; ++i)
        if (g.party.characters[i].party_status == loc)
            ++n;
    return n;
}
int32_t inn_rest_price(InnInfo i, int32_t n, uint8_t intel) {
    return shop_buy_price(i.rate * n, intel);
}
int32_t inn_monthly_rate(InnInfo i, uint8_t intel) { return shop_buy_price(i.rate * 10, intel); }
int32_t inn_pickup_price(InnInfo i, uint8_t intel, int32_t months) {
    return inn_monthly_rate(i, intel) * std::max<int32_t>(1, months);
}
ShopResult buy_equipment(GameState &g, int32_t item, int32_t price) {
    if (item < 0 || item >= 256)
        return invalid();
    const auto qty = item < g.equipment_count ? g.equipment_quantities[item] : 0;
    if (qty >= 99)
        return {false, msg_buyFull, ShopFailure::Cap};
    if (g.gold < price)
        return {false, "", ShopFailure::Gold};
    if (!gold_fits(g, price))
        return range();
    pay(g, price);
    g.equipment_quantities[item] = (item == 0x1b || item == 0x1d) ? 99 : std::min<int32_t>(99, qty + 1);
    extend_equipment(g, item);
    return {true, msg_buySold};
}
ShopResult sell_equipment(GameState &g, int32_t item, int32_t price) {
    if (item < 0 || item >= 256)
        return invalid();
    if (item == 0x1b || item == 0x1d || item >= g.equipment_count ||
        g.equipment_quantities[item] <= 0)
        return {};
    const auto gold = std::min<int64_t>(9999, int64_t(g.gold) + price);
    if (gold < 0)
        return range();
    --g.equipment_quantities[item];
    g.gold = uint16_t(gold);
    return {true, msg_sellDealYes};
}
ShopResult buy_reagent(GameState &g, int32_t item, int32_t qty, int32_t price) {
    if (item < 0 || item >= 8)
        return invalid();
    if (qty <= 0)
        return {false, "Buy how many?"};
    if (g.reagent_quantities[item] >= 99)
        return {false, msg_reagentFull};
    if (g.gold < price)
        return {false, "", ShopFailure::Gold};
    if (!gold_fits(g, price))
        return range();
    pay(g, price);
    g.reagent_quantities[item] =
        int32_t(std::min<int64_t>(99, int64_t(g.reagent_quantities[item]) + qty));
    return {true, msg_reagentThanks};
}
ShopResult buy_guild_item(GameState &g, int32_t town, int32_t item, uint8_t intel) {
    if (item < 0 || item >= 3)
        return invalid();
    auto price = guild_price(town, item, intel);
    if (g.gold < price)
        return {false, "", ShopFailure::Gold};
    if (!gold_fits(g, price))
        return range();
    pay(g, price);
    auto &q = item == 0 ? g.keys : item == 1 ? g.gems : g.torches;
    q = int32_t(std::min<int64_t>(99, int64_t(q) + GUILD_GRANT[item]));
    return {true, msg_guildSold};
}
ShopResult buy_horse(GameState &g, int32_t town, uint8_t intel) {
    auto price = horse_price(town, intel);
    if (g.gold < price)
        return {false, msg_horseBroke};
    if (!gold_fits(g, price))
        return range();
    pay(g, price);
    return {true, msg_horseYesExcl};
}
ShopResult buy_ship(GameState &g, int32_t town, bool frigate, uint8_t intel) {
    auto price = ship_price(town, frigate, intel);
    if (g.gold < price)
        return {false, "", ShopFailure::Gold};
    if (!gold_fits(g, price))
        return range();
    pay(g, price);
    ShopResult r{true, "She awaits thee at the dock!"};
    r.has_coordinates = town >= 0 && town < 4;
    r.x = at(SHIP_DOCK_X, town);
    r.y = at(SHIP_DOCK_Y, town);
    return r;
}
ShopResult buy_wine(GameState &g, int32_t item) {
    auto p = wine_price(item);
    if (g.gold < p)
        return {false, "", ShopFailure::Gold};
    pay(g, p);
    return {true, msg_wineEnjoy};
}
ShopResult buy_rations(GameState &g, int32_t town, uint8_t intel, int32_t qty) {
    auto price = ration_price(town, intel);
    ShopResult r;
    if (price < 0 && qty > 0) {
        const int64_t units =
            std::min<int64_t>(qty, std::max<int32_t>(1, (9999 - int32_t(g.food) + 24) / 25));
        if (int64_t(g.gold) - units * price > 65535)
            return range();
    }
    while (r.bought < qty && g.gold >= price) {
        if (!gold_fits(g, price)) {
            r.reason = ShopFailure::NumericRange;
            return r;
        }
        pay(g, price);
        g.food = uint16_t(std::min<int32_t>(9999, int32_t(g.food) + 25));
        ++r.bought;
        if (g.food == 9999) {
            r.full = true;
            break;
        }
    }
    r.ok = r.bought > 0;
    r.message = r.ok ? "Anything else?" : "Hrumph.";
    return r;
}
ShopResult healer_heal(GameState &g, int32_t i, HealerService service, int32_t price) {
    if (!member(g, i))
        return {false, "No such person."};
    auto &c = g.party.characters[i];
    bool needs = service == HealerService::Heal   ? c.current_hp < c.max_hp
                 : service == HealerService::Cure ? c.status == 'P'
                                                  : c.status == 'D';
    if (!needs)
        return {false, "Thou hast no need of this art!"};
    if (g.gold < price)
        return {false, "", ShopFailure::Gold};
    if (!gold_fits(g, price))
        return range();
    pay(g, price);
    if (service == HealerService::Heal)
        c.current_hp = c.max_hp;
    else {
        c.status = 'G';
        if (service == HealerService::Resurrect)
            c.current_hp = 1;
    }
    return {true, "It is done."};
}
ShopResult inn_rest(GameState &g, int32_t buyer, int32_t loc) {
    auto inn = inn_at(loc);
    if (!inn.present)
        return {false, "There is no inn here."};
    if (inn.capacity <= inn_guest_count(g, loc))
        return {false, "I have no room available!"};
    if (!member(g, buyer))
        return {false, "No such person."};
    auto price = inn_rest_price(inn, g.party.party_size, g.party.characters[buyer].intelligence);
    if (g.gold < price)
        return {false, "\n\n\"Highwaymen!\nCheap, at that!\nOUT!\" screams\n$.\n"};
    if (!gold_fits(g, price))
        return range();
    pay(g, price);
    for (int i = 0; i < g.party.character_count; ++i) {
        auto &c = g.party.characters[i];
        if (c.party_status != 0 || c.status == 'D')
            continue;
        c.current_hp = c.max_hp;
        mp(c);
        if (c.status == 'P') {
            c.status = 'D';
            c.current_hp = 0;
        } else if (c.status == 'S')
            c.status = 'G';
    }
    ShopResult r{true, "Morning!"};
    r.has_coordinates = true;
    r.x = inn.x;
    r.y = inn.y;
    return r;
}
ShopResult inn_leave(GameState &g, int32_t i, int32_t loc) {
    auto &p = g.party;
    if (p.party_size <= 1)
        return {false, "One must first be left behind!"};
    if (i == 0)
        return {false, "Thy friend will not leave thee!"};
    if (!member(g, i) || p.characters[i].party_status != 0)
        return {false, "That one is not in thy party."};
    if (loc < 0 || loc > 255)
        return invalid();
    if (p.active_character == i)
        p.active_character = 255;
    else if (p.active_character != 255 && i < p.active_character)
        --p.active_character;
    auto rec = p.characters[i];
    rec.party_status = uint8_t(loc);
    rec.months_at_inn = 0;
    for (int j = i; j + 1 < p.character_count; ++j)
        p.characters[j] = p.characters[j + 1];
    p.characters[p.character_count - 1] = rec;
    --p.party_size;
    return {true, "It shall be done."};
}
ShopResult inn_pickup(GameState &g, int32_t buyer, int32_t i, int32_t loc) {
    auto &p = g.party;
    if (p.party_size >= 6)
        return {false, "One must first be left behind!"};
    auto inn = inn_at(loc);
    if (!inn.present)
        return {false, "There is no inn here."};
    if (!member(g, buyer) || !member(g, i) || p.characters[i].party_status != loc)
        return {false, "No one here is from thy party!"};
    if (p.party_size < 0 || p.party_size >= p.character_count)
        return invalid();
    auto price =
        inn_pickup_price(inn, p.characters[buyer].intelligence, p.characters[i].months_at_inn);
    if (g.gold < price)
        return {false, "", ShopFailure::Gold};
    if (!gold_fits(g, price))
        return range();
    pay(g, price);
    auto rec = p.characters[i];
    rec.months_at_inn = 0;
    for (int j = i; j + 1 < p.character_count; ++j)
        p.characters[j] = p.characters[j + 1];
    const int dest = p.party_size;
    for (int j = p.character_count - 1; j > dest; --j)
        p.characters[j] = p.characters[j - 1];
    rec.party_status = 0;
    bool died = rec.status == 'P';
    if (died) {
        rec.status = 'D';
        rec.current_hp = 0;
    }
    mp(rec);
    p.characters[dest] = rec;
    ++p.party_size;
    ShopResult r{true, died ? "Thy friend has died, by the way.\"\n"
                            : "I hope thou hast found thy stay enjoyable,\"\n"};
    r.died = died;
    return r;
}
void inn_night_pass(GameState &g, TurnState &t, Rand rand, TurnHook tiles, const SkyRefresh *sky) {
    for (int i = 0; i < 12; ++i)
        advance_clock(g, t, 5, &rand, sky);
    for (int step = 0; g.time.hour != 6 && step < 1000; ++step) {
        for (int i = 0; i < std::min<int32_t>({g.party.party_size, int32_t(g.party.character_count), 6});
             ++i) {
            auto &c = g.party.characters[i];
            if (c.status != 'D' && c.ring == 0x2c && rand(0, 7) == 7)
                c.current_hp = uint16_t(std::min(int32_t(c.current_hp) + 1, int32_t(c.max_hp)));
        }
        advance_clock(g, t, 9, &rand, sky);
        if (g.time.hour == 20 || g.time.hour == 5)
            tiles();
    }
}
int32_t shop_post_purchase_drain(GameState &g, int32_t shadowlord, Rand rand) {
    if (shadowlord != 0)
        return 0;
    auto amount = std::min<int32_t>(g.gold, rand(1, 64));
    g.gold = uint16_t(g.gold - amount);
    return amount;
}
const char *tavern_alive_word(int32_t n) {
    static constexpr const char *words[] = {"", "one", "two", "three", "four", "five", "six"};
    return n >= 0 && n <= 6 ? words[n] : "";
}
TavernResult buy_tavern_round(GameState &g, int32_t town, int32_t north, int32_t south,
                              bool house) {
    TavernResult r;
    for (int i = 0; i < g.party.character_count; ++i)
        if (g.party.characters[i].party_status == 0 && g.party.characters[i].status != 'D')
            ++r.living;
    r.cost = (house ? 1 : at(TAVERN_ROUND_PRICE, town)) * r.living;
    r.sir = g.party.character_count && g.party.characters[0].gender == 0x0b;
    if (g.gold < r.cost) {
        r.message = "CAN'T PAY? Beat it!";
        return r;
    }
    pay(g, r.cost);
    r.ok = true;
    r.message = msg_roundEnjoy;
    if (house || r.living == 0) {
        r.counts_as_service = true;
        return r;
    }
    g.food = uint16_t(std::min<int32_t>(9999, int32_t(g.food) + r.living));
    if (north == 0x95) {
        r.plate_dy = -1;
        r.plate_tile = 0x9b;
    } else if (south == 0x95) {
        r.plate_dy = 1;
        r.plate_tile = 0x9a;
    }
    return r;
}
ShopResult pay_rumor(GameState &g, int32_t i) {
    if (i < 0 || i >= 26 || g.gold < RUMOR_PRICES[i])
        return {};
    pay(g, RUMOR_PRICES[i]);
    ShopResult r{true, ""};
    r.subject = RUMOR_SUBJECTS[i];
    r.place = RUMOR_PLACES[RUMOR_GOSSIP_MAP[i]];
    return r;
}
} // namespace openu5
