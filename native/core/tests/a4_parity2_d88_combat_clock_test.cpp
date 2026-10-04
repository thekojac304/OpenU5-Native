// Alpha 4 A4-PARITY2 D-88 (targets/tdeck/ALPHA4_UI.md section 16) -- the 1988 combat advances the game clock ONE
// MINUTE every TEN unit activations.
//
// Derivation (native/core/a4-parity2-findings/D88-FINAL.md; every address re-disassembled):
//  * COMBAT.OVL's round loop (file 0x0B94) runs `inc byte [0x5882]` at 0x0C64 for every unit ACTIVATION that survives
//    four skip tests (empty slot, gone slot, a party member whose roster status is 'D', a unit on tile 0x84/0x85) and
//    whose initiative countdown reaches 0. `cmp byte [0x5882],0xA / jne` (equality, 8-bit wrap): at the 10th the byte is
//    zeroed BEFORE the call and `advance_clock(1)` (kernel 0x4F7C) runs, at the START of that activation -- after the
//    countdown reload, before the AI / human turn routine, so before any of that unit's text, prompt, sound or RNG draw.
//    It is the plain clock routine: not a world turn, no housekeeping.
//  * `[0x5882]` is NOT combat-local. Nothing initialises it at combat entry or exit; it lies inside the 0x1060-byte
//    SAVED.GAM window at file offset 0x2DC and is loaded and saved verbatim: it survives fights, saves and loads.
//  * Inside an arena g_location is 0xFF (ULTIMA.EXE 0x5FB4 .. 0x6091): the sky strip / moon latch is skipped, and the
//    midnight Shadowlord re-roll excludes no town (the compare is against 0xFF), drawing from the fight's own stream.
//  * advance_clock in a fight: minute + 1 (one carry), torch and light-spell minutes - 1, hour / day / month / year
//    rollover. It does NOT touch food, HP, status or the turn counter: that is housekeeping, which runs only in world
//    loops. So no meal, starvation damage or poison tick ever happens inside a fight; a crossed hour is charged at the
//    next housekeeping iff the LAST nonzero advance before it is the one that crossed.
//
// Neither port had any of it: `action_count` counted activations and nothing read it.
//
//   K1 9 activations: no minute; the 10th: +1, the counter 0, torch -1; the tick is at the START of the activation
//   K2 11 / 19 / 20 / 21 / 30 activations; the counter equals the activations mod 10 over a long fight
//   K3 the counter survives a fight (a loaded 7 ticks after 3; two fights in a row); equality with 8-bit wrap (255)
//   K4 a minute / hour rollover; no meal, starvation, poison or turn counter inside a fight; the swallow / charge rule
//   K5 Time Stop 'T' and Quickness 'Q'
//   K6 the midnight Shadowlord re-roll: from the fight's stream, and a town equal to the live location is NOT excluded
//   K7 the byte round-trips through SAVED.GAM +0x2DC and survives a save and a load
//   K9 every side counts (an enemy's activation is an activation); the live location never matters inside a fight
//   K8 the PC bridge carries it: an original SAVED.GAM with +0x2DC = 7 imports as 7 and exports back as 7 (and 0 as 0)
//
//   a4_parity2_d88_combat_clock <game/assets/init.gam> [<original SAVED.GAM> <original SAVED.OOL>]
#include "openu5/combat.h"
#include "openu5/pc_save.h"
#include "openu5/persistence.h"
#include "openu5/turn.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
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

CombatEnemy make_foe() {
    CombatEnemy e;
    e.name = "Orc";
    e.index = 1;
    e.tile = 64;
    e.hp = 50;
    e.dexterity = 1;
    return e;
}
const CombatEnemy kFoe = make_foe();

struct Fx {
    GameState game{};
    TurnState turn{};
    CombatState battle{};
    CombatContext arena;
    explicit Fx(bool foe_acts = false) : arena{game, turn, battle} {
        game.party.party_size = game.party.character_count = 2;
        for (int i = 0; i < 2; ++i) {
            auto &m = game.party.characters[i];
            std::snprintf(m.name, sizeof m.name, "M%d", i);
            m.party_status = 0;
            m.status = 'G';
            m.current_hp = m.max_hp = 999;
            m.dexterity = 20;
        }
        game.time = {139, 4, 5, 12, 30};
        turn.prev_hour = 12;
        game.torch_turns = 50;
        game.food = 100;
        battle.initialized = true;
        battle.count = 3;
        for (int i = 0; i < 2; ++i) start_actor(i);
        auto &foe = battle.actors[2];
        foe.id = 3;
        foe.member = 255;
        foe.enemy = &kFoe;
        foe.status = CombatStatus::Active;
        foe.position = {9, 5};
        foe.counter = foe_acts ? 1 : 255; // far from its turn unless the test wants the enemy to act too
        foe.speed = foe_acts ? 35 : 1;
        foe.hp = foe.max_hp = 50;
        battle.current = -1;
    }
    void start_actor(int i) {
        auto &a = battle.actors[i];
        a.id = int16_t(i + 1);
        a.member = uint8_t(i);
        a.status = CombatStatus::Active;
        a.position = {int8_t(3 + i), 5};
        a.counter = 1; // due on every scan: each scan is one activation
        a.speed = 35;
        a.hp = a.max_hp = 999;
    }
    /** Run until `n` activations have BEGUN: the nth unit is the current one, its tick done, it has not acted. */
    void run_to(uint32_t n) {
        for (int guard = 0; guard < 4000; ++guard) {
            auto *a = current_combat_actor(arena);
            if (!a || battle.action_count >= n) return;
            combat_action(arena, a->enemy ? CombatAction::EnemyStep : CombatAction::Pass);
        }
    }
    std::string clock() const {
        char b[16];
        std::snprintf(b, sizeof b, "%d:%02d", game.time.hour, game.time.minute);
        return b;
    }
};

Rand scripted(const std::vector<int32_t> &values, size_t &used) {
    struct Ctx { const std::vector<int32_t> *v; size_t *n; };
    static Ctx ctx;
    ctx = {&values, &used};
    return Rand{&ctx, [](void *p, int32_t, int32_t) -> int32_t {
        auto &c = *static_cast<Ctx *>(p);
        const int32_t v = (*c.v)[*c.n % c.v->size()];
        ++*c.n;
        return v;
    }};
}

void test_sides_and_location() {
    // K9. The enemy acts too: the 10th activation overall ticks, whoever it is.
    Fx both(true);
    both.run_to(9);
    const bool none_at_9 = both.clock() == "12:30" && both.turn.combat_clock == 9;
    Fx both10(true);
    both10.run_to(10);
    // Whatever the live location, the midnight re-roll inside a fight consults g_location == 0xFF: the outcome must not depend on it.
    std::vector<std::array<int32_t, 3>> outcomes;
    for (int location : {1, 2, 3, 4, 5, 6, 7, 8}) {
        Fx f;
        f.game.time = {139, 4, 5, 23, 59};
        f.turn.has_shadowlords = true;
        f.turn.shadowlord_locations = {{0, 0, 0}};
        f.game.position.map.location = location;
        f.battle.rng.seed(0x4242);
        f.run_to(10);
        outcomes.push_back(f.turn.shadowlord_locations);
    }
    bool independent = true;
    for (const auto &o : outcomes) independent = independent && o == outcomes[0];
    check(none_at_9 && both10.clock() == "12:31" && both10.turn.combat_clock == 0 && independent, "K9",
          "an enemy's activation counts like a player's (9: " + std::string(none_at_9 ? "no minute" : "a minute") + ", the 10th: " + both10.clock() +
              "); the midnight re-roll in a fight gives the same Shadowlords whatever the live town (1..8): " + (independent ? "yes" : "NO"));
}

std::vector<uint8_t> slurp(const char *path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

void test_pc_bridge(const char *gam_path, const char *ool_path) {
    auto gam = slurp(gam_path);
    const auto ool = slurp(ool_path);
    if (gam.size() != 4192 || ool.size() != 512) {
        check(false, "K8", "the original SAVED.GAM / SAVED.OOL are not readable (" + std::string(gam_path) + ")");
        return;
    }
    auto through = [&](uint8_t byte) {
        gam[0x2dc] = byte;
        GameState g{};
        TurnState t{};
        save::Json doc;
        const bool ok = save::pc::check_original(gam.data(), gam.size(), ool.data(), ool.size()) == save::pc::Check::Ok &&
                        save::pc::import_original(gam.data(), ool.data(), g, t, doc) == save::Error::None;
        save::Gam out{};
        save::Json side;
        save::Ool out_ool{};
        // The export's base is a DIFFERENT file with its own byte at +0x2DC: it must not leak through.
        std::vector<uint8_t> base = gam;
        base[0x2dc] = 99;
        const bool exported = ok && save::export_native_state(g, t, doc, base.data(), base.size(), out, side) == save::Error::None;
        if (exported) save::pc::complete_export(doc, out, out_ool);
        return std::pair<int, int>{ok ? int(t.combat_clock) : -1, exported ? int(out[0x2dc]) : -1};
    };
    const auto seven = through(7), zero = through(0);
    check(seven.first == 7 && seven.second == 7 && zero.first == 0 && zero.second == 0, "K8",
          "the PC bridge: an original SAVED.GAM with +0x2DC = 7 imports as " + std::to_string(seven.first) + " and exports back as " +
              std::to_string(seven.second) + "; 0 imports as " + std::to_string(zero.first) + " and exports as " + std::to_string(zero.second) +
              " even over a base file whose byte is 99");
}

void test_all(const char *init_gam) {
    // K1
    {
        Fx a;
        a.run_to(9);
        const std::string at9 = a.clock();
        const int c9 = a.turn.combat_clock, torch9 = a.game.torch_turns;
        Fx b;
        b.run_to(10);
        check(at9 == "12:30" && c9 == 9 && torch9 == 50 && b.clock() == "12:31" && b.turn.combat_clock == 0 && b.game.torch_turns == 49 &&
                  b.turn.prev_hour == 12,
              "K1", "9 activations: " + at9 + " counter " + std::to_string(c9) + "; the 10th: " + b.clock() + ", counter " +
                        std::to_string(b.turn.combat_clock) + ", torch " + std::to_string(b.game.torch_turns) +
                        " (want 12:30 / 9 and 12:31 / 0 / torch 49); the tick is already done when the 10th unit is handed over, before it acts");
    }
    // K2
    {
        struct Row { uint32_t n; const char *at; int counter; };
        const Row rows[] = {{11, "12:31", 1}, {19, "12:31", 9}, {20, "12:32", 0}, {21, "12:32", 1}, {30, "12:33", 0}};
        std::string bad;
        for (const auto &r : rows) {
            Fx f;
            f.run_to(r.n);
            if (f.clock() != r.at || f.turn.combat_clock != r.counter) bad += " " + std::to_string(r.n) + "->" + f.clock() + "/" + std::to_string(f.turn.combat_clock);
        }
        Fx long_fight;
        bool invariant = true;
        for (uint32_t n = 1; n <= 45; ++n) {
            long_fight.run_to(n);
            invariant = invariant && long_fight.turn.combat_clock == n % 10;
        }
        check(bad.empty() && invariant && long_fight.clock() == "12:34", "K2",
              "11 / 19 / 20 / 21 / 30 activations: 12:31 / 12:31 / 12:32 / 12:32 / 12:33 and counter 1 / 9 / 0 / 1 / 0" +
                  (bad.empty() ? "" : " -- wrong:" + bad) + "; counter == activations mod 10 over 45 (" + long_fight.clock() + ")");
    }
    // K3
    {
        Fx a;
        a.run_to(9);
        const bool carried = a.turn.combat_clock == 9 && a.clock() == "12:30";
        // a second fight on the same TurnState: nothing re-initialises the byte
        CombatState second{};
        second.initialized = true;
        second.count = 3;
        CombatContext ctx2{a.game, a.turn, second};
        for (int i = 0; i < 2; ++i) {
            auto &x = second.actors[i];
            x.id = int16_t(i + 1); x.member = uint8_t(i); x.status = CombatStatus::Active; x.position = {int8_t(3 + i), 5};
            x.counter = 1; x.speed = 35; x.hp = x.max_hp = 999;
        }
        second.actors[2].id = 3; second.actors[2].member = 255; second.actors[2].enemy = &kFoe;
        second.actors[2].status = CombatStatus::Active; second.actors[2].position = {9, 5};
        second.actors[2].counter = 255; second.actors[2].speed = 1; second.actors[2].hp = second.actors[2].max_hp = 50;
        second.current = -1;
        current_combat_actor(ctx2);
        const bool next_fight = a.turn.combat_clock == 0 && a.clock() == "12:31";
        Fx seven;
        seven.turn.combat_clock = 7;
        seven.run_to(3);
        Fx wrap;
        wrap.turn.combat_clock = 255;
        wrap.run_to(1);
        const bool wrapped = wrap.turn.combat_clock == 0 && wrap.clock() == "12:30";
        wrap.run_to(11);
        const bool then_ticks = wrap.turn.combat_clock == 0 && wrap.clock() == "12:31";
        Fx twelve;
        twelve.turn.combat_clock = 12;
        twelve.run_to(253);
        const bool long_wait = twelve.clock() == "12:30";
        twelve.run_to(254);
        check(carried && next_fight && seven.turn.combat_clock == 0 && seven.clock() == "12:31" && wrapped && then_ticks && long_wait &&
                  twelve.clock() == "12:31",
              "K3", "the counter survives a fight (9, then the next fight's first activation ticks); a loaded 7 ticks after 3; a loaded 255 wraps to 0 with "
                    "no tick and ticks on the 11th; a loaded 12 ticks only at the 254th");
    }
    // K4
    {
        Fx roll;
        roll.game.time.minute = 59;
        roll.turn.light_spell_minutes = 5;
        roll.run_to(10);
        const bool rollover = roll.clock() == "13:00" && roll.turn.prev_hour == 12 && roll.game.torch_turns == 49 && roll.turn.light_spell_minutes == 4;
        Fx hunger;
        hunger.game.food = 0;
        hunger.game.party.characters[1].status = 'P';
        hunger.game.time = {139, 4, 5, 8, 55};
        hunger.turn.prev_hour = 8;
        const auto hp = hunger.game.party.characters[1].current_hp;
        hunger.run_to(100);
        const bool none = hunger.clock() == "9:05" && hunger.game.food == 0 && hunger.game.party.characters[1].status == 'P' &&
                          hunger.game.party.characters[1].current_hp == hp && hunger.game.turns_since_start == 0;
        // 05:59 + one tick -> 06:00 (prev 5); the next world advance(2) overwrites prev_hour before housekeeping: the meal is SWALLOWED
        Fx swallow;
        swallow.game.time = {139, 4, 5, 5, 59};
        swallow.turn.prev_hour = 5;
        swallow.run_to(10);
        const bool pending = swallow.clock() == "6:00" && swallow.turn.prev_hour == 5;
        Rand none_rand{nullptr, [](void *, int32_t, int32_t) -> int32_t { return 0; }};
        advance_clock(swallow.game, swallow.turn, 2, &none_rand);
        turn_housekeeping(swallow.game, swallow.turn, none_rand);
        const bool swallowed = swallow.clock() == "6:02" && swallow.game.food == 100;
        Fx charged;
        charged.game.time = {139, 4, 5, 5, 58};
        charged.turn.prev_hour = 5;
        charged.run_to(10);
        advance_clock(charged.game, charged.turn, 2, &none_rand);
        turn_housekeeping(charged.game, charged.turn, none_rand);
        const bool meal = charged.clock() == "6:01" && charged.game.food == 98; // two members eat
        check(rollover && none && pending && swallowed && meal, "K4",
              "12:59 -> 13:00 keeps prev_hour 12, torch and light -1; 100 activations at food 0 with a poisoned member change no food / HP / status / turn "
              "counter (ten minutes pass); the 06:00 crossed by a tick is swallowed by the next world advance, a 05:58 start still charges the meal");
    }
    // K5
    {
        Fx t;
        t.turn.time_spell = 'T';
        t.game.time = {139, 4, 5, 5, 59};
        t.turn.prev_hour = 4;
        t.run_to(10);
        Fx q;
        q.turn.time_spell = 'Q';
        q.run_to(10);
        check(t.clock() == "5:59" && t.turn.prev_hour == 5 && t.game.torch_turns == 50 && q.clock() == "12:31", "K5",
              "Time Stop: the tick only snapshots prev_hour (a pending flank is discarded), no minute, no torch; Quickness: 1 minute, not 0");
    }
    // K6
    {
        // from the FIGHT's stream, not the live one
        Fx f;
        f.game.time = {139, 4, 5, 23, 59};
        f.turn.has_shadowlords = true;
        f.turn.shadowlord_locations = {{0, 0, 0}};
        f.game.position.map.location = 5;
        f.game.rng.seed(0x1111);
        f.battle.rng.seed(0x2222);
        f.run_to(10);
        const bool stream = f.game.rng.get_seed() == 0x1111 && f.battle.rng.get_seed() != 0x2222 && f.game.time.hour == 0 && f.game.time.day == 6;
        bool all_set = true;
        for (auto l : f.turn.shadowlord_locations) all_set = all_set && l >= 1 && l <= 8;
        // absent slots draw nothing
        Fx absent;
        absent.game.time = {139, 4, 5, 23, 59};
        absent.turn.has_shadowlords = true;
        absent.turn.shadowlord_locations = {{128, 128, 128}};
        absent.battle.rng.seed(0x2222);
        absent.run_to(10);
        // an exact exclusion check through the parameter the combat tick passes (0xFF): a scripted first draw equal to the live town is ACCEPTED
        auto roll = [](int32_t party_location) {
            Fx x;
            x.game.time = {139, 4, 5, 23, 59};
            x.turn.has_shadowlords = true;
            x.turn.shadowlord_locations = {{0, 0, 0}};
            x.game.position.map.location = 5;
            std::vector<int32_t> script{5, 6, 7, 8, 1, 2, 3, 4};
            size_t used = 0;
            Rand r = scripted(script, used);
            advance_clock(x.game, x.turn, 1, &r, nullptr, party_location);
            return x.turn.shadowlord_locations[0];
        };
        check(stream && all_set && absent.battle.rng.get_seed() == 0x2222 && roll(INT32_MIN) == 6 && roll(0xff) == 5, "K6",
              "the midnight Shadowlord re-roll draws from the fight's stream (the live seed is untouched) and absent slots draw nothing; with the live location 5 "
              "a first draw of 5 is rejected, and with g_location 0xFF (the arena) it is accepted");
    }
    // K7
    {
        std::ifstream f(init_gam, std::ios::binary);
        const std::vector<uint8_t> base((std::istreambuf_iterator<char>(f)), {});
        GameState g{};
        TurnState t{};
        save::Json journey;
        save::SidecarSource source{};
        const bool loaded = base.size() >= 0x1000 &&
                            save::load_native_state(base.data(), base.size(), nullptr, g, t, journey, source) == save::Error::None;
        const bool starts_zero = loaded && t.combat_clock == 0;
        t.combat_clock = 7;
        save::Gam gam{};
        save::Json side;
        std::string side_text;
        const bool exported = loaded && save::export_native_state(g, t, journey, base.data(), base.size(), gam, side) == save::Error::None &&
                              save::encode_json(side, side_text) == save::JsonError::None;
        GameState g2{};
        TurnState t2{};
        save::Json r2;
        save::SidecarSource s2{};
        const bool back = exported && save::load_native_state(gam.data(), gam.size(), &side_text, g2, t2, r2, s2) == save::Error::None;
        // The encoder writes an EXPLICIT 0 when the document has no counter: a template (base) file's byte must never leak through.
        std::vector<uint8_t> dirty = base;
        dirty[0x2dc] = 99;
        t.combat_clock = 0;
        save::Gam zero_gam{};
        save::Json zero_side;
        const bool zero_ok = loaded && save::export_native_state(g, t, journey, dirty.data(), dirty.size(), zero_gam, zero_side) == save::Error::None;
        check(zero_ok && zero_gam[0x2dc] == 0, "K7b", "a counter of 0 exports as 0 over a base file whose byte is 99 (the template byte never leaks)");
        check(starts_zero && exported && gam[0x2dc] == 7 && back && t2.combat_clock == 7, "K7",
              "init.gam starts at 0; the byte is written to SAVED.GAM +0x2DC (" + (exported ? std::to_string(int(gam[0x2dc])) : std::string("n/a")) +
                  ") and a load gives it back (" + std::to_string(int(t2.combat_clock)) + ")");
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a4_parity2_d88_combat_clock <game/assets/init.gam>\n");
        return 2;
    }
    test_all(argv[1]);
    test_sides_and_location();
    if (argc >= 4) test_pc_bridge(argv[2], argv[3]);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
