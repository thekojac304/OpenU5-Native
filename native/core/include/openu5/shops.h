#pragma once
#include "turn.h"
namespace openu5 {
enum class ShopType : uint8_t {
    Blacksmith,
    Barkeeper,
    HorseSeller,
    Shipwright,
    MagicSeller,
    GuildMaster,
    Healer,
    InnKeeper
};
enum class ShopFailure : uint8_t { None, Cap, Gold, InvalidInput, NumericRange };
enum class HealerService : uint8_t { Heal, Cure, Resurrect };
struct ShopResult {
    bool ok = false;
    const char *message = "";
    ShopFailure reason = ShopFailure::None;
    int32_t bought = 0;
    bool full = false, died = false, has_coordinates = false;
    int32_t x = 0, y = 0;
    const char *subject = nullptr, *place = nullptr;
};
struct InnInfo {
    bool present = false;
    int32_t town = -1, rate = 0, capacity = 0, x = 0, y = 0;
};
struct ShopNumbers {
    const int32_t *values = nullptr;
    size_t count = 0;
    int32_t at(int32_t i) const { return values && i >= 0 && size_t(i) < count ? values[i] : 0; }
};
struct ShopRecord {
    int32_t location = 0;
    ShopType type{};
    int32_t index = 0;
    const char *name = "", *keeper = "";
};
struct ShopData {
    const ShopRecord *records = nullptr;
    size_t record_count = 0;
    ShopNumbers equipment_prices{}, weapons{}, reagent_prices{}, reagent_grants{}, heal_prices{},
        cure_prices{}, resurrect_prices{};
};
const ShopRecord *shop_lookup(const ShopData &, int32_t location, ShopType);
int32_t shop_town_index(ShopType, int32_t location);
bool shop_is_open(const uint8_t (&times)[4], uint8_t hour);
int32_t shop_buy_price(int32_t base, uint8_t intelligence);
int32_t shop_sell_price(int32_t base, uint8_t intelligence);
int32_t guild_price(int32_t town, int32_t item, uint8_t intelligence);
int32_t ration_price(int32_t town, uint8_t intelligence);
int32_t ship_price(int32_t town, bool frigate, uint8_t intelligence);
int32_t horse_price(int32_t town, uint8_t intelligence);
int32_t wine_price(int32_t item);
InnInfo inn_at(int32_t location);
int32_t inn_guest_count(const GameState &, int32_t location);
int32_t inn_rest_price(InnInfo, int32_t party_size, uint8_t intelligence);
int32_t inn_monthly_rate(InnInfo, uint8_t intelligence);
int32_t inn_pickup_price(InnInfo, uint8_t intelligence, int32_t months);
ShopResult buy_equipment(GameState &, int32_t item, int32_t price);
ShopResult sell_equipment(GameState &, int32_t item, int32_t price);
ShopResult buy_reagent(GameState &, int32_t item, int32_t quantity, int32_t price);
ShopResult buy_guild_item(GameState &, int32_t town, int32_t item, uint8_t intelligence);
ShopResult buy_horse(GameState &, int32_t town, uint8_t intelligence);
ShopResult buy_ship(GameState &, int32_t town, bool frigate, uint8_t intelligence);
ShopResult buy_wine(GameState &, int32_t item);
ShopResult buy_rations(GameState &, int32_t town, uint8_t intelligence, int32_t quantity);
ShopResult healer_heal(GameState &, int32_t member, HealerService, int32_t price);
ShopResult inn_rest(GameState &, int32_t buyer, int32_t location);
ShopResult inn_leave(GameState &, int32_t member, int32_t location);
ShopResult inn_pickup(GameState &, int32_t buyer, int32_t member, int32_t location);
void inn_night_pass(GameState &, TurnState &, Rand, TurnHook hour_tiles = {},
                    const SkyRefresh *sky = nullptr);
int32_t shop_post_purchase_drain(GameState &, int32_t shadowlord_here, Rand);
struct TavernResult : ShopResult {
    int32_t cost = 0, living = 0, plate_dy = 0, plate_tile = 0;
    bool sir = false, counts_as_service = false;
};
TavernResult buy_tavern_round(GameState &, int32_t town, int32_t north = 0, int32_t south = 0,
                              bool house = false);
const char *tavern_alive_word(int32_t living);
ShopResult pay_rumor(GameState &, int32_t index);
} // namespace openu5
