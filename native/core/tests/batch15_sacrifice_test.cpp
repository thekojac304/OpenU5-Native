// Batch 15 / R-23 -- the Blackthorn sacrifice roster compaction and the
// downstream party-index parity that depends on it.
// See native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md section 3 R-23.
//
// The reference is BLCKTHRN.OVL 0x0438 `sacrifice_member`, read through
// re/notes/blackthorn.md 3.3, re/notes/blackthorn-cota-party-size.md 1 (the
// full instruction trace 0x0438-0x04d4) and re/notes/party-contiguidad-acta.md
// (the sibling inn (L)eave/(P)ickup bodies and the g_active_char census), with
// game/src/core/world/blackthorn.ts `sacrificeFirstCompanion` as secondary
// corroboration:
//
//   0438  al = g_party_size ; or ax,ax / je 0x46c      <- 0 means never enter
//   044c  cmp byte [si],'D' / inc cx / cmp cx,2        <- victim = 2nd LIVING
//   046c  TEMP = record[victim]          (repne movsw, cx=0x10 = the 32 B)
//   0487  cmp ax,0xf / jge                             <- victim 15 skips the shift
//   04ab  loop record[i] = record[i+1] until si == 0x57a8
//         (0x55a8 + 512 -- the END OF ALL 16 RECORDS, not the party bound)
//   04c2  record[15] = TEMP              (DS:0x5788 = 0x55a8 + 15*32)
//   04cf  byte [0x57A7] = 0x7f           (record +0x1F = partyStatus)
//   04d4  dec [g_party_size]                           <- LAST, exactly once
//
// and, just as load-bearing, what is NOT in that body: there is no re-indexing
// of g_active_char. The only routine that re-indexes it is the inn (L)eave,
// SHOPPES3 0x03dd-0x0400 (native inn_leave, shops.cpp), and the inn (P)ickup
// already carries the same declared asymmetry. Group B pins both sides so a
// later "consistency" pass cannot quietly unify them.
//
// These tests drive production code only: the real
// openu5::sacrifice_first_companion seam, the real execute_command()
// SetActivePlayer/NewOrder handlers, the real inn_leave, the real Batch 14
// zstats model and -- in group E -- the real blackthorn_action() interrogation
// state machine. Nothing here re-implements the mutation under test.
#include "openu5/blackthorn.h"
#include "openu5/commands.h"
#include "openu5/persistence.h"
#include "openu5/shops.h"
#include "openu5/shrine.h"
#include "openu5/world.h"
#include "openu5/zstats.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "[FAIL] batch15 check " << checks << ": " << what << "\n";
    } else {
        std::cerr << "[PASS] batch15 check " << checks << ": " << what << "\n";
    }
}
std::string name_of(const CharacterState &c) { return std::string(c.name); }

// A roster of `records` named slots, the first `party` of them in the party.
// `dead` marks slots whose status is 'D'; everyone else is 'G'.
GameState roster(int records, int party, std::initializer_list<int> dead = {}) {
    static const char *names[16] = {"Avatar",  "Shamino", "Iolo",   "Dupre",
                                    "Jaana",   "Katrina", "Mariah", "Julia",
                                    "Geoffrey", "Gwenno", "Sentri", "Johne",
                                    "Maxwell", "Toshi",   "Saduj",  "Tseramed"};
    GameState g{};
    for (int i = 0; i < records && i < kRosterCapacity; ++i) {
        auto &c = g.party.characters[i];
        std::snprintf(c.name, sizeof(c.name), "%s", names[i]);
        c.status = 'G';
        c.character_class = i ? 'B' : 'A';
        c.party_status = i < party ? 0 : 255;
        c.max_hp = c.current_hp = 30;
        c.level = 1;
        c.strength = c.dexterity = c.intelligence = 20;
    }
    for (int d : dead) {
        g.party.characters[d].status = 'D';
        g.party.characters[d].current_hp = 0;
    }
    g.party.character_count = uint8_t(records);
    g.party.party_size = party;
    g.party.active_character = 255;
    return g;
}

// ---------------------------------------------------------------------------
// Group A -- the mutation itself (15B B1/B2/B3, 15C C1/C2/C3).
// ---------------------------------------------------------------------------
void group_a() {
    { // A1 -- CONTROL. The full 16-record roster every .GAM produces
      // (persistence.cpp import_native's `for (i = 0; i < 16)`) and every
      // quest_parity Blackthorn scenario builds (tools/check-quests.ts:174).
      // Here the shift already reaches the end of the table, so this passes
      // before and after the fix; it exists to prove the fix changes nothing
      // on the shipped roster shape.
        auto g = roster(16, 6);
        std::vector<std::string> before;
        for (int i = 0; i < 16; ++i) before.push_back(name_of(g.party.characters[i]));
        const auto victim = sacrifice_first_companion(g);
        check(victim == "Shamino", "A1 victim is the second living member (0x044c cx==2)");
        bool shifted = true;
        for (int i = 1; i <= 14; ++i)
            if (name_of(g.party.characters[i]) != before[size_t(i + 1)]) shifted = false;
        check(shifted, "A1 slots 1..14 hold the old 2..15 (0x04ab-0x04c0 runs to si==0x57a8)");
        check(name_of(g.party.characters[0]) == "Avatar", "A1 slot 0 untouched");
        check(name_of(g.party.characters[15]) == "Shamino", "A1 slot 15 is the parked victim (0x04c2)");
        check(g.party.characters[15].party_status == 0x7f, "A1 slot 15 partyStatus = 0x7f (0x04cf)");
        check(g.party.party_size == 5, "A1 party_size decremented exactly once (0x04d4)");
        check(g.party.character_count == 16, "A1 roster stays 16 records");
    }
    { // A2 -- THE DEFECT. A roster shorter than 16 records is accepted by
      // save::deserialize/restore_core (persistence.cpp:521 and
      // save_core.cpp:278 allow any 1..16) and is what every JSON-only state
      // carries. The reference shift runs to the end of the 16-record table
      // regardless of g_party_size, so slot `records-1` must receive slot
      // `records`'s content -- NOT keep a stale duplicate of its own occupant.
        auto g = roster(6, 6);
        const auto tail_before = name_of(g.party.characters[5]);
        sacrifice_first_companion(g);
        check(name_of(g.party.characters[4]) == tail_before,
              "A2 slot 4 receives the old tail (Katrina) -- the shift happened");
        check(name_of(g.party.characters[5]) != tail_before,
              "A2 slot 5 must NOT keep a stale duplicate of the old tail: the reference shift does "
              "not stop at the roster length, it runs to the end of all 16 records");
        check(g.party.characters[5].name[0] == 0,
              "A2 slot 5 receives the (empty) old slot 6, exactly as 0x04ab-0x04c0 would");
        check(name_of(g.party.characters[15]) == "Shamino" &&
                  g.party.characters[15].party_status == 0x7f,
              "A2 the parked victim still lands in slot 15 with partyStatus 0x7f");
        check(g.party.party_size == 5 && g.party.character_count == 16,
              "A2 counts: party_size 6->5, roster forced to 16 records");
    }
    { // A3 -- 15C C1, the batch brief's own worked example. Iolo is only the
      // SECOND LIVING member when Shamino is dead; the rule is "2nd living",
      // not "slot 1", and dead members are NOT skipped by the compaction.
        auto g = roster(16, 5, {1});
        const auto victim = sacrifice_first_companion(g);
        check(victim == "Iolo", "A3 with Shamino dead the victim is Iolo (living #2)");
        check(name_of(g.party.characters[0]) == "Avatar" &&
                  name_of(g.party.characters[1]) == "Shamino" &&
                  name_of(g.party.characters[2]) == "Dupre" &&
                  name_of(g.party.characters[3]) == "Jaana",
              "A3 survivors keep their order and the DEAD member stays in the roster");
        check(g.party.characters[1].status == 'D', "A3 the dead member is still dead in slot 1");
        check(g.party.party_size == 4, "A3 party_size 5->4");
    }
    { // A4 -- 15C C2. With everybody alive the earliest legally removable slot
      // is 1: the Avatar is living #1 and can never be living #2.
        auto g = roster(16, 6);
        sacrifice_first_companion(g);
        check(name_of(g.party.characters[0]) == "Avatar" && g.party.characters[0].party_status == 0,
              "A4 slot 0 is never the victim when the Avatar is alive");
        check(name_of(g.party.characters[1]) == "Iolo", "A4 slot 1 compacts up to the old slot 2");
    }
    { // A5 -- and the rule really is "2nd living", not a hard Avatar guard:
      // with slot 0 dead, living #1 is slot 1 and the victim is slot 2.
        auto g = roster(16, 6, {0});
        const auto victim = sacrifice_first_companion(g);
        check(victim == "Iolo", "A5 a dead slot 0 makes slot 2 the victim (no Avatar special case)");
        check(name_of(g.party.characters[1]) == "Shamino", "A5 slot 1 survives untouched");
    }
    { // A6 -- 15C C3. The victim is the LAST active member.
        auto g = roster(16, 2);
        sacrifice_first_companion(g);
        check(g.party.party_size == 1, "A6 party_size 2->1");
        check(name_of(g.party.characters[1]) == "Iolo",
              "A6 the vacated last active slot receives the next roster record, not a hole");
        check(name_of(g.party.characters[15]) == "Shamino", "A6 victim parked in slot 15");
    }
    { // A7 -- the 0x0487 gate (`cmp ax,0xf / jge`): a victim that already IS
      // slot 15 skips the shift entirely rather than reading record[16].
        auto g = roster(16, 16);
        for (int i = 0; i < 14; ++i) g.party.characters[i].status = 'D';
        const auto victim = sacrifice_first_companion(g);
        check(victim == "Tseramed", "A7 with only slots 14 and 15 alive the victim is slot 15");
        check(name_of(g.party.characters[14]) == "Saduj",
              "A7 no shift runs when victim == 15 (0x0487 jge)");
        check(name_of(g.party.characters[15]) == "Tseramed" &&
                  g.party.characters[15].party_status == 0x7f,
              "A7 slot 15 is its own record re-parked with partyStatus 0x7f");
        check(g.party.party_size == 15, "A7 party_size still decremented once");
    }
    { // A8 -- fewer than two living: 0x0438's loop falls through to the 0x046c
      // exit and NOTHING is mutated.
        auto g = roster(16, 6, {1, 2, 3, 4, 5});
        const auto snapshot = g.party;
        const auto victim = sacrifice_first_companion(g);
        check(victim.empty(), "A8 no second living member -> empty name");
        check(g.party.party_size == 6 && g.party.character_count == 16,
              "A8 counts untouched when nothing is sacrificed");
        check(std::memcmp(&snapshot, &g.party, sizeof(PartyState)) == 0,
              "A8 not one roster byte moves when nothing is sacrificed");
    }
    { // A9 -- `or ax,ax / je 0x46c`: party_size 0 never enters the loop.
        auto g = roster(16, 0);
        check(sacrifice_first_companion(g).empty() && g.party.party_size == 0,
              "A9 party_size 0 is an early-out, not an underflow");
    }
}

// ---------------------------------------------------------------------------
// Group B -- 15C C4/C5/C6. Every persistent index into the roster.
//
// The census in re/notes/party-contiguidad-acta.md 3 is that exactly ONE
// persistent field is a roster index: activeCharacter (g_active_char).
// guardIdx/buyerIdx/casterIdx/memberIdx are single-command parameters that do
// not survive the command. And sacrifice_member does NOT re-index it.
// ---------------------------------------------------------------------------
void group_b() {
    { // B1 -- 15C C5. A reference to a survivor BEFORE the victim.
        auto g = roster(16, 6);
        g.party.active_character = 0;
        sacrifice_first_companion(g);
        check(g.party.active_character == 0, "B1 an index before the victim is unchanged");
        check(name_of(g.party.characters[g.party.active_character]) == "Avatar",
              "B1 and it still identifies the same character");
    }
    { // B2 -- 15C C4. A reference to a survivor AFTER the victim. The binary
      // does NOT re-index here (no counterpart of SHOPPES3 0x03dd-0x0400 in
      // BLCKTHRN 0x03ae-0x04d4), so the index keeps its numeric value and
      // therefore lands on a DIFFERENT character. This is the original's own
      // behaviour and is cloned bug-for-bug -- see B4 for the contrast.
        auto g = roster(16, 6);
        g.party.active_character = 4; // Jaana
        check(name_of(g.party.characters[g.party.active_character]) == "Jaana",
              "B2 setup: index 4 is Jaana");
        sacrifice_first_companion(g);
        check(g.party.active_character == 4,
              "B2 sacrifice_member does NOT decrement an index after the victim (no re-index block "
              "in BLCKTHRN 0x03ae-0x04d4)");
        check(name_of(g.party.characters[4]) == "Katrina",
              "B2 so the unchanged index now names the NEXT character -- the reference's own "
              "behaviour, not a native defect");
    }
    { // B3 -- 15C C6. The reference IS the victim. Again untouched: it is not
      // cleared to 0xff the way the inn (L)eave clears it.
        auto g = roster(16, 6);
        g.party.active_character = 1; // Shamino, who is about to be sacrificed
        sacrifice_first_companion(g);
        check(g.party.active_character == 1,
              "B3 an index ON the victim is NOT cleared to 0xff by sacrifice_member");
        check(name_of(g.party.characters[1]) == "Iolo",
              "B3 it now addresses the survivor that compacted into the slot");
    }
    { // B4 -- the asymmetry, pinned from the other side. inn_leave IS the
      // routine with the re-index block (SHOPPES3 0x03dd-0x0400), so the two
      // must NOT be unified into one shared primitive.
        auto g = roster(16, 6);
        g.party.active_character = 4;
        inn_leave(g, 1, 7);
        check(g.party.active_character == 3,
              "B4 inn_leave DOES decrement an index after the departure (SHOPPES3 0x03dd-0x0400)");
        auto h = roster(16, 6);
        h.party.active_character = 1;
        inn_leave(h, 1, 7);
        check(h.party.active_character == 255,
              "B4 inn_leave DOES clear an index ON the departure -- the opposite of B3");
    }
}

// ---------------------------------------------------------------------------
// Group C -- 15C C7. Real production commands that resolve a member BY PARTY
// INDEX, run after the compaction. The assertion is identity, not validity.
// ---------------------------------------------------------------------------
struct World {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    std::vector<uint8_t> large = std::vector<uint8_t>(65536, 5), small = std::vector<uint8_t>(1024, 5);
    MapData local{};
    WorldData world_data{};
    CommandContext ctx;
    std::vector<std::pair<GameEventKind, std::string>> events;
    explicit World(const GameState &seed) : game(seed), ctx(game, turn, travel, commands, world_data) {
        local = MapData{{18, 0}, small.data(), small.size()};
        world_data = WorldData{large.data(), large.data(), large.size(), large.size(), &local, 1};
        ctx.events = {this, &World::on_event};
        game.position.map = {18, 0};
        game.position.xy = {10, 10};
        game.time.hour = 10;
    }
    static void on_event(void *p, const GameEvent &e) {
        auto &w = *static_cast<World *>(p);
        w.events.emplace_back(e.kind, e.text ? e.text : "");
    }
    bool said(const char *substring) const {
        for (auto &e : events)
            if (e.second.find(substring) != std::string::npos) return true;
        return false;
    }
};

void group_c() {
    { // C1 -- (1..6) Set Active Player, commands.cpp's SetActivePlayer handler.
      // After Shamino is sacrificed, member 2 (index 1) must be IOLO -- the
      // character that compacted into the slot -- and not a stale Shamino.
        auto g = roster(16, 6);
        sacrifice_first_companion(g);
        World w(g);
        Command cmd;
        cmd.kind = CommandKind::SetActivePlayer;
        cmd.member = 2;
        const auto r = execute_command(w.ctx, cmd);
        check(r.status == CommandStatus::Success,
              "C1 SetActivePlayer(2) is accepted after the sacrifice");
        check(w.game.party.active_character == 1, "C1 it selects index 1");
        check(w.said("Iolo"),
              "C1 and the selected member is IOLO -- the surviving character that compacted into "
              "slot 1, not the sacrificed Shamino");
        check(!w.said("Shamino"), "C1 the sacrificed member is not addressable from the party");
    }
    { // C2 -- the index bound followed party_size down. With 5 members left,
      // member 6 is out of the party and must be refused (`i >= party_size`).
        auto g = roster(16, 6);
        sacrifice_first_companion(g);
        World w(g);
        Command cmd;
        cmd.kind = CommandKind::SetActivePlayer;
        cmd.member = 6;
        execute_command(w.ctx, cmd);
        check(w.said("Invalid!"), "C2 member 6 is refused once party_size is 5");
        check(w.game.party.active_character == 255, "C2 and no selection is made");
    }
    { // C3 -- (N)ew Order, the other by-index party command, proving the
      // compacted slots are what a second command sees.
        auto g = roster(16, 6);
        sacrifice_first_companion(g);
        World w(g);
        Command cmd;
        cmd.kind = CommandKind::NewOrder;
        cmd.member = 1;
        cmd.item = 2;
        const auto r = execute_command(w.ctx, cmd);
        check(r.status == CommandStatus::Success, "C3 NewOrder(1,2) runs after the sacrifice");
        check(name_of(w.game.party.characters[1]) == "Dupre" &&
                  name_of(w.game.party.characters[2]) == "Iolo",
              "C3 it swaps the SURVIVORS now at those indexes (Iolo/Dupre), not the pre-sacrifice "
              "pair");
    }
}

// ---------------------------------------------------------------------------
// Group D -- 15E. Batch 14's (Z)-stats cursor must not survive a sacrifice
// pointing into the dead window [party*2, 0x0b] of the page ring.
// ---------------------------------------------------------------------------
void group_d() {
    auto g = roster(16, 6);
    const int stranded = zstats_page_for_member(5); // Katrina's Stats page, 0x0a
    check(zstats_clamp_page(stranded, 6) == stranded, "D1 setup: page 0x0a is live with a party of 6");
    sacrifice_first_companion(g);
    const int party = int(g.party.party_size);
    check(party == 5, "D1 the party shrank to 5 under the open modal");
    const int folded = zstats_clamp_page(stranded, party);
    check(folded == party * 2 - 1,
          "D1 the stranded page folds back to the last live member's Arms page, the ring's own "
          "boundary (zstats_clamp_page)");
    check(zstats_member_of_page(folded) == party - 1, "D1 and it names a member that still exists");
    check(zstats_clamp_page(zstats_page_for_member(0), party) == zstats_page_for_member(0),
          "D2 a page for a surviving member is left alone");
    ZStatsInput in;
    in.game = &g;
    const auto stale = compose_zstats_page(in, stranded, 0);
    check(stale.member == -1 && stale.row_count == 0,
          "D3 composing the STALE page reads no out-of-range member: it yields an empty page "
          "(member_valid gate), so a render before the next input cannot touch a dead slot");
    const auto live = compose_zstats_page(in, folded, 0);
    check(live.member == party - 1 && live.row_count > 0, "D3 the folded page composes normally");
}

// ---------------------------------------------------------------------------
// Group E -- 15C C8. The REAL scripted event, not the seam. Capture at the
// Palace, fail four interrogation rounds, and the pendulum runs
// sacrifice_member(1) (BLCKTHRN 0x05ea) through blackthorn_action().
// ---------------------------------------------------------------------------
struct Interrogation {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    NpcActors actors{};
    BlackthornSession blackthorn{};
    ShrineSession shrine_session{};
    ShrineData shrine_data{};
    ShrineServices shrine_services{shrine_session};
    std::vector<uint8_t> large = std::vector<uint8_t>(65536, 5), small = std::vector<uint8_t>(1024, 5);
    MapData local{};
    WorldData world_data{};
    CommandContext ctx;
    std::vector<std::pair<GameEventKind, std::string>> events;

    explicit Interrogation(const GameState &seed)
        : game(seed), ctx(game, turn, travel, commands, world_data) {
        local = MapData{{18, 0}, small.data(), small.size()};
        world_data = WorldData{large.data(), large.data(), large.size(), large.size(), &local, 1};
        ctx.actors = &actors;
        ctx.blackthorn = &blackthorn;
        ctx.events = {this, &Interrogation::on_event};
        game.position.map = {18, 0};
        game.position.xy = {10, 10};
        game.time.hour = 10;
        shrine_data.count = 1;
        shrine_data.virtues[0] = u"Compassion";
        shrine_data.mantras[0] = u"MU";
        shrine_services.data = &shrine_data;
        shrine_services.record = &Interrogation::miscmsg;
        ctx.shrine_services = &shrine_services;
    }
    // A content-free MISCMSG stand-in, the same device batch4_group_a_test and
    // ui_mode_test use: blackthorn_action only needs twelve non-null records.
    static const char *miscmsg(void *, int32_t i) {
        static char text[12][8];
        const size_t slot = size_t(i < 0 ? 0 : i % 12);
        std::snprintf(text[slot], sizeof(text[slot]), "[r%d]", int(i));
        return text[slot];
    }
    static void on_event(void *p, const GameEvent &e) {
        auto &w = *static_cast<Interrogation *>(p);
        w.events.emplace_back(e.kind, e.text ? e.text : "");
    }
    EventSink sink() { return ctx.events; }
    bool said(const char *substring) const {
        for (auto &e : events)
            if (e.second.find(substring) != std::string::npos) return true;
        return false;
    }
};
Rand no_rand() { return {nullptr, [](void *, int32_t lo, int32_t) { return lo; }}; }

void group_e() {
    { // E1 -- the pendulum branch. Eight roster records, so the short-roster
      // compaction defect is reachable from the REAL scripted path and not
      // only from the seam.
        Interrogation w(roster(8, 6));
        const auto tail_before = name_of(w.game.party.characters[7]);
        auto st = blackthorn_action(w.ctx, BlackthornAction::Capture, {}, false, w.sink(), no_rand());
        check(st == CommandStatus::AwaitingResponse, "E1 the capture opens the interrogation");
        check(w.blackthorn.living == 6, "E1 the session recorded six living members");
        for (int round = 0; round < 4; ++round)
            st = blackthorn_action(w.ctx, BlackthornAction::Answer, u"NO", false, w.sink(), no_rand());
        check(st == CommandStatus::Success, "E1 the fourth failed round resolves the interrogation");
        check(w.said("is sliced in half!"), "E1 the pendulum falls");
        check(w.said("Shamino"), "E1 and it names the sacrificed companion");
        check(w.game.party.party_size == 5, "E1 the real path decrements party_size once");
        check(name_of(w.game.party.characters[1]) == "Iolo" &&
                  name_of(w.game.party.characters[2]) == "Dupre" &&
                  name_of(w.game.party.characters[3]) == "Jaana" &&
                  name_of(w.game.party.characters[4]) == "Katrina",
              "E1 the survivors compacted in order");
        check(name_of(w.game.party.characters[5]) == "Mariah" &&
                  name_of(w.game.party.characters[6]) == "Julia",
              "E1 the non-party roster records compacted too (0x04ab-0x04c0 covers all 16)");
        check(name_of(w.game.party.characters[7]) != tail_before,
              "E1 slot 7 must not keep a stale duplicate of the old last record");
        check(name_of(w.game.party.characters[15]) == "Shamino" &&
                  w.game.party.characters[15].party_status == 0x7f,
              "E1 the scripted path parks the victim in slot 15 with partyStatus 0x7f");
    }
    { // E2 -- ordering pin. The "die!" threat names slot 1 FIXED (0x0531
      // prints DS 0x55c8) and is read BEFORE the engine compacts the roster.
        Interrogation w(roster(8, 6));
        blackthorn_action(w.ctx, BlackthornAction::Capture, {}, false, w.sink(), no_rand());
        blackthorn_action(w.ctx, BlackthornAction::Answer, u"NO", false, w.sink(), no_rand());
        check(w.said("Shamino") && w.said(" die!"),
              "E2 the first failed round threatens slot 1 by name, before any compaction");
        check(w.game.party.party_size == 6 && name_of(w.game.party.characters[1]) == "Shamino",
              "E2 and nothing has been removed yet on round 1");
    }
    { // E3 -- the conceded-mantra branch (0x0580) also sacrifices when more
      // than one member lives, through the very same routine.
        Interrogation w(roster(8, 6));
        blackthorn_action(w.ctx, BlackthornAction::Capture, {}, false, w.sink(), no_rand());
        const auto st =
            blackthorn_action(w.ctx, BlackthornAction::Answer, u"MU", false, w.sink(), no_rand());
        check(st == CommandStatus::Success, "E3 the correct mantra resolves the interrogation at once");
        check(w.game.party.party_size == 5 && name_of(w.game.party.characters[1]) == "Iolo",
              "E3 conceding the mantra compacts the roster the same way");
        check(name_of(w.game.party.characters[7]) != "Julia",
              "E3 and its tail obeys the same full-table shift");
    }
    { // E4 -- alone, nothing is sacrificed (0x058e, the pardon).
        Interrogation w(roster(8, 1));
        blackthorn_action(w.ctx, BlackthornAction::Capture, {}, false, w.sink(), no_rand());
        blackthorn_action(w.ctx, BlackthornAction::Answer, u"MU", false, w.sink(), no_rand());
        check(w.game.party.party_size == 1 && name_of(w.game.party.characters[1]) == "Shamino",
              "E4 a lone Avatar loses nobody and the roster is not touched");
    }
}

// ---------------------------------------------------------------------------
// Group F -- 15E. The party layout IS the save (16 records x 32 B from 0x02),
// so the post-sacrifice roster has to survive a capture/restore round trip.
// ---------------------------------------------------------------------------
void group_f() {
    auto g = roster(16, 6);
    TurnState t{};
    sacrifice_first_companion(g);
    save::Json doc = save::Json::object();
    save::capture_core(g, t, doc);
    GameState back{};
    TurnState back_turn{};
    check(save::restore_core(doc, back, back_turn) == save::Error::None,
          "F1 the post-sacrifice state round-trips");
    check(back.party.character_count == 16 && back.party.party_size == 5,
          "F1 counts survive the round trip");
    bool same = true;
    for (int i = 0; i < 16; ++i)
        if (name_of(back.party.characters[i]) != name_of(g.party.characters[i])) same = false;
    check(same, "F1 all sixteen records, in order, survive the round trip");
    check(back.party.characters[15].party_status == 0x7f,
          "F1 including the parked victim's 0x7f partyStatus -- the save-window residue");
}
} // namespace

int main() {
    group_a();
    group_b();
    group_c();
    group_d();
    group_e();
    group_f();
    std::cout << "batch15 sacrifice roster checks=" << checks << " failures=" << failures << "\n";
    return failures ? 1 : 0;
}
