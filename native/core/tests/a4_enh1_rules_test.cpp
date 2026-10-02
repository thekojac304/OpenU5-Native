// Alpha 4 A4-ENH1 (targets/tdeck/ALPHA4_UI.md section 10) -- the cheat and
// difficulty framework in core, through the real interfaces: apply_cheat(),
// turn housekeeping, the combat engine, the save codec and the sidecar
// export/import (the extras[] whitelist).
//
//   C  the player cheats (enhanced.h): God Mode, Heal, Cure, Add / Max Gold
//   P  their persistence: the "enhanced" key, absent at the defaults
//   R  the difficulty presets: Original's identity, the presets' numbers and
//      rounding, XP and incoming damage through the real combat engine, poison
//      and meals through the real housekeeping (across a save and load), the
//      encounter share, God Mode over a preset, and the saved difficulty
#include "openu5/combat.h"
#include "openu5/enhanced.h"
#include "openu5/loot.h"
#include "openu5/persistence.h"
#include "openu5/save_json.h"
#include "openu5/turn.h"

#include <cstdio>
#include <cstring>
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

GameState party_of(int n) {
    GameState g{};
    g.party.character_count = uint8_t(n);
    g.party.party_size = n;
    for (int i = 0; i < n; ++i) {
        auto &c = g.party.characters[i];
        std::snprintf(c.name, sizeof(c.name), "M%d", i);
        c.status = 'G';
        c.party_status = 0;
        c.character_class = 'F';
        c.current_hp = c.max_hp = 100;
        c.strength = c.dexterity = 20;
        c.helmet = c.armor = c.shield = c.ring = c.amulet = 255;
        c.weapon = 1;
    }
    return g;
}

void test_cheats() {
    GameState g = party_of(4);
    // C1. God Mode is a persistent toggle and marks the journey.
    auto r = apply_cheat(g, CheatKind::GodMode);
    check(r.applied && g.enhanced.god_mode && std::string(r.text) == "God Mode: ON" &&
              g.enhanced.cheats_used == cheat_bit(CheatKind::GodMode),
          "C1", "God Mode toggles ON and sets its cheats-used bit");
    // C2. Under God Mode no HP-loss site takes HP: apply_damage (poison,
    // starvation, fire, quakes, traps), a poison tick, the chest trap.
    g.party.characters[1].status = 'P';
    g.party.characters[1].current_hp = 3;
    TurnState t{};
    OriginalRng rng(7);
    for (int i = 0; i < 20; ++i) advance_turn(g, t, 1, rng_source(rng), nullptr);
    apply_damage(g, 0, 500);
    party_random_damage(g, rng_source(rng));
    int opener = 2;
    for (int i = 0; i < 40; ++i) chest_trap(g, 0, opener, rng_source(rng));
    bool whole = g.party.characters[1].current_hp == 3;
    for (int i : {0, 2, 3}) whole = whole && g.party.characters[i].current_hp == 100 && g.party.characters[i].status != 'D';
    check(whole && g.party.characters[1].status == 'P', "C2",
          "God Mode: 20 poison ticks, 500 damage, a random-damage hazard and 40 chest traps take no HP");
    // C3. ... and in the arena: an orc's blows land and take nothing.
    {
        GameState a = party_of(1);
        a.enhanced.god_mode = true;
        a.rng.seed(99);
        TurnState at{};
        CombatState s{};
        CombatEnemy orc{};
        orc.index = 12; orc.name = "orc"; orc.group_name = "orcs"; orc.hp = 400; orc.strength = 30;
        orc.dexterity = 30; orc.damage = 30; orc.range = 1; orc.max_per_map = 1; orc.tile = 0x70;
        const CombatEnemy *enemies[] = {&orc};
        CombatMap map{};
        for (auto &tile : map.tiles) tile = 5;
        map.start_count[2] = 1; map.starts[2][0] = {5, 6};
        map.unit_count = 1; map.units[0] = {5, 5};
        CombatContext c{a, at, s};
        int hits = 0;
        c.events = {&hits, [](void *p, const CombatEvent &e) { if (e.kind == CombatEventKind::Attacked) ++*static_cast<int *>(p); }};
        initialize_combat(c, map, CombatDirection::South, enemies, 1);
        for (int i = 0; i < 300 && !combat_over(s); ++i) {
            CombatActor *cur = current_combat_actor(c);
            if (!cur) break;
            if (cur->member == 255) combat_action(c, CombatAction::EnemyStep);
            else combat_action(c, CombatAction::Pass);
        }
        check(hits > 5 && a.party.characters[0].current_hp == 100 && s.actors[0].hp == 100 &&
                  s.actors[0].status == CombatStatus::Active,
              "C3", "God Mode in combat: " + std::to_string(hits) + " blows land, the member keeps 100 HP");
        a.enhanced.god_mode = false;
        for (int i = 0; i < 300 && s.actors[0].hp == 100 && !combat_over(s); ++i) {
            CombatActor *cur = current_combat_actor(c);
            if (!cur) break;
            if (cur->member == 255) combat_action(c, CombatAction::EnemyStep);
            else combat_action(c, CombatAction::Pass);
        }
        check(s.actors[0].hp < 100, "C4", "God Mode off: the same arena hurts again (control)");
    }
    // C5. Heal and Cure: the living only, through the roster fields; refused in combat.
    GameState h = party_of(4);
    h.party.characters[0].current_hp = 10;
    h.party.characters[1].status = 'P';
    h.party.characters[2].status = 'S';
    h.party.characters[3].status = 'D';
    h.party.characters[3].current_hp = 0;
    r = apply_cheat(h, CheatKind::HealParty, 0, true);
    check(!r.applied && h.party.characters[0].current_hp == 10 && std::string(r.text) == "Not during combat" &&
              h.enhanced.cheats_used == 0,
          "C5", "Heal Party in combat: refused, nothing changed, nothing marked");
    r = apply_cheat(h, CheatKind::HealParty);
    check(r.applied && h.party.characters[0].current_hp == 100 && h.party.characters[3].current_hp == 0 &&
              h.party.characters[3].status == 'D' && std::string(r.text) == "Healed: 1",
          "C6", "Heal Party: the hurt member to max HP; the dead stay dead (revive is not a cheat yet)");
    r = apply_cheat(h, CheatKind::CureParty);
    check(r.applied && h.party.characters[1].status == 'G' && h.party.characters[2].status == 'G' &&
              h.party.characters[3].status == 'D' && std::string(r.text) == "Cured: 2",
          "C7", "Cure Party: poison and sleep cured; the dead untouched");
    r = apply_cheat(h, CheatKind::CureParty);
    check(!r.applied && std::string(r.text) == "No one needs curing", "C8", "a second Cure: nothing to do");
    // C9. Gold: the add amount is clamped, the cap 9999 is never passed nor lowered.
    h.gold = 9950;
    r = apply_cheat(h, CheatKind::AddGold, 100);
    check(r.applied && h.gold == 9999 && std::string(r.text) == "Gold: 9999 (+49)", "C9", "Add Gold +100 at 9950: 9999");
    r = apply_cheat(h, CheatKind::AddGold, 100);
    check(!r.applied && h.gold == 9999, "C10", "at 9999 Add Gold does nothing");
    h.gold = 60000; // a Developer preset can leave more
    r = apply_cheat(h, CheatKind::MaxGold);
    check(!r.applied && h.gold == 60000, "C11", "Max Gold never lowers gold above the cap");
    h.gold = 0;
    r = apply_cheat(h, CheatKind::AddGold, -500);
    check(!r.applied && h.gold == 0, "C12", "a negative amount adds nothing");
    r = apply_cheat(h, CheatKind::AddGold, 1000000);
    check(r.applied && h.gold == 9999, "C13", "an absurd amount clamps to the cap");
    h.gold = 5;
    r = apply_cheat(h, CheatKind::MaxGold);
    check(r.applied && h.gold == 9999 &&
              h.enhanced.cheats_used == (cheat_bit(CheatKind::HealParty) | cheat_bit(CheatKind::CureParty) |
                                         cheat_bit(CheatKind::AddGold) | cheat_bit(CheatKind::MaxGold)),
          "C14", "Max Gold: 9999; the journey's cheats-used bits name exactly the cheats applied");
    r = apply_cheat(h, CheatKind::Count);
    check(!r.applied, "C15", "an unknown cheat applies nothing");
}

save::Json state_doc(const GameState &g) {
    save::Json doc = save::Json::object();
    save::capture_core(g, TurnState{}, doc);
    return doc;
}

void test_persistence(const char *init_gam) {
    // P1. The defaults write no key; anything else writes "enhanced".
    GameState plain = party_of(2);
    check(!state_doc(plain).has("enhanced"), "P1", "a journey with no cheat saves no \"enhanced\" key");
    GameState god = party_of(2);
    apply_cheat(god, CheatKind::GodMode);
    apply_cheat(god, CheatKind::AddGold, 10);
    const auto doc = state_doc(god);
    check(doc.has("enhanced") && doc["enhanced"]["godMode"].truth() &&
              doc["enhanced"]["cheatsUsed"].integer() == int64_t(cheat_bit(CheatKind::GodMode) | cheat_bit(CheatKind::AddGold)),
          "P2", "God Mode on: \"enhanced\": {godMode: true, cheatsUsed: 9}");
    // P3. Through the real save file: the sidecar keeps it (persistence.cpp
    // extras[]), a load restores it, and the .GAM is the same bytes.
    std::ifstream in(init_gam, std::ios::binary);
    const std::vector<uint8_t> base((std::istreambuf_iterator<char>(in)), {});
    if (base.size() < 0x1000) {
        check(false, "P3", "INIT.GAM not readable");
        return;
    }
    // The device's own path: a journey is INIT.GAM imported (New Journey),
    // played, exported (the save) and imported again (the load).
    GameState journey{};
    TurnState journey_turn{};
    save::Json journey_doc;
    save::SidecarSource source{};
    if (save::load_native_state(base.data(), base.size(), nullptr, journey, journey_turn, journey_doc, source) !=
        save::Error::None) {
        check(false, "P3", "INIT.GAM does not import");
        return;
    }
    auto round = [&](const EnhancedState &e, GameState &back, std::string &side_text, save::Gam &gam) {
        GameState g = journey;
        g.gold = 777;
        g.enhanced = e;
        save::Json side;
        if (save::export_native_state(g, journey_turn, journey_doc, base.data(), base.size(), gam, side) != save::Error::None)
            return false;
        if (save::encode_json(side, side_text) != save::JsonError::None) return false;
        TurnState t{};
        save::Json kept;
        save::SidecarSource from{};
        return save::load_native_state(gam.data(), gam.size(), &side_text, back, t, kept, from) == save::Error::None &&
               back.gold == 777;
    };
    GameState back{};
    std::string side_text;
    save::Gam gam_god{}, gam_plain{};
    const bool god_ok = round(god.enhanced, back, side_text, gam_god);
    check(god_ok && side_text.find("\"enhanced\"") != std::string::npos && back.enhanced.god_mode &&
              back.enhanced.cheats_used == god.enhanced.cheats_used,
          "P3", "export -> sidecar text -> load: God Mode and the cheats-used bits survive");
    GameState plain_back{};
    std::string plain_side;
    const bool plain_ok = round(EnhancedState{}, plain_back, plain_side, gam_plain);
    check(plain_ok && plain_side.find("enhanced") == std::string::npos && !plain_back.enhanced.god_mode &&
              plain_back.enhanced.cheats_used == 0 && gam_god == gam_plain,
          "P4", "the same journey without Enhanced state: no key in the sidecar, and the .GAM is byte-identical");
    // P5. Malformed values take their defaults; the save still loads.
    save::Json bad = state_doc(god);
    bad["enhanced"]["godMode"] = save::Json(7);
    bad["enhanced"]["cheatsUsed"] = save::Json("lots");
    GameState b{};
    TurnState bt{};
    check(save::restore_core(bad, b, bt) == save::Error::None && !b.enhanced.god_mode && b.enhanced.cheats_used == 0,
          "P5", "a malformed \"enhanced\" value is the default, not a refused save");
    // P6. A pre-A4-ENH1 save (no key) loads as the defaults.
    save::Json old = state_doc(plain);
    GameState o{};
    o.enhanced.god_mode = true; // restore_core builds a fresh state, never keeps the caller's
    check(save::restore_core(old, o, bt) == save::Error::None && enhanced_is_default(o.enhanced), "P6",
          "an older save (no key) loads with God Mode off and no cheats used");
}
// One enemy, one fighter: `strike` true = the fighter attacks (and with a
// 99-attack weapon kills in one blow), false = it passes while the enemy hits.
struct Duel {
    GameState g = party_of(1);
    TurnState t{};
    CombatState s{};
    CombatEnemy orc{};
    const CombatEnemy *enemies[1]{&orc};
    int32_t attack[256]{}, range[256]{}, defense[256]{};
    std::vector<int> hits; // damage of every hit on the party, in order
    explicit Duel(Difficulty d, int seed = 0x77) {
        g.enhanced.difficulty = d;
        g.rng.seed(seed);
        g.party.characters[0].current_hp = g.party.characters[0].max_hp = 5000; // nobody dies inside the budget
        orc.index = 12; orc.name = "orc"; orc.group_name = "orcs"; orc.hp = 40; orc.strength = 25;
        orc.dexterity = 25; orc.damage = 40; orc.range = 1; orc.max_per_map = 1; orc.tile = 0x70;
        attack[1] = 99; range[1] = 1;
        defense[20] = 10; // armour: an orc's hit is 40 - rand(1, 10), so the hits vary
        g.party.characters[0].armor = 20;
    }
    CombatContext ctx() {
        CombatContext c{g, t, s};
        c.tables.attack = attack; c.tables.range = range; c.tables.defense = defense; c.tables.count = 256;
        c.events = {this, [](void *p, const CombatEvent &e) {
                        auto &self = *static_cast<Duel *>(p);
                        if (e.kind == CombatEventKind::Attacked && e.target == 1 && e.damage > 0) self.hits.push_back(e.damage);
                    }};
        return c;
    }
    void fight(bool strike, int actions) {
        CombatMap map{};
        for (auto &tile : map.tiles) tile = 5;
        map.start_count[2] = 1; map.starts[2][0] = {5, 6};
        map.unit_count = 1; map.units[0] = {5, 5};
        auto c = ctx();
        initialize_combat(c, map, CombatDirection::South, enemies, 1);
        for (int i = 0; i < actions && !combat_over(s) && !s.victory; ++i) {
            CombatActor *cur = current_combat_actor(c);
            if (!cur) break;
            if (cur->member == 255) combat_action(c, CombatAction::EnemyStep);
            else if (strike) combat_action(c, CombatAction::Attack, 5, 5);
            else combat_action(c, CombatAction::Pass);
        }
    }
};

void test_rules(const char *init_gam) {
    const GameState original = party_of(1);
    // R1. Original is the identity at every hook, over the whole domain tried.
    bool same = true;
    for (int d = -5; d <= 300; ++d)
        same = same && rules_incoming_damage(original, d) == d && rules_outgoing_damage(original, d) == d &&
               rules_xp_award(original, d) == d;
    GameState walk = original;
    for (int64_t turn = -50; turn < 20000 && same; ++turn) {
        walk.turns_since_start = turn;
        walk.time.year = 139 + int32_t(turn / 1000);
        walk.time.month = 1 + int32_t((turn / 84) % 13);
        walk.time.day = 1 + int32_t((turn / 3) % 28);
        walk.time.hour = int32_t(6 * (1 + turn % 3));
        same = rules_poison_due(walk) && rules_meal_due(walk) && rules_encounter_allowed(walk);
    }
    const auto &o = gameplay_rules(Difficulty::Original);
    check(same && o.incoming_damage_pct == 100 && o.outgoing_damage_pct == 100 && o.xp_pct == 100 &&
              o.poison_interval == 1 && o.hunger_pct == 100 && o.encounter_pct == 100,
          "R1", "Original returns every input unchanged: damage -5..300, XP, 20,050 turns of poison, meals, spawns");
    // R2. The provisional presets (section 10's table), and an out-of-range one falls back to Original.
    // A4-ENH2 (changed on purpose): Easy hits 120 % and meets 65 % of the spawns.
    const auto &rx = gameplay_rules(Difficulty::Relaxed), &ez = gameplay_rules(Difficulty::Easy);
    check(rx.incoming_damage_pct == 85 && rx.outgoing_damage_pct == 100 && rx.xp_pct == 150 && rx.poison_interval == 4 &&
              rx.hunger_pct == 75 && rx.encounter_pct == 90 && ez.incoming_damage_pct == 65 &&
              ez.outgoing_damage_pct == 120 && ez.xp_pct == 200 && ez.poison_interval == 10 && ez.hunger_pct == 50 &&
              ez.encounter_pct == 65 && &gameplay_rules(Difficulty(7)) == &o,
          "R2", "Relaxed 85/100/150/4/75/90, Easy 65/120/200/10/50/65; an unknown preset is Original");
    // R3. XP: x1.5 and x2, rounded half up, never below 1.
    GameState relaxed = original, easy = original;
    relaxed.enhanced.difficulty = Difficulty::Relaxed;
    easy.enhanced.difficulty = Difficulty::Easy;
    check(rules_xp_award(relaxed, 1) == 2 && rules_xp_award(relaxed, 11) == 17 && rules_xp_award(relaxed, 64) == 96 &&
              rules_xp_award(easy, 1) == 2 && rules_xp_award(easy, 11) == 22 && rules_xp_award(easy, 64) == 128 &&
              rules_xp_award(relaxed, 0) == 0,
          "R3", "XP 1 / 11 / 64 -> Relaxed 2 / 17 / 96, Easy 2 / 22 / 128 (round half up)");
    // R4. Incoming damage: 85 % / 65 %, a hit stays at least 1, 99 keeps its kill-outright meaning and is never made.
    check(rules_incoming_damage(relaxed, 10) == 9 && rules_incoming_damage(relaxed, 1) == 1 &&
              rules_incoming_damage(easy, 10) == 7 && rules_incoming_damage(easy, 2) == 1 &&
              rules_incoming_damage(easy, 99) == 99 && rules_incoming_damage(relaxed, 116) == 98 &&
              rules_incoming_damage(easy, 0) == 0,
          "R4", "hits 10 -> 9 / 7, 1 -> 1, 2 -> 1 (Easy), 99 stays 99, a scaled 99 becomes 98, 0 stays 0");
    // R5. XP through the real combat kill(): one 40-HP orc ((40 >> 2) + 1 = 11 XP), slain in one blow.
    int xp[3]{};
    for (int d = 0; d < 3; ++d) {
        Duel duel{static_cast<Difficulty>(d)};
        duel.fight(true, 40);
        xp[d] = duel.g.party.characters[0].exp;
    }
    Duel capped(Difficulty::Easy);
    capped.g.party.characters[0].exp = 9990;
    capped.fight(true, 40);
    check(xp[0] == 11 && xp[1] == 17 && xp[2] == 22 && capped.g.party.characters[0].exp == 9999, "R5",
          "a real kill: Original 11 XP, Relaxed 17, Easy 22 (applied once); the 9999 cap still holds (9990 + 22)");
    // R6. Incoming damage through the real combat damage(): the same fight, the
    // same draws -- every Easy hit is the Original hit scaled, nothing else moves.
    Duel o_duel(Difficulty::Original), e_duel(Difficulty::Easy), r_duel(Difficulty::Relaxed);
    o_duel.fight(false, 120);
    e_duel.fight(false, 120);
    r_duel.fight(false, 120);
    bool scaled = o_duel.hits.size() >= 5 && o_duel.hits.size() == e_duel.hits.size() &&
                  o_duel.hits.size() == r_duel.hits.size();
    for (size_t i = 0; scaled && i < o_duel.hits.size(); ++i)
        scaled = e_duel.hits[i] == rules_incoming_damage(easy, o_duel.hits[i]) &&
                 r_duel.hits[i] == rules_incoming_damage(relaxed, o_duel.hits[i]);
    int distinct = 0;
    for (size_t i = 1; i < o_duel.hits.size(); ++i) distinct += o_duel.hits[i] != o_duel.hits[0];
    check(scaled && distinct > 0 && o_duel.s.rng.get_seed() == e_duel.s.rng.get_seed(), "R6",
          "an orc's " + std::to_string(o_duel.hits.size()) +
              " hits: each Easy / Relaxed hit is the Original hit at 65 % / 85 %, with the same RNG draws");
    // R7. Poison through the real housekeeping: 40 turns, one poisoned member.
    auto poison_loss = [](Difficulty d, int turns, int save_at) {
        GameState g = party_of(2);
        g.enhanced.difficulty = d;
        g.party.characters[1].status = 'P';
        g.food = 500;
        g.time.year = 139; g.time.month = 1; g.time.day = 1; g.time.hour = 9;
        TurnState t{};
        t.prev_hour = g.time.hour;
        OriginalRng rng(3);
        for (int i = 0; i < turns; ++i) {
            if (i == save_at) { // a save and a load in the middle
                save::Json doc = save::Json::object();
                save::capture_core(g, t, doc);
                std::string text;
                save::encode_json(doc, text);
                GameState back{};
                TurnState back_turn{};
                save::Json kept;
                save::load_state(text, back, back_turn, kept);
                back.rng = g.rng;
                g = back;
                t = back_turn;
            }
            advance_turn(g, t, 1, rng_source(rng), nullptr);
        }
        return 100 - int(g.party.characters[1].current_hp);
    };
    const int lo = poison_loss(Difficulty::Original, 40, -1), lr = poison_loss(Difficulty::Relaxed, 40, -1),
              le = poison_loss(Difficulty::Easy, 40, -1);
    const int lr_saved = poison_loss(Difficulty::Relaxed, 40, 17), le_saved = poison_loss(Difficulty::Easy, 40, 23);
    check(lo == 40 && lr == 10 && le == 4 && lr_saved == lr && le_saved == le, "R7",
          "40 poisoned turns: Original 40 HP, Relaxed 10 (every 4th), Easy 4 (every 10th); a save and load midway "
          "changes nothing");
    // R8. Meals through the real housekeeping: 8 days of turns (24 meals), two eaters.
    auto eaten = [](Difficulty d, int save_at) {
        GameState g = party_of(2);
        g.enhanced.difficulty = d;
        g.food = 900;
        g.time.year = 139; g.time.month = 2; g.time.day = 3; g.time.hour = 4; g.time.minute = 0;
        TurnState t{};
        t.prev_hour = g.time.hour;
        OriginalRng rng(5);
        for (int i = 0; i < 8 * 24 * 6; ++i) { // 10-minute turns
            if (i == save_at) {
                save::Json doc = save::Json::object();
                save::capture_core(g, t, doc);
                std::string text;
                save::encode_json(doc, text);
                GameState back{};
                TurnState back_turn{};
                save::Json kept;
                save::load_state(text, back, back_turn, kept);
                g = back;
                t = back_turn;
            }
            advance_turn(g, t, 10, rng_source(rng), nullptr);
        }
        return 900 - int(g.food);
    };
    const int fo = eaten(Difficulty::Original, -1), fr = eaten(Difficulty::Relaxed, -1), fe = eaten(Difficulty::Easy, -1);
    check(fo == 48 && fr == 36 && fe == 24 && eaten(Difficulty::Relaxed, 500) == fr && eaten(Difficulty::Easy, 777) == fe,
          "R8", "8 days, 2 eaters: Original eats 48 (24 meals), Relaxed 36 (18), Easy 24 (12); a save and load midway "
                "changes nothing");
    // R9. Encounters: the share of passed spawn rolls that spawn, over 100,000 turns.
    int allowed[3]{};
    GameState e = original;
    for (int d = 0; d < 3; ++d) {
        e.enhanced.difficulty = Difficulty(d);
        for (int64_t turn = 0; turn < 100000; ++turn) {
            e.turns_since_start = turn;
            allowed[d] += rules_encounter_allowed(e);
        }
    }
    // A4-ENH2 (changed on purpose): Easy 65 %.
    check(allowed[0] == 100000 && allowed[1] > 89000 && allowed[1] < 91000 && allowed[2] > 64000 && allowed[2] < 66000,
          "R9", "spawns allowed per 100,000 turns: Original " + std::to_string(allowed[0]) + ", Relaxed " +
                    std::to_string(allowed[1]) + ", Easy " + std::to_string(allowed[2]));
    // R10. God Mode wins over any preset.
    Duel god(Difficulty::Easy);
    god.g.enhanced.god_mode = true;
    god.fight(false, 60);
    check(god.hits.empty() && god.g.party.characters[0].current_hp == 5000, "R10", "God Mode on Easy: no HP lost");
    // R11. The difficulty is saved under "enhanced" and comes back; names are checked.
    GameState jr = party_of(1);
    jr.enhanced.difficulty = Difficulty::Easy;
    save::Json doc = save::Json::object();
    save::capture_core(jr, TurnState{}, doc);
    GameState back{};
    TurnState bt{};
    const bool round = save::restore_core(doc, back, bt) == save::Error::None && back.enhanced.difficulty == Difficulty::Easy;
    save::Json odd = doc;
    odd["enhanced"]["difficulty"] = save::Json("brutal");
    GameState odd_back{};
    const bool unknown = save::restore_core(odd, odd_back, bt) == save::Error::None &&
                         odd_back.enhanced.difficulty == Difficulty::Original;
    jr.enhanced.difficulty = Difficulty::Original;
    save::Json plain = save::Json::object();
    save::capture_core(jr, TurnState{}, plain);
    check(round && doc["enhanced"]["difficulty"].string == save::Json("easy").string && unknown && !plain.has("enhanced"),
          "R11", "\"difficulty\": \"easy\" saved and restored; an unknown name loads as Original; Original alone saves no key");
    (void)init_gam;
}
} // namespace

int main(int argc, char **argv) {
    test_cheats();
    test_persistence(argc > 1 ? argv[1] : "");
    test_rules(argc > 1 ? argv[1] : "");
    std::printf("\nA4-ENH1 rules: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
