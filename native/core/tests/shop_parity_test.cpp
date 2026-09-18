#include "shop_test_state.h"
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream in(argv[1]);
    int op, v, a, b, d, count = 0;
    while (in >> op) {
        if (op == 0) {
            int buy, sell;
            in >> a >> b >> buy >> sell;
            if (shop_buy_price(a, uint8_t(b)) != buy || shop_sell_price(a, uint8_t(b)) != sell)
                return 1;
            ++count;
            continue;
        }
        in >> v >> a >> b >> d;
        GameState g;
        TurnState t;
        init(g, t, v);
        if (op == 18)
            g.gold = uint16_t(d);
        int calls = 0;
        for (int j = 0; j < 3; ++j) {
            ShopResult r;
            TavernResult bar;
            bar.message = nullptr;
            auto rand = rng_source(g.rng);
            switch (op) {
            case 18:
            case 1:
                r = buy_equipment(g, a, b);
                break;
            case 2:
                r = sell_equipment(g, a, b);
                break;
            case 3:
                r = buy_reagent(g, a, b, d);
                break;
            case 4:
                r = buy_guild_item(g, a, b, g.party.characters[0].intelligence);
                break;
            case 5:
                r = buy_horse(g, a, g.party.characters[0].intelligence);
                break;
            case 6:
                r = buy_ship(g, a, b != 0, g.party.characters[0].intelligence);
                break;
            case 7:
                r = buy_wine(g, a);
                break;
            case 8:
                r = buy_rations(g, a, g.party.characters[0].intelligence, b);
                break;
            case 9:
                r = healer_heal(g, a, HealerService(b), d);
                break;
            case 10:
                r = inn_rest(g, a, b);
                break;
            case 11:
                r = inn_leave(g, a, b);
                break;
            case 12:
                r = inn_pickup(g, a, b, d);
                break;
            case 13:
                bar = buy_tavern_round(g, a, b, 149, d != 0);
                r = bar;
                break;
            case 14:
                r = pay_rumor(g, a);
                break;
            case 15:
                inn_night_pass(g, t, rand, {&calls, [](void *p) { ++*static_cast<int *>(p); }});
                break;
            case 16:
                r.bought = shop_post_purchase_drain(g, v % 4 - 1, rand);
                break;
            default:
                return 2;
            }
            uint32_t expected;
            in >> expected;
            auto actual = digest(g, t, r, bar, calls);
            if (actual != expected) {
                std::cerr << "shop mismatch op=" << op << " v=" << v << " args=" << a << "," << b
                          << "," << d << " step=" << j << " expected=" << expected
                          << " actual=" << actual << "\n";
                return 1;
            }
            ++count;
        }
    }
    std::cout << count << " shop helper snapshots passed\n";
    return count ? 0 : 2;
}
