#include "openu5/magic.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace openu5;
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream file(argv[1]);
    std::string line;
    int cases = 0;
    while (std::getline(file, line)) {
        std::istringstream in(line);
        int spell, v, seed;
        in >> spell >> v >> seed;
        std::vector<int> expected;
        for (int n; in >> n;)
            expected.push_back(n);
        GameState g{};
        TurnState t{};
        t.spell_turns = 0;
        t.wind_drift_counter = 9;
        auto &p = g.party.characters[0];
        p.status = "GPSD"[v % 4];
        p.character_class = "AMBF"[v % 4];
        p.current_hp = uint16_t(v % 3 == 0 ? 100 : 10);
        p.max_hp = 100;
        p.current_mp = uint8_t(v % 5 == 0 ? 0 : 30);
        p.level = uint8_t(v % 7 == 0 ? 1 : 8);
        p.intelligence = 20;
        p.exp = 1000;
        std::fill_n(g.spell_quantities, 48, v % 11 == 0 ? 0 : 3);
        std::fill_n(g.reagent_quantities, 8, v % 3 == 0 ? 0 : 10);
        g.food = 9998;
        g.worn_crown = v % 2 == 0;
        struct Stream {
            OriginalRng rng;
            std::vector<int> draws;
        } stream;
        stream.rng.seed(seed);
        Rand rng{&stream, [](void *p, int32_t lo, int32_t hi) -> int32_t {
                     auto &s = *static_cast<Stream *>(p);
                     int n = s.rng.next(lo, hi).value;
                     s.draws.insert(s.draws.end(), {lo, hi, n});
                     return n;
                 }};
        auto id = SpellId(spell);
        bool mix = mix_spell(g, id, v % 5 - 1);
        const int locations[] = {0, 1, 18, 29, 33, 128};
        auto r = cast_spell(g, t, p, id, {locations[v % 6], v % 2 == 1, -1, uint8_t(v % 6)}, rng);
        int heal = -1;
        auto kind = r.effect.kind;
        if (kind == MagicEffect::Mani || kind == MagicEffect::FullHeal ||
            kind == MagicEffect::Cure || kind == MagicEffect::Awaken ||
            kind == MagicEffect::Resurrect)
            heal = apply_target_spell(p, kind, uint8_t(v * 3), rng);
        std::string messages[] = {"", "Absorbed!", "Not here!", "None mixed!", "M.P. too low!"};
        int msg = -1;
        for (int i = 0; i < 5; ++i)
            if (messages[i] == r.message)
                msg = i;
        std::vector<int> got = {mix,           r.ok,
                                r.consumed,    msg,
                                heal,          stream.rng.get_seed(),
                                p.current_hp,  p.max_hp,
                                p.current_mp,  p.level,
                                p.exp,         p.status,
                                g.food,        t.time_spell,
                                t.spell_turns, t.light_spell_minutes,
                                t.wind,        t.wind_drift_counter};
        got.insert(got.end(), std::begin(g.spell_quantities), std::end(g.spell_quantities));
        got.insert(got.end(), std::begin(g.reagent_quantities), std::end(g.reagent_quantities));
        got.push_back(int(stream.draws.size()));
        got.insert(got.end(), stream.draws.begin(), stream.draws.end());
        if (got != expected) {
            size_t i = 0;
            while (i < got.size() && i < expected.size() && got[i] == expected[i])
                ++i;
            std::cerr << "magic " << spell << " variant " << v << " seed " << seed << " field " << i
                      << " got " << (i < got.size() ? got[i] : -999) << " expected "
                      << (i < expected.size() ? expected[i] : -999) << "\n";
            return 1;
        }
        ++cases;
    }
    if (!cases)
        return 2;
    std::cout << cases << " magic parity cases passed\n";
}
