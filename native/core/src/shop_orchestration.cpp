#include "openu5/shop_orchestration.h"
#include "openu5/display_names.h"
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string_view>
namespace openu5 {
namespace {
#include "shop_greetings.inc"
Rand shop_rand(CommandContext &c) {
    return {&c, [](void *p, int32_t lo, int32_t hi) -> int32_t {
        auto &ctx = *static_cast<CommandContext *>(p);
        const auto value = ctx.game.rng.next(lo, hi).value;
        if (ctx.rng_trace.emit)
            ctx.rng_trace.emit(ctx.rng_trace.context, "shop", lo, hi, value);
        return value;
    }};
}
bool space(char16_t c) {
    return (c >= 9 && c <= 13) || c == 32 || c == 160 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
           c == 0x205f || c == 0x3000 || c == 0xfeff;
}
std::u16string_view trimmed(const ShopInput &input) {
    if (!input.text)
        return {};
    size_t a = 0, b = input.length;
    while (a < b && space(input.text[a]))
        ++a;
    while (b > a && space(input.text[b - 1]))
        --b;
    return {input.text + a, b - a};
}
int rumor_index(std::u16string_view text) {
    char16_t buffer[15]{};
    size_t n = std::min<size_t>(15, text.size());
    for (size_t i = 0; i < n; ++i) {
        auto c = text[i];
        buffer[i] = c >= u'A' && c <= u'Z' ? char16_t(c + 32) : c;
    }
    for (int i = 0; i < 26; ++i) {
        for (size_t at = 0; at + 4 <= n; ++at) {
            bool match = true;
            for (size_t k = 0; k < 4; ++k)
                if (buffer[at + k] != RUMOR_KEYWORDS[i][k])
                    match = false;
            if (match) {
                if (at == 0 || buffer[at - 1] == u' ')
                    return i;
                break;
            }
        }
    }
    return -1;
}
struct ShopRun {
    CommandContext &c;
    ShopServices &v;
    ShopSession &s;
    ActionResult out{};
    Rand rand;
    int loc() const { return s.location; }
    uint8_t intel() const {
        return c.game.party.character_count ? c.game.party.characters[0].intelligence : 15;
    }
    const char *item_name() const {
        if (s.item < 0) return "service";
        switch (s.phase) {
        case ShopPhase::Reagent:
        case ShopPhase::ReagentDeal:
        case ShopPhase::ReagentFull: return reagent_display_name(s.item);
        case ShopPhase::Guild:
        case ShopPhase::GuildDeal: return guild_item_display_name(s.item);
        case ShopPhase::Ship:
        case ShopPhase::ShipDeal:
        case ShopPhase::ShipAgain: return ship_service_display_name(s.item);
        case ShopPhase::Wine: return wine_display_name(s.item);
        case ShopPhase::HealerMember:
        case ShopPhase::HealerDeal:
        case ShopPhase::InnLeaveMember:
        case ShopPhase::InnLeaveDeal:
        case ShopPhase::InnPickupMember:
            return member(s.item) ? c.game.party.characters[s.item].name : "companion";
        default: return equipment_display_name(s.item);
        }
    }
    void expand(const char *raw, char *expanded, size_t capacity, bool open_quote = false) const {
        if (!capacity) return;
        size_t at = 0;
        const char *keeper = s.record && s.record->keeper ? s.record->keeper : "Merchant";
        const char *shop = s.record && s.record->name ? s.record->name : "Shop";
        const char *day = c.game.time.hour < 12 ? "morning" : c.game.time.hour < 18 ? "afternoon" : "evening";
        const char *item = item_name();
        char price[24]{}, quantity[24]{};
        std::snprintf(price, sizeof(price), "%ld", long(s.price));
        std::snprintf(quantity, sizeof(quantity), "%ld", long(s.quantity));
        const char *place = s.item >= 0 && size_t(s.item) < std::size(RUMOR_GOSSIP_MAP)
                                ? RUMOR_PLACES[RUMOR_GOSSIP_MAP[s.item]]
                                : "Britannia";
        auto add = [&](const char *text) {
            while (text && *text && at + 1 < capacity) expanded[at++] = *text++;
        };
        if (open_quote) add("\"");
        for (const char *p = raw ? raw : ""; *p && at + 1 < capacity; ++p) {
            switch (*p) {
            case '$': add(keeper); break;
            case '#': add(shop); break;
            case '@': add(day); break;
            case '%':
                if (p[1] == 's') { add(item ? item : "item"); ++p; }
                else if (p[1] == 'd' || p[1] == 'u') { add(price); ++p; }
                else if (p[1] == '%') { expanded[at++] = '%'; ++p; }
                else add(price);
                break;
            case '&': add(item ? item : "item"); break;
            case '^': add(quantity); break;
            case '*': add(place); break;
            default: expanded[at++] = *p; break;
            }
        }
        expanded[at] = 0;
    }
    bool rec(int i, bool open_quote = false) {
        if (i < 0) return false;
        const char *raw = v.record ? v.record(v.context, i) : nullptr;
        const bool present = raw || (v.record_present && v.record_present(v.context, i));
        if (!present) return false;
        if (!raw || !*raw) return true;
        char expanded[768]{};
        expand(raw, expanded, sizeof(expanded), open_quote);
        message(expanded);return true;
    }
    void event(ShopEventKind kind, const ShopResult *result = nullptr, const TavernResult *tavern = nullptr) {
        ShopEvent p{kind, &s, result, tavern};
        GameEvent e;
        e.kind = GameEventKind::Shop;
        e.shop = &p;
        ++out.event_count;
        if (c.events.emit)
            c.events.emit(c.events.context, e);
    }
    void message(const char *text) {
        GameEvent e;
        e.kind = GameEventKind::Message;
        e.text = text;
        ++out.event_count;
        if (c.events.emit)
            c.events.emit(c.events.context, e);
    }
    void phase(ShopPhase p) {
        s.phase = p;
        // Pending offers live only until the reference clears its pending* record.
        if (p != ShopPhase::BuyDeal && p != ShopPhase::SellDeal &&
            p != ShopPhase::ReagentDeal && p != ShopPhase::GuildDeal &&
            p != ShopPhase::HorseDeal && p != ShopPhase::ShipDeal &&
            p != ShopPhase::HealerDeal &&
            !(s.transactional_services && p == ShopPhase::InnRestDeal) &&
            p != ShopPhase::InnLeaveDeal && p != ShopPhase::RumorDeal &&
            !(s.transactional_services && p == ShopPhase::TavernRoundDeal) &&
            !(s.transactional_services && p == ShopPhase::TavernDrinkDeal) &&
            !(s.transactional_services && p == ShopPhase::RationsQuantity)) {
            s.item = -1;
            s.price = s.quantity = 0;
        }
        event(ShopEventKind::State);
        out.status =
            p == ShopPhase::Closed ? CommandStatus::Success : CommandStatus::AwaitingResponse;
    }
    void result(ShopResult r) {
        char expanded[768]{};
        if (r.message) { expand(r.message, expanded, sizeof(expanded)); r.message = expanded; }
        event(ShopEventKind::Result, &r);
    }
    void tavern_result(const TavernResult &source) {
        auto r = source;
        char expanded[768]{};
        if (r.message) { expand(r.message, expanded, sizeof(expanded)); r.message = expanded; }
        event(ShopEventKind::Result, &r, &r);
    }
    bool names() const {
        return s.record && s.record->keeper && *s.record->keeper && s.record->name &&
               *s.record->name;
    }
    void drain(bool yes) {
        if (yes)
            shop_post_purchase_drain(c.game, c.travel.shadowlord_here, rand);
    }
    void mark(bool yes) {
        if (yes) {
            s.purchased = true;
            if (s.type == ShopType::Blacksmith && s.buying)
                s.bought_in_buy = true;
        }
    }
    void end() {
        bool silent = s.thrown_out || (s.type == ShopType::Blacksmith && s.selling);
        if (!silent && names() && (v.record || v.record_present)) {
            const int variant=rand(0, 3);
            rec((s.purchased?SHOPPE_FAREWELL_PURCHASE:SHOPPE_FAREWELL_NO_PURCHASE)[int(s.type)][variant],true);
        }
        s.phase = ShopPhase::Closed;
        s.item = -1;
        s.price = s.quantity = 0;
        event(ShopEventKind::Exited);
        out.status = CommandStatus::Success;
    }
    int first() {
        for (int i = 0; i < c.game.party.character_count; ++i)
            if (c.game.party.characters[i].party_status == 0)
                return i;
        return -1;
    }
    bool room() {
        auto inn = inn_at(loc());
        return inn.present && inn_guest_count(c.game, loc()) < inn.capacity;
    }
    bool member(int i) const { return i >= 0 && i < c.game.party.character_count; }
    bool selling_any() {
        for (int i = 0; i < c.game.equipment_count; ++i)
            if (c.game.equipment_quantities[i] > 0)
                return true;
        return false;
    }
    void buy_list() {
        rand(0, 3);
        phase(ShopPhase::Buy);
    }
    void sell_exit() {
        rand(0, 3);
        end();
    }
    void after_sell() {
        if (!selling_any())
            sell_exit();
        else {
            phase(ShopPhase::Sell);
            rand(0, 3);
        }
    }
    void tavern_again(bool served) {
        if (served) {
            s.tavern_served = true;
            mark(true);
        }
        phase(ShopPhase::TavernAgain);
    }
    void menu() {
        switch (s.type) {
        case ShopType::MagicSeller:
            phase(ShopPhase::Reagent);
            break;
        case ShopType::GuildMaster:
            phase(ShopPhase::Guild);
            break;
        case ShopType::Shipwright:
            phase(ShopPhase::Ship);
            break;
        case ShopType::Barkeeper:
            phase(ShopPhase::Tavern);
            break;
        case ShopType::Healer:
            phase(s.legacy_healer ? ShopPhase::Menu : ShopPhase::HealerNeed);
            break;
        default:
            phase(ShopPhase::Menu);
            break;
        }
    }
    bool spot(int &x, int &y) {
        if (!v.tile || !v.occupied)
            return false;
        const int dx[] = {0, 0, 1, -1}, dy[] = {1, -1, 0, 0};
        for (int i = 0; i < 4; ++i) {
            x = int(c.game.position.xy.x) + dx[i];
            y = int(c.game.position.xy.y) + dy[i];
            if (v.occupied(v.context, x, y))
                continue;
            int t = v.tile(v.context, x, y);
            if (t == 0x44 || t == 0x45 || t == 5)
                return true;
        }
        return false;
    }
    void horse() {
        int x = 0, y = 0;
        if (!v.horse || !v.reserve) {
            out.status = CommandStatus::Unsupported;
            return;
        }
        if (!spot(x, y)) {
            result({false, "The stables are closed."});
            end();
            return;
        }
        if (!v.reserve(v.context, false)) {
            out.status = CommandStatus::NeedsStorage;
            return;
        }
        auto before = c.game.gold;
        auto r = buy_horse(c.game, s.town, intel());
        result(r);
        if (r.ok) {
            drain(true);
            v.horse(v.context, x, y);
        } else if (s.phase == ShopPhase::HorseDeal)
            s.thrown_out = true;
        mark(c.game.gold < before);
        end();
    }
    void healer_member(int i) {
        if (!member(i))
            return;
        auto &ch = c.game.party.characters[i];
        bool needs = s.remedy == HealerService::Heal ? ch.status != 'D' && ch.current_hp < ch.max_hp
                     : s.remedy == HealerService::Cure ? ch.status == 'P'
                                                       : ch.status == 'D';
        if (!needs) {
            result({false, "Thou hast no need of this art!"});
            phase(ShopPhase::HealerAgain);
            return;
        }
        s.item = i;
        s.price = (s.remedy == HealerService::Heal   ? v.data.heal_prices
                   : s.remedy == HealerService::Cure ? v.data.cure_prices
                                                     : v.data.resurrect_prices)
                      .at(s.town);
        if (loc() == 5 && s.remedy != HealerService::Resurrect) {
            result(healer_heal(c.game, i, s.remedy, 0));
            phase(ShopPhase::HealerAgain);
            return;
        }
        phase(ShopPhase::HealerDeal);
    }
    void pickup(int i) {
        if (!member(i) || c.game.party.characters[i].party_status != loc())
            return;
        auto r = inn_pickup(c.game, first(), i, loc());
        mark(r.ok);
        drain(r.ok);
        result(r);
        if (r.reason == ShopFailure::Gold)
            end();
        else
            phase(ShopPhase::InnAgain);
    }
    void round(bool house) {
        if (!house && (!v.tile || !v.plate)) {
            out.status = CommandStatus::Unsupported;
            return;
        }
        int x = c.game.position.xy.x, y = c.game.position.xy.y;
        auto r = buy_tavern_round(c.game, s.town, house ? 0 : v.tile(v.context, x, y - 1),
                                  house ? 0 : v.tile(v.context, x, y + 1), house);
        tavern_result(r);
        if (!r.ok) {
            s.thrown_out = true;
            end();
            return;
        }
        if (r.plate_dy)
            v.plate(v.context, x, y + r.plate_dy, r.plate_tile);
        if (r.counts_as_service)
            ++s.served;
        drain(true);
        tavern_again(true);
    }
    void drink() {
        if (TAVERN_SUBTYPE[s.town] == TAVERN_SUBTYPE_WINE)
            phase(ShopPhase::Wine);
        else
            round(true);
    }
    void buy_done(ShopResult r, bool fallback = false) {
        result(r);
        if (!r.ok && r.reason == ShopFailure::Gold) {
            message(BLACKSMITH_BUY_BROKE[rand(0, 3)]);
            s.thrown_out = true;
            end();
            return;
        }
        if (!r.ok && r.reason == ShopFailure::Cap && !fallback) {
            phase(ShopPhase::BuyFull);
            return;
        }
        drain(r.ok);
        mark(r.ok);
        buy_list();
    }
    void reagent_done(ShopResult r, bool fallback = false) {
        result(r);
        if (!r.ok && (!fallback || r.reason == ShopFailure::Gold)) {
            if(r.reason==ShopFailure::Gold)rec(MAGIC_SELLER_BROKE_INDEX);
            s.thrown_out = true;
            end();
            return;
        }
        drain(r.ok);
        mark(r.ok);
        phase(ShopPhase::Reagent);
    }
    void guild_done(ShopResult r) {
        result(r);
        if (!r.ok) {
            if(r.reason==ShopFailure::Gold)rec(GUILD_BROKE_INDEX);
            s.thrown_out = true;
            end();
            return;
        }
        drain(true);
        mark(true);
        phase(ShopPhase::Guild);
    }
    void confirm() {
        switch (s.phase) {
        case ShopPhase::Greeting:
        case ShopPhase::HealerAgain:
        case ShopPhase::InnAgain:
            if (s.type == ShopType::HorseSeller) {
                s.price = horse_price(s.town, intel());
                if (rec(HORSE_PITCH_INDEX)) {
                    phase(ShopPhase::HorseDeal);
                    break;
                }
                horse();
            } else
                menu();
            break;
        case ShopPhase::TavernAgain:
            phase(ShopPhase::Tavern);
            break;
        case ShopPhase::ShipAgain:
            phase(ShopPhase::Ship);
            break;
        case ShopPhase::Drunk:
            tavern_again(true);
            break;
        case ShopPhase::BuyDeal:
            buy_done(buy_equipment(c.game, s.item, s.price));
            break;
        case ShopPhase::SellDeal:
            result(sell_equipment(c.game, s.item, s.price));
            after_sell();
            break;
        case ShopPhase::ReagentDeal:
            reagent_done(buy_reagent(c.game, s.item, s.quantity, s.price));
            break;
        case ShopPhase::GuildDeal:
            guild_done(buy_guild_item(c.game, s.town, s.item, intel()));
            break;
        case ShopPhase::HorseDeal:
            horse();
            break;
        case ShopPhase::HealerDeal: {
            bool paid = c.game.gold >= s.price;
            auto r = healer_heal(c.game, s.item, s.remedy,
                                 paid                           ? s.price
                                 : s.price <= 100 && loc() == 7 ? 0
                                                                : s.price);
            if (paid) {
                drain(r.ok);
                mark(r.ok);
            }
            result(r);
            phase(ShopPhase::HealerAgain);
            break;
        }
        case ShopPhase::ShipDeal: {
            if (!v.ship || !v.reserve) {
                out.status = CommandStatus::Unsupported;
                break;
            }
            if (!v.reserve(v.context, true)) {
                out.status = CommandStatus::NeedsStorage;
                break;
            }
            auto r = buy_ship(c.game, s.town, s.item == 0, intel());
            result(r);
            if (!r.ok) {
                s.thrown_out = true;
                end();
                break;
            }
            mark(true);
            v.ship(v.context, r.x, r.y, s.item == 0 ? 0x125 : 0x129, 99, s.item == 0 ? 2 : 0);
            phase(ShopPhase::ShipAgain);
            break;
        }
        case ShopPhase::InnLeaveDeal:
            result(inn_leave(c.game, s.item, loc()));
            phase(ShopPhase::InnAgain);
            break;
        case ShopPhase::InnRestDeal: {
            if (!v.hour_tiles || !v.wake_npcs) {
                out.status = CommandStatus::Unsupported;
                break;
            }
            auto r = inn_rest(c.game, first(), loc());
            mark(r.ok);
            drain(r.ok);
            result(r);
            if (r.ok) {
                c.game.position.xy = {uint8_t(r.x), uint8_t(r.y)};
                inn_night_pass(c.game, c.turn, rand, {v.context, v.hour_tiles}, c.sky);
                v.wake_npcs(v.context);
                ++c.game.position.xy.x;
            }
            end();
            break;
        }
        case ShopPhase::TavernRoundDeal:
            round(false);
            break;
        case ShopPhase::TavernDrinkDeal:
            round(true);
            break;
        case ShopPhase::RumorDeal: {
            auto r = pay_rumor(c.game, s.item);
            result(r);
            if (r.ok) {
                mark(true);
                if (v.record_present)
                    rand(0, 3);
            }
            tavern_again(true);
            break;
        }
        case ShopPhase::Menu:
            if (s.type == ShopType::HorseSeller)
                horse();
            break;
        default:
            break;
        }
    }
    void decline() {
        switch (s.phase) {
        case ShopPhase::Menu:
            if (s.type == ShopType::HorseSeller) end();
            break;
        case ShopPhase::BuyDeal:
            buy_list();
            break;
        case ShopPhase::SellDeal:
            after_sell();
            break;
        case ShopPhase::ReagentDeal:
            phase(ShopPhase::Reagent);
            break;
        case ShopPhase::GuildDeal:
            phase(ShopPhase::Guild);
            break;
        case ShopPhase::HealerDeal:
            phase(ShopPhase::HealerAgain);
            break;
        case ShopPhase::InnLeaveDeal:
            phase(ShopPhase::InnAgain);
            break;
        case ShopPhase::RumorDeal:
            tavern_again(true);
            break;
        case ShopPhase::TavernRoundDeal:
        case ShopPhase::TavernDrinkDeal:
            phase(ShopPhase::Tavern);
            break;
        case ShopPhase::Drunk:
            c.turn.drunk_turns = DRUNK_TIMER_TURNS;
            if (c.game.karma)
                --c.game.karma;
            drink();
            break;
        case ShopPhase::Greeting:
        case ShopPhase::HealerAgain:
        case ShopPhase::InnAgain:
        case ShopPhase::TavernAgain:
        case ShopPhase::HorseDeal:
        case ShopPhase::ShipDeal:
        case ShopPhase::ShipAgain:
        case ShopPhase::InnRestDeal:
            end();
            break;
        default:
            break;
        }
    }
    void select(int i) {
        switch (s.phase) {
        case ShopPhase::Buy: {
            bool stock = false;
            for (int j = 0; j < 8; ++j)
                if (size_t(s.town * 8 + j) < v.data.weapons.count &&
                    v.data.weapons.at(s.town * 8 + j) == i && i != 255)
                    stock = true;
            if (!stock || i < 0 || i >= 256)
                return;
            s.item = i;
            s.price = shop_buy_price(v.data.equipment_prices.at(i), intel());
            if (i >= int(std::size(BLACKSMITH_BUY_PITCH_INDEX)) ||
                !rec(BLACKSMITH_BUY_PITCH_INDEX[i]))
                buy_done(buy_equipment(c.game, i, s.price), true);
            else {
                rand(0, 3);
                phase(ShopPhase::BuyDeal);
            }
            break;
        }
        case ShopPhase::Sell: {
            if (i < 0 || i >= c.game.equipment_count || c.game.equipment_quantities[i] <= 0)
                return;
            if (i == 0x1b || i == 0x1d) {
                result({false, ""});
                end();
                return;
            }
            if (v.data.equipment_prices.at(i) <= 0) {
                result({false, ""});
                after_sell();
                return;
            }
            s.item = i;
            s.price = shop_sell_price(v.data.equipment_prices.at(i), intel());
            bool pitch = v.record_present && rec(BLACKSMITH_SELL_OFFER_INDEX[rand(0, 7)]);
            if (pitch)
                phase(ShopPhase::SellDeal);
            else {
                result(sell_equipment(c.game, i, s.price));
                after_sell();
            }
            break;
        }
        case ShopPhase::Reagent:
            if (i < 0 || i >= 8 || v.data.reagent_prices.at(s.town * 8 + i) <= 0)
                return;
            s.item = i;
            s.quantity = v.data.reagent_grants.at(s.town * 8 + i);
            s.price = shop_buy_price(v.data.reagent_prices.at(s.town * 8 + i), intel());
            if (!rec(MAGIC_SELLER_PITCH_INDEX[i]))
                reagent_done(buy_reagent(c.game, i, s.quantity, s.price), true);
            else if (c.game.reagent_quantities[i] >= 99)
                phase(ShopPhase::ReagentFull);
            else
                phase(ShopPhase::ReagentDeal);
            break;
        case ShopPhase::Guild:
            if (i == 3 && rec(GUILD_EASTEREGG_INDEX)) {
                phase(ShopPhase::Guild);
                return;
            }
            if (i < 0 || i >= 3)
                return;
            s.item = i;
            s.price = guild_price(s.town, i, intel());
            if (rec(GUILD_PITCH_INDEX[i]))
                phase(ShopPhase::GuildDeal);
            else
                guild_done(buy_guild_item(c.game, s.town, i, intel()));
            break;
        case ShopPhase::Ship:
            if (i < 0 || i > 1)
                return;
            s.item = i;
            s.price = ship_price(s.town, i == 0, intel());
            phase(ShopPhase::ShipDeal);
            break;
        case ShopPhase::Wine: {
            if (i < 0 || i >= 6)
                return;
            auto r = buy_wine(c.game, i);
            result(r);
            if (!r.ok) {
                s.thrown_out = true;
                end();
                return;
            }
            mark(true);
            ++s.served;
            drain(true);
            tavern_again(true);
            break;
        }
        default:
            break;
        }
    }
};
} // namespace
ActionResult begin_shop(CommandContext &c, const NpcActor &npc) {
    ActionResult result;
    if (!c.shop_services || c.combat || c.dungeon) {
        result.status = CommandStatus::InvalidContext;
        return result;
    }
    auto &v = *c.shop_services;
    auto &s = v.session;
    if (s.phase != ShopPhase::Closed) {
        result.status = CommandStatus::AwaitingResponse;
        return result;
    }
    if (npc.schedule.dialog < 0x81 || npc.schedule.dialog > 0x88) {
        result.status = CommandStatus::Rejected;
        return result;
    }
    ShopRun r{c, v, s, {}, shop_rand(c)};
    if (!shop_is_open(npc.schedule.times, uint8_t(c.game.time.hour))) {
        r.message("A merchant says:\n\"Come see me at\nmy shoppe, when\nit's open!\"\n");
        r.out.status = CommandStatus::Rejected;
        return r.out;
    }
    auto type = ShopType(npc.schedule.dialog - 0x81);
    if ((c.turn.transport_tile & 0xfc) == 0x10 && type != ShopType::HorseSeller) {
        r.message("A merchant says:\n\"GET THAT HORSE OUT OF HERE!\"\n");
        r.out.status = CommandStatus::Rejected;
        return r.out;
    }
    auto town = shop_town_index(type, c.game.position.map.location);
    if (town < 0) {
        result.status = CommandStatus::Rejected;
        return result;
    }
    s = {};
    s.owner = &c.game;
    s.type = type;
    s.transactional_services = v.transactional_services;
    s.location = c.game.position.map.location;
    s.town = town;
    s.record = shop_lookup(v.data, s.location, type);
    if (type == ShopType::HorseSeller) {
        int x = 0, y = 0;
        if (!v.tile || !v.occupied) {
            r.out.status = CommandStatus::Unsupported;
            return r.out;
        }
        if (!r.spot(x, y)) {
            r.message("The stables are closed.\n");
            return r.out;
        }
    }
    r.event(ShopEventKind::Entered);
    bool greeted = r.names();
    if (greeted && type != ShopType::Blacksmith)
        greeted = (v.record || v.record_present) && r.rec(SHOPPE_GREETING_INDEX[int(type)][r.rand(0, 3)],true);
    if (greeted && type == ShopType::Blacksmith) {
        const char *day=c.game.time.hour<12?"morning":c.game.time.hour<18?"afternoon":"evening";
        char greeting[192]{};std::snprintf(greeting,sizeof(greeting),"\"Good %s, and welcome to %s!\"",day,s.record->name);
        r.message(greeting);
    }
    s.legacy_healer = !greeted && type == ShopType::Healer;
    if (greeted)
        r.phase(type == ShopType::Blacksmith ? ShopPhase::BlacksmithPause : ShopPhase::Greeting);
    else
        r.menu();
    return r.out;
}
ActionResult execute_shop(CommandContext &c, ShopInput input) {
    ActionResult error;
    error.status = CommandStatus::InvalidContext;
    if (!c.shop_services)
        return error;
    auto &v = *c.shop_services;
    auto &s = v.session;
    if (s.owner != &c.game || s.phase == ShopPhase::Closed || c.combat || c.dungeon)
        return error;
    ShopRun r{c, v, s, {}, shop_rand(c)};
    r.out.status = CommandStatus::AwaitingResponse;
    if (input.action == ShopAction::Confirm || input.action == ShopAction::Decline) {
        if (s.phase == ShopPhase::BlacksmithPause) {
            const int variant=r.rand(0, 1);char line[256]{};std::snprintf(line,sizeof(line),"%s says,\n\"%s",s.record&&s.record->keeper?s.record->keeper:"Merchant",variant?"Greetings, traveller! Wish ye to Buy, or hast thou wares to Sell?\"":"Hail, friend! Wouldst thou Buy or Sell?\"");r.message(line);
            r.phase(ShopPhase::Menu);
            return r.out;
        }
        if (s.phase == ShopPhase::BuyFull) {
            r.buy_list();
            return r.out;
        }
        if (s.phase == ShopPhase::ReagentFull) {
            r.phase(ShopPhase::Reagent);
            return r.out;
        }
    }
    auto &g = c.game;
    switch (input.action) {
    case ShopAction::End:
        r.end();
        break;
    case ShopAction::Confirm:
        r.confirm();
        break;
    case ShopAction::Decline:
        r.decline();
        break;
    case ShopAction::Continue:
        if (s.phase == ShopPhase::BlacksmithPause) {
            const int variant=r.rand(0, 1);char line[256]{};std::snprintf(line,sizeof(line),"%s says,\n\"%s",s.record&&s.record->keeper?s.record->keeper:"Merchant",variant?"Greetings, traveller! Wish ye to Buy, or hast thou wares to Sell?\"":"Hail, friend! Wouldst thou Buy or Sell?\"");r.message(line);
            r.phase(ShopPhase::Menu);
        } else if (s.phase == ShopPhase::BuyFull)
            r.buy_list();
        else if (s.phase == ShopPhase::ReagentFull)
            r.phase(ShopPhase::Reagent);
        else if (s.phase == ShopPhase::Reagent || s.phase == ShopPhase::HealerNeed)
            r.end();
        break;
    case ShopAction::Buy:
        if (s.phase == ShopPhase::Menu && s.type == ShopType::Blacksmith) {
            s.buying = true;
            s.selling = false;
            s.bought_in_buy = false;
            s.thrown_out = false;
            r.rand(0, 3);
            r.rand(0, 3);
            r.buy_list();
        }
        break;
    case ShopAction::Sell:
        if (s.phase == ShopPhase::Menu && s.type == ShopType::Blacksmith) {
            s.selling = true;
            s.buying = false;
            if (!r.selling_any())
                r.end();
            else {
                r.rand(0, 3);
                r.phase(ShopPhase::Sell);
            }
        }
        break;
    case ShopAction::SelectItem:
        r.select(input.value);
        break;
    case ShopAction::Heal:
    case ShopAction::Cure:
    case ShopAction::Resurrect:
        if (s.phase != ShopPhase::HealerNeed &&
            !(s.phase == ShopPhase::Menu && s.type == ShopType::Healer))
            break;
        s.remedy = input.action == ShopAction::Heal   ? HealerService::Heal
                   : input.action == ShopAction::Cure ? HealerService::Cure
                                                      : HealerService::Resurrect;
        if (s.legacy_healer) {
            r.phase(ShopPhase(int(ShopPhase::LegacyHeal) + int(s.remedy)));
            break;
        }
        if (g.party.party_size <= 1)
            r.healer_member(0);
        else
            r.phase(ShopPhase::HealerMember);
        break;
    case ShopAction::SelectMember:
        if (s.phase >= ShopPhase::LegacyHeal && s.phase <= ShopPhase::LegacyResurrect) {
            auto offered = party_members(g.party);
            bool found = false;
            for (int i = 0; i < offered.count; ++i)
                if (offered.indices[i] == input.value)
                    found = true;
            if (!found)
                break;
            auto price = (s.remedy == HealerService::Heal   ? v.data.heal_prices
                          : s.remedy == HealerService::Cure ? v.data.cure_prices
                                                            : v.data.resurrect_prices)
                             .at(s.town);
            auto tx = healer_heal(g, input.value, s.remedy, price);
            if (!tx.ok && tx.reason == ShopFailure::Gold) {
                if (price <= 100 && s.location == 7)
                    tx = healer_heal(g, input.value, s.remedy, 0);
                else {
                    r.result(tx);
                    r.menu();
                    break;
                }
            }
            r.drain(tx.ok);
            r.mark(tx.ok);
            r.result(tx);
            r.menu();
        } else if (s.phase == ShopPhase::HealerMember && r.member(input.value) &&
                   g.party.characters[input.value].party_status == 0)
            r.healer_member(input.value);
        else if (s.phase == ShopPhase::InnPickupMember)
            r.pickup(input.value);
        else if (s.phase == ShopPhase::InnLeaveMember && r.member(input.value) &&
                 g.party.characters[input.value].party_status == 0) {
            if (input.value == 0)
                r.phase(ShopPhase::InnLeaveMember);
            else if (g.party.characters[input.value].status == 'D')
                r.end();
            else {
                s.item = input.value;
                r.phase(ShopPhase::InnLeaveDeal);
            }
        }
        break;
    case ShopAction::Rest:
    case ShopAction::LeaveMember:
    case ShopAction::Pickup:
        if (s.phase != ShopPhase::Menu || s.type != ShopType::InnKeeper)
            break;
        if (input.action == ShopAction::Pickup) {
            if (g.party.party_size >= 6 || inn_guest_count(g, s.location) == 0)
                r.phase(ShopPhase::InnAgain);
            else if (inn_guest_count(g, s.location) == 1) {
                for (int i = 0; i < g.party.character_count; ++i)
                    if (g.party.characters[i].party_status == s.location) {
                        r.pickup(i);
                        break;
                    }
            } else
                r.phase(ShopPhase::InnPickupMember);
        } else if (!r.room())
            r.phase(ShopPhase::InnAgain);
        else if (input.action == ShopAction::Rest) {
            if (s.transactional_services) {
                const auto inn = inn_at(s.location);
                const auto buyer = r.first();
                s.item = 0;
                s.quantity = 1;
                s.price = buyer >= 0 ? inn_rest_price(inn, g.party.party_size,
                                                      g.party.characters[buyer].intelligence)
                                     : 0;
            }
            r.phase(ShopPhase::InnRestDeal);
        } else if (g.party.party_size <= 1)
            r.end();
        else
            r.phase(ShopPhase::InnLeaveMember);
        break;
    case ShopAction::Round:
        if (s.phase == ShopPhase::Tavern) {
            if (s.transactional_services) {
                s.item = 0;
                s.quantity = 1;
                s.price = tavern_round_price(g, s.town, false);
                r.phase(ShopPhase::TavernRoundDeal);
            } else
                r.round(false);
        }
        break;
    case ShopAction::Drink:
        if (s.phase == ShopPhase::Tavern) {
            if (s.served == DRUNK_CUP_GATE)
                r.phase(ShopPhase::Drunk);
            else if (TAVERN_SUBTYPE[s.town] == TAVERN_SUBTYPE_WINE)
                r.drink();
            else if (s.transactional_services) {
                s.item = 1;
                s.quantity = 1;
                s.price = tavern_round_price(g, s.town, true);
                r.phase(ShopPhase::TavernDrinkDeal);
            } else
                r.drink();
        }
        break;
    case ShopAction::Rations:
        if (s.phase == ShopPhase::Tavern &&
            (TAVERN_SUBTYPE[s.town] == 0 || TAVERN_SUBTYPE[s.town] == 3)) {
            r.rand(0, 6);
            if (s.transactional_services) {
                s.item = 2;
                s.price = ration_price(s.town, r.intel());
                s.quantity = 25;
            }
            r.phase(ShopPhase::RationsQuantity);
        }
        break;
    case ShopAction::Rumor:
        if (s.phase == ShopPhase::Tavern && s.tavern_served)
            r.phase(ShopPhase::RumorText);
        break;
    case ShopAction::Text: {
        if (!input.text && input.length) {
            r.out.status = CommandStatus::Rejected;
            break;
        }
        auto text = trimmed(input);
        if (s.phase == ShopPhase::RumorText) {
            if (text.empty()) {
                r.tavern_again(true);
                break;
            }
            int i = rumor_index(text);
            if (i < 0) {
                r.phase(ShopPhase::RumorText);
                break;
            }
            s.item = i;
            s.price = RUMOR_PRICES[i];
            r.phase(ShopPhase::RumorDeal);
        } else if (s.phase == ShopPhase::RationsQuantity) {
            size_t i = 0;
            bool negative = false;
            if (!text.empty() && (text[0] == u'+' || text[0] == u'-')) {
                negative = text[0] == u'-';
                ++i;
            }
            int64_t qty = 0;
            while (i < text.size() && text[i] >= u'0' && text[i] <= u'9') {
                qty = std::min<int64_t>(2147483647, qty * 10 + text[i++] - u'0');
            }
            if (negative || qty == 0) {
                r.result({false, "Hrumph."});
                r.tavern_again(false);
                break;
            }
            auto tx = buy_rations(g, s.town, r.intel(), int32_t(qty));
            r.result(tx);
            if (tx.bought < qty && !tx.full) {
                if (tx.bought == 0) {
                    if (g.food < 3)
                        g.food = uint16_t(g.food + r.rand(0, 1) + 1);
                    s.thrown_out = true;
                    r.end();
                } else
                    r.tavern_again(true);
            } else {
                r.drain(true);
                r.tavern_again(true);
            }
        }
        break;
    }
    case ShopAction::Cancel:
        if (s.phase == ShopPhase::RumorText)
            r.tavern_again(true);
        else if (s.phase == ShopPhase::RationsQuantity)
            r.tavern_again(false);
        else if (s.phase == ShopPhase::HealerMember)
            r.phase(ShopPhase::HealerAgain);
        else if (s.phase == ShopPhase::InnLeaveMember || s.phase == ShopPhase::InnPickupMember)
            r.phase(ShopPhase::InnAgain);
        else if (s.phase == ShopPhase::Sell)
            r.sell_exit();
        else if (s.phase == ShopPhase::Wine)
            r.tavern_again(false);
        else if (s.phase == ShopPhase::BlacksmithPause) {
            const int variant=r.rand(0, 1);char line[256]{};std::snprintf(line,sizeof(line),"%s says,\n\"%s",s.record&&s.record->keeper?s.record->keeper:"Merchant",variant?"Greetings, traveller! Wish ye to Buy, or hast thou wares to Sell?\"":"Hail, friend! Wouldst thou Buy or Sell?\"");r.message(line);
            r.phase(ShopPhase::Menu);
        } else if (s.phase == ShopPhase::BuyFull)
            r.buy_list();
        else if (s.phase == ShopPhase::ReagentFull)
            r.phase(ShopPhase::Reagent);
        else if (s.phase == ShopPhase::Menu || s.phase == ShopPhase::Buy ||
                 s.phase == ShopPhase::Reagent || s.phase == ShopPhase::Guild ||
                 s.phase == ShopPhase::Ship || s.phase == ShopPhase::Tavern ||
                 s.phase == ShopPhase::HealerNeed || s.phase >= ShopPhase::LegacyHeal)
            r.end();
        break;
    }
    return r.out;
}
} // namespace openu5

namespace openu5 {
int32_t tavern_round_price(const GameState &g, int32_t town, bool house_round) {
    int32_t living = 0;
    const auto members = party_members(g.party);
    for (int i = 0; i < members.count; ++i)
        if (g.party.characters[members.indices[i]].status != 'D')
            ++living;
    if (house_round)
        return living;
    return town >= 0 && size_t(town) < std::size(TAVERN_ROUND_PRICE)
               ? TAVERN_ROUND_PRICE[town] * living
               : 0;
}

void shop_offerings(const GameState &g, const ShopSession &s, const ShopData &d, void *ctx,
                    void (*emit)(void *, ShopOffer)) {
    if (!emit || s.owner != &g || s.town < 0)
        return;
    uint8_t intel = g.party.character_count ? g.party.characters[0].intelligence : 15;
    switch (s.phase) {
    case ShopPhase::Buy:
        for (int j = 0; j < 8; ++j) {
            int index = s.town * 8 + j;
            if (size_t(index) >= d.weapons.count)
                break;
            int id = d.weapons.at(index);
            if (id != 255)
                emit(ctx, {id, shop_buy_price(d.equipment_prices.at(id), intel),
                           (id == 27 || id == 29) ? 99 : 1});
        }
        break;
    case ShopPhase::Sell:
        for (int i = 0; i < g.equipment_count; ++i)
            if (g.equipment_quantities[i] > 0)
                emit(ctx, {i, shop_sell_price(d.equipment_prices.at(i), intel),
                           g.equipment_quantities[i]});
        break;
    case ShopPhase::Reagent:
        for (int i = 0; i < 8; ++i)
            if (d.reagent_prices.at(s.town * 8 + i) > 0)
                emit(ctx, {i, shop_buy_price(d.reagent_prices.at(s.town * 8 + i), intel),
                           d.reagent_grants.at(s.town * 8 + i)});
        break;
    case ShopPhase::Guild:
        for (int i = 0; i < 3; ++i)
            emit(ctx, {i, guild_price(s.town, i, intel), i + 3});
        break;
    case ShopPhase::Ship:
        for (int i = 0; i < 2; ++i)
            emit(ctx, {i, ship_price(s.town, i == 0, intel), 1});
        break;
    case ShopPhase::Wine:
        for (int i = 0; i < 6; ++i)
            emit(ctx, {i, wine_price(i), 1});
        break;
    case ShopPhase::RationsQuantity:
        if (s.transactional_services)
            emit(ctx, {2, s.price, 25});
        break;
    case ShopPhase::InnPickupMember:
        for (int i = 0; i < g.party.character_count; ++i)
            if (g.party.characters[i].party_status == s.location)
                emit(ctx, {i, 0, 1});
        break;
    case ShopPhase::HealerMember:
    case ShopPhase::InnLeaveMember:
    case ShopPhase::LegacyHeal:
    case ShopPhase::LegacyCure:
    case ShopPhase::LegacyResurrect: {
        auto members = party_members(g.party);
        for (int j = 0; j < members.count; ++j)
            emit(ctx, {members.indices[j], 0, 1});
        break;
    }
    default:
        break;
    }
}
bool shop_offering_at(const GameState &g, const ShopSession &s, const ShopData &d, size_t row,
                      ShopOffer &offer) {
    struct Capture {
        size_t wanted = 0, seen = 0;
        ShopOffer offer{};
        bool found = false;
    } capture{row};
    shop_offerings(g, s, d, &capture, [](void *context, ShopOffer candidate) {
        auto &value = *static_cast<Capture *>(context);
        if (value.seen++ == value.wanted) {
            value.offer = candidate;
            value.found = true;
        }
    });
    if (capture.found)
        offer = capture.offer;
    return capture.found;
}
size_t shop_offering_count(const GameState &g, const ShopSession &s, const ShopData &d) {
    size_t count = 0;
    shop_offerings(g, s, d, &count,
                   [](void *context, ShopOffer) { ++*static_cast<size_t *>(context); });
    return count;
}
} // namespace openu5
