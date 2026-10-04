// Alpha 4 A4-PARITY2 small item (ALPHA4_UI.md section 16.12) -- In Mani Corp's SCROLL on a living target.
//
// Derivation (native/core/a4-parity2-findings/MANI-FINAL.md, re-disassembled): the scroll reader CAST.OVL 0x11de runs arm 6
// (0x12d8): "Resurrection!", the "On who:" picker, then `resurrect_apply` (CAST2.OVL 0x05e0) with flag 1 -- and for any status
// byte other than 'D' (a BYTE compare against 0x44, so G, P, S and every other byte alike) with a non-zero flag that routine prints
// "Not dead!" (DS 0x953c, `060a mov ax,0x953c`) and returns 0. The reader returns 0, so the (U)se epilogue (CAST.OVL 0x1b8a) prints
// "Failed!" (DS 0x4a7b) and a glide tone. The scroll is consumed first (0x11ec). Zero RNG draws. Cancelled picker (idx -1) prints
// nothing more, a dead target is revived silently. The SPELL (flag 0) is silent and prints only the Cast tail's "Failed!": both ports
// already do that. Both ports discarded the boolean here and printed nothing after "Resurrection!".
//
//   M1 a living target (G, P, S, and status bytes that are none of them): Scroll / Resurrection! / Not dead! / Failed!, in that
//      order, nothing else (no Sfx, no ceremony); scroll consumed; the record byte for byte; no RNG draw
//   M2 the same on a town floor (the dispatcher result 1 -> TOWN 0x159a); location 1 and 5
//   M3 CONTROL: a dead ('D') target is revived, silently after "Resurrection!"; the neighbours 0x43 / 0x45 are NOT dead
//   M4 CONTROL: no target (member -1 / past the roster): Scroll / Resurrection! only; scroll still consumed
//   M5 CONTROL: the other seven scrolls are untouched (batch13 pins the opening line; here no "Not dead!" anywhere)
//
//   a4_parity2_mani_not_dead
#include "openu5/commands.h"
#include "openu5/world_commands.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int failures = 0, checks = 0;
void check(bool ok, const std::string &what) {
    ++checks;
    std::cout << (ok ? "GREEN " : "RED ") << what << "\n";
    if (!ok) ++failures;
}

struct Seen {
    GameEventKind kind{};
    std::string text;
    int note = 0;
};
void capture(void *context, const GameEvent &event) {
    static_cast<std::vector<Seen> *>(context)->push_back({event.kind, event.text ? event.text : "", event.note});
}
std::string trace(const std::vector<Seen> &events) {
    std::string out;
    for (const auto &e : events) {
        if (!out.empty()) out += "|";
        if (e.kind == GameEventKind::Message) out += "m:" + e.text;
        else if (e.kind == GameEventKind::Sfx) out += "s:" + e.text;
        else if (e.kind == GameEventKind::MagicCeremony) out += "c:" + std::to_string(e.note);
        else out += "?";
    }
    return out;
}

struct Draws { int n = 0; };
int32_t counting_draw(void *p, int32_t lo, int32_t hi) {
    ++static_cast<Draws *>(p)->n;
    const int32_t v = lo + 2;
    return v > hi ? hi : v;
}

struct World {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    std::vector<uint8_t> tiles;
    MapData map_data{};
    WorldData world{};
    std::vector<Seen> events;
    Draws draws;

    explicit World(int32_t location = 1) : tiles(32 * 32, 5) {
        map_data = MapData{{LocationId(location), 0}, tiles.data(), tiles.size()};
        world.small_maps = &map_data;
        world.small_map_count = 1;
        game.position = {{4, 4}, {LocationId(location), 0}};
        game.party.character_count = game.party.party_size = 1;
        auto &m = game.party.characters[0];
        std::strncpy(m.name, "Avatar", sizeof(m.name) - 1);
        m.party_status = 0;
        m.status = 'G';
        m.current_hp = 50;
        m.max_hp = 100;
        m.current_mp = 50;
        m.level = 8;
        m.intelligence = 30;
        m.exp = 1000;
        for (auto &q : game.scroll_quantities) q = 2;
        game.karma = 97;
    }
    std::vector<Seen> use(int32_t item, int32_t member) {
        events.clear();
        draws.n = 0;
        const auto active = get_active_map(world, game.position.map);
        CommandContext context{game, turn, travel, commands, world};
        EventSink sink{&events, capture};
        Command cmd{};
        cmd.kind = CommandKind::UseItem;
        cmd.item = item;
        cmd.member = member;
        world_magic(context, cmd, active.value, sink, Rand{&draws, counting_draw});
        return events;
    }
};
bool same_record(const CharacterState &a, const CharacterState &b) { return std::memcmp(&a, &b, sizeof a) == 0; }
const char *kLiving = "m:Scroll|m:Resurrection!|m:Not dead!|m:Failed!";
} // namespace

int main() {
    // M1 -- every non-'D' status byte is "not dead" (the binary compares only against 0x44).
    for (const uint8_t status : {uint8_t('G'), uint8_t('P'), uint8_t('S'), uint8_t('X'), uint8_t(0), uint8_t(0x43), uint8_t(0x45)}) {
        World w;
        w.game.party.characters[0].status = status;
        const CharacterState before = w.game.party.characters[0];
        const auto ev = w.use(6, 0);
        check(trace(ev) == kLiving, "M1 status 0x" + std::to_string(status) + ": " + trace(ev));
        check(w.game.scroll_quantities[6] == 1 && same_record(w.game.party.characters[0], before) && w.draws.n == 0,
              "M1 status 0x" + std::to_string(status) + ": scroll consumed, the record untouched, no RNG draw (" + std::to_string(w.draws.n) + ")");
    }
    // M2 -- the town floors (and a second location): the same producer.
    for (const int32_t location : {1, 5, 29}) {
        World w(location);
        const auto ev = w.use(6, 0);
        check(trace(ev) == kLiving, "M2 location " + std::to_string(location) + ": " + trace(ev));
    }
    // M3 -- a dead target is revived, silently.
    {
        World w;
        auto &m = w.game.party.characters[0];
        m.status = 'D';
        m.current_hp = 0;
        const auto ev = w.use(6, 0);
        const std::string t = trace(ev);
        check(t.find("Not dead!") == std::string::npos && t.find("Failed!") == std::string::npos && t.rfind("m:Scroll|m:Resurrection!", 0) == 0,
              "M3 a dead target prints neither line: " + t);
        check(m.status == 'G' && m.current_hp == 1 && w.game.scroll_quantities[6] == 1, "M3 ... and is revived (G, HP 1), the scroll consumed");
    }
    // M4 -- no target: nothing after "Resurrection!".
    for (const int32_t member : {-1, 1, 7}) {
        World w;
        const auto ev = w.use(6, member);
        check(trace(ev) == "m:Scroll|m:Resurrection!" && w.game.scroll_quantities[6] == 1,
              "M4 member " + std::to_string(member) + ": " + trace(ev));
    }
    // M5 -- no other scroll says it.
    for (int item = 0; item < 8; ++item) {
        if (item == 6) continue;
        World w;
        const auto ev = w.use(item, 0);
        const std::string t = trace(ev);
        check(t.find("Not dead!") == std::string::npos, "M5 scroll " + std::to_string(item) + " never prints Not dead!");
    }
    std::cout << "A4-PARITY2 MANI: " << (checks - failures) << "/" << checks << " checks\n";
    return failures ? 1 : 0;
}
