// Batch 4 GROUP B -- RED tests for R-10 (NPC-initiated Talk / Shop). See
// native/targets/tdeck/ALPHA20_BATCH4_ADJUDICATION.md.
//
// Architecture fact from the adjudication (section C/D): NPC-initiated Talk
// AND Shop both ultimately reuse the SAME CommandKind::BeginConversation
// with cmd.member = npc.schedule.slot (dialogue_orchestration.cpp:132,
// already matches on slot alone with no adjacency/floor test, and :139,
// already routes dialog 0x81..0x88 into begin_shop() via
// DialogueHandoff::Shop). No separate Talk/Shop-initiation command is
// invented anywhere in this file.
//
// These tests drive the REAL town-turn -> blackthorn_turn_effect trigger
// (native/core/src/blackthorn.cpp:56-68, the SAME function Group A's guard
// flows go through -- NpcInitiatesTalk/NpcInitiatesShop are emitted from its
// Tribute-effect tail), the REAL UiSession::consume()/handle_input() path,
// and -- for the GREEN-guard halves of B2/B4 that exercise the
// already-complete BeginConversation entry point directly -- the REAL
// execute_dialogue_command()/begin_shop() production path. No
// AlphaRuntime::consume_event() code is reimplemented: UiSession::consume()'s
// switch has no case for NpcInitiatesTalk/NpcInitiatesShop today (falls to
// `default: break;`, ui_session.cpp:979), and this file adds no consumer for
// it anywhere -- that is the documented gap these tests prove.
#include "openu5/actors.h"
#include "openu5/blackthorn.h"
#include "openu5/commands.h"
#include "openu5/dialogue.h"
#include "openu5/dialogue_orchestration.h"
#include "openu5/npc_path.h"
#include "openu5/shop_orchestration.h"
#include "openu5/shops.h"
#include "openu5/ui_session.h"
#include "openu5/world.h"
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
        std::cerr << "[FAIL] batch4 group B check " << checks << ": " << what << "\n";
    } else {
        std::cerr << "[PASS] batch4 group B check " << checks << ": " << what << "\n";
    }
}

// Real town-turn NPC fixture, shared by the Talk and Shop scenarios. Wires a
// real DialogueSession/DialogueServices (talk registry resolving dialog 9 at
// a Towne location) and, when enable_shop() is called, a real
// ShopSession/ShopServices/ShopData (one Blacksmith record at location 2,
// matching shop_tables.inc's real towns_Blacksmith list) -- exactly the
// production services CommandKind::BeginConversation/Talk already route
// through (dialogue_orchestration.cpp, shop_orchestration.cpp).
//
// add_npc() places the NPC EXACTLY at its own scheduled post for whatever
// (times, idx) pair the caller picks -- see batch4_group_a_test.cpp's
// GuardWorld comment for why this deterministically keeps the real
// actors()/tick_npcs() NPC-movement pass a no-op regardless of idx or ai.
struct NpcWorld {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    NpcActors actors{};
    NpcScanGrid scratch{};
    BlackthornSession blackthorn{};
    DialogueSession dialogue_session{};
    TalkScript records[1]{};
    TalkCatalog catalog{};
    TalkRegistry registry{};
    DialogueServices dialogue_services{dialogue_session};
    ShopSession shop_session{};
    ShopRecord blacksmith_record{2, ShopType::Blacksmith, 0, "The Blacksmith Shoppe", "Grunyon"};
    ShopData shop_data{};
    ShopServices shop_services{shop_session, shop_data};
    std::vector<uint8_t> large, small;
    MapData local{};
    WorldData world_data{};
    CommandContext ctx;
    UiTextBlock blocks[64]{};
    UiSession ui;
    std::vector<UiIntent> intents;
    ActionResult last_command_result{};

    explicit NpcWorld(uint8_t location, int16_t floor = 0, uint8_t hour = 10)
        : large(65536, 5), small(1024, 5), ctx(game, turn, travel, commands, world_data),
          ui({blocks, 64}, {this, &NpcWorld::dispatch}, {60, 12, 16}) {
        local = MapData{{location, floor}, small.data(), small.size()};
        world_data = WorldData{large.data(), large.data(), large.size(), large.size(), &local, 1};
        ctx.actors = &actors;
        ctx.npc_scratch = &scratch;
        ctx.blackthorn = &blackthorn;
        ctx.events = ui.event_sink();
        game.position.map.location = location;
        game.position.map.floor = floor;
        game.position.xy = {10, 10};
        game.time.hour = hour;
        game.party.party_size = game.party.character_count = 1;
        game.party.characters[0].status = 'G';
        records[0].npc_index = 9; // A trivial, otherwise-empty script: Conversation::run()'s
                                  // state machine always reaches prompt(Interest) at root
                                  // state 5 regardless of script content (dialogue.cpp:314),
                                  // so this alone arms a real DialogueOutputKind::Prompt.
        for (auto &m : catalog.masters) m = {records, 1};
        registry = {&catalog, talk_catalog_lookup};
        dialogue_services.registry = registry;
        ctx.dialogue_services = &dialogue_services;
        shop_data = ShopData{&blacksmith_record, 1};
    }
    void enable_shop() { ctx.shop_services = &shop_services; }
    NpcActor &add_npc(uint8_t slot, uint8_t type, uint8_t dialog, uint8_t ai, const uint8_t (&times)[4],
                     size_t idx, int16_t npc_x, int16_t npc_y) {
        auto &n = actors.actors[actors.count++];
        n.location = game.position.map.location;
        n.z = game.position.map.floor;
        n.x = npc_x;
        n.y = npc_y;
        n.schedule.slot = slot;
        n.schedule.type = type;
        n.schedule.dialog = dialog;
        for (int i = 0; i < 4; ++i) n.schedule.times[i] = times[i];
        n.schedule.ai[idx] = ai;
        n.schedule.x[idx] = uint8_t(npc_x);
        n.schedule.y[idx] = uint8_t(npc_y);
        n.schedule.z[idx] = uint8_t(game.position.map.floor);
        return n;
    }
    static void dispatch(void *p, const UiIntent &i) {
        auto &w = *static_cast<NpcWorld *>(p);
        w.intents.push_back(i);
        if (i.kind == UiIntentKind::Command) w.last_command_result = dispatch_world_command(w.ctx, i.command);
    }
    // The real, production town-turn trigger.
    void pass() {
        UiAction a;
        a.kind = UiActionKind::Confirm;
        ui.handle_input(a);
    }
};
} // namespace

// B1: npc_initiates_talk_reaches_begin_conversation.
static void b1_npc_initiates_talk_reaches_begin_conversation() {
    NpcWorld w(2);
    const uint8_t times[4] = {10, 10, 10, 10};
    auto &npc = w.add_npc(/*slot=*/5, /*type=*/0x40, /*dialog=*/9, /*ai=*/4, times, /*idx=*/0, 10, 9);
    w.pass();

    check(w.dialogue_session.active && w.dialogue_session.npc.schedule.slot == npc.schedule.slot,
          "B1: the real town-turn NpcInitiatesTalk trigger (blackthorn_turn_effect, "
          "blackthorn.cpp:56-68) must reach CommandKind::BeginConversation{member=npc.schedule.slot} "
          "so the dialogue session opens with the emitting actor's identity -- RED expected: "
          "UiSession::consume's switch has no NpcInitiatesTalk case (falls to `default: break;`, "
          "ui_session.cpp:979), so nothing calls BeginConversation");
    std::cout << "B1 (NPC-initiated talk reaches BeginConversation) executed\n";
}

// B2: npc_initiates_talk_owns_and_returns_mode.
static void b2_npc_initiates_talk_owns_and_returns_mode() {
    const uint8_t times[4] = {10, 10, 10, 10};
    // Part 1 -- RED: the same real trigger as B1.
    {
        NpcWorld w(2);
        w.add_npc(5, 0x40, 9, 4, times, 0, 10, 9);
        check(w.ui.mode() == UiMode::Exploration, "B2 part 1 setup: mode starts Exploration");
        w.pass();
        check(w.ui.base_mode() == UiMode::Dialogue,
              "B2 part 1: the real NPC-initiated talk trigger must move UiSession's base mode from "
              "Exploration into Dialogue (the same DialogueOutputKind::Prompt -> "
              "set_base_mode(Dialogue) path Batch-1's R-18 already covers for player-triggered Talk "
              "-- base_mode, not mode, is the right level: an active conversation's own reply prompt "
              "always sits in the TextEntry modal on top of it) -- RED expected: nothing dispatches "
              "BeginConversation today (see B1), so base_mode never leaves Exploration");
    }
    // Part 2 -- GREEN guard: CommandKind::BeginConversation is already a
    // complete, already-tested production entry point, and UiSession's
    // Dialogue mode-ownership/return-register machinery (Batch-1 R-18) is
    // already complete and orthogonal to R-10. This drives BOTH directly --
    // through the same real CommandContext/UiSession pair, no test-local
    // reimplementation of either -- proving the Batch-4 fix only needs to
    // connect the missing initiation (B1's RED gap), never redesign mode
    // ownership.
    {
        NpcWorld w(2);
        auto &npc = w.add_npc(5, 0x40, 9, 4, times, 0, 10, 9);
        check(w.ui.mode() == UiMode::Exploration, "B2 part 2 setup: mode starts Exploration");
        Command begin;
        begin.kind = CommandKind::BeginConversation;
        begin.member = int16_t(npc.schedule.slot);
        dispatch_world_command(w.ctx, begin);
        check(w.ui.base_mode() == UiMode::Dialogue && w.dialogue_session.active,
              "B2 part 2 GREEN guard: dispatching the real CommandKind::BeginConversation{member=slot} "
              "directly (the production initiation path the Batch-4 fix must wire NPC-initiation "
              "into) already correctly moves UiSession's base mode into Dialogue today");
        Command end;
        end.kind = CommandKind::EndConversation;
        dispatch_world_command(w.ctx, end);
        check(w.ui.base_mode() == UiMode::Exploration && !w.dialogue_session.active,
              "B2 part 2 GREEN guard: EndConversation already correctly restores UiSession's base "
              "mode to Exploration (the Batch-1 R-18 dialogue_return_mode_ register) -- the Batch-4 "
              "fix must connect initiation without touching this");
    }
    std::cout << "B2 (NPC-initiated talk owns and returns mode) executed\n";
}

// B3: npc_initiates_shop_opens_the_shop_session.
static void b3_npc_initiates_shop_opens_the_shop_session() {
    NpcWorld w(2, 0, 6); // hour=6 with times={0,6,0,6} below resolves an OPEN shop schedule index.
    w.enable_shop();
    const uint8_t times[4] = {0, 6, 0, 6};
    w.add_npc(/*slot=*/6, /*type=*/0x50, /*dialog=*/0x81, /*ai=*/4, times, /*idx=*/1, 10, 9);
    w.pass();

    check(w.shop_session.phase != ShopPhase::Closed && w.shop_session.type == ShopType::Blacksmith &&
              w.ui.mode() == UiMode::Shop,
          "B3: the real town-turn NpcInitiatesShop trigger must reach "
          "CommandKind::BeginConversation{member=slot} -> DialogueHandoff::Shop -> begin_shop() "
          "(dialogue_orchestration.cpp:139), opening a real shop session and moving UiSession to "
          "Shop mode -- RED expected: same UiSession::consume `default: break;` gap as B1, so "
          "nothing ever calls BeginConversation and shop.phase stays Closed");
    std::cout << "B3 (NPC-initiated shop opens the shop session) executed\n";
}

// B4: npc_initiated_shop_uses_same_entry_as_player_talk.
static void b4_npc_initiated_shop_uses_same_entry_as_player_talk() {
    const uint8_t times[4] = {0, 6, 0, 6};
    ShopPhase player_phase{};
    ShopType player_type{};
    uint8_t player_slot = 0;
    // Ordinary player-triggered entry: real CommandKind::Talk with a
    // direction, through the SAME real
    // execute_dialogue_command()/begin_shop() production path
    // (dialogue_orchestration.cpp:125-139) that NPC-initiation must reuse.
    {
        NpcWorld w(2, 0, 6);
        w.enable_shop();
        w.add_npc(6, 0x50, 0x81, 4, times, 1, 10, 9);
        Command talk;
        talk.kind = CommandKind::Talk;
        talk.has_direction = true;
        talk.direction = Direction::North; // party (10,10) faces the npc at (10,9).
        dispatch_world_command(w.ctx, talk);
        player_phase = w.shop_session.phase;
        player_type = w.shop_session.type;
        player_slot = w.dialogue_session.npc.schedule.slot;
        check(player_phase != ShopPhase::Closed,
              "B4 setup: ordinary player-triggered Talk already opens the shop (pre-existing, GREEN)");
    }
    // NPC-initiated entry: the real town-turn trigger, identical NPC/location fixture.
    {
        NpcWorld w(2, 0, 6);
        w.enable_shop();
        w.add_npc(6, 0x50, 0x81, 4, times, 1, 10, 9);
        w.pass();
        const bool equivalent = w.shop_session.phase == player_phase && w.shop_session.type == player_type &&
                                w.dialogue_session.npc.schedule.slot == player_slot &&
                                w.ui.mode() == UiMode::Shop;
        check(equivalent,
              "B4: NPC-initiated shop entry must land in the SAME production shop state as ordinary "
              "player-triggered Talk for the identical NPC -- phase, shop type, NPC identity and "
              "mode ownership -- proving the Batch-4 fix reuses BeginConversation rather than "
              "inventing a second shop entry point. RED expected: the NPC-initiated arm is "
              "unreachable today (same gap as B3), so its shop stays Closed while the "
              "player-triggered arm above already succeeds");
    }
    std::cout << "B4 (NPC-initiated shop uses the same entry as player Talk) executed\n";
}

// B5: npc_initiation_identity_lifetime.
static void b5_npc_initiation_identity_lifetime() {
    // GameEvent::npc is a borrowed pointer, valid only for synchronous
    // delivery ("borrowed identity for semantic conversation/shop
    // initiation", movement.h:56). This test taps the SAME real event stream
    // every other Group B test already flows through ctx.events, and copies
    // ONLY the stable identity (schedule.slot, location) out of the event
    // during that synchronous window -- it never stores or later
    // dereferences the borrowed NpcActor* itself.
    NpcWorld w(2);
    const uint8_t times[4] = {10, 10, 10, 10};
    auto &npc = w.add_npc(5, 0x40, 9, 4, times, 0, 10, 9);

    struct Tap {
        UiSession *ui;
        int32_t slot = -1;
        uint8_t location = 0;
    } tap{&w.ui};
    w.ctx.events = {&tap, [](void *p, const GameEvent &e) {
                        auto &t = *static_cast<Tap *>(p);
                        if (e.kind == GameEventKind::NpcInitiatesTalk && e.npc) {
                            t.slot = e.npc->schedule.slot;
                            t.location = e.npc->location;
                        }
                        t.ui->consume(e); // Forward, exactly like ui.event_sink() would.
                    }};
    w.pass();

    check(tap.slot == int32_t(npc.schedule.slot) && tap.location == npc.location,
          "B5 setup: the real NpcInitiatesTalk event's borrowed npc pointer resolves to the "
          "expected (schedule.slot, location) identity at synchronous delivery time -- the ONLY "
          "identity a deferred consumer may retain");
    NpcActor *resolved = nullptr;
    for (size_t i = 0; i < w.actors.count; ++i)
        if (w.actors.actors[i].location == tap.location && w.actors.actors[i].schedule.slot == tap.slot)
            resolved = &w.actors.actors[i];
    check(resolved == &npc,
          "B5: re-resolving the actor from the copied (slot, location) identity, AFTER the emitting "
          "command has fully returned/unwound, must find the SAME actor the event named -- the "
          "invariant the future deferred drain (R-10 ruling: AlphaRuntime::consume_event, deferred, "
          "never the immediate re-entrant call the adjudication measured as merely tolerated) "
          "depends on");
    check(!w.dialogue_session.active,
          "B5: there is no production deferred register/consumer at all today (see the R-10 "
          "ruling) -- nothing re-resolves this identity after the turn unwinds and dispatches "
          "BeginConversation from it, so a dialogue session never opens even though this test just "
          "proved the identity round-trip is sound. This legitimately remains RED with no "
          "pending/deferred register added in this pass");
    std::cout << "B5 (NPC-initiated identity lifetime) executed\n";
}

int main() {
    b1_npc_initiates_talk_reaches_begin_conversation();
    b2_npc_initiates_talk_owns_and_returns_mode();
    b3_npc_initiates_shop_opens_the_shop_session();
    b4_npc_initiated_shop_uses_same_entry_as_player_talk();
    b5_npc_initiation_identity_lifetime();
    std::cout << checks << " batch4 group B checks executed, " << failures << " failed\n";
    return failures > 0 ? 1 : 0;
}
