// Alpha 4 A4-PARITY2 D-85 (targets/tdeck/ALPHA4_UI.md section 16) -- the location-29 (Stonegate) trapdoor
// kills the PARTY, not the roster.
//
// Derivation (native/core/a4-parity2-findings/D85-FINAL.md; re-disassembled): the kill loop TOWN.OVL
// 0x0ff9-0x103a is bounded by the BYTE [0x585b] = party size, re-read each pass, unsigned compare:
//   0ff9 mov al,[0x585b] / or ax,ax / jne / jmp 0x10c7      ; party size 0 -> nothing
//   100b mov word [si],0     ; HP := 0        (si = 0x55b8 + 0x20*i)
//   100f mov byte [di],0x44  ; status := 'D'  (di = 0x55b3 + 0x20*i)
//   1012 noise_burst, 1021 K:2900 panel redraw, i++, `cmp [bp-4],[0x585b] / jb`
// It touches roster indices 0 .. party_size-1 ONLY, every one regardless of status (an already dead member is
// rewritten to HP 0; 'S' and 'P' die), with no cap at 6. Roster members parked at an inn (index >= party_size)
// are not read, not written, get no sound and no redraw. INIT.GAM / SAVED.GAM hold 16 roster records with party
// size 3: the roster loop killed 13 never-recruited or inn-parked characters in one stroke. Native looped over
// `character_count`. No test saw it: every test had roster == party.
//
//   D1 party 2 of a roster of 5: the party dies, the inn companions are byte-identical (RED before the fix)
//   D2 the stock save shape (party 3, roster 16)
//   D3 party == roster; a party of 6 in a roster of 16 kills exactly six (no cap, no overshoot)
//   D4 no status filter inside the party ('S', 'P', a dead member with HP 7); an outside 'S' is untouched
//   D5 only HP and status are written
//   D6 party size 0 writes nothing; a party larger than the roster is clamped (no out-of-bounds write)
//   D7 the same commands: the map wipe, the object erase, MapChanged + PartyChanged, PartyKilled
//   D8 control: another location kills nobody
#include "openu5/commands.h"
#include "openu5/quest_world.h"

#include <cstdio>
#include <cstring>
#include <memory>
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

struct Pool {
    std::vector<QuestObject> objects;
    int tile_writes = 0, lava = 0;
};
size_t pool_count(void *p) { return static_cast<Pool *>(p)->objects.size(); }
QuestObject pool_read(void *p, size_t i) { return static_cast<Pool *>(p)->objects[i]; }
bool pool_reserve(void *, size_t) { return true; }
void pool_append(void *p, const QuestObject &o) { static_cast<Pool *>(p)->objects.push_back(o); }
void pool_erase(void *p, size_t i) {
    auto &v = static_cast<Pool *>(p)->objects;
    v.erase(v.begin() + long(i));
}
void pool_tile(void *p, int32_t, int32_t, int32_t tile) {
    auto &o = *static_cast<Pool *>(p);
    ++o.tile_writes;
    o.lava += tile == 143;
}

struct Member {
    char status = 'G';
    uint16_t hp = 50;
    uint8_t party_status = 0;
};
struct Result {
    GameState g;
    TrapdoorOutcome outcome{};
    Pool pool;
    int map_changed = 0, party_changed = 0;
};

void sink_emit(void *p, const GameEvent &e) {
    auto &r = *static_cast<Result *>(p);
    r.map_changed += e.kind == GameEventKind::MapChanged;
    r.party_changed += e.kind == GameEventKind::PartyChanged;
}

void fill(Result &r, const std::vector<Member> &members, int party_size, int location = 29) {
    auto &g = r.g;
    g.party.character_count = uint8_t(members.size());
    g.party.party_size = party_size;
    for (size_t i = 0; i < members.size(); ++i) {
        auto &c = g.party.characters[i];
        std::snprintf(c.name, sizeof c.name, "M%zu", i);
        c.status = members[i].status;
        c.current_hp = members[i].hp;
        c.max_hp = 99;
        c.current_mp = 7;
        c.exp = 321;
        c.level = 4;
        c.party_status = members[i].party_status;
    }
    g.position.map = {location, 0};
    g.position.xy = {15, 15};
    // one object on this floor (erased by the wipe), one elsewhere (kept)
    r.pool.objects.push_back(QuestObject{});
    r.pool.objects.back().location = location;
    r.pool.objects.back().floor = 0;
    r.pool.objects.push_back(QuestObject{});
    r.pool.objects.back().location = 4;
    r.pool.objects.back().floor = 0;
}

std::unique_ptr<Result> run(const std::vector<Member> &members, int party_size, int location = 29) {
    auto r = std::make_unique<Result>();
    fill(*r, members, party_size, location);
    TurnState t;
    TravelState travel;
    CommandState commands;
    WorldData world;
    CommandContext c{r->g, t, travel, commands, world};
    QuestWorldServices q;
    q.context = &r->pool;
    q.count = pool_count;
    q.read = pool_read;
    q.reserve = pool_reserve;
    q.append = pool_append;
    q.erase = pool_erase;
    q.volatile_tile = pool_tile;
    c.quest_world = &q;
    r->outcome = quest_trapdoor(c, EventSink{r.get(), sink_emit});
    return r;
}

bool same(const CharacterState &a, const CharacterState &b) { return std::memcmp(&a, &b, sizeof a) == 0; }
bool dead(const CharacterState &c) { return c.status == 'D' && c.current_hp == 0; }

void test_all() {
    // D1
    {
        std::vector<Member> m{{}, {}, {'G', 50, 7}, {'S', 50, 7}, {'P', 9, 7}};
        Result before;
        fill(before, m, 2);
        auto r = run(m, 2);
        bool outside = true;
        for (int i = 2; i < 5; ++i) outside = outside && same(r->g.party.characters[i], before.g.party.characters[i]);
        check(dead(r->g.party.characters[0]) && dead(r->g.party.characters[1]) && outside, "D1",
              "party 2 of a roster of 5: the party dies; the three inn companions are byte-identical (statuses now " +
                  std::string(1, r->g.party.characters[2].status) + std::string(1, r->g.party.characters[3].status) +
                  std::string(1, r->g.party.characters[4].status) + ", want GSP)");
    }
    // D2
    {
        std::vector<Member> m(16);
        for (int i = 3; i < 16; ++i) m[size_t(i)].party_status = 0xff;
        auto r = run(m, 3);
        int dead_n = 0, alive_n = 0;
        for (int i = 0; i < 16; ++i) (dead(r->g.party.characters[i]) ? dead_n : alive_n)++;
        check(dead_n == 3 && alive_n == 13 && dead(r->g.party.characters[2]) && !dead(r->g.party.characters[3]), "D2",
              "the stock save shape (party 3, roster 16): " + std::to_string(dead_n) + " dead, " + std::to_string(alive_n) +
                  " alive (want 3 / 13)");
    }
    // D3
    {
        auto full = run(std::vector<Member>(3), 3);
        auto six = run(std::vector<Member>(16), 6);
        int six_dead = 0;
        for (int i = 0; i < 16; ++i) six_dead += dead(six->g.party.characters[i]);
        check(dead(full->g.party.characters[0]) && dead(full->g.party.characters[1]) && dead(full->g.party.characters[2]) && six_dead == 6 &&
                  dead(six->g.party.characters[5]) && !dead(six->g.party.characters[6]),
              "D3", "party == roster: all three die; a party of 6 in a roster of 16 kills exactly six (" + std::to_string(six_dead) + ")");
    }
    // D4
    {
        auto r = run({{'S', 50, 0}, {'P', 50, 0}, {'D', 7, 0}, {'S', 50, 0}}, 3);
        check(dead(r->g.party.characters[0]) && dead(r->g.party.characters[1]) && dead(r->g.party.characters[2]) &&
                  r->g.party.characters[3].status == 'S' && r->g.party.characters[3].current_hp == 50,
              "D4", "'S', 'P' and a dead member with HP 7 inside the party all end HP 0 / 'D'; an outside 'S' is untouched");
    }
    // D5
    {
        Result before;
        fill(before, {{}, {}}, 1);
        auto r = run({{}, {}}, 1);
        auto a = r->g.party.characters[0];
        const auto &b = before.g.party.characters[0];
        check(a.max_hp == b.max_hp && a.current_mp == b.current_mp && a.exp == b.exp && a.level == b.level && dead(a), "D5",
              "only HP and status are written: max HP, MP, experience and level are untouched");
    }
    // D6
    {
        std::vector<Member> two(2);
        auto zero = run(two, 0);
        const bool none = !dead(zero->g.party.characters[0]) && !dead(zero->g.party.characters[1]);
        auto big = run(two, 6); // party larger than the roster: clamped to the two records
        auto huge = run(std::vector<Member>(16), 17);
        int huge_dead = 0;
        for (int i = 0; i < 16; ++i) huge_dead += dead(huge->g.party.characters[i]);
        check(none && dead(big->g.party.characters[0]) && dead(big->g.party.characters[1]) && huge_dead == 16, "D6",
              "party size 0 writes nothing; party 6 over a roster of 2 and party 17 over 16 are clamped (no out-of-bounds write)");
    }
    // D7
    {
        auto r = run({{}, {}, {}}, 2);
        const bool objects = r->pool.objects.size() == 1 && r->pool.objects[0].location == 4;
        check(r->outcome == TrapdoorOutcome::PartyKilled && r->pool.tile_writes == 1024 && r->pool.lava == 1024 && objects &&
                  r->map_changed == 1 && r->party_changed == 1,
              "D7", "the whole 32x32 map becomes lava (143), the objects of this floor are erased, one MapChanged + one PartyChanged, PartyKilled");
    }
    // D8
    {
        auto r = run({{}, {}, {}}, 2, 4);
        bool none = true;
        for (int i = 0; i < 3; ++i) none = none && !dead(r->g.party.characters[i]);
        check(none && r->outcome != TrapdoorOutcome::PartyKilled, "D8", "control: another location kills nobody");
    }
}
} // namespace

int main() {
    test_all();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
