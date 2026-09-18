#include "openu5/shop_orchestration.h"
#include "shop_test_state.h"
#include <vector>
#include "../build-shops/assets.inc"
struct Owner {
    int calls = 0;
    std::vector<int> effects;
};
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream in(argv[1]);
    int id, v, n, count = 0;
    while (in >> id >> v >> n) {
        GameState g;
        TurnState t;
        init(g, t, v);
        g.position.map.location = uint8_t(records[id].location);
        TravelState travel;
        travel.shadowlord_here = int8_t(v % 4 - 1);
        CommandState commands;
        WorldData world;
        CommandContext c{g, t, travel, commands, world};
        ShopSession session;
        ShopData data;
        data.records = records;
        data.record_count = std::size(records);
#define A(field, array) data.field = {array, std::size(array)}
        A(equipment_prices, equipmentBasePrices);
        A(weapons, weaponsSoldByMerchants);
        A(reagent_prices, reagentBasePrices);
        A(reagent_grants, reagentQuantities);
        A(heal_prices, healPrices);
        A(cure_prices, curePrices);
        A(resurrect_prices, resurrectPrices);
#undef A
        Owner owner;
        ShopServices services{session, data};
        services.context = &owner;
        services.record_present = [](void *, int32_t i) {
            return i >= 0 && i < int(std::size(present)) && present[i];
        };
        services.tile = [](void *, int32_t, int32_t) { return 149; };
        services.occupied = [](void *, int32_t, int32_t) { return false; };
        services.reserve = [](void *, bool) { return true; };
        services.plate = [](void *p, int32_t x, int32_t y, int32_t tile) {
            auto &e = static_cast<Owner *>(p)->effects;
            e.insert(e.end(), {1, x, y, tile});
        };
        services.ship = [](void *p, int32_t x, int32_t y, int32_t tile, int32_t hull,
                           int32_t skiffs) {
            auto &e = static_cast<Owner *>(p)->effects;
            e.insert(e.end(), {2, x, y, tile, hull, skiffs});
        };
        services.horse = [](void *p, int32_t x, int32_t y) {
            auto &e = static_cast<Owner *>(p)->effects;
            e.insert(e.end(), {4, x, y});
        };
        services.hour_tiles = [](void *p) { ++static_cast<Owner *>(p)->calls; };
        services.wake_npcs = [](void *p) { static_cast<Owner *>(p)->effects.push_back(3); };
        if (v >= 30 && v < 60)
            services.record_present = nullptr;
        if (v >= 60) {
            data.records = nullptr;
            data.record_count = 0;
        }
        if (records[id].type == ShopType::HorseSeller)
            services.tile = [](void *, int32_t, int32_t) { return 5; };
        c.shop_services = &services;
        NpcActor npc;
        npc.schedule.dialog = uint8_t(0x81 + int(records[id].type));
        npc.schedule.times[0] = uint8_t(g.time.hour + 1);
        npc.schedule.times[1] = uint8_t(g.time.hour);
        npc.schedule.times[2] = 0;
        npc.schedule.times[3] = 0;
        // Times [0,0,0,0] select slot 1 through the shared schedule 3->1 rule.
        begin_shop(c, npc);
        auto hash = [&]() {
            Hash h;
            TavernResult bar;
            bar.message = nullptr;
            h.n(digest(g, t, {}, bar, owner.calls));
            for (int z : {int(g.position.xy.x), int(g.position.xy.y), int(g.karma), t.drunk_turns})
                h.n(z);
            h.n(int(session.phase));
            h.n(session.item); h.n(session.phase==ShopPhase::ShipDeal?0:session.price); h.n(session.quantity);
            for (int z : {int(session.purchased), int(session.bought_in_buy), int(session.selling),
                          int(session.buying), int(session.thrown_out), int(session.tavern_served),
                          session.served})
                h.n(z);
            std::vector<ShopOffer> offers;
            shop_offerings(g,session,data,&offers,[](void*p,ShopOffer o){static_cast<std::vector<ShopOffer>*>(p)->push_back(o);});
            h.n(offers.size());for(auto o:offers){h.n(o.item);h.n(o.price);h.n(o.quantity);}
            for (int z : owner.effects)
                h.n(z);
            return h.h;
        };
        uint32_t expected;
        in >> expected;
        if (hash() != expected) {
            std::cerr << "start mismatch shop=" << id << " v=" << v
                      << " phase=" << int(session.phase) << "\n";
            return 1;
        }
        ++count;
        for (int step = 0; step < n; ++step) {
            int action, value;
            in >> action >> value >> expected;
            if (session.phase != ShopPhase::Closed) {
                ShopInput input{ShopAction(action), value};
                if (input.action == ShopAction::Text) {
                    input.text = texts[value];
                    input.length = std::char_traits<char16_t>::length(input.text);
                }
                execute_shop(c, input);
            }
            auto actual = hash();
            if (actual != expected) {
                std::cerr << "flow mismatch shop=" << id << " v=" << v << " step=" << step
                          << " action=" << action << " value=" << value
                          << " phase=" << int(session.phase) << " expected=" << expected
                          << " actual=" << actual << "\n";
                return 1;
            }
            ++count;
        }
    }
    std::cout << count << " real shop flow snapshots passed\n";
    return count ? 0 : 2;
}
