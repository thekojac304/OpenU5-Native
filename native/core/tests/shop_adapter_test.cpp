#include "openu5/dialogue_orchestration.h"
#include "openu5/display_names.h"
#include "openu5/shop_orchestration.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
using namespace openu5;
int main() {
    GameState g;
    TurnState t;
    TravelState travel;
    CommandState commands;
    WorldData world;
    CommandContext c{g, t, travel, commands, world};
    g.position.map.location = 2;
    g.party.character_count = g.party.party_size = 1;
    g.party.characters[0].intelligence = 15;
    g.party.characters[0].party_status = 0;
    g.gold = 100;
    ShopSession shop;
    ShopData data;
    ShopServices services{shop, data};
    c.shop_services = &services;
    NpcActors actors;
    actors.count = 1;
    auto &npc = actors.actors[0];
    npc.location = 2;
    npc.schedule.dialog = 0x81;
    npc.schedule.slot = 3;
    npc.schedule.times[0] = 1;
    c.actors = &actors;
    DialogueSession talk;
    DialogueServices dialogue{talk};
    c.dialogue_services = &dialogue;
    std::vector<int> events;
    c.events = {&events, [](void *p, const GameEvent &e) {
                    static_cast<std::vector<int> *>(p)->push_back(
                        e.kind == GameEventKind::Dialogue
                            ? 100
                            : int(e.kind == GameEventKind::Shop ? int(e.shop->kind) : 200));
                }};
    Command cmd;
    cmd.kind = CommandKind::BeginConversation;
    cmd.member = 3;
    auto start = execute_command(c, cmd);
    assert(start.status == CommandStatus::AwaitingResponse);
    assert((events == std::vector<int>{100, 0, 1}));
    assert(shop.owner == &g);
    assert(!talk.active);
    cmd.kind = CommandKind::Pass;
    auto blocked = execute_command(c, cmd);
    assert(blocked.status == CommandStatus::AwaitingResponse && g.turns_since_start == 0);
    cmd.kind = CommandKind::ShopAction;
    cmd.item = int16_t(ShopAction::Buy);
    execute_command(c, cmd);
    assert(shop.phase == ShopPhase::Buy);
    int count = 0;
    shop_offerings(g, shop, data, &count, [](void *p, ShopOffer) { ++*static_cast<int *>(p); });
    assert(count == 0);
    g.equipment_quantities[4] = 1;
    g.equipment_quantities[16] = 2;
    shop.phase = ShopPhase::Sell;
    ShopOffer selected{};
    assert(shop_offering_at(g, shop, data, 0, selected) && selected.item == 4 &&
           selected.quantity == 1);
    assert(shop_offering_at(g, shop, data, 1, selected) && selected.item == 16 &&
           selected.quantity == 2);
    assert(!shop_offering_at(g, shop, data, 2, selected));
    assert(shop_offering_count(g, shop, data) == 2);
    shop.phase = ShopPhase::Buy;
    auto before = g.gold;
    execute_shop(c, {ShopAction::SelectItem, 0});
    assert(g.gold == before && shop.phase == ShopPhase::Buy); // Absent stock is not item 0.
    execute_shop(c, {ShopAction::End});
    assert(shop.phase == ShopPhase::Closed);
    t.transport_tile = 0x10;
    npc.schedule.times[0] = 0;
    npc.schedule.times[1] = 0;
    auto closed = begin_shop(c, npc);
    assert(closed.status == CommandStatus::Rejected && shop.phase == ShopPhase::Closed);
    npc.schedule.times[0] = 1;
    auto mounted = begin_shop(c, npc);
    assert(mounted.status == CommandStatus::Rejected);
    t.transport_tile = 28;
    npc.schedule.dialog = 0x80;
    assert(begin_shop(c, npc).status == CommandStatus::Rejected);
    npc.schedule.dialog = 0x81;
    c.combat = true;
    assert(begin_shop(c, npc).status == CommandStatus::InvalidContext);
    c.combat = false;
    GameState other;
    CommandContext wrong{other, t, travel, commands, world};
    wrong.shop_services = &services;
    assert(execute_shop(wrong, {ShopAction::Buy}).status == CommandStatus::InvalidContext);
    g.position.map.location = 3;
    npc.schedule.dialog = 0x84;
    begin_shop(c, npc);
    execute_shop(c, {ShopAction::SelectItem, 0});
    before = g.gold;
    assert(execute_shop(c, {ShopAction::Confirm}).status == CommandStatus::Unsupported &&
           g.gold == before);
    services.ship = [](void *, int32_t, int32_t, int32_t, int32_t, int32_t) {};
    services.reserve = [](void *, bool) { return false; };
    assert(execute_shop(c, {ShopAction::Confirm}).status == CommandStatus::NeedsStorage &&
           g.gold == before);
    auto bad = buy_equipment(g, 256, 1);
    assert(!bad.ok && bad.reason == ShopFailure::InvalidInput && g.gold == before);
    bad = buy_reagent(g, 8, 1, 1);
    assert(!bad.ok && bad.reason == ShopFailure::InvalidInput);
    g.food = 0;
    g.gold = 65000;
    bad = buy_rations(g, 0, 255, 100);
    assert(bad.reason == ShopFailure::NumericRange && g.food == 0 && g.gold == 65000);

    // A resource-backed keeper greeting is transcript output before the first
    // transactional selector state.  This is the device Gwenno regression:
    // the selector must not replace the original shopkeeper conversation.
    struct Capture { std::vector<int> shop_kinds; std::vector<std::string> messages; } capture;
    c.events = {&capture, [](void *p, const GameEvent &e) {
                    auto &out = *static_cast<Capture *>(p);
                    if (e.kind == GameEventKind::Message && e.text) out.messages.emplace_back(e.text);
                    if (e.kind == GameEventKind::Shop && e.shop) {
                        out.shop_kinds.push_back(int(e.shop->kind));
                        if (e.shop->result && e.shop->result->message)
                            out.messages.emplace_back(e.shop->result->message);
                    }
                }};
    int barkeeper_location = 1;
    while (barkeeper_location <= 32 && shop_town_index(ShopType::Barkeeper, barkeeper_location) < 0)
        ++barkeeper_location;
    assert(barkeeper_location <= 32);
    ShopRecord record{barkeeper_location, ShopType::Barkeeper, 0, "The Wayfarer", "Gwenno"};
    data.records = &record; data.record_count = 1;
    services.record_present = [](void *, int32_t index) { return index >= 0; };
    const char *authored_text = "Welcome to #, says $. Good @.";
    services.context = &authored_text;
    services.record = [](void *p, int32_t) -> const char * {
        return *static_cast<const char **>(p);
    };
    shop = {}; g.position.map.location = uint8_t(barkeeper_location); t.transport_tile = 28;
    npc.schedule.dialog = 0x82; npc.schedule.times[0] = 1;
    auto greeted = begin_shop(c, npc);
    assert(greeted.status == CommandStatus::AwaitingResponse && shop.phase == ShopPhase::Greeting);
    assert(!capture.messages.empty());
    assert(capture.messages.front().find("Gwenno") != std::string::npos &&
           capture.messages.front().find("The Wayfarer") != std::string::npos);

    // Authoritative U5 shop records use single-character substitutions, not
    // printf.  They must be resolved synchronously before the borrowed text
    // reaches the transcript event sink.
    static int32_t prices[256]{};prices[4]=100;
    static int32_t weapons[8]={4,255,255,255,255,255,255,255};
    data.equipment_prices={prices,256};data.weapons={weapons,8};
    auto contains=[](const std::vector<std::string>&lines,const std::string&needle){
        for(const auto&line:lines)if(line.find(needle)!=std::string::npos)return true;
        return false;
    };
    authored_text="Not much demand for %s these days. I can only give thee % gp for it.";
    capture={};g.equipment_quantities[4]=1;shop.owner=&g;shop.record=&record;
    shop.type=ShopType::Blacksmith;shop.town=0;shop.phase=ShopPhase::Sell;
    execute_shop(c,{ShopAction::SelectItem,4});
    assert(shop.phase==ShopPhase::SellDeal&&contains(capture.messages,equipment_display_name(4))&&
           contains(capture.messages,std::to_string(shop.price))&&!contains(capture.messages,"%s")&&
           !contains(capture.messages,"%"));
    execute_shop(c,{ShopAction::Confirm});
    assert(g.equipment_quantities[4]==0);

    authored_text="For &, my price is % gold; I offer ^.";
    capture={};g.gold=1000;g.equipment_quantities[4]=0;shop.owner=&g;shop.record=&record;
    shop.type=ShopType::Blacksmith;shop.town=0;shop.phase=ShopPhase::Buy;
    execute_shop(c,{ShopAction::SelectItem,4});
    assert(shop.phase==ShopPhase::BuyDeal&&contains(capture.messages,equipment_display_name(4))&&
           contains(capture.messages,std::to_string(shop.price))&&!contains(capture.messages,"&")&&
           !contains(capture.messages,"%")&&!contains(capture.messages,"^"));
    execute_shop(c,{ShopAction::Confirm});
    assert(g.equipment_quantities[4]==1);

    capture={};g.gold=0;g.equipment_quantities[4]=0;shop.owner=&g;shop.record=&record;
    shop.type=ShopType::Blacksmith;shop.town=0;shop.phase=ShopPhase::Buy;
    execute_shop(c,{ShopAction::SelectItem,4});execute_shop(c,{ShopAction::Confirm});
    assert(!capture.messages.empty()&&!contains(capture.messages,"&")&&!contains(capture.messages,"%"));

    capture={};g.gold=1000;g.equipment_quantities[4]=99;shop.owner=&g;shop.record=&record;
    shop.type=ShopType::Blacksmith;shop.town=0;shop.phase=ShopPhase::Buy;
    execute_shop(c,{ShopAction::SelectItem,4});execute_shop(c,{ShopAction::Confirm});
    assert(shop.phase==ShopPhase::BuyFull&&!capture.messages.empty());

    int inn_location=1;while(inn_location<=32&&!inn_at(inn_location).present)++inn_location;
    assert(inn_location<=32);record.location=inn_location;record.type=ShopType::InnKeeper;
    authored_text="Welcome to #, says $.";capture={};g.gold=0;shop.owner=&g;shop.record=&record;
    shop.type=ShopType::InnKeeper;shop.location=inn_location;shop.phase=ShopPhase::InnRestDeal;
    services.hour_tiles=[](void*){};services.wake_npcs=[](void*){};
    execute_shop(c,{ShopAction::Confirm});
    assert(contains(capture.messages,"Gwenno")&&!contains(capture.messages,"$."));

    // T-Deck opts into price-first transactional services while the reference
    // adapter remains immediate.  Exercise the full generic inn contract.
    services.transactional_services = true;
    shop = {}; shop.owner = &g; shop.record = &record; shop.type = ShopType::InnKeeper;
    shop.location = inn_location; shop.town = shop_town_index(ShopType::InnKeeper, inn_location);
    shop.phase = ShopPhase::Menu; shop.transactional_services = true;
    g.position.map.location = uint8_t(inn_location); g.party.characters[0].status = 'G';
    g.party.characters[0].current_hp = 1; g.party.characters[0].max_hp = 100;
    g.gold = 1000;
    assert(execute_shop(c,{ShopAction::Rest}).status == CommandStatus::AwaitingResponse);
    const int inn_price = shop.price;
    assert(shop.phase == ShopPhase::InnRestDeal && inn_price > 0 && g.gold == 1000);
    assert(execute_shop(c,{ShopAction::Decline}).status == CommandStatus::Success &&
           shop.phase == ShopPhase::Closed && g.gold == 1000);

    shop = {}; shop.owner = &g; shop.record = &record; shop.type = ShopType::InnKeeper;
    shop.location = inn_location; shop.town = shop_town_index(ShopType::InnKeeper, inn_location);
    shop.phase = ShopPhase::Menu; shop.transactional_services = true; g.gold = uint16_t(inn_price - 1);
    execute_shop(c,{ShopAction::Rest});
    auto poor_rest = execute_shop(c,{ShopAction::Confirm});
    assert(poor_rest.status != CommandStatus::Unsupported && g.gold == inn_price - 1 &&
           shop.phase == ShopPhase::Closed);

    shop = {}; shop.owner = &g; shop.record = &record; shop.type = ShopType::InnKeeper;
    shop.location = inn_location; shop.town = shop_town_index(ShopType::InnKeeper, inn_location);
    shop.phase = ShopPhase::Menu; shop.transactional_services = true; g.gold = 1000;
    g.party.characters[0].current_hp = 1; g.time.hour = 12; g.time.minute = 0;
    execute_shop(c,{ShopAction::Rest});
    assert(shop.price == inn_price);
    auto rested = execute_shop(c,{ShopAction::Confirm});
    assert(rested.status == CommandStatus::Success && shop.phase == ShopPhase::Closed &&
           g.gold == 1000 - inn_price && g.party.characters[0].current_hp == 100 &&
           g.time.hour == 6);

    // Find an authoritative food-serving tavern and verify all three service
    // paths expose a cost before commitment.  Food must produce a real row.
    int tavern_location = 0;
    for (int location = 1; location <= 32 && !tavern_location; ++location) {
        const int town = shop_town_index(ShopType::Barkeeper, location);
        if (town < 0) continue;
        shop = {}; shop.owner = &g; shop.record = &record; shop.type = ShopType::Barkeeper;
        shop.location = location; shop.town = town; shop.phase = ShopPhase::Tavern;
        shop.transactional_services = true;
        if (execute_shop(c,{ShopAction::Rations}).status == CommandStatus::AwaitingResponse &&
            shop.phase == ShopPhase::RationsQuantity)
            tavern_location = location;
    }
    assert(tavern_location && shop.price > 0 && shop.quantity == 25 &&
           shop_offering_count(g,shop,data) == 1 && shop_offering_at(g,shop,data,0,selected) &&
           selected.item == 2 && selected.price == shop.price && selected.quantity == 25);
    record.location = tavern_location; record.type = ShopType::Barkeeper;
    services.tile = [](void *,int32_t,int32_t){return 0;};
    services.plate = [](void *,int32_t,int32_t,int32_t){};
    const int tavern_town = shop_town_index(ShopType::Barkeeper,tavern_location);
    shop = {}; shop.owner=&g; shop.record=&record; shop.type=ShopType::Barkeeper;
    shop.location=tavern_location; shop.town=tavern_town; shop.phase=ShopPhase::Tavern;
    shop.transactional_services=true; g.gold=1000;
    execute_shop(c,{ShopAction::Round}); const int round_price=shop.price;
    assert(shop.phase==ShopPhase::TavernRoundDeal&&round_price==tavern_round_price(g,tavern_town,false)&&
           round_price>0&&g.gold==1000);
    assert(execute_shop(c,{ShopAction::Confirm}).status!=CommandStatus::Unsupported&&
           g.gold==1000-round_price);
    shop.phase=ShopPhase::Tavern; g.gold=1000;
    execute_shop(c,{ShopAction::Drink}); const int drink_price=shop.price;
    assert(shop.phase==ShopPhase::TavernDrinkDeal&&drink_price==tavern_round_price(g,tavern_town,true)&&
           drink_price>0&&g.gold==1000);
    assert(execute_shop(c,{ShopAction::Confirm}).status!=CommandStatus::Unsupported&&
           g.gold==1000-drink_price);

    // The result and reference keeper response are emitted before Exited; the
    // UI session regression separately proves both survive the mode change.
    capture = {}; g.gold = 0; shop.owner = &g; shop.record = &record;
    shop.type = ShopType::Blacksmith; shop.phase = ShopPhase::BuyDeal;
    shop.item = 0; shop.price = 10; shop.quantity = 1; shop.buying = true;
    auto broke = execute_shop(c, {ShopAction::Confirm});
    assert(broke.status == CommandStatus::Success && shop.phase == ShopPhase::Closed);
    assert((capture.shop_kinds == std::vector<int>{int(ShopEventKind::Result),
                                                   int(ShopEventKind::Exited)}));
    assert(!capture.messages.empty());
    std::cout << "Shop handoff, event ordering, command blocking, availability, storage and range "
                 "contracts passed\n";
}
