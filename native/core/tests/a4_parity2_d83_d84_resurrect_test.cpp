// Alpha 4 A4-PARITY2 D-83 / D-84 (targets/tdeck/ALPHA4_UI.md section 16) -- the healer's Resurrect and
// the Refuge run the SHARED resurrection routine (CAST2.OVL 0x05e0 `resurrect_apply`, kernel stub 0x7ef6)
// and THEN copy the recomputed maximum over HP.
//
// Derivation (native/core/a4-parity2-findings/D83D84-FINAL.md; every address re-disassembled, the real
// bytes also executed in an independent 8086 interpreter over 160,000+ inputs):
//  * exactly four callers: CAST 0x10f3 (In Mani Corp, mode 0), CAST 0x12ee (scroll 6, mode 1),
//    SHOPPES 0x16f5 (healer, mode 0xff), BLCKTHRN 0x0b95 (Refuge, mode 0xff);
//  * for a member whose status byte is 'D': status 'G', HP 1, MP by class (A, M = INT; B = INT >> 1; any
//    other class untouched), then if karma (unsigned byte DS:0x5888) < 0x62 (98): XP = low 16 bits of
//    trunc(XP * karma / 100), and for EVERY karma level = 1 + bitlength(XP / 100), max HP = 30 * level.
//    No RNG, no karma write. Not 'D': nothing changes;
//  * SHOPPES 0x16f8-0x1703 and BLCKTHRN 0x0b98-0x0b9d then set HP := the member's NEW max HP (the Refuge's
//    copy is unconditional, even for a non-'D' member, and writes no status);
//  * the Refuge sees the karma the party DIED with: the floor of 75 is applied at 0x0bfd, after the loop.
// The ports' healer set 'G' and HP 1 and skipped the routine; their Refuge set HP := the STORED max.
//
//   R1 INIT.GAM's Avatar through the healer (XP 150, karma 75 -> XP 112, L2/60, HP 60, MP 15)
//   R2 every karma x XP boundary cell of the binary's arithmetic, through the healer AND the Refuge
//   R3 the threshold (97 cuts, 98 does not), truncation, level recomputed at karma >= 98, XP 0
//   R4 MP by class; the other classes are never zeroed
//   R5 the Refuge's worked rows; the death karma (74 cuts, the floor comes after the loop)
//   R6 only the party is revived; a mixed party (a 'P' member keeps its status, gets HP := max)
//   R7 agreement: the shared routine through the spell, the healer and the Refuge differs only in HP
//   R8 no RNG on any path; the payment and the gate of the healer are unchanged
#include "openu5/commands.h"
#include "openu5/magic.h"
#include "openu5/quest_world.h"
#include "openu5/shops.h"
#include "openu5/turn.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *id, const std::string &what) {
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s %s\n", ok ? "GREEN" : "RED", id, what.c_str());
}

struct Row {
    char status = 'D';
    char cls = 'F';
    int intel = 20, mp = 5, exp = 0, level = 5, max_hp = 150, hp = 0;
};
GameState party(const std::vector<Row> &rows, int karma, int party_size = -1, int gold = 5000) {
    GameState g{};
    g.party.character_count = uint8_t(rows.size());
    g.party.party_size = party_size < 0 ? int(rows.size()) : party_size;
    g.karma = uint8_t(karma);
    g.gold = uint16_t(gold);
    for (size_t i = 0; i < rows.size(); ++i) {
        auto &c = g.party.characters[i];
        std::snprintf(c.name, sizeof c.name, "M%zu", i);
        c.status = rows[i].status;
        c.character_class = rows[i].cls;
        c.intelligence = uint8_t(rows[i].intel);
        c.current_mp = uint8_t(rows[i].mp);
        c.exp = uint16_t(rows[i].exp);
        c.level = uint8_t(rows[i].level);
        c.max_hp = uint16_t(rows[i].max_hp);
        c.current_hp = uint16_t(rows[i].hp);
        c.party_status = 0;
    }
    return g;
}

struct Model { int exp, level, max_hp; };
// The binary's arithmetic, written from the disassembly (NOT from resurrect_apply).
Model model(int exp, int karma) {
    const int e = karma < 98 ? exp * karma / 100 : exp;
    int level = 1;
    for (int q = e / 100; q > 0; q >>= 1) ++level;
    return {e, level, 30 * level};
}

void heal(GameState &g, int i = 0) { healer_heal(g, i, HealerService::Resurrect, 200); }

// resolve_refuge needs a command context; the world is irrelevant to the revive.
struct Refuge {
    TurnState t;
    TravelState travel;
    CommandState commands;
    WorldData world;
    QuestWorldServices quest;
    Refuge(GameState &g, uint32_t seed = 0x1234) {
        g.rng.seed(seed);
        world.overworld = nullptr;
        world.overworld_size = 0;
        run(g);
    }
    void run(GameState &g) {
        CommandContext c{g, t, travel, commands, world};
        c.quest_world = &quest;
        resolve_refuge(c, EventSink{});
    }
};

const int kKarmas[] = {0, 1, 50, 74, 75, 96, 97, 98, 99, 100, 255};
const int kExps[] = {0, 1, 99, 100, 101, 199, 200, 399, 400, 799, 800, 1599, 1600, 3199, 3200, 6399, 6400, 9999};

void test_all() {
    // R1
    {
        auto g = party({{'D', 'A', 15, 0, 150, 2, 60, 0}}, 75, -1, 500);
        const auto r = healer_heal(g, 0, HealerService::Resurrect, 200);
        const auto &c = g.party.characters[0];
        check(r.ok && c.status == 'G' && c.exp == 112 && c.level == 2 && c.max_hp == 60 && c.current_hp == 60 && c.current_mp == 15 &&
                  g.gold == 300,
              "R1", "INIT.GAM's Avatar (A, INT 15, XP 150, L2/60) at karma 75: XP " + std::to_string(c.exp) + " (want 112), L" +
                        std::to_string(c.level) + ", max " + std::to_string(c.max_hp) + ", HP " + std::to_string(c.current_hp) +
                        " (want 60), MP " + std::to_string(c.current_mp) + " (want 15), gold " + std::to_string(g.gold));
    }
    // R2
    {
        int bad_heal = 0, bad_refuge = 0, cells = 0;
        std::string first;
        for (int k : kKarmas)
            for (int x : kExps) {
                const auto m = model(x, k);
                auto g = party({{'D', 'F', 20, 5, x, 5, 150, 0}}, k);
                heal(g);
                auto h = g;
                const auto &c = g.party.characters[0];
                const bool ok_h = c.status == 'G' && c.exp == m.exp && c.level == m.level && c.max_hp == m.max_hp && c.current_hp == m.max_hp;
                bad_heal += !ok_h;
                auto r = party({{'D', 'F', 20, 5, x, 5, 150, 0}}, k);
                Refuge rf(r);
                const auto &d = r.party.characters[0];
                const bool ok_r = d.status == 'G' && d.exp == m.exp && d.level == m.level && d.max_hp == m.max_hp && d.current_hp == m.max_hp;
                bad_refuge += !ok_r;
                if ((!ok_h || !ok_r) && first.empty()) first = "karma " + std::to_string(k) + " XP " + std::to_string(x);
                ++cells;
                (void)h;
            }
        check(bad_heal == 0 && bad_refuge == 0 && cells == 11 * 18, "R2",
              std::to_string(cells) + " karma x XP cells through the healer and the Refuge: " + std::to_string(bad_heal) + " + " +
                  std::to_string(bad_refuge) + " differ from the binary's arithmetic" + (first.empty() ? "" : " (first: " + first + ")"));
    }
    // R3
    {
        auto at = [](int karma, int exp, int old_level = 5, int old_max = 150) {
            auto g = party({{'D', 'F', 20, 5, exp, old_level, old_max, 0}}, karma);
            heal(g);
            const auto &c = g.party.characters[0];
            return std::vector<int>{c.exp, c.level, c.max_hp, c.current_hp};
        };
        const bool thr = at(97, 100) == std::vector<int>{97, 1, 30, 30} && at(98, 100) == std::vector<int>{100, 2, 60, 60} &&
                         at(99, 100) == std::vector<int>{100, 2, 60, 60};
        const bool trunc = at(97, 1)[0] == 0 && at(75, 150)[0] == 112;
        const bool relevel = at(99, 450, 2, 60) == std::vector<int>{450, 4, 120, 120};
        bool zero = true;
        for (int k : kKarmas) zero = zero && at(k, 0) == std::vector<int>{0, 1, 30, 30};
        check(thr && trunc && relevel && zero, "R3",
              "threshold 98 (97 cuts, 98 does not); truncation (XP 1 at 97 -> 0, 150 at 75 -> 112); level recomputed at karma 99 "
              "(XP 450, stored L2 -> L4/120); XP 0 -> L1/30 at every karma");
    }
    // R4
    {
        auto mp = [](char cls, int intel, int old) {
            auto g = party({{'D', cls, intel, old, 500, 5, 150, 0}}, 99);
            heal(g);
            return int(g.party.characters[0].current_mp);
        };
        const bool classes = mp('A', 15, 0) == 15 && mp('M', 22, 0) == 22 && mp('A', 255, 0) == 255 && mp('B', 17, 0) == 8 &&
                             mp('B', 18, 0) == 9 && mp('B', 1, 0) == 0 && mp('B', 255, 0) == 127;
        const bool untouched = mp('F', 20, 0) == 0 && mp('F', 20, 5) == 5 && mp('T', 20, 7) == 7;
        check(classes && untouched, "R4", "MP: A/M = INT, B = INT >> 1 (17 -> 8, 255 -> 127); F and T are never zeroed");
    }
    // R5
    {
        auto g = party({{'D', 'A', 20, 0, 0, 1, 30, 0}, {'D', 'B', 17, 3, 250, 3, 90, 0}, {'D', 'M', 22, 2, 800, 5, 150, 0}}, 50);
        Refuge rf(g);
        const auto &a = g.party.characters[0], &b = g.party.characters[1], &m = g.party.characters[2];
        const bool rows = a.status == 'G' && a.exp == 0 && a.level == 1 && a.max_hp == 30 && a.current_hp == 30 && a.current_mp == 20 &&
                          b.exp == 125 && b.level == 2 && b.max_hp == 60 && b.current_hp == 60 && b.current_mp == 8 && m.exp == 400 &&
                          m.level == 4 && m.max_hp == 120 && m.current_hp == 120 && m.current_mp == 22;
        auto d74 = party({{'D', 'F', 20, 5, 100, 5, 150, 0}}, 74);
        Refuge r74(d74);
        auto d75 = party({{'D', 'F', 20, 5, 100, 5, 150, 0}}, 75);
        Refuge r75(d75);
        check(rows && g.karma == 75 && d74.party.characters[0].exp == 74 && d74.karma == 75 && d75.party.characters[0].exp == 75, "R5",
              "Refuge worked rows (A XP 0 -> L1/30/HP 30/MP 20; B XP 250 -> 125 L2/60 MP 8; M XP 800 -> 400 L4/120 MP 22), karma "
              "afterwards 75; death karma 74 cuts XP 100 to 74 (the floor comes after the loop), karma 75 to 75");
    }
    // R6
    {
        std::vector<Row> rows(6, Row{'D', 'F', 20, 5, 400, 3, 90, 0});
        auto g = party(rows, 50, 3);
        const auto outside = g;
        Refuge rf(g);
        bool party_ok = true, rest_same = true;
        for (int i = 0; i < 3; ++i) party_ok = party_ok && g.party.characters[i].status == 'G' && g.party.characters[i].exp == 200;
        for (int i = 3; i < 6; ++i) rest_same = rest_same && std::memcmp(&g.party.characters[i], &outside.party.characters[i], sizeof(CharacterState)) == 0;
        auto mixed = party({{'D', 'F', 20, 5, 300, 3, 90, 0}, {'P', 'F', 20, 5, 300, 3, 90, 40}}, 50);
        Refuge rm(mixed);
        const auto &d = mixed.party.characters[0], &p = mixed.party.characters[1];
        check(party_ok && rest_same && d.status == 'G' && d.exp == 150 && d.level == 2 && d.current_hp == 60 && p.status == 'P' && p.exp == 300 &&
                  p.level == 3 && p.current_hp == 90,
              "R6", "roster 6 / party 3: members 3..5 untouched; mixed party: the 'D' member is resurrected (XP 150, L2, HP 60), the 'P' member "
                    "keeps status 'P' and gets HP := max (90)");
    }
    // R7
    {
        bool agree = true;
        for (int k : {0, 50, 97, 98, 99})
            for (int x : {0, 100, 450, 9999}) {
                Row row{'D', 'B', 17, 0, x, 5, 150, 0};
                auto spell = party({row}, k);
                Rand none{nullptr, [](void *, int32_t, int32_t) -> int32_t { return 0; }};
                apply_target_spell(spell.party.characters[0], MagicEffect::Resurrect, uint8_t(k), none);
                auto hg = party({row}, k);
                heal(hg);
                auto rg = party({row}, k);
                Refuge rf(rg);
                auto same = [](CharacterState a, CharacterState b) {
                    a.current_hp = b.current_hp = 0;
                    return std::memcmp(&a, &b, sizeof a) == 0;
                };
                const auto &s = spell.party.characters[0], &h = hg.party.characters[0], &r = rg.party.characters[0];
                agree = agree && s.current_hp == 1 && same(s, h) && same(s, r) && h.current_hp == h.max_hp && r.current_hp == r.max_hp;
            }
        check(agree, "R7", "the spell, the healer and the Refuge leave identical status / XP / level / max HP / MP and differ only in HP (1 vs the new max)");
    }
    // R8
    {
        auto g = party({{'D', 'F', 20, 5, 150, 5, 150, 0}}, 50, -1, 500);
        g.rng.seed(0x0c4c);
        heal(g);
        const bool no_rng_heal = g.rng.get_seed() == 0x0c4c;
        auto live = party({{'G', 'F', 20, 5, 150, 5, 150, 10}}, 50, -1, 500);
        const auto before = live.party.characters[0];
        const auto r = healer_heal(live, 0, HealerService::Resurrect, 200);
        const bool gate = !r.ok && live.gold == 500 && std::memcmp(&live.party.characters[0], &before, sizeof before) == 0;
        auto poor = party({{'D', 'F', 20, 5, 150, 5, 150, 0}}, 50, -1, 199);
        const auto rp = healer_heal(poor, 0, HealerService::Resurrect, 200);
        const bool gold = !rp.ok && poor.party.characters[0].status == 'D' && poor.party.characters[0].exp == 150;
        auto rg = party({{'D', 'F', 20, 5, 150, 5, 150, 0}}, 50);
        Refuge rf(rg, 0x0c4c);
        check(no_rng_heal && gate && gold && rg.rng.get_seed() == 0x0c4c, "R8",
              "no RNG on either path; a living member is refused unpaid with nothing changed; too little gold refuses with the member still dead");
    }
}
} // namespace

int main() {
    test_all();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
