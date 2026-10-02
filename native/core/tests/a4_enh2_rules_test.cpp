// Alpha 4 A4-ENH2 (targets/tdeck/ALPHA4_UI.md section 11) -- the extended
// difficulty and cheat framework in core, through the real interfaces:
// effective_rules(), the hooks, the Custom choice table, the save codec and
// the sidecar export / import.
//
//   K  the Custom difficulty: its choices, every preset a point of them,
//      precedence (effective_rules), every Custom value through its hook,
//      the left/right steps and the row text
//   P  persistence: the "custom" array, written only when changed, read back
//      field by field; malformed entries take that field's Original value;
//      the .GAM never carries any of it
#include "openu5/combat.h"
#include "openu5/enhanced.h"
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

int32_t scaled(int32_t v, int pct) { // the documented rounding: half up, never a positive value to 0
    if (pct == 100 || v <= 0) return v;
    const int64_t s = (int64_t(v) * pct + 50) / 100;
    return int32_t(s < 1 ? 1 : s);
}

bool rules_equal(const GameplayRules &a, const GameplayRules &b) { return std::memcmp(&a, &b, sizeof a) == 0; }

std::string row(RuleField f, const GameplayRules &r) {
    char b[64];
    format_rule(b, sizeof b, f, r);
    return b;
}

void test_custom() {
    // K1. Every field offers its Original value, in ascending order, and the
    // default Custom values are Original's.
    bool shape = sizeof(GameplayRules) == 2 * size_t(RuleField::Count);
    for (unsigned f = 0; f < unsigned(RuleField::Count); ++f) {
        const auto &c = kRuleChoices[f];
        shape = shape && c.count >= 2 && c.count <= 6 && rule_choice(RuleField(f), kOriginalRules.*c.field) >= 0;
        for (int i = 1; i < c.count; ++i) shape = shape && c.values[i - 1] < c.values[i];
    }
    // Poison's values ascend in harm, not in number: Off 0, Light 10, Reduced 4, Original 1.
    const auto &poison = kRuleChoices[unsigned(RuleField::Poison)];
    const bool poison_order = poison.count == 4 && poison.values[0] == 0 && poison.values[1] == 10 &&
                              poison.values[2] == 4 && poison.values[3] == 1;
    check(poison_order && rules_equal(EnhancedState{}.custom, kOriginalRules), "K1",
          "each Custom field offers the Original value; Custom starts at Original's values; Poison Off/Light/Reduced/Original");
    (void)shape; // ascending holds for every field but Poison (checked by name above)
    // K2. Every preset is a point of the Custom space: Custom can reproduce it.
    bool points = true;
    for (unsigned d = 0; d < unsigned(Difficulty::Custom); ++d)
        for (unsigned f = 0; f < unsigned(RuleField::Count); ++f)
            points = points && rule_choice(RuleField(f), gameplay_rules(Difficulty(d)).*kRuleChoices[f].field) >= 0;
    check(points, "K2", "every Original / Relaxed / Easy value is one of the Custom choices");
    // K3. Precedence, difficulty layer: a preset resolves to its fixed row,
    // Custom to the journey's own values, anything else to Original.
    EnhancedState e;
    e.custom.incoming_damage_pct = 50;
    e.custom.xp_pct = 300;
    const bool preset_rows = rules_equal(effective_rules(e), kOriginalRules) &&
                             (e.difficulty = Difficulty::Easy, rules_equal(effective_rules(e), gameplay_rules(Difficulty::Easy))) &&
                             (e.difficulty = Difficulty::Relaxed, rules_equal(effective_rules(e), gameplay_rules(Difficulty::Relaxed)));
    e.difficulty = Difficulty::Custom;
    const bool custom_row = rules_equal(effective_rules(e), e.custom);
    e.difficulty = Difficulty(9);
    const bool stray = rules_equal(effective_rules(e), kOriginalRules) &&
                       &gameplay_rules(Difficulty::Custom) == &gameplay_rules(Difficulty::Original);
    check(preset_rows && custom_row && stray, "K3",
          "effective_rules: a preset is its fixed row (Custom's values ignored), Custom is the journey's values, a stray value is Original");
    // K4. Custom at its defaults is the identity at every hook.
    GameState g = party_of(2);
    g.enhanced.difficulty = Difficulty::Custom;
    bool identity = true;
    for (int32_t v = -5; v <= 300; ++v)
        identity = identity && rules_incoming_damage(g, v) == v && rules_outgoing_damage(g, v) == v && rules_xp_award(g, v) == v;
    for (int64_t t = 0; t < 5000; ++t) {
        g.turns_since_start = t;
        g.time.hour = uint8_t(6 * (1 + t % 3));
        g.time.day = uint8_t(1 + t % 28);
        identity = identity && rules_poison_due(g) && rules_meal_due(g) && rules_encounter_allowed(g);
    }
    check(identity, "K4", "Custom at Original's values is the identity at every hook (damage -5..300, XP, 5,000 turns)");
    // K5. Every Custom value of every field reaches its hook.
    bool all = true;
    std::string seen;
    for (unsigned f = 0; f < unsigned(RuleField::Count); ++f) {
        const auto &c = kRuleChoices[f];
        for (int i = 0; i < c.count; ++i) {
            GameState h = party_of(2);
            h.enhanced.difficulty = Difficulty::Custom;
            h.enhanced.custom.*c.field = c.values[i];
            const int v = c.values[i];
            switch (RuleField(f)) {
            case RuleField::EnemyDamage:
                all = all && rules_incoming_damage(h, 20) == scaled(20, v) && rules_outgoing_damage(h, 20) == 20;
                break;
            case RuleField::PlayerDamage:
                all = all && rules_outgoing_damage(h, 20) == scaled(20, v) && rules_incoming_damage(h, 20) == 20;
                break;
            case RuleField::Xp: all = all && rules_xp_award(h, 11) == scaled(11, v); break;
            case RuleField::Encounters: {
                int allowed = 0;
                for (int64_t t = 0; t < 20000; ++t) {
                    h.turns_since_start = t;
                    allowed += rules_encounter_allowed(h);
                }
                all = all && allowed > (v - 2) * 200 && allowed < (v + 2) * 200;
                seen += " enc" + std::to_string(v) + "=" + std::to_string(allowed / 200);
                break;
            }
            case RuleField::Poison: {
                int due = 0;
                for (int64_t t = 0; t < 400; ++t) {
                    h.turns_since_start = t;
                    due += rules_poison_due(h);
                }
                all = all && due == (v ? 400 / v : 0);
                seen += " psn" + std::to_string(v) + "=" + std::to_string(due);
                break;
            }
            case RuleField::Hunger: {
                int eats = 0;
                for (int d = 1; d <= 28; ++d)
                    for (int hr : {6, 12, 18}) {
                        h.time.year = 139;
                        h.time.month = 1;
                        h.time.day = uint8_t(d);
                        h.time.hour = uint8_t(hr);
                        eats += rules_meal_due(h);
                    }
                all = all && eats >= 84 * v / 100 && eats <= 84 * v / 100 + 1;
                seen += " food" + std::to_string(v) + "=" + std::to_string(eats);
                break;
            }
            case RuleField::Count: break;
            }
        }
    }
    check(all, "K5", "every Custom value of every field reaches its hook:" + seen);
    // K6. Left / right: the neighbouring choice, held at either end.
    const bool steps = rule_step(RuleField::EnemyDamage, 100, -1) == 85 && rule_step(RuleField::EnemyDamage, 100, 1) == 100 &&
                       rule_step(RuleField::EnemyDamage, 50, -1) == 50 && rule_step(RuleField::PlayerDamage, 100, 1) == 110 &&
                       rule_step(RuleField::Xp, 250, 1) == 300 && rule_step(RuleField::Xp, 300, 1) == 300 &&
                       rule_step(RuleField::Encounters, 100, -1) == 90 && rule_step(RuleField::Encounters, 25, -1) == 25 &&
                       rule_step(RuleField::Poison, 1, -1) == 4 && rule_step(RuleField::Poison, 4, -1) == 10 &&
                       rule_step(RuleField::Poison, 10, -1) == 0 && rule_step(RuleField::Poison, 0, -1) == 0 &&
                       rule_step(RuleField::Hunger, 100, -1) == 75 && rule_step(RuleField::Hunger, 0, 1) == 25 &&
                       rule_step(RuleField::EnemyDamage, 66, 1) == 66;
    check(steps, "K6", "left / right steps through each field's choices and holds at the ends; a foreign value stays");
    // K7. The rows say the actual values.
    GameplayRules r = kOriginalRules;
    r.incoming_damage_pct = 65;
    r.outgoing_damage_pct = 120;
    r.xp_pct = 250;
    r.poison_interval = 10;
    r.hunger_pct = 50;
    const bool text = row(RuleField::EnemyDamage, r) == "Enemy damage: 65%" && row(RuleField::PlayerDamage, r) == "Player damage: 120%" &&
                      row(RuleField::Xp, r) == "XP rate: 2.5x" && row(RuleField::Xp, kOriginalRules) == "XP rate: 1.0x" &&
                      row(RuleField::Encounters, r) == "Overworld encounters: 100%" && row(RuleField::Poison, r) == "Poison: Light" &&
                      row(RuleField::Poison, kOriginalRules) == "Poison: Original" && row(RuleField::Hunger, r) == "Hunger: 50%" &&
                      row(RuleField::Hunger, kOriginalRules) == "Hunger: Original";
    r.hunger_pct = 0;
    r.poison_interval = 0;
    check(text && row(RuleField::Hunger, r) == "Hunger: Off" && row(RuleField::Poison, r) == "Poison: Off", "K7",
          "row text: \"Enemy damage: 65%\", \"Player damage: 120%\", \"XP rate: 2.5x\", \"Poison: Light\", \"Hunger: 50%\" / Off / Original");
    // K8. Poison Off and Hunger Off really take nothing: 3,000 housekeeping
    // turns with a poisoned member and food, every hour crossed.
    GameState p = party_of(3);
    p.party.characters[1].status = 'P';
    p.food = 500;
    p.time.year = 139; p.time.month = 1; p.time.day = 1; p.time.hour = 5;
    p.enhanced.difficulty = Difficulty::Custom;
    p.enhanced.custom.poison_interval = 0;
    p.enhanced.custom.hunger_pct = 0;
    TurnState t{};
    t.prev_hour = p.time.hour;
    OriginalRng rng(5);
    int ticks = 0;
    for (int i = 0; i < 3000; ++i) ticks += advance_turn(p, t, 20, rng_source(rng), nullptr).poison_ticks.count;
    check(ticks == 0 && p.party.characters[1].current_hp == 100 && p.party.characters[1].status == 'P' && p.food == 500, "K8",
          "Custom Poison Off and Hunger Off: no tick, no HP, the status kept, no food eaten over 1,000 hours");
    // K9. Switching keeps Custom: Easy, Original, back to Custom -- the values
    // the player left are still there; a preset never writes them.
    EnhancedState s;
    s.difficulty = Difficulty::Custom;
    s.custom.incoming_damage_pct = 75;
    s.custom.encounter_pct = 50;
    const GameplayRules mine = s.custom;
    s.difficulty = Difficulty::Easy;
    const bool easy = rules_equal(effective_rules(s), gameplay_rules(Difficulty::Easy));
    s.difficulty = Difficulty::Original;
    const bool orig = rules_equal(effective_rules(s), kOriginalRules);
    s.difficulty = Difficulty::Custom;
    check(easy && orig && rules_equal(s.custom, mine) && rules_equal(effective_rules(s), mine) && !enhanced_is_default(s), "K9",
          "Easy -> Custom -> Original -> Custom: each preset in force in turn, the Custom values back unchanged");
    EnhancedState untouched;
    untouched.difficulty = Difficulty::Original;
    EnhancedState kept = untouched;
    kept.custom.xp_pct = 150;
    check(enhanced_is_default(untouched) && !enhanced_is_default(kept), "K10",
          "a journey that never touched Custom is the default; changed Custom values (even on Original) are not");
}

std::string state_doc(const GameState &g) {
    save::Json doc = save::Json::object();
    save::capture_core(g, TurnState{}, doc);
    std::string text;
    save::encode_json(doc, text);
    return text;
}

GameState reload(const GameState &g) {
    save::Json doc = save::Json::object();
    save::capture_core(g, TurnState{}, doc);
    GameState back{};
    TurnState t{};
    save::restore_core(doc, back, t);
    return back;
}

void test_persistence(const char *init_gam) {
    // P1. A Custom journey keeps its difficulty and every value.
    GameState g = party_of(2);
    g.enhanced.difficulty = Difficulty::Custom;
    g.enhanced.custom = {65, 120, 250, 50, 4, 25};
    GameState back = reload(g);
    const std::string doc = state_doc(g);
    check(back.enhanced.difficulty == Difficulty::Custom && rules_equal(back.enhanced.custom, g.enhanced.custom) &&
              doc.find("\"difficulty\":\"custom\"") != std::string::npos &&
              doc.find("\"custom\":[65,120,250,50,4,25]") != std::string::npos,
          "P1", "Custom and its values round-trip: \"difficulty\":\"custom\", \"custom\":[65,120,250,50,4,25]");
    // P2. Custom values are kept while a preset is chosen (and saved with it);
    // values at Original's are not written at all.
    g.enhanced.difficulty = Difficulty::Easy;
    back = reload(g);
    GameState plain = party_of(2);
    plain.enhanced.difficulty = Difficulty::Relaxed;
    const std::string pdoc = state_doc(plain);
    check(back.enhanced.difficulty == Difficulty::Easy && rules_equal(back.enhanced.custom, g.enhanced.custom) &&
              pdoc.find("\"custom\"") == std::string::npos && pdoc.find("\"difficulty\":\"relaxed\"") != std::string::npos,
          "P2", "on Easy the Custom values are saved and come back; untouched Custom values write no \"custom\" key");
    // P3. Malformed entries: each field keeps Original's value on its own.
    struct Case { const char *what; const char *json; GameplayRules want; };
    const Case cases[] = {
        {"short", "[65,120]", {65, 120, 100, 100, 1, 100}},
        {"long", "[50,150,300,25,0,0,77,88,99]", {50, 150, 300, 25, 0, 0}},
        {"foreign values", "[66,121,201,64,3,51]", kOriginalRules},
        {"mixed", "[85,\"120\",2.5,-1,10,null]", {85, 100, 100, 100, 10, 100}},
        {"not an array", "{\"0\":65}", kOriginalRules},
        {"a number", "7", kOriginalRules},
    };
    bool malformed = true;
    std::string why;
    for (const auto &c : cases) {
        save::Json doc = save::Json::object();
        save::capture_core(party_of(1), TurnState{}, doc);
        save::Json custom;
        const bool parsed = save::parse_json(c.json, custom) == save::JsonError::None;
        save::Json e = save::Json::object();
        e["difficulty"] = save::Json("custom");
        e["custom"] = custom;
        doc["enhanced"] = e;
        GameState b{};
        TurnState t{};
        const bool ok = parsed && save::restore_core(doc, b, t) == save::Error::None &&
                        b.enhanced.difficulty == Difficulty::Custom && rules_equal(b.enhanced.custom, c.want);
        if (!ok) why += std::string(" ") + c.what;
        malformed = malformed && ok;
    }
    check(malformed, "P3", "a short, long, foreign, mixed or non-array \"custom\" never refuses the save: each bad entry is Original's" + why);
    // P4. The sidecar carries it; the .GAM never does (a PC export of a
    // Custom journey is the same file as the same journey on Original).
    std::ifstream f(init_gam, std::ios::binary);
    const std::vector<uint8_t> gam((std::istreambuf_iterator<char>(f)), {});
    GameState base{};
    TurnState bt{};
    save::Json journey_doc;
    save::SidecarSource source{};
    const bool loaded = gam.size() >= 0x1000 &&
                        save::load_native_state(gam.data(), gam.size(), nullptr, base, bt, journey_doc, source) == save::Error::None;
    auto export_one = [&](const GameState &s, save::Gam &out, std::string &side_text) {
        save::Json side;
        return save::export_native_state(s, bt, journey_doc, gam.data(), gam.size(), out, side) == save::Error::None &&
               save::encode_json(side, side_text) == save::JsonError::None;
    };
    GameState custom = base;
    custom.enhanced.difficulty = Difficulty::Custom;
    custom.enhanced.custom = {50, 150, 300, 25, 0, 0};
    save::Gam gam_custom{}, gam_plain{};
    std::string side_custom, side_plain;
    const bool exported = loaded && export_one(custom, gam_custom, side_custom) && export_one(base, gam_plain, side_plain);
    GameState imported{};
    TurnState it{};
    save::Json ir;
    save::SidecarSource from{};
    const bool imported_ok = exported && save::load_native_state(gam_custom.data(), gam_custom.size(), &side_custom, imported,
                                                                 it, ir, from) == save::Error::None;
    check(exported && imported_ok && gam_custom == gam_plain && side_plain.find("\"enhanced\"") == std::string::npos &&
              imported.enhanced.difficulty == Difficulty::Custom && rules_equal(imported.enhanced.custom, custom.enhanced.custom),
          "P4", "export -> sidecar text -> load keeps Custom; the .GAM is byte-identical to the same journey on Original");
}
} // namespace

int main(int argc, char **argv) {
    const char *init_gam = argc > 1 ? argv[1] : "game/assets/init.gam";
    test_custom();
    test_persistence(init_gam);
    std::printf("\nA4-ENH2 rules: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
