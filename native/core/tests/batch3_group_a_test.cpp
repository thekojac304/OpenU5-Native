// Batch 3 GROUP A -- RED tests for R-19 (Yell/sails), R-20 (harpsichord),
// Y-20 (SetActivePlayer). See native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md.
//
// These tests exercise the REAL production UiSession::handle_input() ->
// dispatch() path with a spy UiControllerServices, exactly like
// native/core/tests/ui_session_test.cpp. They do not touch AlphaRuntime
// (not host-buildable) or invent any modal UX. Where UiSession needs
// context it does not have today (frigate/sail state, harpsichord
// adjacency), the tests use the two Batch-3 RED-test seam setters added to
// ui_session.h (set_sail_context / set_harpsichord_active). Those setters
// are pure storage -- see the seam comment in ui_session.h -- they do not
// change any routing decision, so using them does not itself constitute a
// fix.
#include "openu5/quest.h"
#include "openu5/quest_world.h"
#include "openu5/ui_session.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace openu5;
namespace {
int checks = 0, failures = 0;
void check(bool v, const char *what) {
    ++checks;
    if (!v) {
        ++failures;
        std::cerr << "[FAIL] batch3 group A check " << checks << ": " << what << "\n";
    } else {
        std::cerr << "[PASS] batch3 group A check " << checks << ": " << what << "\n";
    }
}
struct Spy {
    std::vector<UiIntent> intents;
    static void send(void *p, const UiIntent &i) { static_cast<Spy *>(p)->intents.push_back(i); }
};
UiAction ch(char c) {
    UiAction a;
    a.kind = UiActionKind::Character;
    a.character = char16_t(c);
    return a;
}
} // namespace

static void group_a1_a2_a3_yell_sails() {
    // A1: RED -- aboard a frigate, in a location sails are legal, 'y' must
    // dispatch CommandKind::YellSails and must NOT open the YellText modal.
    // Desired per game.ts::yell(): isFrigate(transport) && location<0x80 ->
    // yellSails(); UiSession has no route to this today (ui_session.cpp's
    // 'y' case is unconditional), so this is expected to fail RED.
    {
        UiTextBlock blocks[8];
        Spy spy;
        UiSession ui{{blocks, 8}, {&spy, Spy::send}, {40, 8, 12}};
        ui.set_sail_context(/*frigate_aboard=*/true, /*location_allows_sails=*/true);
        const auto before = spy.intents.size();
        ui.handle_input(ch('y'));
        const bool dispatched_sails =
            spy.intents.size() > before &&
            spy.intents.back().kind == UiIntentKind::Command &&
            spy.intents.back().command.kind == CommandKind::YellSails;
        const bool opened_yell_text = ui.mode() == UiMode::TextEntry;
        check(dispatched_sails && !opened_yell_text,
              "A1: expected 'y' aboard a frigate to dispatch YellSails without opening YellText "
              "(RED expected: current handle_exploration always opens YellText)");
    }
    // A2: GREEN guard -- non-frigate transport (or a location where sails
    // are not legal) must still fall through to the ordinary word-of-power
    // Yell prompt. This should already pass today since it's the unconditional
    // current behavior.
    {
        UiTextBlock blocks[8];
        Spy spy;
        UiSession ui{{blocks, 8}, {&spy, Spy::send}, {40, 8, 12}};
        ui.set_sail_context(/*frigate_aboard=*/false, /*location_allows_sails=*/true);
        ui.handle_input(ch('y'));
        check(ui.mode() == UiMode::TextEntry && ui.request() == UiRequestId::YellText,
              "A2 GREEN guard: non-frigate Yell must still open the YellText modal");
    }
    // A3: GREEN guard -- the non-frigate Yell path still produces a real
    // CommandKind::Yell once the word is entered (word-of-power Yell
    // unaffected by any future sails routing).
    {
        UiTextBlock blocks[8];
        Spy spy;
        UiSession ui{{blocks, 8}, {&spy, Spy::send}, {40, 8, 12}};
        ui.set_sail_context(false, true);
        ui.handle_input(ch('y'));
        ui.handle_input(ch('v'));
        ui.handle_input(ch('a'));
        ui.handle_input(ch('s'));
        UiAction confirm;
        confirm.kind = UiActionKind::Confirm;
        ui.handle_input(confirm);
        check(spy.intents.back().kind == UiIntentKind::Command &&
                  spy.intents.back().command.kind == CommandKind::Yell,
              "A3 GREEN guard: word-of-power Yell still reaches CommandKind::Yell");
    }
    std::cout << "A1/A2/A3 (Yell/sails) executed\n";
}

static void group_a4_set_active_player() {
    // A4: RED -- a plain digit key in ordinary exploration (no harpsichord
    // context) should dispatch CommandKind::SetActivePlayer (Y-20). Today
    // UiSession::handle_exploration has zero digit-key handling, so digits
    // fall into the `default: "What?"` arm. Expected to fail RED.
    UiTextBlock blocks[8];
    Spy spy;
    UiSession ui{{blocks, 8}, {&spy, Spy::send}, {40, 8, 12}};
    ui.set_harpsichord_active(false);
    const auto before = spy.intents.size();
    ui.handle_input(ch('2'));
    const bool dispatched =
        spy.intents.size() > before && spy.intents.back().kind == UiIntentKind::Command &&
        spy.intents.back().command.kind == CommandKind::SetActivePlayer &&
        spy.intents.back().command.member == 2;
    check(dispatched,
          "A4: expected digit '2' outside harpsichord context to dispatch SetActivePlayer(member=2) "
          "(RED expected: no digit-key route exists in handle_exploration today)");
    std::cout << "A4 (SetActivePlayer digit routing) executed\n";
}

static void group_a5_harpsichord_intercept() {
    // A5: RED -- at the harpsichord tile/location, the SAME digit input must
    // route to CommandKind::HarpsichordNote instead, and must NOT dispatch
    // SetActivePlayer (reference: game.ts digit routing gives the
    // harpsichord intercept priority over SetActivePlayer). Expected to fail
    // RED for the same reason as A4 (no digit routing at all yet).
    UiTextBlock blocks[8];
    Spy spy;
    UiSession ui{{blocks, 8}, {&spy, Spy::send}, {40, 8, 12}};
    ui.set_harpsichord_active(true);
    const auto before = spy.intents.size();
    ui.handle_input(ch('6'));
    const bool dispatched_note =
        spy.intents.size() > before && spy.intents.back().kind == UiIntentKind::Command &&
        spy.intents.back().command.kind == CommandKind::HarpsichordNote;
    const bool dispatched_active_player =
        spy.intents.size() > before && spy.intents.back().kind == UiIntentKind::Command &&
        spy.intents.back().command.kind == CommandKind::SetActivePlayer;
    check(dispatched_note && !dispatched_active_player,
          "A5: expected digit '6' at the harpsichord to dispatch HarpsichordNote, not "
          "SetActivePlayer (RED expected: no digit-key route exists in handle_exploration today)");
    std::cout << "A5 (harpsichord digit intercept) executed\n";
}

static void group_a6_harpsichord_melody_core() {
    // A6: core-level GREEN guard. play_harpsichord()/CommandKind::HarpsichordNote
    // is already fully implemented (quest_world.cpp) independent of any UI
    // route. This drives the real 13-note melody
    // [6,7,8,9,8,7,8,7,6,7,6,5,3] through execute_command() at location 17
    // floor 2 and asserts the Cove passage-open mutation fires. A light grep
    // of the existing quest_driver/quest_parity fixtures did not turn up an
    // existing case that exercises the full 13-note sequence end-to-end
    // through execute_command(), so this is added as new, minimal core
    // coverage; it is expected to be GREEN since production already
    // implements it.
    struct Objects {
        std::vector<QuestObject> values;
    };
    Objects objects;
    auto count = [](void *p) { return static_cast<Objects *>(p)->values.size(); };
    auto read = [](void *p, size_t i) { return static_cast<Objects *>(p)->values[i]; };

    GameState game{};
    game.position.map.location = 17;
    game.position.map.floor = 2;
    game.party.party_size = game.party.character_count = 1;
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    static uint8_t large[65536]{}, small[1024]{};
    MapData local{{game.position.map.location, game.position.map.floor}, small, 1024};
    WorldData world{large, large, sizeof(large), sizeof(large), &local, 1};
    CommandContext ctx{game, turn, travel, commands, world};
    QuestWorldServices quest{};
    quest.context = &objects;
    quest.count = count;
    quest.read = read;
    ctx.quest_world = &quest;

    const int melody[] = {6, 7, 8, 9, 8, 7, 8, 7, 6, 7, 6, 5, 3};
    for (int digit : melody) {
        Command cmd;
        cmd.kind = CommandKind::HarpsichordNote;
        cmd.item = int16_t(digit);
        auto result = execute_command(ctx, cmd);
        check(result.status == CommandStatus::Success, "A6: each HarpsichordNote digit succeeds");
    }
    check(quest.passage_open, "A6 GREEN guard: the 13-note melody must open the Cove passage");
    std::cout << "A6 (harpsichord melody core, GREEN guard) executed\n";
}

int main() {
    group_a1_a2_a3_yell_sails();
    group_a4_set_active_player();
    group_a5_harpsichord_intercept();
    group_a6_harpsichord_melody_core();
    std::cout << checks << " batch3 group A checks executed, " << failures << " failed\n";
    return failures > 0 ? 1 : 0;
}
