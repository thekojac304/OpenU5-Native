// Batch 1 RED regression suite: R-01, R-18 and the ShrineSpecial strand.
//
// These tests encode the DESIRED invariants established by the Batch 1
// root-cause investigation.  Against baseline cfd42139 most of them are
// expected to FAIL; that is the point of this pass.  No production behavior is
// changed by this file.
//
// AlphaRuntime cannot be host-compiled (esp_log / esp_timer / esp_heap_caps /
// FreeRTOS / Board), so the per-input mode arbitration it performs is exercised
// through tdeck::resolve_synchronized_base_mode() in main/ui_mode_policy.h,
// which AlphaRuntime itself now calls.  Everything else here is the real
// UiSession and the real core orchestration.

#include "../main/ui_mode_policy.h"

#include "openu5/actors.h"
#include "openu5/commands.h"
#include "openu5/dialogue_orchestration.h"
#include "openu5/shop_orchestration.h"
#include "openu5/shrine.h"
#include "openu5/ui_session.h"

#include <cstdio>
#include <memory>
#include <vector>

using namespace openu5;

namespace {

// ---------------------------------------------------------------------------
// Result reporting.  Every case runs; the binary fails if any RED case is red.
// ---------------------------------------------------------------------------
int g_failures = 0;
int g_checks = 0;

void expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    if (ok) {
        std::printf("  GREEN  %-12s %s\n", id, what);
        return;
    }
    ++g_failures;
    std::printf("  RED    %-12s %s\n", id, what);
}

UiAction ch(char c) {
    UiAction a;
    a.kind = UiActionKind::Character;
    a.character = char16_t(c);
    return a;
}
UiAction dir(Direction d) {
    UiAction a;
    a.kind = UiActionKind::Direction;
    a.direction = d;
    return a;
}
UiAction act(UiActionKind k) {
    UiAction a;
    a.kind = k;
    return a;
}

const char *mode_name(UiMode m) {
    switch (m) {
    case UiMode::Exploration: return "Exploration";
    case UiMode::Dungeon: return "Dungeon";
    case UiMode::Combat: return "Combat";
    case UiMode::Dialogue: return "Dialogue";
    case UiMode::Shop: return "Shop";
    case UiMode::ShrineSpecial: return "ShrineSpecial";
    case UiMode::TextEntry: return "TextEntry";
    case UiMode::NumericEntry: return "NumericEntry";
    case UiMode::YesNo: return "YesNo";
    case UiMode::PartySelection: return "PartySelection";
    case UiMode::InventorySelection: return "InventorySelection";
    case UiMode::EquipmentSelection: return "EquipmentSelection";
    case UiMode::SpellSelection: return "SpellSelection";
    case UiMode::TargetSelection: return "TargetSelection";
    case UiMode::DebugMenu: return "DebugMenu";
    }
    return "?";
}

// A world mode is one the exploration/dungeon/combat routers accept.
bool is_world_mode(UiMode m) {
    return m == UiMode::Exploration || m == UiMode::Dungeon || m == UiMode::Combat;
}

// ---------------------------------------------------------------------------
// Device loop fixture.
//
// Mirrors AlphaRuntime::handle() for the gameplay path only:
//   ui_->handle_input(action)            alpha_runtime.cpp  (dispatch is synchronous)
//   synchronize_after_debug(...)         alpha_runtime.cpp  (tail of handle())
// and AlphaRuntime::dispatch() for the intent kinds these tests produce.
// ---------------------------------------------------------------------------
struct DeviceWorld {
    GameState g;
    TurnState turn;
    TravelState travel;
    CommandState commands;
    WorldData world;
    CommandContext c{g, turn, travel, commands, world};

    ShopSession shop;
    ShopData shop_data;
    ShopServices shop_services{shop, shop_data};
    NpcActors actors;
    DialogueSession talk;
    DialogueServices dialogue{talk};
    ShrineSession shrine;
    ShrineServices shrine_services{shrine};

    UiTextBlock blocks[32]{};
    UiSession ui{{blocks, 32}, {this, &DeviceWorld::thunk}, {32, 8, 12}};

    std::vector<UiIntent> intents;
    bool combat = false;
    bool dungeon = false;
    // When false the extracted per-input rebind is skipped, modelling the
    // world after R-01 is repaired.  SHRINE-2 compares both settings.
    bool apply_device_rebind = true;

    DeviceWorld() {
        c.shop_services = &shop_services;
        c.dialogue_services = &dialogue;
        c.shrine_services = &shrine_services;
        c.actors = &actors;
        c.events = ui.event_sink();
        g.party.character_count = g.party.party_size = 1;
        g.party.characters[0].intelligence = 15;
        g.party.characters[0].party_status = 0;
        g.party.characters[0].character_class = 'A';
        g.gold = 500;
    }

    static void thunk(void *p, const UiIntent &i) { static_cast<DeviceWorld *>(p)->dispatch(i); }

    // AlphaRuntime::dispatch(), restricted to the intents these tests raise.
    void dispatch(const UiIntent &i) {
        intents.push_back(i);
        if (i.kind == UiIntentKind::Command) {
            execute_command(c, i.command);
        } else if (i.kind == UiIntentKind::Shop) {
            execute_shop(c, i.shop);
        } else if (i.kind == UiIntentKind::ModalResponse) {
            modal(i);
        }
    }

    // AlphaRuntime::modal(), shrine-donate branch only.
    void modal(const UiIntent &i) {
        if (!i.value.accepted) return;
        if (i.request != UiRequestId::ShrineDonate) return;
        ShrineInput input;
        input.action = ShrineAction::Donate;
        input.value = i.value.number;
        Command cmd;
        cmd.kind = CommandKind::ShrineAction;
        cmd.shrine = &input;
        execute_command(c, cmd);
    }

    // One translated gameplay input, in AlphaRuntime::handle()'s exact order.
    void input(const UiAction &a) {
        ui.handle_input(a);
        if (!apply_device_rebind) return;
        ui.set_base_mode(tdeck::resolve_synchronized_base_mode(ui.base_mode(), combat, dungeon));
    }

    // A blacksmith standing one tile east of the party, as the real Talk
    // dispatcher resolves it (dialogue_orchestration.cpp).
    void place_shopkeeper() {
        g.position.map.location = 2;
        g.position.map.floor = 0;
        g.position.xy = {10, 10};
        actors.count = 1;
        auto &npc = actors.actors[0];
        npc.location = 2;
        npc.z = 0;
        npc.x = 11;
        npc.y = 10;
        npc.schedule.dialog = 0x81; // ShopType::Blacksmith
        npc.schedule.slot = 3;
        npc.schedule.times[0] = 1;
    }
};

// A bare UiSession with a recording dispatcher, for the pure mode-register
// tests.  No device rebind is applied: these model the post-R-01 world, which
// is exactly where R-18 becomes observable.
struct Bare {
    UiTextBlock blocks[32]{};
    std::vector<UiIntent> intents;
    UiSession ui{{blocks, 32}, {this, &Bare::thunk}, {32, 8, 12}};
    static void thunk(void *p, const UiIntent &i) {
        static_cast<Bare *>(p)->intents.push_back(i);
    }

    void combat_round_trip() {
        GameEvent e{};
        e.kind = GameEventKind::CombatStarted;
        ui.consume(e);
        e = {};
        e.kind = GameEventKind::CombatEnded;
        ui.consume(e);
    }
    void dungeon_exited() {
        GameEvent e{};
        e.kind = GameEventKind::DungeonExited;
        ui.consume(e);
    }
};

// ---------------------------------------------------------------------------
// R01-1 / R01-2
// ---------------------------------------------------------------------------
void r01_shop_survives_entry_and_routes_shop_input() {
    std::printf("R01 - shop survives per-input synchronization\n");
    auto w = std::make_unique<DeviceWorld>();
    w->place_shopkeeper();

    expect(w->ui.mode() == UiMode::Exploration, "R01-pre", "session starts in Exploration");

    // 't' arms the Talk direction modal; the direction key completes the Talk
    // command, which hands off to begin_shop() inside the same input.
    w->input(ch('t'));
    expect(w->ui.mode() == UiMode::TargetSelection, "R01-pre",
           "Talk arms a direction modal (modal modes are already shielded)");

    w->input(dir(Direction::East));

    // Fixture sanity: the authoritative shop session really did open.  If this
    // is red the fixture is wrong, not the production code.
    expect(w->shop.phase != ShopPhase::Closed, "R01-pre",
           "core opened a real shop session on the Talk input");

    std::printf("         observed mode after entry input: %s (base %s)\n",
                mode_name(w->ui.mode()), mode_name(w->ui.base_mode()));

    // R01-1: the desired invariant.
    expect(w->ui.mode() == UiMode::Shop, "R01-1",
           "UiMode::Shop survives the per-input mode synchronization");

    // R01-2: 'd' is the discriminator.  In handle_shop() 'd' is ShopAction::Drink
    // and dispatches a UiIntentKind::Shop intent.  In handle_exploration() 'd'
    // is not a verb at all: it falls to the default arm, appends "D-What?" and
    // dispatches NOTHING.  So the two routes differ in whether an intent is
    // produced, not merely in its contents -- no key-meaning ambiguity.
    const size_t before = w->intents.size();
    w->input(ch('d'));
    const bool routed_as_shop = w->intents.size() == before + 1 &&
                                w->intents.back().kind == UiIntentKind::Shop &&
                                w->intents.back().shop.action == ShopAction::Drink;
    std::printf("         intents raised by 'd': %u (shop-routed=%d)\n",
                unsigned(w->intents.size() - before), int(routed_as_shop));
    expect(routed_as_shop, "R01-2",
           "'d' after shop entry routes as ShopAction::Drink, not an Exploration no-op");
}

// ---------------------------------------------------------------------------
// R18-1
// ---------------------------------------------------------------------------
void r18_dialogue_exit_is_not_stale_dungeon() {
    std::printf("R18-1 - dialogue exit cannot restore a stale pre-combat Dungeon\n");
    Bare b;
    b.ui.set_base_mode(UiMode::Dungeon);
    b.combat_round_trip();
    expect(b.ui.base_mode() == UiMode::Dungeon, "R18-1-pre",
           "combat return to Dungeon is correct and must remain correct");

    b.dungeon_exited();
    expect(b.ui.base_mode() == UiMode::Exploration, "R18-1-pre",
           "leaving the dungeon returns the session to Exploration");

    DialogueOutput prompt;
    prompt.kind = DialogueOutputKind::Prompt;
    prompt.question = false;
    DialogueEvent open{DialogueEventKind::Output, &prompt};
    GameEvent e{};
    e.kind = GameEventKind::Dialogue;
    e.dialogue = &open;
    b.ui.consume(e);
    expect(b.ui.mode() == UiMode::TextEntry && b.ui.base_mode() == UiMode::Dialogue,
           "R18-1-pre", "dialogue prompt is a TextEntry modal over base Dialogue");

    DialogueEvent ended{DialogueEventKind::Ended, nullptr};
    e.dialogue = &ended;
    b.ui.consume(e);

    std::printf("         base mode after dialogue end: %s\n", mode_name(b.ui.base_mode()));
    expect(b.ui.base_mode() == UiMode::Exploration, "R18-1",
           "dialogue exit returns to Exploration, not the stale combat register");
    // Instruction B: the originating mode must be captured before the session
    // mode overwrites it.  A fix that captures base_mode_ after
    // set_base_mode(Dialogue) would satisfy nothing and must stay red here.
    expect(b.ui.base_mode() != UiMode::Dialogue, "R18-1b",
           "Dialogue is never its own return mode");
}

// ---------------------------------------------------------------------------
// R18-2
// ---------------------------------------------------------------------------
void r18_shop_exit_is_not_stale_dungeon() {
    std::printf("R18-2 - shop exit cannot restore a stale pre-combat Dungeon\n");
    Bare b;
    b.ui.set_base_mode(UiMode::Dungeon);
    b.combat_round_trip();
    expect(b.ui.base_mode() == UiMode::Dungeon, "R18-2-pre",
           "combat return to Dungeon is correct and must remain correct");
    b.dungeon_exited();
    expect(b.ui.base_mode() == UiMode::Exploration, "R18-2-pre",
           "leaving the dungeon returns the session to Exploration");

    ShopRecord record{2, ShopType::Blacksmith, 0, "The Hammer and Anvil", "Gwenneth"};
    ShopSession session;
    session.phase = ShopPhase::Menu;
    session.type = ShopType::Blacksmith;
    session.record = &record;
    ShopEvent entered{ShopEventKind::Entered, &session};
    GameEvent e{};
    e.kind = GameEventKind::Shop;
    e.shop = &entered;
    b.ui.consume(e);
    expect(b.ui.mode() == UiMode::Shop, "R18-2-pre", "shop session is live");

    session.phase = ShopPhase::Closed;
    entered.kind = ShopEventKind::Exited;
    b.ui.consume(e);

    std::printf("         base mode after shop exit: %s\n", mode_name(b.ui.base_mode()));
    expect(b.ui.base_mode() == UiMode::Exploration, "R18-2",
           "shop exit returns to Exploration, not the stale combat register");
    expect(b.ui.base_mode() != UiMode::Shop, "R18-2b", "Shop is never its own return mode");
}

// ---------------------------------------------------------------------------
// R18-3
// ---------------------------------------------------------------------------
void r18_combat_return_is_independent_of_session_return() {
    std::printf("R18-3 - combat return state is independent of session return state\n");
    Bare b;
    b.ui.set_base_mode(UiMode::Dungeon);
    b.combat_round_trip();
    expect(b.ui.base_mode() == UiMode::Dungeon, "R18-3-pre",
           "a dungeon fight returns to Dungeon");

    b.dungeon_exited();

    ShopRecord record{2, ShopType::Blacksmith, 0, "The Hammer and Anvil", "Gwenneth"};
    ShopSession session;
    session.phase = ShopPhase::Menu;
    session.type = ShopType::Blacksmith;
    session.record = &record;
    ShopEvent ev{ShopEventKind::Entered, &session};
    GameEvent e{};
    e.kind = GameEventKind::Shop;
    e.shop = &ev;
    b.ui.consume(e);
    session.phase = ShopPhase::Closed;
    ev.kind = ShopEventKind::Exited;
    b.ui.consume(e);

    // A shop round-trip on the surface must not be able to disturb where the
    // NEXT fight returns to.  Asserted purely on observable base mode: this
    // prescribes no member variable, only that the two concepts cannot share
    // one register.
    b.combat_round_trip();
    std::printf("         base mode after surface fight following a shop: %s\n",
                mode_name(b.ui.base_mode()));
    expect(b.ui.base_mode() == UiMode::Exploration, "R18-3",
           "a surface fight after a shop round-trip returns to Exploration");
}

// ---------------------------------------------------------------------------
// SHRINE-1 / SHRINE-2
//
// Smallest REAL shrine lifecycle drivable here: the donation ceremony.
// shrine.cpp re-emits ShrineDonatePrompt when a donation is unaffordable, so a
// genuine core-emitted prompt opens the modal; the response is routed exactly
// as AlphaRuntime::modal() routes UiRequestId::ShrineDonate, and the accepted
// donation is applied by the real execute_shrine().  The visit/restore chain
// needs ShrineData tables and MISCMSG records the host suite does not carry;
// the donation branch exercises the identical mode machinery (set_base_mode
// (ShrineSpecial) + modal + return_mode_) with no fixture data at all.
// ---------------------------------------------------------------------------

// Returns true when the session escaped ShrineSpecial after the ceremony.
bool run_shrine_donation(bool apply_device_rebind, DeviceWorld &w, bool report_preconditions) {
    w.apply_device_rebind = apply_device_rebind;

    ShrineInput unaffordable;
    unaffordable.action = ShrineAction::Donate;
    unaffordable.value = 9; // 900gp against 500gp on hand -> core re-prompts.
    execute_shrine(w.c, unaffordable);

    if (report_preconditions) {
        expect(w.ui.mode() == UiMode::NumericEntry, "SHRINE-pre",
               "core-emitted ShrineDonatePrompt opens a NumericEntry modal");
        expect(w.ui.base_mode() == UiMode::ShrineSpecial, "SHRINE-pre",
               "the shrine prompt sets base mode ShrineSpecial");
    }

    w.input(ch('1')); // one cycle = 100gp, affordable
    w.input(act(UiActionKind::Confirm));

    if (report_preconditions) {
        expect(w.g.gold == 400, "SHRINE-pre", "the real donation was applied by the core");
    }
    return w.ui.mode() != UiMode::ShrineSpecial;
}

void shrine_completed_flow_is_routable() {
    std::printf("SHRINE-1 - a completed shrine ceremony leaves a routable mode\n");
    auto w = std::make_unique<DeviceWorld>();
    // No device rebind: this is the post-R-01 world the repair must be safe in.
    const bool escaped = run_shrine_donation(false, *w, true);

    std::printf("         mode after the ceremony: %s\n", mode_name(w->ui.mode()));
    expect(escaped, "SHRINE-1a",
           "the session is not stranded in ShrineSpecial after the ceremony");
    expect(is_world_mode(w->ui.mode()), "SHRINE-1b",
           "the terminal mode is a world mode the routers accept");
    // The invariant is routability, not any particular dismissal gesture: the
    // next ordinary gameplay key must reach a handler.
    expect(w->ui.handle_input(ch('z')), "SHRINE-1c",
           "subsequent ordinary gameplay input has a valid route");
}

void shrine_escape_must_not_depend_on_device_rebind() {
    std::printf("SHRINE-2 - escape must not depend on the external per-input rebind\n");
    auto without = std::make_unique<DeviceWorld>();
    auto with = std::make_unique<DeviceWorld>();
    const bool escaped_without = run_shrine_donation(false, *without, false);
    const bool escaped_with = run_shrine_donation(true, *with, false);

    std::printf("         escaped without device rebind: %d   with device rebind: %d\n",
                int(escaped_without), int(escaped_with));
    // Evidence, not a requirement: today the rebind is the only exit.
    expect(escaped_with, "SHRINE-2-ev",
           "evidence: the device rebind currently clears ShrineSpecial");
    // The invariant, stated without prescribing any fix mechanism.
    expect(escaped_without == escaped_with, "SHRINE-2",
           "escaping ShrineSpecial does not depend on the external mode rebind");
}

// ---------------------------------------------------------------------------
// GUARD - combat must never strand the UI in a session mode
// ---------------------------------------------------------------------------
void guard_combat_never_strands_a_session_mode() {
    std::printf("GUARD - CombatStarted/CombatEnded never strand a closed session\n");

    {
        Bare b;
        ShopRecord record{2, ShopType::Blacksmith, 0, "The Hammer and Anvil", "Gwenneth"};
        ShopSession session;
        session.phase = ShopPhase::Menu;
        session.type = ShopType::Blacksmith;
        session.record = &record;
        ShopEvent ev{ShopEventKind::Entered, &session};
        GameEvent e{};
        e.kind = GameEventKind::Shop;
        e.shop = &ev;
        b.ui.consume(e);
        b.combat_round_trip();
        std::printf("         shop  -> combat -> end: %s\n", mode_name(b.ui.mode()));
        expect(is_world_mode(b.ui.mode()), "GUARD-shop",
               "combat around a shop does not strand the UI in Shop");
    }
    {
        Bare b;
        DialogueOutput prompt;
        prompt.kind = DialogueOutputKind::Prompt;
        prompt.question = false;
        DialogueEvent open{DialogueEventKind::Output, &prompt};
        GameEvent e{};
        e.kind = GameEventKind::Dialogue;
        e.dialogue = &open;
        b.ui.consume(e);
        b.combat_round_trip();
        std::printf("         talk  -> combat -> end: %s\n", mode_name(b.ui.mode()));
        expect(is_world_mode(b.ui.mode()), "GUARD-talk",
               "combat around a conversation does not strand the UI in Dialogue");
    }
    {
        Bare b;
        GameEvent e{};
        e.kind = GameEventKind::ShrineDonatePrompt;
        b.ui.consume(e);
        b.combat_round_trip();
        std::printf("         shrine-> combat -> end: %s\n", mode_name(b.ui.mode()));
        expect(is_world_mode(b.ui.mode()), "GUARD-shrine",
               "combat around a shrine does not strand the UI in ShrineSpecial");
    }
}

} // namespace

int main() {
    std::printf("Batch 1 mode-ownership regression suite (R-01 / R-18 / ShrineSpecial)\n\n");
    r01_shop_survives_entry_and_routes_shop_input();
    std::printf("\n");
    r18_dialogue_exit_is_not_stale_dungeon();
    std::printf("\n");
    r18_shop_exit_is_not_stale_dungeon();
    std::printf("\n");
    r18_combat_return_is_independent_of_session_return();
    std::printf("\n");
    shrine_completed_flow_is_routable();
    std::printf("\n");
    shrine_escape_must_not_depend_on_device_rebind();
    std::printf("\n");
    guard_combat_never_strands_a_session_mode();
    std::printf("\n%d checks, %d red\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
