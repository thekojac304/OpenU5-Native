// Alpha 4 A4-ENH1 (targets/tdeck/ALPHA4_UI.md section 10) -- the cheat and
// difficulty framework in core, through the real interfaces: apply_cheat(),
// turn housekeeping, the combat engine, the save codec and the sidecar
// export/import (the extras[] whitelist).
//
//   C  the player cheats (enhanced.h): God Mode, Heal, Cure, Add / Max Gold
//   P  their persistence: the "enhanced" key, absent at the defaults
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
} // namespace

int main(int argc, char **argv) {
    test_cheats();
    test_persistence(argc > 1 ? argv[1] : "");
    std::printf("\nA4-ENH1 rules: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
