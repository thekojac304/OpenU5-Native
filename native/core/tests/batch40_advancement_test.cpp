// Batch 40: OUTSUBS.OVL 0x06ea-0x079c, 0x07fb-0x090e, reached only through
// CMDS.OVL 0x04e7-0x0502 after an uninterrupted Camp.
#include "openu5/commands.h"
#include "openu5/rest.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace openu5;
namespace {
int checks = 0, failures = 0;
void check(bool good, const char *id) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", id);
}
struct Draw { int lo, hi, value; };
struct Fixture {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    std::array<uint8_t, 65536> tiles{};
    std::array<uint8_t, 1024> local_tiles{};
    MapData local{{17, 0}, local_tiles.data(), local_tiles.size()};
    WorldData world{tiles.data(), tiles.data(), tiles.size(), tiles.size(), &local, 1};
    RestServices rest{};
    std::vector<Draw> draws;
    std::vector<std::string> messages;
    int party_changes = 0;
    int later_level_at_first_chime = -1;
    Fixture(int members = 1) {
        tiles.fill(5);
        local_tiles.fill(171);
        game.position.map = {0, 0};
        game.position.xy = {80, 80};
        game.time.hour = 12;
        game.time.minute = 55;
        game.food = 80;
        game.party.character_count = uint8_t(members);
        game.party.party_size = members;
        game.party.active_character = 255;
        for (int i = 0; i < members; ++i) {
            auto &m = game.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "Member%d", i);
            m.status = 'G';
            m.character_class = 'F';
            m.level = 1;
            m.max_hp = 30;
            m.current_hp = 5;
            m.strength = m.dexterity = m.intelligence = 10;
        }
        rest.context = this;
        rest.snap_npcs = [](void *) {};
        rest.occupied = [](void *, int32_t, int32_t, int32_t) { return false; };
        rest.karma_record = [](void *, int32_t) { return "\"Karma record\""; };
    }
    ActionResult run(CommandKind kind, int hours = 0) {
        draws.clear(); messages.clear(); party_changes = 0; later_level_at_first_chime = -1;
        CommandContext context{game, turn, travel, commands, world};
        context.rest_services = &rest;
        context.events = {this, [](void *p, const GameEvent &e) {
            auto &f = *static_cast<Fixture *>(p);
            if (e.kind == GameEventKind::Message && e.text) f.messages.emplace_back(e.text);
            if (e.kind == GameEventKind::PartyChanged) ++f.party_changes;
            if (e.kind == GameEventKind::Sfx && e.text &&
                std::strcmp(e.text, "apparition-heal-chime") == 0 &&
                f.later_level_at_first_chime < 0 && f.game.party.character_count >= 3)
                f.later_level_at_first_chime = f.game.party.characters[2].level;
        }};
        context.rng_trace = {this, [](void *p, const char *, int32_t lo, int32_t hi, int32_t value) {
            static_cast<Fixture *>(p)->draws.push_back({int(lo), int(hi), int(value)});
        }};
        Command cmd{};
        cmd.kind = kind;
        cmd.hours = int16_t(hours);
        return execute_command(context, cmd);
    }
    bool apparition() const {
        return std::any_of(messages.begin(), messages.end(), [](const auto &s) {
            return s.find("An apparition!") != std::string::npos;
        });
    }
    int rolls(int lo, int hi) const {
        return int(std::count_if(draws.begin(), draws.end(), [=](const Draw &d) {
            return d.lo == lo && d.hi == hi;
        }));
    }
};
int seed_for(bool want_apparition) {
    for (int seed = 1; seed <= 512; ++seed) {
        Fixture f;
        f.game.rng.seed(seed);
        const auto result = f.run(CommandKind::Rest, 1);
        if (result.status == CommandStatus::Success && f.apparition() == want_apparition)
            return seed;
    }
    return -1;
}
void one_member(int seed, int exp, int expected_level, const char *id) {
    Fixture f;
    f.game.rng.seed(seed);
    auto &m = f.game.party.characters[0];
    m.exp = uint16_t(exp);
    const auto result = f.run(CommandKind::Rest, 1);
    check(result.status == CommandStatus::Success && f.apparition() &&
          f.game.time.hour == 13 && f.rolls(0, 99) == 1, id);
    check(m.exp == exp && m.level == expected_level &&
          m.max_hp == (expected_level == 1 ? 30 : 30 * expected_level) &&
          m.current_hp == m.max_hp, "level, XP, and HP");
    const int gains = int(m.strength == 11) + int(m.dexterity == 11) +
                      int(m.intelligence == 11);
    check(gains == (expected_level == 1 ? 0 : 1) &&
          f.rolls(1, 3) == (expected_level == 1 ? 0 : 1), "one stat draw only on change");
}
void multi_member() {
    int seed = -1;
    for (int s = 1; s <= 512; ++s) {
        Fixture probe(4);
        probe.game.party.characters[1].status = 'D';
        probe.game.party.characters[0].exp = 100;
        probe.game.party.characters[2].exp = 200;
        probe.game.party.characters[3].exp = 400;
        probe.game.rng.seed(s);
        probe.run(CommandKind::Rest, 1);
        std::vector<int> trial_rolls;
        for (const auto &d : probe.draws)
            if (d.lo == 1 && d.hi == 3) trial_rolls.push_back(d.value);
        if (probe.apparition() && trial_rolls.size() == 3 &&
            trial_rolls[0] != trial_rolls[1] &&
            trial_rolls[1] != trial_rolls[2] &&
            trial_rolls[0] != trial_rolls[2]) { seed = s; break; }
    }
    check(seed > 0, "four-member fixture reaches apparition");
    if (seed < 0) return;
    Fixture f(4);
    f.game.rng.seed(seed);
    auto &a = f.game.party.characters;
    a[0].exp = 100; a[0].character_class = 'A';
    a[1].exp = 6400; a[1].status = 'D'; a[1].character_class = 'M';
    a[2].exp = 200; a[2].character_class = 'B';
    a[3].exp = 400; a[3].character_class = 'F';
    a[0].intelligence = 30; // A-class MP reset; a rolled INT gain is capped.
    a[1].intelligence = 20;
    a[2].intelligence = 21;
    auto result = f.run(CommandKind::Rest, 1);
    check(result.status == CommandStatus::Success && f.apparition() &&
          a[0].level == 2 && a[2].level == 3 && a[3].level == 4 &&
          a[1].level == 1, "all live eligible slots advance; dead slot is skipped");
    check(a[0].max_hp == 60 && a[2].max_hp == 90 && a[3].max_hp == 120 &&
          a[1].max_hp == 30, "direct jumps use 30 times computed level");
    check(a[0].current_mp == a[0].intelligence && a[1].current_mp == 20 &&
          a[2].current_mp == (a[2].intelligence >> 1), "class MP reset includes dead");
    check(f.rolls(1, 3) == 3 && a[1].status == 'D' &&
          a[0].exp == 100 && a[2].exp == 200 && a[3].exp == 400,
          "one draw per live changed slot; XP and death preserved");
    std::vector<int> stat_rolls;
    for (const auto &d : f.draws) if (d.lo == 1 && d.hi == 3) stat_rolls.push_back(d.value);
    auto gain_matches = [](const CharacterState &m, int roll, int initial_int) {
        return m.strength == (roll == 1 ? 11 : 10) &&
               m.dexterity == (roll == 2 ? 11 : 10) &&
               m.intelligence == (roll == 3 ? std::min(30, initial_int + 1) : initial_int);
    };
    check(stat_rolls.size() == 3 &&
          gain_matches(a[0], stat_rolls[0], 30) &&
          gain_matches(a[2], stat_rolls[1], 21) &&
          gain_matches(a[3], stat_rolls[2], 10),
          "stat draws map to eligible roster slots in order, including capped INT");
    std::string text;
    for (const auto &s : f.messages) text += s;
    const auto p0 = text.find("Hail, Member0!");
    const auto p2 = text.find("Hail, Member2!");
    const auto p3 = text.find("Hail, Member3!");
    check(p0 != std::string::npos && p2 > p0 && p3 > p2 &&
          text.find("Hail, Member1!") == std::string::npos,
          "advancement messages follow roster order");
}
}
int main(int argc, char **argv) {
    if (argc > 1 && std::strcmp(argv[1], "--presentation-probe") == 0) {
        for (int seed = 1; seed <= 512; ++seed) {
            Fixture f(3);
            f.game.rng.seed(seed);
            f.game.party.characters[0].exp = 100;
            f.game.party.characters[2].exp = 200;
            f.run(CommandKind::Rest, 1);
            if (!f.apparition()) continue;
            check(f.later_level_at_first_chime == 1,
                  "H-175 RED: second member remains unadvanced at first member chime");
            std::printf("H-175 presentation probe: %d/%d checks\n", checks - failures, checks);
            return failures ? 1 : 0;
        }
        return 2;
    }
    const int yes = seed_for(true), no = seed_for(false);
    check(yes > 0 && no > 0, "deterministic Camp seeds reach both gate outcomes");
    if (yes < 0 || no < 0) return 1;
    one_member(yes, 99, 1, "99 XP remains below L2 threshold");
    one_member(yes, 100, 2, "100 XP reaches L2 exactly");
    one_member(yes, 450, 4, "450 XP jumps directly to L4");
    multi_member();
    Fixture ordinary_turn;
    ordinary_turn.game.party.characters[0].exp = 100;
    ordinary_turn.run(CommandKind::Pass);
    check(ordinary_turn.game.party.characters[0].level == 1 &&
          ordinary_turn.game.party.characters[0].max_hp == 30 &&
          ordinary_turn.rolls(1, 3) == 0,
          "crossed XP threshold does not advance on an ordinary turn");    Fixture missed;
    missed.game.rng.seed(no);
    missed.game.party.characters[0].exp = 100;
    missed.run(CommandKind::Rest, 1);
    check(!missed.apparition() && missed.game.party.characters[0].level == 1 &&
          missed.rolls(0, 99) == 1 && missed.rolls(1, 3) == 0,
          "completed Camp without apparition does not advance");
    bool beds_reached = true, beds_preserved = true;
    for (int seed = 1; seed <= 64; ++seed) {
        Fixture bed;
        bed.game.rng.seed(seed);
        bed.game.position.map = {17, 0};
        bed.game.position.xy = {9, 7};
        bed.game.party.characters[0].exp = 100;
        const auto bed_result = bed.run(CommandKind::Rest, 1);
        bool zzz = false;
        for (const auto &s : bed.messages)
            if (s.find("Zzzzzzz") != std::string::npos) zzz = true;
        beds_reached &= bed_result.status == CommandStatus::Success && zzz &&
                        bed.game.time.hour == 13;
        beds_preserved &= bed.game.party.characters[0].level == 1 &&
                          bed.rolls(1, 3) == 0;
    }
    check(beds_reached, "64 town bed Rest commands reach sleep path");
    check(beds_preserved, "town bed sleep never advances eligible XP");
    std::printf("Batch 40 advancement: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
