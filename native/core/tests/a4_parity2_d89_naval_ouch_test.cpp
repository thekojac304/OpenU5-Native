// Alpha 4 A4-PARITY2 D-89 (targets/tdeck/ALPHA4_UI.md section 16) -- the NAVAL "OUCH!"
// damages the WHOLE party (kernel 0x2AA8 party_random_damage), not the active member.
//
// Derivation (native/core/a4-parity2-findings/D89-FINAL.md, every address re-disassembled):
// MAINOUT.OVL has ONE blocked-move tail (0x0312-0x0347) shared by foot, horse, carpet, skiff
// and a rowed frigate:
//   0322 "Blocked!"  0329 cmp [bp-6],0x2f  032f "OUCH!"  0336 call 0xffffa8d8 = K:2AA8
//   033c (else) beep   0347 K:1B16 keyboard flush   return 0 (no clock tick)
// K:2AA8 draws rand(1,8) for every slot i < min(party_size,6) whose status is not 'D' (no draw
// for a dead member), in slot order, each draw followed at once by apply_damage (K:2A52:
// HP -= n; HP <= 0 -> HP 0, 'D', the active character cleared if it was the one that died;
// K:2900 redraws the party panel). From seed 0x0C4C the stream is 4, 8, 3, 8, 5, 3.
//
// Before the fix naval_step rolled ONE rand(1,8) on the active member (clamped to 0). The foot
// path was already right. Every parity corpus uses a ONE-member party, so nothing saw it.
//
//   N1 three members, active 1: each takes its own draw        (RED before the fix)
//   N2 a dead member is skipped WITHOUT a draw
//   N3 'S' and 'P' are damaged and keep their status
//   N4 lethal boundary (HP == damage kills) and the active-member rule
//   N5 the active index is irrelevant; party_size bounds the loop; six members, six draws
//   N6 God Mode: the same draws, no HP written (the A4-ENH1/ENH2 contract survives)
//   N7 a rowed frigate: Rowing!, Blocked!, OUCH! in that order
//   N8 controls: a grass obstacle (no OUCH, no draw), a sailing ship (COLLISION!, one rand(1,30))
//   N9 naval / foot equivalence: same party, same seed, same HP and the same draws
//   N10 exactly one PartyChanged after OUCH!, as on foot (K:2A52 ends in K:2900)
#include "openu5/commands.h"
#include "openu5/enhanced.h"
#include "openu5/turn.h"

#include <cstdio>
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

constexpr uint32_t kSeed = 0x0c4c;
// rand(1,8) from kSeed: 4, 8, 3, 8, 5, 3; the seed after each draw.
constexpr uint32_t kSeedAfter[6] = {0x01ab, 0xe047, 0x7c2a, 0xd397, 0x7f04, 0x1072};

struct Member {
    char status = 'G';
    uint16_t hp = 100;
};
struct Setup {
    std::vector<Member> members{Member{}};
    int party_size = -1;          // -1: members.size()
    int active = 0;
    uint8_t obstacle = 0x2f;      // terrain south... east of (20,20)
    TransportMode transport = TransportMode::Skiff;
    uint8_t transport_tile = 0x2a; // a skiff facing east
    uint8_t sail_dir = 0;
    bool god = false;
    bool foot = false;
};
struct Outcome {
    std::vector<int> hp;
    std::vector<char> status;
    int active = 0;
    uint32_t seed = 0;
    std::vector<std::string> events; // "m:<text>", "sfx:<id>", "PartyChanged"
    int x = 0;
    int hull = 0;
};

Outcome run(const Setup &u, uint32_t seed = kSeed) {
    GameState s;
    TurnState t;
    TravelState travel;
    CommandState commands;
    std::vector<uint8_t> terrain(65536, u.foot ? 5 : 1);
    terrain[20 * 256 + 21] = u.obstacle;
    WorldData world;
    world.overworld = terrain.data();
    world.overworld_size = terrain.size();
    CommandContext c{s, t, travel, commands, world};
    Outcome out;
    c.events = {&out, [](void *p, const GameEvent &e) {
        auto &o = *static_cast<Outcome *>(p);
        if (e.kind == GameEventKind::Message) o.events.push_back(std::string("m:") + (e.text ? e.text : ""));
        else if (e.kind == GameEventKind::Sfx) o.events.push_back(std::string("sfx:") + (e.text ? e.text : ""));
        else if (e.kind == GameEventKind::PartyChanged) o.events.push_back("PartyChanged");
    }};
    s.position.xy = {20, 20};
    s.party.character_count = uint8_t(u.members.size());
    s.party.party_size = u.party_size < 0 ? int(u.members.size()) : u.party_size;
    s.party.active_character = uint8_t(u.active);
    for (size_t i = 0; i < u.members.size(); ++i) {
        auto &m = s.party.characters[i];
        m.status = u.members[i].status;
        m.current_hp = m.max_hp = 100;
        m.current_hp = u.members[i].hp;
        m.party_status = 0;
        m.weapon = m.shield = m.helmet = m.armor = m.ring = m.amulet = 255;
    }
    s.food = 100;
    if (u.foot) {
        s.transport = TransportMode::Foot;
        t.transport_tile = 0x1c;
    } else {
        s.transport = u.transport;
        t.transport_tile = u.transport_tile;
    }
    t.sail_dir = u.sail_dir;
    s.ship_hull = 50;
    s.enhanced.god_mode = u.god;
    s.rng.seed(seed);
    Command move;
    move.kind = CommandKind::Move;
    move.direction = Direction::East;
    move.has_direction = true;
    execute_command(c, move);
    for (size_t i = 0; i < u.members.size(); ++i) {
        out.hp.push_back(int(s.party.characters[i].current_hp));
        out.status.push_back(s.party.characters[i].status);
    }
    out.active = int(s.party.active_character);
    out.seed = s.rng.get_seed();
    out.x = s.position.xy.x;
    out.hull = int(s.ship_hull);
    return out;
}

// The ports still run a naval turn (wind roll) after a BLOCKED rowed step, which the binary does
// not (MAINOUT 0x0C30; adjacent divergence D-92, not D-89). So the seed after the step is "the
// damage draws, then that turn". The turn's own draws are measured on a grass obstacle (no
// damage) started from the seed the damage draws leave behind: this pins the number and ORDER of
// the damage draws without asserting the adjacent behaviour.
uint32_t seed_after_turn_from(uint32_t seed) {
    Setup u;
    u.obstacle = 5;
    return run(u, seed).seed;
}

bool has(const Outcome &o, const std::string &e) {
    for (const auto &x : o.events)
        if (x == e) return true;
    return false;
}
int index_of(const Outcome &o, const std::string &e) {
    for (size_t i = 0; i < o.events.size(); ++i)
        if (o.events[i] == e) return int(i);
    return -1;
}
std::string hp_text(const Outcome &o) {
    std::string r;
    for (int h : o.hp) r += std::to_string(h) + " ";
    return r;
}

void test_all() {
    // N1
    {
        Setup u;
        u.members = {Member{}, Member{}, Member{}};
        u.active = 1;
        const auto o = run(u);
        check(o.hp == std::vector<int>{96, 92, 97} && o.seed == seed_after_turn_from(kSeedAfter[2]) && o.x == 20 &&
                  !has(o, "sfx:move-blocked") && index_of(o, "m:Blocked!") >= 0 &&
                  index_of(o, "m:Blocked!") < index_of(o, "m:OUCH!"),
              "N1", "3 members, active 1: HP " + hp_text(o) + "(want 96 92 97 = draws 4, 8, 3), Blocked! then OUCH!, no beep");
    }
    // N2
    {
        Setup u;
        u.members = {Member{}, Member{'D', 0}, Member{}};
        const auto o = run(u);
        check(o.hp == std::vector<int>{96, 0, 92} && o.status[1] == 'D' && o.seed == seed_after_turn_from(kSeedAfter[1]), "N2",
              "a dead member is skipped with NO draw: slot 2 takes the SECOND draw (HP " + hp_text(o) + "want 96 0 92)");
    }
    // N3 -- the adjacent naval turn ticks poison once; measure it on a grass obstacle
    {
        Setup ctl;
        ctl.members = {Member{'P', 100}};
        ctl.obstacle = 5;
        const int tick = 100 - run(ctl).hp[0];
        Setup u;
        u.members = {Member{'S', 100}, Member{'P', 100}, Member{}};
        const auto o = run(u);
        check(o.hp == std::vector<int>{96, 92 - tick, 97} && o.status == std::vector<char>{'S', 'P', 'G'}, "N3",
              "'S' and 'P' are damaged and keep their status (HP " + hp_text(o) + "; the adjacent turn's poison tick is " +
                  std::to_string(tick) + ")");
    }
    // N4
    {
        Setup u;
        u.members = {Member{'G', 4}, Member{'G', 9}};
        const auto o = run(u);
        Setup v;
        v.members = {Member{'G', 3}, Member{}};
        v.active = 0;
        const auto ov = run(v);
        Setup w;
        w.members = {Member{}, Member{'G', 3}};
        w.active = 0;
        const auto ow = run(w);
        check(o.hp == std::vector<int>{0, 1} && o.status == std::vector<char>{'D', 'G'} && ov.status[0] == 'D' && ov.active == 255 &&
                  ow.status[1] == 'D' && ow.active == 0,
              "N4", "HP == damage kills (4 vs 4 -> 0 'D'; 9 vs 8 -> 1); the active member dying clears the active index, another one dying does not");
    }
    // N5
    {
        Setup u;
        u.members = {Member{}, Member{}};
        u.active = 255;
        const auto none = run(u);
        Setup v;
        v.members = {Member{}, Member{}, Member{}, Member{}};
        v.party_size = 2;
        const auto small = run(v);
        Setup w;
        w.members = {Member{}, Member{}, Member{}, Member{}, Member{}, Member{}};
        const auto six = run(w);
        check(none.hp == std::vector<int>{96, 92} && small.hp == std::vector<int>{96, 92, 100, 100} &&
                  small.seed == seed_after_turn_from(kSeedAfter[1]) &&
                  six.hp == std::vector<int>{96, 92, 97, 92, 95, 97} && six.seed == seed_after_turn_from(kSeedAfter[5]),
              "N5", "active 255 still damages everyone; party_size 2 of 4 records -> two draws; six members -> six draws (4 8 3 8 5 3)");
    }
    // N6
    {
        Setup u;
        u.members = {Member{}, Member{}};
        u.god = true;
        const auto g = run(u);
        u.god = false;
        const auto n = run(u);
        check(g.hp == std::vector<int>{100, 100} && g.seed == n.seed && n.hp == std::vector<int>{96, 92}, "N6",
              "God Mode: no HP written, the same draws (seed identical to the unprotected run)");
    }
    // N7
    {
        Setup u;
        u.members = {Member{}, Member{}};
        u.transport = TransportMode::Ship;
        u.transport_tile = 0x25;
        const auto o = run(u);
        const int r = index_of(o, "m:Rowing!"), b = index_of(o, "m:Blocked!"), ou = index_of(o, "m:OUCH!");
        check(r >= 0 && r < b && b < ou && o.hp == std::vector<int>{96, 92} && o.hull == 50 && o.x == 20, "N7",
              "a rowed frigate: Rowing!, Blocked!, OUCH! in that order; the party is damaged, the hull is not");
    }
    // N8 controls
    {
        Setup g;
        g.members = {Member{}, Member{}};
        g.obstacle = 5;
        const auto og = run(g);
        Setup sl;
        sl.members = {Member{}, Member{}};
        sl.transport = TransportMode::Ship;
        sl.transport_tile = 0x22; // sails up, facing east
        sl.sail_dir = 4;
        const auto os = run(sl);
        check(og.hp == std::vector<int>{100, 100} && !has(og, "m:OUCH!") && has(og, "m:Blocked!") && has(og, "sfx:move-blocked") &&
                  os.hp == std::vector<int>{100, 100} && !has(os, "m:OUCH!"),
              "N8", "controls: grass is Blocked! + beep with no OUCH and no damage; a sailing ship into a cactus never reaches OUCH "
                    "(its hull takes the hit)");
    }
    // N9
    {
        Setup boat;
        boat.members = {Member{}, Member{'S', 100}, Member{}};
        const auto ob = run(boat);
        Setup foot = boat;
        foot.foot = true;
        const auto of = run(foot);
        check(ob.hp == of.hp && ob.status == of.status && !of.hp.empty() && of.hp[0] == 96 && of.hp[1] == 92 && of.hp[2] == 97, "N9",
              "naval / foot equivalence: the same party and seed lose the same HP by boat and on foot (" + hp_text(ob) + ")");
    }
    // N10
    {
        Setup u;
        u.members = {Member{}, Member{}};
        const auto o = run(u);
        int n = 0;
        for (const auto &e : o.events) n += e == "PartyChanged";
        check(n == 1 && index_of(o, "PartyChanged") > index_of(o, "m:OUCH!"), "N10",
              "exactly one PartyChanged, after OUCH! (K:2A52 ends in the K:2900 panel redraw), as on foot");
    }
}
} // namespace

int main() {
    test_all();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
