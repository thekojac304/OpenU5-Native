#pragma once
#include "commands.h"
#include "shops.h"
namespace openu5 {
enum class ShopPhase : uint8_t {
    Closed,
    Greeting,
    BlacksmithPause,
    Menu,
    Buy,
    Sell,
    Reagent,
    Guild,
    Ship,
    HealerNeed,
    HealerMember,
    HealerAgain,
    InnAgain,
    Tavern,
    TavernAgain,
    Wine,
    Drunk,
    RationsQuantity,
    RumorText,
    BuyDeal,
    SellDeal,
    ReagentDeal,
    GuildDeal,
    HorseDeal,
    ShipDeal,
    HealerDeal,
    InnRestDeal,
    InnLeaveMember,
    InnLeaveDeal,
    InnPickupMember,
    RumorDeal,
    BuyFull,
    ReagentFull,
    ShipAgain,
    LegacyHeal,
    LegacyCure,
    LegacyResurrect,
    TavernRoundDeal,
    TavernDrinkDeal
};
enum class ShopAction : uint8_t {
    Continue,
    Buy,
    Sell,
    SelectItem,
    SelectMember,
    Heal,
    Cure,
    Resurrect,
    Rest,
    LeaveMember,
    Pickup,
    Round,
    Drink,
    Rations,
    Rumor,
    Confirm,
    Decline,
    Cancel,
    End,
    Text
};
struct ShopInput {
    ShopAction action{};
    int32_t value = 0;
    const char16_t *text = nullptr;
    size_t length = 0;
};
struct ShopSession {
    GameState *owner = nullptr;
    const ShopRecord *record = nullptr;
    ShopType type{};
    ShopPhase phase = ShopPhase::Closed;
    int32_t location = 0, town = -1, item = -1, price = 0, quantity = 0, served = 0;
    HealerService remedy{};
    bool purchased = false, bought_in_buy = false, selling = false, buying = false,
         thrown_out = false, tavern_served = false, transactional_services = false;
    bool legacy_healer = false;
};
enum class ShopEventKind : uint8_t { Entered, State, Result, Exited };
struct ShopEvent {
    ShopEventKind kind{};
    const ShopSession *session = nullptr;
    const ShopResult *result = nullptr;
    const TavernResult *tavern = nullptr; // Price announcement and plate/service metadata.
};
struct ShopOffer {
    int32_t item = 0, price = 0, quantity = 0;
};
// Streams the current transaction's offerings without allocating or changing RNG/state.
void shop_offerings(const GameState &, const ShopSession &, const ShopData &, void *,
                    void (*)(void *, ShopOffer));
// Resolves a zero-based UI row to the authoritative offer identifier.
bool shop_offering_at(const GameState &, const ShopSession &, const ShopData &, size_t,
                      ShopOffer &);
size_t shop_offering_count(const GameState &, const ShopSession &, const ShopData &);
// Read-only price helpers used by transactional frontends before commitment.
int32_t tavern_round_price(const GameState &, int32_t town, bool house_round = false);
// World owners reuse their map/object pools; reserve must precede payment.
struct ShopServices {
    ShopSession &session;
    const ShopData &data;
    void *context = nullptr;
    bool (*record_present)(void *, int32_t) = nullptr;
    const char *(*record)(void *, int32_t) = nullptr;
    int32_t (*tile)(void *, int32_t, int32_t) = nullptr;
    bool (*occupied)(void *, int32_t, int32_t) = nullptr;
    bool (*reserve)(void *, bool ship) = nullptr;
    void (*horse)(void *, int32_t, int32_t) = nullptr;
    void (*ship)(void *, int32_t, int32_t, int32_t tile, int32_t hull, int32_t skiffs) = nullptr;
    void (*plate)(void *, int32_t, int32_t, int32_t) = nullptr;
    void (*hour_tiles)(void *) = nullptr;
    void (*wake_npcs)(void *) = nullptr;
    // Device frontends may stage otherwise-immediate services so price/confirmation
    // can be presented without changing the reference command semantics.
    bool transactional_services = false;
};
ActionResult begin_shop(CommandContext &, const NpcActor &);
ActionResult execute_shop(CommandContext &, ShopInput);
} // namespace openu5
