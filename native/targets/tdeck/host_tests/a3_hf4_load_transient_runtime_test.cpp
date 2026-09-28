// Alpha 3 A3-HF4 -- a successful load starts the loaded game with no prompt,
// picker, view or presentation effect left over from the game it replaced.
//
//   a3_hf4_load_transient_runtime <openu5-alpha1-resources.bin>
//
// The H-201/H-202 capture (ALPHA3_AUDIO.md section 28.21.9) opened Mix
// (UI_MODE explore -> spell), then took the System Menu's Load: the load
// completed, and UI_MODE stayed `spell` until Mic cancelled it.
//
// Everything here goes through the REAL AlphaRuntime: raw keys in, the
// device's own Save/Load routes (Alt+S / Alt+L and System Menu -> Load /
// Save Management -> Continue Latest) over the two-slot memory card, on the
// host esp_timer shim's virtual clock.
//
//   L1  Mix (SpellSelection) open, then a load        -- the hardware defect
//   L2  a target selector (Look's "Direction?") open, then a load
//   L3  the town-exit yes/no, both halves (UiSession + awaiting_exit)
//   L3b the core's other pending questions (troll toll, Blackthorn guard)
//   L4  the Ready and Use pickers
//   L5  device-owned views and timers: (Z)-stats, gem view, zodiac view, map
//       reveal, the magic-ceremony inversion, the quake, a queued NPC approach
//   L5c a live conversation / shop session
//   L6  control: an ordinary load from Explore is unchanged
//   L7  a FAILED load leaves the live prompt exactly as it was
//   L8  the key that loaded, and its release, do nothing in the loaded game
//   L9  a load taken from inside the Developer menu drops the parked picker
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/look.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

using namespace openu5;
using tdeck::RawInputKind;

namespace tdeck {
void host_memory_save_forget_for_test();
void host_memory_save_damage_for_test();
} // namespace tdeck
void batch37_reset_screen();
const char *batch37_last_overlay();
bool batch37_last_selection_shown();

namespace {
int checks = 0, failures = 0;
bool check(bool good, const char *id, const char *label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s %s\n", good ? "GREEN" : "RED", id, label);
    return good;
}
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr uint8_t kCastle = 17;
enum class Route { Menu, AltL };
const char *route_name(Route r) { return r == Route::Menu ? "menu" : "alt-l"; }

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    size_t mark = 0;
    Run() {
        openu5_host_virtual_clock_us() = 5'000'000;
        batch37_reset_screen();
        tdeck::host_memory_save_forget_for_test();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.render_pixels = true;
        f.indexed_test_tiles = true;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.position.xy = {pack->location_x[kCastle - 1], pack->location_y[kCastle - 1]};
        g.time.hour = 12;
        g.food = 80;
        g.gold = 100;
        g.gems = 3;
        for (auto &r : g.reagent_quantities) r = 5; // a Mix from a stale picker would succeed
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.character_class = 'A';
        m.level = 1;
        m.current_hp = m.max_hp = 300;
        m.strength = m.dexterity = m.intelligence = 30;
        frames(3);
    }
    GameState &g() { return rt->game(); }
    const UiSession &ui() { return *rt->ui(); }
    UiMode mode() { return ui().mode(); }
    void frames(int n) {
        for (int i = 0; i < n; ++i) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
        }
    }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        frames(1);
    }
    void key(uint8_t code, bool alt = false, tdeck::KeyTransition t = tdeck::KeyTransition::Pressed) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = t;
        e.code = e.base_code = code;
        e.modifiers.alt = alt;
        raw(e);
    }
    void mic() {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.column = tdeck::kMicrophoneKeyColumn;
        e.row = tdeck::kMicrophoneKeyRow;
        e.transition = tdeck::KeyTransition::Pressed;
        raw(e);
        e.transition = tdeck::KeyTransition::Released;
        raw(e);
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000; // past the trackball debounce
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    void north() { ball(RawInputKind::TrackballUp); }
    void south() { ball(RawInputKind::TrackballDown); }
    void east() { ball(RawInputKind::TrackballRight); }
    void west() { ball(RawInputKind::TrackballLeft); }
    void set_mark() { mark = ui().transcript_size(); }
    bool saw(const char *needle) {
        for (size_t i = mark; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i); b && std::strstr(b->text, needle)) return true;
        return false;
    }
    bool saw_ever(const char *needle) {
        for (size_t i = 0; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i); b && std::strstr(b->text, needle)) return true;
        return false;
    }
    bool alt_save() {
        set_mark();
        key('s', true);
        return saw("Save complete");
    }
    /** System Menu -> Load / Save Management -> Continue Latest, or Alt+L. */
    bool load(Route r) {
        set_mark();
        if (r == Route::AltL) {
            key('l', true);
        } else {
            key('m', true);
            south();
            south();
            key('\r'); // Load / Save Management
            key('\r'); // Continue Latest
        }
        return saw("Load complete") && !rt->system_menu_open();
    }
    /** (L)ook north answered by the CORE -- proves no stale gate refuses commands. */
    bool look_north() {
        set_mark();
        key('l');
        north();
        return saw("Thou dost see");
    }
    void emit(const GameEvent &e) {
        auto &ctx = rt->command_context_for_test();
        ctx.events.emit(ctx.events.context, e);
    }
    /** The canonical post-load UI: the loaded game's own world mode, nothing open. */
    bool clean(UiMode world = UiMode::Exploration) {
        UiSelectionView v{};
        const auto p = rt->transient_probe_for_test();
        return mode() == world && ui().base_mode() == world && ui().request() == UiRequestId::None &&
               ui().prompt()[0] == 0 && ui().input_length() == 0 && !ui().selection_view(v) && !p.gem_view &&
               !p.zodiac_view && !p.zstats && !p.map_reveal && !p.magic_invert && !p.quake && !p.parked_pick &&
               !p.npc_initiation && batch37_last_overlay()[0] == 0 && !batch37_last_selection_shown();
    }
    void show(const char *what) {
        const auto p = rt->transient_probe_for_test();
        std::printf("         %s: mode=%d base=%d request=%d prompt='%s' overlay='%s' picker=%d probe=%d%d%d%d%d%d%d%d\n",
                    what, int(mode()), int(ui().base_mode()), int(ui().request()), ui().prompt(),
                    batch37_last_overlay(), batch37_last_selection_shown(), p.gem_view, p.zodiac_view, p.zstats,
                    p.map_reveal, p.magic_invert, p.quake, p.parked_pick, p.npc_initiation);
    }
};

/** A Run with one generation on the card, taken at the castle's overworld tile. */
struct Saved : Run {
    WorldPosition saved{};
    uint32_t saved_gold = 0;
    bool ok = false;
    Saved() {
        saved = g().position;
        saved_gold = g().gold;
        ok = alt_save();
    }
    /** Something for the load to put back. */
    void drift() { g().gold = saved_gold + 250; }
    bool restored() {
        const auto &p = g().position;
        return p.map.location == saved.map.location && p.map.floor == saved.map.floor && p.xy.x == saved.xy.x &&
               p.xy.y == saved.xy.y && g().gold == saved_gold;
    }
};

// ---- L1: the hardware defect --------------------------------------------
void test_mix(Route r) {
    const bool menu = r == Route::Menu;
    Saved h;
    check(h.ok, menu ? "L1.0m" : "L1.0a", "precondition: Alt+S wrote a generation");
    h.drift();
    h.key('m');
    const bool opened = h.mode() == UiMode::SpellSelection && h.ui().request() == UiRequestId::Custom &&
                        batch37_last_selection_shown();
    check(opened, menu ? "L1.1m" : "L1.1a", "precondition: 'm' opened the Mix picker (UI_MODE explore -> spell)");
    const bool loaded = h.load(r);
    h.show(route_name(r));
    check(loaded && h.restored(), menu ? "L1.2m" : "L1.2a", "the load completes and restores the saved game");
    check(h.clean(), menu ? "L1.3m" : "L1.3a",
          "** after the load no Mix picker remains: Explore, no request, no prompt, no picker panel **");
    const GameState before = h.g();
    h.set_mark();
    h.key('\r');
    check(h.saw("Pass") &&
              std::memcmp(before.spell_quantities, h.g().spell_quantities, sizeof before.spell_quantities) == 0 &&
              std::memcmp(before.reagent_quantities, h.g().reagent_quantities, sizeof before.reagent_quantities) == 0,
          menu ? "L1.4m" : "L1.4a", "Enter after the load is Explore's (P)ass: nothing is mixed from the old list");
    check(h.look_north(), menu ? "L1.5m" : "L1.5a", "the loaded game takes commands at once (no Mic needed)");
    h.mic();
    h.key('m');
    const bool reopened = h.mode() == UiMode::SpellSelection && h.ui().request() == UiRequestId::Custom;
    h.mic();
    check(reopened && h.mode() == UiMode::Exploration, menu ? "L1.6m" : "L1.6a",
          "control: Mix opens afresh in the loaded game and Mic still closes it");
}

// ---- L2: target selector ------------------------------------------------
void test_target(Route r) {
    const bool menu = r == Route::Menu;
    Saved h;
    h.drift();
    h.key('l');
    check(h.mode() == UiMode::TargetSelection && h.ui().request() == UiRequestId::Direction,
          menu ? "L2.1m" : "L2.1a", "precondition: (L)ook is waiting for a direction");
    const bool loaded = h.load(r);
    h.show(route_name(r));
    check(loaded && h.restored() && h.clean(), menu ? "L2.2m" : "L2.2a",
          "** the direction prompt is gone after the load **");
    h.set_mark();
    h.north();
    check(!h.saw("Thou dost see"), menu ? "L2.3m" : "L2.3a",
          "the first direction after the load is not delivered to the old game's Look");
}

// ---- L3: the town-exit question -----------------------------------------
bool raise_exit_question(Run &h) {
    h.key('e');
    h.g().position.xy = {0, 15};
    h.west();
    return h.mode() == UiMode::YesNo && h.ui().request() == UiRequestId::TownExit && h.rt->commands().awaiting_exit;
}
void test_exit_question(Route r) {
    const bool menu = r == Route::Menu;
    Saved h;
    check(raise_exit_question(h), menu ? "L3.1m" : "L3.1a",
          "precondition: the castle's west edge asks \"Leave this place?\" (UI + awaiting_exit)");
    const bool loaded = h.load(r);
    h.show(route_name(r));
    check(loaded && h.restored() && h.clean(), menu ? "L3.2m" : "L3.2a", "** the yes/no is gone after the load **");
    check(!h.rt->commands().awaiting_exit, menu ? "L3.3m" : "L3.3a",
          "** and so is its core half: awaiting_exit is clear **");
    check(h.look_north(), menu ? "L3.4m" : "L3.4a",
          "the loaded game's commands are not refused by the old question (H-118's silent-refusal class)");
}
void test_core_questions() {
    Saved h;
    auto &ctx = h.rt->command_context_for_test();
    h.rt->commands().awaiting_troll = true;
    h.rt->commands().troll_toll = 40;
    check(!h.look_north(), "L3b.1", "precondition: a pending troll toll refuses every world command");
    h.mic();
    h.rt->commands().awaiting_troll = false;
    ctx.blackthorn->tribute = true;
    ctx.blackthorn->npc_slot = 3;
    check(!h.look_north(), "L3b.2", "precondition: a pending guard tribute refuses every world command");
    h.mic();
    h.rt->commands().awaiting_troll = true;
    const bool loaded = h.load(Route::Menu);
    check(loaded && !h.rt->commands().awaiting_troll && h.rt->commands().troll_toll == 0 &&
              !ctx.blackthorn->tribute && ctx.blackthorn->npc_slot == -1,
          "L3b.3", "** the load drops the old game's troll toll and guard demand **");
    check(h.look_north(), "L3b.4", "and the loaded game takes commands");
}

// ---- L4: Ready / Use pickers --------------------------------------------
void test_pickers(Route r) {
    const bool menu = r == Route::Menu;
    {
        Saved h;
        h.drift();
        h.key('r');
        UiSelectionView v{};
        check(h.ui().selection_view(v), menu ? "L4.1m" : "L4.1a", "precondition: (R)eady opened a picker");
        const bool loaded = h.load(r);
        h.show(route_name(r));
        check(loaded && h.restored() && h.clean(), menu ? "L4.2m" : "L4.2a", "** the Ready picker is gone **");
    }
    {
        Saved h;
        h.g().potion_quantities[0] = 2;
        h.key('u');
        check(h.mode() == UiMode::InventorySelection, menu ? "L4.3m" : "L4.3a",
              "precondition: (U)se opened the inventory picker");
        const bool loaded = h.load(r);
        check(loaded && h.clean() && h.g().potion_quantities[0] == 0, menu ? "L4.4m" : "L4.4a",
              "** the Use picker is gone and the saved inventory is back **");
    }
    {
        Saved h;
        h.g().potion_quantities[0] = 2; // Use -> potion -> "on whom?" parks the item
        h.key('u');
        h.key('\r');
        const bool parked = h.rt->transient_probe_for_test().parked_pick && h.mode() == UiMode::PartySelection;
        check(parked, menu ? "L4.5m" : "L4.5a", "precondition: a potion is parked behind the member picker");
        const bool loaded = h.load(r);
        check(loaded && h.clean(), menu ? "L4.6m" : "L4.6a", "** the parked item and its member picker are gone **");
    }
}

// ---- L5: device-owned views, timers and queued work ------------------------
void test_views() {
    {
        Saved h;
        h.key('z');
        h.key('\r');
        check(h.rt->zstats_active(), "L5.1", "precondition: (Z)-stats is open");
        const bool loaded = h.load(Route::Menu);
        check(loaded && h.clean(), "L5.2", "** (Z)-stats is closed by the load **");
    }
    for (Route r : {Route::Menu, Route::AltL}) {
        const bool menu = r == Route::Menu;
        Saved h;
        h.key('v');
        check(h.rt->transient_probe_for_test().gem_view, menu ? "L5.3m" : "L5.3a", "precondition: the gem view is up");
        const bool loaded = h.load(r);
        check(loaded && h.clean() && h.g().gems == 3, menu ? "L5.4m" : "L5.4a",
              "** the gem view is closed and owes no turn in the loaded game **");
        check(h.look_north(), menu ? "L5.5m" : "L5.5a", "the first key after the load is a command, not a view close");
    }
    {
        Saved h;
        static ZodiacView sky{};
        GameEvent z{};
        z.kind = GameEventKind::Zodiac;
        z.zodiac = &sky;
        h.emit(z);
        GameEvent reveal{};
        reveal.kind = GameEventKind::MapReveal;
        reveal.note = 200;
        h.emit(reveal);
        GameEvent quake{};
        quake.kind = GameEventKind::Quake;
        h.emit(quake);
        GameEvent magic{};
        magic.kind = GameEventKind::MagicCeremony;
        h.emit(magic);
        const auto p = h.rt->transient_probe_for_test();
        check(p.zodiac_view && p.map_reveal && p.quake && p.magic_invert, "L5.6",
              "precondition: zodiac view, map reveal, quake and magic inversion are all live");
        const bool loaded = h.load(Route::AltL);
        h.show("views");
        check(loaded && h.clean(), "L5.7", "** none of them survives the load **");
        check(h.look_north(), "L5.8", "input is not swallowed by the old game's map reveal");
    }
    {
        Saved h;
        // A queued NPC approach waits for the input to unwind; the System
        // Menu's load returns before that drain runs.
        NpcActor npc{};
        npc.location = 0;
        npc.schedule.slot = 1;
        GameEvent approach{};
        approach.kind = GameEventKind::NpcInitiatesTalk;
        approach.npc = &npc;
        h.emit(approach);
        check(h.rt->transient_probe_for_test().npc_initiation, "L5.9", "precondition: an NPC approach is queued");
        h.key('m', true);
        h.south();
        h.south();
        h.key('\r');
        h.set_mark();
        h.key('\r');
        check(h.saw("Load complete") && h.clean(), "L5.10",
              "** the old game's queued approach is dropped by the menu load **");
    }
}

// ---- L5c: a conversation or a shop -----------------------------------------
// The host fixture binds no TLK data (dialogue_assets_.bind(nullptr, 0)), so
// every Talk answers "Funny, no response!". The session is seeded instead
// through the same event sink the core's dialogue/shop orchestration emits
// into, with the core session marked live the way BeginConversation leaves it.
bool start_talk(Run &h) {
    auto &session = h.rt->command_context_for_test().dialogue_services->session;
    session.active = true;
    session.game = &h.g();
    static DialogueOutput prompt{};
    prompt.kind = DialogueOutputKind::Prompt;
    static DialogueEvent de{};
    de.kind = DialogueEventKind::Output;
    de.output = &prompt;
    GameEvent e{};
    e.kind = GameEventKind::Dialogue;
    e.dialogue = &de;
    h.emit(e);
    h.frames(1);
    return h.ui().base_mode() == UiMode::Dialogue && h.mode() == UiMode::TextEntry &&
           h.ui().request() == UiRequestId::Dialogue;
}
bool start_shop(Run &h) {
    auto &session = h.rt->command_context_for_test().shop_services->session;
    session.owner = &h.g();
    session.type = ShopType::Blacksmith;
    session.phase = ShopPhase::Menu;
    static ShopEvent se{};
    se.kind = ShopEventKind::Entered;
    se.session = &session;
    GameEvent e{};
    e.kind = GameEventKind::Shop;
    e.shop = &se;
    h.emit(e);
    h.frames(1);
    return h.mode() == UiMode::Shop && h.ui().shop_phase() == ShopPhase::Menu;
}
void test_sessions() {
    {
        Saved h;
        const bool talking = start_talk(h);
        check(talking && h.rt->command_context_for_test().dialogue_services->session.active, "L5c.1",
              "precondition: a conversation is open (base mode Dialogue, session active)");
        const bool loaded = h.load(Route::Menu);
        h.show("dialogue");
        check(loaded && h.restored() && h.clean() && !h.rt->command_context_for_test().dialogue_services->session.active,
              "L5c.2", "** the conversation and its session end with the load **");
        check(h.look_north(), "L5c.3", "Explore owns input in the loaded game");
    }
    {
        Saved h;
        const bool shopping = start_shop(h);
        check(shopping, "L5c.4", "precondition: a shop is open (base mode Shop)");
        const bool loaded = h.load(Route::AltL);
        h.show("shop");
        check(loaded && h.restored() && h.clean() && h.ui().shop_phase() == ShopPhase::Closed &&
                  h.rt->command_context_for_test().shop_services->session.phase == ShopPhase::Closed,
              "L5c.5", "** the shop closes with the load (UI and session) **");
    }
}

// ---- L6: the ordinary load --------------------------------------------------
void test_ordinary(Route r) {
    const bool menu = r == Route::Menu;
    Saved h;
    h.drift();
    h.north();
    const size_t before = h.ui().transcript_size();
    const bool loaded = h.load(r);
    check(loaded && h.restored() && h.clean(), menu ? "L6.1m" : "L6.1a",
          "control: a load from Explore restores position and gold into Explore");
    check(h.ui().transcript_size() > before && h.saw_ever("Save complete"), menu ? "L6.2m" : "L6.2a",
          "the transcript is kept (the old lines and the new 'Load complete')");
    check(h.look_north(), menu ? "L6.3m" : "L6.3a", "commands run in the loaded game");
}

// ---- L7: a failed load ------------------------------------------------------
void test_failed(Route r) {
    const bool menu = r == Route::Menu;
    {
        Run h; // nothing on the card
        h.key('m');
        h.set_mark();
        if (menu) {
            h.key('m', true);
            h.south();
            h.south();
            h.key('\r');
            h.key('\r');
            check(h.saw("No valid save") && h.rt->system_menu_open(), menu ? "L7.1m" : "L7.1a",
                  "precondition: the menu's Continue Latest finds no save and stays open");
            h.key('\b'); // Load page -> root
            h.key('\b'); // root -> Resume
        } else {
            h.key('l', true);
            check(h.saw("No valid save"), "L7.1a", "precondition: Alt+L finds no save");
        }
        check(!h.rt->system_menu_open() && h.mode() == UiMode::SpellSelection &&
                  h.ui().request() == UiRequestId::Custom && batch37_last_selection_shown(),
              menu ? "L7.2m" : "L7.2a", "** a failed load leaves the Mix picker exactly as it was **");
        h.mic();
        check(h.mode() == UiMode::Exploration, menu ? "L7.3m" : "L7.3a", "and Mic closes it as before");
    }
    {
        Saved h;
        tdeck::host_memory_save_damage_for_test(); // the only generation, torn
        check(raise_exit_question(h), menu ? "L7.4m" : "L7.4a", "precondition: \"Leave this place?\" is pending");
        const bool loaded = h.load(r);
        if (menu) {
            h.key('\b');
            h.key('\b');
        }
        check(!loaded && h.mode() == UiMode::YesNo && h.ui().request() == UiRequestId::TownExit &&
                  h.rt->commands().awaiting_exit && h.g().position.map.location == kCastle,
              menu ? "L7.5m" : "L7.5a", "** a refused (corrupt) load keeps both halves of the question and the world **");
        h.key('n');
        check(h.mode() == UiMode::Exploration && !h.rt->commands().awaiting_exit, menu ? "L7.6m" : "L7.6a",
              "and the question still answers");
    }
}

// ---- L8: the loading key and its release -----------------------------------
void test_edges() {
    Saved h;
    h.key('m');
    h.load(Route::Menu);
    const auto cmds = h.rt->routed_command_count();
    const auto pos = h.g().position;
    h.set_mark();
    h.key('\r', false, tdeck::KeyTransition::Released); // the Enter that loaded
    h.key('l', true, tdeck::KeyTransition::Released);   // an Alt+L release
    tdeck::RawInputEvent orphan{};                       // a Mic release with no press
    orphan.kind = RawInputKind::Keyboard;
    orphan.column = tdeck::kMicrophoneKeyColumn;
    orphan.row = tdeck::kMicrophoneKeyRow;
    orphan.transition = tdeck::KeyTransition::Released;
    h.raw(orphan);
    check(h.rt->routed_command_count() == cmds && std::memcmp(&pos, &h.g().position, sizeof pos) == 0 && h.clean() &&
              h.ui().transcript_size() == h.mark,
          "L8.1", "releases after the load route nothing and open nothing");
    h.key('l', true);
    const auto cmds2 = h.rt->routed_command_count();
    h.key('l', true, tdeck::KeyTransition::Released);
    check(h.clean() && h.rt->routed_command_count() == cmds2, "L8.2", "Alt+L's own release does nothing either");
}

// ---- L9: a load from inside the Developer menu -------------------------------
void test_developer() {
    Saved h;
    h.key('m');
    h.key('d', true);
    check(h.mode() == UiMode::DebugMenu, "L9.1", "precondition: the Developer menu is open over the Mix picker");
    h.set_mark();
    h.key('l', true);
    check(h.saw("Load complete") && h.mode() == UiMode::DebugMenu, "L9.2",
          "Alt+L inside the Developer menu loads and leaves the menu open");
    h.key('d', true); // Alt+D closes it
    for (int k = 0; k < 4 && h.mode() == UiMode::DebugMenu; ++k) h.mic();
    h.show("developer");
    check(h.clean(), "L9.3", "** closing it returns to Explore, not to the old game's Mix picker **");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;

    for (Route r : {Route::Menu, Route::AltL}) test_mix(r);
    for (Route r : {Route::Menu, Route::AltL}) test_target(r);
    for (Route r : {Route::Menu, Route::AltL}) test_exit_question(r);
    test_core_questions();
    for (Route r : {Route::Menu, Route::AltL}) test_pickers(r);
    test_views();
    test_sessions();
    for (Route r : {Route::Menu, Route::AltL}) test_ordinary(r);
    for (Route r : {Route::Menu, Route::AltL}) test_failed(r);
    test_edges();
    test_developer();

    std::printf("\nA3-HF4 load transient state: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
