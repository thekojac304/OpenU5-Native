// Batch 4 GROUP A -- RED tests for R-09 (Blackthorn/guard modal handling and
// FountainDrink presentation). See
// native/targets/tdeck/ALPHA20_BATCH4_ADJUDICATION.md.
//
// These tests drive the REAL production town-turn -> blackthorn_turn_effect
// trigger (native/core/src/blackthorn.cpp, reached from the real town-turn
// tail at native/core/src/commands.cpp:288), the REAL UiSession
// handle_input()/consume()/finish_modal()/cancel_modal() modal machinery,
// and -- wherever the modal answer produces a real Command -- the REAL
// execute_command()/dispatch_world_command() gameplay path. This is the
// "real event source -> real UiSession -> real Command execution -> real
// gameplay state" chain the batch spec requires.
//
// No AlphaRuntime code is duplicated and no test-local copy of the modal
// translation is built. The GuardWorld harness below only forwards whatever
// UiIntentKind::Command a real UiSession::finish_modal()/command() call
// actually produces, exactly mirroring AlphaRuntime::command()'s own
// dispatch_world_command() call (native/targets/tdeck/main/alpha_runtime.cpp
// :317-355) and AlphaRuntime::consume_event()'s ui_->consume(e) forwarding
// (:253-254). Today finish_modal() has no translation for the four
// Blackthorn/guard requests (Blackthorn, GuardPassword, GuardTribute,
// GuardArrest), so it produces a bare UiIntentKind::ModalResponse instead of
// a Command; the harness records that intent but has nothing to execute,
// exactly reproducing the documented softlock -- it is not a stand-in modal
// translation, and it is never extended to cover those four requests.
#include "openu5/actors.h"
#include "openu5/blackthorn.h"
#include "openu5/commands.h"
#include "openu5/look.h"
#include "openu5/npc_path.h"
#include "openu5/ui_session.h"
#include "openu5/world.h"
#include <algorithm>
#include <cstring>
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
        std::cerr << "[FAIL] batch4 group A check " << checks << ": " << what << "\n";
    } else {
        std::cerr << "[PASS] batch4 group A check " << checks << ": " << what << "\n";
    }
}
UiAction ch(char16_t c) {
    UiAction a;
    a.kind = UiActionKind::Character;
    a.character = c;
    return a;
}
UiAction confirm_action() {
    UiAction a;
    a.kind = UiActionKind::Confirm;
    return a;
}
UiAction cancel_action() {
    UiAction a;
    a.kind = UiActionKind::Cancel;
    return a;
}

// Real town-turn guard fixture: one walkable 32x32 small map (matching the
// MapData/WorldData shape native/core/tests/batch3_group_a_test.cpp's A6
// already uses for a real town turn), a real BlackthornSession, and a real
// UiSession whose dispatch() forwards real Commands into the same real
// CommandContext -- mirroring AlphaRuntime::command()'s
// dispatch_world_command() call.
//
// A guard NPC added via add_guard() is parked EXACTLY at its own scheduled
// post for a times={hour,hour,hour,hour} schedule (schedule_index always
// resolves slot 0 deterministically). That is what the real NPC-movement
// pass (actors()/tick_npcs(), which this fixture deliberately does NOT
// bypass or stub -- it runs for real on every Pass) requires to leave a
// freshly-scheduled actor in place: npc_check_schedule()'s final
// position-match check forces state back to 1, and tick_npcs() takes its
// `if (n.state == 1) continue;` no-op path (native/core/src/npc_path.cpp
// :295-296) before any pathfinding/AI-step code runs. No adjacency-distance
// argument is required for this -- it holds for any ai value.
struct GuardWorld {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    NpcActors actors{};
    NpcScanGrid scratch{};
    BlackthornSession blackthorn{};
    std::vector<uint8_t> large, small;
    MapData local{};
    WorldData world_data{};
    CommandContext ctx;
    UiTextBlock blocks[64]{};
    UiSession ui;
    std::vector<UiIntent> intents;
    ActionResult last_command_result{};

    explicit GuardWorld(uint8_t location, int16_t floor = 0, uint8_t hour = 10)
        : large(65536, 5), small(1024, 5), ctx(game, turn, travel, commands, world_data),
          ui({blocks, 64}, {this, &GuardWorld::dispatch}, {60, 12, 16}) {
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
        game.party.party_size = game.party.character_count = 2;
        for (int i = 0; i < 2; ++i) game.party.characters[i].status = 'G';
    }
    NpcActor &add_guard(uint8_t slot, uint8_t type, uint8_t dialog, uint8_t ai, int16_t npc_x,
                       int16_t npc_y) {
        auto &n = actors.actors[actors.count++];
        n.location = game.position.map.location;
        n.z = game.position.map.floor;
        n.x = npc_x;
        n.y = npc_y;
        n.schedule.slot = slot;
        n.schedule.type = type;
        n.schedule.dialog = dialog;
        for (auto &t : n.schedule.times) t = uint8_t(game.time.hour);
        n.schedule.ai[0] = ai;
        n.schedule.x[0] = uint8_t(npc_x);
        n.schedule.y[0] = uint8_t(npc_y);
        n.schedule.z[0] = uint8_t(game.position.map.floor);
        return n;
    }
    static void dispatch(void *p, const UiIntent &i) {
        auto &w = *static_cast<GuardWorld *>(p);
        w.intents.push_back(i);
        if (i.kind == UiIntentKind::Command) w.last_command_result = dispatch_world_command(w.ctx, i.command);
    }
    // The real, production town-turn trigger: 'Pass' through UiSession's
    // ordinary exploration handler, exactly like a player pressing Space.
    void pass() { ui.handle_input(confirm_action()); }
};
} // namespace

// A1: guard_tribute_yes_debits_and_clears.
static void a1_guard_tribute_yes_debits_and_clears() {
    GuardWorld w(2);
    w.game.gold = 500;
    w.add_guard(/*slot=*/5, /*type=*/112, /*dialog=*/255, /*ai=*/4, /*x=*/10, /*y=*/9);
    w.pass();
    check(w.ui.mode() == UiMode::YesNo && w.ui.request() == UiRequestId::GuardTribute,
          "A1 setup: the real guard tribute path opens the reference YesNo prompt");
    w.ui.handle_input(ch(u'y'));

    check(w.game.gold == 480 && !w.blackthorn.tribute,
          "A1: answering the real tribute prompt Yes must dispatch "
          "CommandKind::BlackthornAction{Tribute,agree=1} so blackthorn_action() debits gold by "
          "10 x living party members (500 -> 480 for 2 living members) and clears "
          "BlackthornSession::tribute -- RED expected: finish_modal has no GuardTribute branch, so "
          "nothing is dispatched, gold is unchanged and the flag is never cleared");

    Command pass;
    pass.kind = CommandKind::Pass;
    auto after = execute_command(w.ctx, pass);
    check(after.status != CommandStatus::AwaitingResponse,
          "A1: after the tribute answer, the world must not remain blocked by AwaitingResponse");
    std::cout << "A1 (guard tribute yes debits and clears) executed\n";
}

// A2: guard_tribute_no_escalates_to_arrest.
static void a2_guard_tribute_no_escalates_to_arrest() {
    GuardWorld w(2);
    w.game.gold = 500;
    w.add_guard(5, 112, 255, 4, 10, 9);
    w.pass();
    check(w.ui.mode() == UiMode::YesNo && w.ui.request() == UiRequestId::GuardTribute,
          "A2 setup: the real guard tribute path opens the reference YesNo prompt");
    w.ui.handle_input(ch(u'n'));

    check(w.ui.mode() == UiMode::YesNo && w.ui.request() == UiRequestId::GuardArrest &&
              w.ui.base_mode() == UiMode::Exploration,
          "A2: refusing the real tribute prompt must dispatch "
          "CommandKind::BlackthornAction{Tribute,agree=0} so blackthorn_action() resolves the "
          "(unpaid) tribute and chains directly into the real GuardArrestPrompt modal, with "
          "base-mode ownership staying Exploration -- RED expected: finish_modal has no "
          "GuardTribute branch, so nothing is dispatched and no chained modal opens (mode falls "
          "back to plain Exploration instead)");
    check(w.game.gold == 500, "A2 GREEN guard: refusing tribute must never debit gold");
    std::cout << "A2 (guard tribute no escalates to arrest) executed\n";
}

// A3: guard_arrest_yes_jails_exactly.
static void a3_guard_arrest_yes_jails_exactly() {
    GuardWorld w(2);
    w.add_guard(7, 112, 255, /*ai=*/6, 10, 9);
    w.pass();
    check(w.ui.mode() == UiMode::YesNo && w.ui.request() == UiRequestId::GuardArrest,
          "A3 setup: an ai>5 hostile guard of type 112 opens the arrest prompt directly, with no "
          "tribute step");
    w.ui.handle_input(ch(u'y'));

    const auto &pos = w.game.position;
    const bool jailed = pos.map.location == 4 && pos.xy.x == 25 && pos.xy.y == 4 && w.game.keys == 0 &&
                        w.game.time.hour == 8;
    check(jailed && !w.blackthorn.arrest,
          "A3: answering the real arrest prompt Yes must dispatch "
          "CommandKind::BlackthornAction{Arrest,agree=1} so blackthorn_action() applies the exact "
          "reference guardArrestJail mutation (location=4, (25,4), keys=0, hour=8) and clears "
          "BlackthornSession::arrest -- RED expected: finish_modal has no GuardArrest branch, so "
          "nothing is dispatched");

    Command pass;
    pass.kind = CommandKind::Pass;
    auto after = execute_command(w.ctx, pass);
    check(after.status != CommandStatus::AwaitingResponse,
          "A3: after the arrest answer, the world must not remain blocked by AwaitingResponse");
    std::cout << "A3 (guard arrest yes jails exactly) executed\n";
}

// A4: guard_arrest_no_consumes_machine.
static void a4_guard_arrest_no_consumes_machine() {
    GuardWorld w(2);
    auto &npc = w.add_guard(7, 112, 255, 6, 10, 9);
    w.pass();
    check(w.ui.mode() == UiMode::YesNo && w.ui.request() == UiRequestId::GuardArrest,
          "A4 setup: the same real direct-arrest path as A3");
    w.ui.handle_input(ch(u'n'));

    check(!w.blackthorn.arrest,
          "A4: answering the real arrest prompt No must dispatch "
          "CommandKind::BlackthornAction{Arrest,agree=0} so blackthorn_action() clears "
          "BlackthornSession::arrest -- RED expected: finish_modal has no GuardArrest branch, so "
          "nothing is dispatched and the flag is never cleared");
    check(npc.schedule.ai[0] == 7,
          "A4: the reference's \"defend thyself\" escalation runs the real dialogue_alarm() over "
          "the town's guards (type 0x70/112 -> ai forced to 7 on every schedule slot, "
          "dialogue_orchestration.cpp:12-14, reached from blackthorn_action()'s real Arrest/No "
          "branch) -- this is a clean, already-production host assertion for the hostile path, "
          "reached only when BlackthornAction::Arrest is actually dispatched. RED expected for the "
          "same reason as above: dialogue_alarm() never runs today because nothing is dispatched");

    Command pass;
    pass.kind = CommandKind::Pass;
    auto after = execute_command(w.ctx, pass);
    check(after.status != CommandStatus::AwaitingResponse,
          "A4: the world must not remain stuck in AwaitingResponse after the arrest refusal");
    std::cout << "A4 (guard arrest no consumes machine) executed\n";
}

// A5: guard_password_impe_passes.
static void a5_guard_password_impe_passes() {
    GuardWorld w(18);
    w.turn.time_spell = '\x1d';
    w.add_guard(7, 112, 255, 4, 10, 9);
    w.pass();
    check(w.ui.mode() == UiMode::TextEntry && w.ui.request() == UiRequestId::GuardPassword,
          "A5 setup: the real Palace badge-guard path opens the reference GuardPasswordPrompt "
          "(byte-exact prompt text is blackthorn.cpp's own record, already verified by the "
          "adjudication's R-09-C repro, not re-tested here)");
    w.ui.handle_input(ch(u'I'));
    w.ui.handle_input(ch(u'M'));
    w.ui.handle_input(ch(u'P'));
    w.ui.handle_input(ch(u'E'));
    w.ui.handle_input(confirm_action());

    bool saw_pass_friend = false;
    for (size_t i = 0; i < w.ui.transcript_size(); ++i) {
        const auto *b = w.ui.transcript_at(i);
        if (b && std::string(b->text, b->length).find("Pass, friend!") != std::string::npos)
            saw_pass_friend = true;
    }
    check(!w.blackthorn.password && saw_pass_friend,
          "A5: entering IMPE through the real text-entry path must dispatch "
          "CommandKind::BlackthornAction{Password,text=\"IMPE\"} so blackthorn_action() clears "
          "BlackthornSession::password and prints \"Pass, friend!\" -- RED expected: finish_modal "
          "has no GuardPassword branch, so nothing is dispatched (do not manually invoke "
          "blackthorn_action -- this drives only the real UiAction text-entry path)");

    Command pass;
    pass.kind = CommandKind::Pass;
    auto after = execute_command(w.ctx, pass);
    check(after.status != CommandStatus::AwaitingResponse,
          "A5: after the password answer, a following world command must not be blocked by "
          "AwaitingResponse");
    std::cout << "A5 (guard password IMPE passes) executed\n";
}

// A6: guard_password_cancel_submits_empty (mandatory).
static void a6_guard_password_cancel_submits_empty() {
    GuardWorld w(18);
    w.turn.time_spell = '\x1d';
    w.add_guard(7, 112, 255, 4, 10, 9);
    w.pass();
    check(w.ui.mode() == UiMode::TextEntry && w.ui.request() == UiRequestId::GuardPassword,
          "A6 setup: the same real Palace badge-guard path as A5");
    w.ui.handle_input(cancel_action());

    check(!w.blackthorn.password,
          "A6: the reference's cancelled text entry resolves as an EMPTY submission "
          "(askText.cancel -> onText(\"\")) -- a FAILED password response, not a silent dismissal "
          "-- which still clears BlackthornSession::password (escalating toward capture); it must "
          "never leave the machine stranded. RED expected: the real UiActionKind::Cancel path "
          "(cancel_modal(), since GuardPassword's begin_text() defaults escape_clears=false) "
          "dispatches a bare accepted=false ModalResponse with no Command translation, so nothing "
          "resolves the machine and it stays stranded");

    Command pass;
    pass.kind = CommandKind::Pass;
    auto after = execute_command(w.ctx, pass);
    check(after.status != CommandStatus::AwaitingResponse,
          "A6: cancelling the password prompt must not leave the world in a hidden permanent "
          "AwaitingResponse state -- this is the single highest-risk softlock the adjudication "
          "identifies (worse than the other three: the prompt is gone, so the player cannot even "
          "see why the game is stuck)");
    std::cout << "A6 (guard password cancel submits empty) executed\n";
}

// A7: blackthorn_answer_advances_interrogation.
static void a7_blackthorn_answer_advances_interrogation() {
    // The full shrine interrogation scene cannot be built end-to-end on the
    // host: blackthorn_action(Answer)'s Capture/re-question path requires the
    // 12 MISCMSG shrine records the device supplies via
    // ShrineServices::record, which this harness does not have (see the
    // adjudication's "Not reproducible on the host" note). This fixture
    // instead arms the real BlackthornSession directly and delivers the real
    // BlackthornPrompt event -- the smallest fixture the adjudication
    // identifies as justified -- then answers through the real UiSession
    // text-entry path. It does NOT build a test-local copy of the modal
    // translation: it only inspects what UiSession itself actually
    // dispatches.
    GuardWorld w(2);
    w.blackthorn.shrine = 0;
    w.blackthorn.round = 0;
    w.blackthorn.living = 2;
    GameEvent e;
    e.kind = GameEventKind::BlackthornPrompt;
    e.text = "\"What sayest thou to this virtue?\"\n\nYour response?";
    w.ui.consume(e);
    check(w.ui.mode() == UiMode::TextEntry && w.ui.request() == UiRequestId::Blackthorn &&
              w.ui.base_mode() == UiMode::ShrineSpecial,
          "A7 setup: BlackthornPrompt opens the real Blackthorn text prompt under the Batch-1 "
          "ShrineSpecial base mode (enter_shrine_mode()) -- guards that register's own semantics "
          "before the answer is exercised");

    w.ui.handle_input(ch(u'N'));
    w.ui.handle_input(ch(u'o'));
    w.ui.handle_input(confirm_action());

    const bool dispatched_answer = std::any_of(w.intents.begin(), w.intents.end(), [](const UiIntent &i) {
        return i.kind == UiIntentKind::Command && i.command.kind == CommandKind::BlackthornAction &&
               i.command.item == int16_t(BlackthornAction::Answer);
    });
    check(dispatched_answer,
          "A7: answering the real Blackthorn text prompt must dispatch "
          "CommandKind::BlackthornAction{item=Answer,text=\"No\"} so the real blackthorn_action() "
          "can advance the interrogation round or resolve the shrine -- RED expected: finish_modal "
          "has no Blackthorn branch, so nothing is dispatched and the armed BlackthornSession is "
          "never touched. (This host fixture cannot go further: even a correct Answer dispatch "
          "would still return InvalidContext here because no ShrineServices::record is wired, "
          "matching the adjudication's stated host limit -- this test proves only that the request "
          "reaches production dispatch, which is the part currently missing.)");
    std::cout << "A7 (Blackthorn answer advances interrogation) executed\n";
}

// A8: blackthorn_machine_never_softlocks_the_world.
static void a8_blackthorn_machine_never_softlocks_the_world() {
    struct Case {
        const char *name;
        void (*arm)(BlackthornSession &);
    };
    const Case cases[] = {
        {"Blackthorn answer", [](BlackthornSession &s) { s.shrine = 0; }},
        {"GuardPassword", [](BlackthornSession &s) { s.password = true; }},
        {"GuardTribute", [](BlackthornSession &s) { s.tribute = true; }},
        {"GuardArrest", [](BlackthornSession &s) { s.arrest = true; }},
    };
    for (const auto &c : cases) {
        GameState game{};
        game.party.party_size = game.party.character_count = 1;
        game.party.characters[0].status = 'G';
        game.position.map.location = 2;
        TurnState turn{};
        TravelState travel{};
        CommandState commands{};
        BlackthornSession blackthorn{};
        c.arm(blackthorn);
        std::vector<uint8_t> large(65536, 5), small(1024, 5);
        MapData local{{2, 0}, small.data(), small.size()};
        WorldData world{large.data(), large.data(), large.size(), large.size(), &local, 1};
        CommandContext ctx{game, turn, travel, commands, world};
        ctx.blackthorn = &blackthorn;
        Command pass;
        pass.kind = CommandKind::Pass;
        auto result = execute_command(ctx, pass);
        check(result.status != CommandStatus::AwaitingResponse,
              std::string("A8: after ") + c.name +
                  " resolves (via a real modal answer flow), a subsequent Pass must not remain "
                  "AwaitingResponse -- RED expected: today all four Blackthorn flows leave their "
                  "flag set forever (see A1/A3/A5/A7), so this reproduces the exact user-visible "
                  "permanent softlock at commands.cpp's `c.blackthorn && (shrine>=0||password||"
                  "tribute||arrest)` AwaitingResponse gate");
    }
    std::cout << "A8 (Blackthorn machine never softlocks the world) executed\n";
}

// A9: fountain_opens_party_selection.
static void a9_fountain_opens_party_selection() {
    GuardWorld w(2);
    GameEvent e;
    e.kind = GameEventKind::FountainDrinkPrompt;
    w.ui.consume(e);

    const bool opened_party_selection = std::any_of(w.intents.begin(), w.intents.end(), [](const UiIntent &i) {
        return i.kind == UiIntentKind::OpenPartySelection && i.request == UiRequestId::FountainDrink;
    });
    check(opened_party_selection,
          "A9: FountainDrinkPrompt must raise UiIntentKind::OpenPartySelection with "
          "request=UiRequestId::FountainDrink (that request id already exists; no new UiRequestId "
          "is needed to express this) instead of the current begin_yes_no(FountainDrink,\"Drink?\") "
          "-- mirrors main.ts's pickMember(\"Who will drink?\"). RED expected against "
          "ui_session.cpp's current FountainDrinkPrompt case");
    check(w.ui.mode() == UiMode::PartySelection,
          "A9: the fountain must present a party/member picker, not a YesNo \"Drink?\" modal -- "
          "RED expected: current mode is YesNo");
    check(std::strcmp(w.ui.prompt(), "Who will drink?") == 0,
          "A9: the fountain prompt text must read \"Who will drink?\", matching the reference's "
          "pickMember(\"Who will drink?\") -- RED expected: current prompt text is \"Drink?\"");
    std::cout << "A9 (fountain opens party selection) executed\n";
}

// A10: fountain_status_text_and_cancel.
//
// GREEN pass addendum: the adjudication's "Stated uncertainties" (section G)
// left A10's owner unresolved between AlphaRuntime (ESP-only) and "a
// not-yet-designed ESP-free helper ('(status,cancelled) -> const char*') that
// does not exist in this tree". That helper now exists --
// openu5::fountain_drink_result(char status, bool cancelled) in
// native/core/include/openu5/look.h / src/look.cpp -- pure, tiny,
// dependency-free, and directly host-testable, exactly the shape the
// adjudication favoured. AlphaRuntime::modal()'s FountainDrink arm (device
// layer, not host-buildable) resolves the picked party member's status and
// calls this same function, so this test is the authoritative host coverage
// for the flavour text the reference (main.ts) prints.
static void a10_fountain_status_text_and_cancel() {
    check(std::strcmp(fountain_drink_result('G', false), "Refreshing...") == 0,
          "A10: a living, healthy member ('G') who drinks must get \"Refreshing...\", matching the "
          "reference's default pickMember(\"Who will drink?\") outcome");
    check(std::strcmp(fountain_drink_result('D', false), "Incapacitated!") == 0,
          "A10: a dead member ('D') must get \"Incapacitated!\", matching the reference's "
          "characters[m].status == 'D' branch");
    check(std::strcmp(fountain_drink_result('S', false), "Incapacitated!") == 0,
          "A10: a sleeping member ('S') must also get \"Incapacitated!\", matching the reference's "
          "characters[m].status == 'S' branch");
    check(std::strcmp(fountain_drink_result('G', true), "None!") == 0,
          "A10: cancelling the party picker must get \"None!\" regardless of any member's status -- "
          "no HP, no party, no turn, no gameplay command, matching the reference's pure flavour-text "
          "contract");
    std::cout << "A10 (fountain status text and cancel) executed\n";
}

int main() {
    a1_guard_tribute_yes_debits_and_clears();
    a2_guard_tribute_no_escalates_to_arrest();
    a3_guard_arrest_yes_jails_exactly();
    a4_guard_arrest_no_consumes_machine();
    a5_guard_password_impe_passes();
    a6_guard_password_cancel_submits_empty();
    a7_blackthorn_answer_advances_interrogation();
    a8_blackthorn_machine_never_softlocks_the_world();
    a9_fountain_opens_party_selection();
    a10_fountain_status_text_and_cancel();
    std::cout << checks << " batch4 group A checks executed, " << failures << " failed\n";
    return failures > 0 ? 1 : 0;
}
