// Alpha 3 A3-HF5 -- the TLK script's own pauses reach the screen.
//
//   a3_hf5_dialogue_pacing_runtime <openu5-alpha1-resources.bin> [--probe]
//
// The player-visible defect: Chuckles' "entertain" routine and Blackthorn's
// throne-room speech arrived in the transcript all at once. Both are plain TLK
// conversations; their scripts carry the interpreter's two pause opcodes
// (TALK.OVL, derivation in openu5/dialogue_pacer.h and ALPHA3_AUDIO.md 30):
//
//   0x83 Pause    TALK 0x0f92-0x0fb3: up to 28 x (compositor 0x5910 + key poll
//                 0x1d5e + delay(1)); a key ends it early and is consumed, and
//                 both exits flush the BIOS keyboard buffer (0x1b24).
//   0x8F KeyWait  TALK 0x1010: getkey_with_redraw 0x266c, any key, discarded.
//
// The core marked both on every DialogueOutput (DialoguePause::Timed / Key)
// since the dialogue port; nothing downstream ever read the mark.
//
// Everything below goes through the REAL AlphaRuntime -- raw keys in, the
// shipped pack's TLK corpus, NPC tables and castles -- drawing on the REAL
// tdeck_board.cpp over the fake ST7789 (A3-04E/F's harness), on the host
// esp_timer shim's virtual clock.
//
//   N1  Chuckles "entertain": four Pauses, 1540 ms apart, in order
//   N1C a key cuts a Pause at once, is consumed, and restarts the next one
//   N1K Chuckles "welcome": four KeyWaits hold until a key, each key one section
//   N2  Blackthorn: description Pause, then "I beg to differ!" / fate, paced;
//       the UI leaves the conversation only when the last line is out
//   N3  a successful load mid-speech drops the rest of it
//   N4  a failed load mid-speech leaves it running
//   N5  the System Menu mid-speech: nothing is released behind it
//   N6  ordinary text stays immediate (unpaused answers, "Funny, no response!")
//   N7  the unpaced harness contract: a zero cadence drains synchronously
//   N9  an NPC approach queued by the same turn waits for the last line
//   N8  the A3-04F transcript row cache: every release draws exactly the rows it
//       changes, and nothing between releases
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/hud.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;
namespace bus = openu5_host_bus;

namespace tdeck {
void host_memory_save_forget_for_test();
} // namespace tdeck

namespace {
int checks = 0, failures = 0;
bool check(bool good, const char *id, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s %s\n", good ? "GREEN" : "RED", id, label.c_str());
    return good;
}
std::string n(long long v) { return std::to_string(v); }
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr uint8_t kLbCastle = 17, kBlackthornCastle = 18;
constexpr uint8_t kChucklesDialog = 9, kBlackthornDialog = 10;
// TALK 0x0fae `cmp si,0x1c`: 28 passes of delay(1), one INT 1Ch tick each,
// in the port's 55 ms tick (scene_timing.h kSceneTickMs).
constexpr int64_t kPauseMs = 28 * 55;
constexpr int64_t kFrameMs = 5;

const char *region_of(const bus::WindowWrite &w) {
    if (w.width() == 320 && w.height() == 240) return "full-clear";
    if (w.x0 == kHudPartyFrameX && w.width() == 320 - kHudPartyFrameX && w.height() >= 200) return "panel-clear";
    if (w.width() <= 2 || w.height() <= 2) return "lines";
    if (w.x1 < kHudPartyFrameX) {
        if (w.y1 < kHudSkyBarY + kHudSkyBarH) return "sky";
        if (w.y0 >= kHudWindBarY) return "wind";
        return "viewport";
    }
    if (w.y1 < kHudWorldFrameY) return "party";
    if (w.y1 < kHudTranscriptSeparatorY) return "status";
    return "transcript";
}

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    size_t mark = 0;
    explicit Run(bool paced = true) {
        openu5_host_virtual_clock_us() = 5'000'000;
        bus::install();
        bus::model() = bus::Model{};
        board.initialize_display();
        idle.attach(bus::idle_passes());
        board.set_idle_service(&idle);
        rt->attach_idle_service(&idle);
        tdeck::host_memory_save_forget_for_test();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.render_pixels = true;
        f.patterned_test_tiles = true;
        f.paced_scenes = paced;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.time.hour = 12;
        g.time.minute = 0;
        g.food = 900;
        g.gold = 100;
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
    CommandContext &ctx() { return rt->command_context_for_test(); }
    bool session_active() { return ctx().dialogue_services->session.active; }
    static int64_t now_ms() { return openu5_host_virtual_clock_us() / 1000; }
    struct Row { int64_t at; std::string text; };
    std::vector<Row> log; // every transcript row, stamped with the frame that first showed it
    size_t logged = 0;
    void frames(int k) {
        for (int i = 0; i < k; ++i) {
            openu5_host_virtual_clock_us() += kFrameMs * 1000;
            // Stamp at the frame's START: the runtime decides a release there,
            // and the fake bus then advances the clock by the frame's modelled
            // SPI time, which differs from frame to frame.
            const int64_t start = now_ms();
            rt->render(board);
            while (logged < ui().transcript_size()) {
                const auto *b = ui().transcript_at(logged++);
                log.push_back({start, b ? b->text : ""});
            }
        }
    }
    void run_ms(int64_t ms) { frames(int(ms / kFrameMs)); }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        frames(1);
    }
    // A press only: every key here lands on the frame it is sent in, so the
    // virtual time between two presses is exactly what the test says it is.
    void key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = e.base_code = code;
        e.modifiers.alt = alt;
        raw(e);
    }
    void type(const char *s) { for (; *s; ++s) key(uint8_t(*s)); }
    // The Mic key: Cancel on the handheld (ESC's seat), press and release.
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
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    void step(Direction d) {
        ball(d == Direction::North ? RawInputKind::TrackballUp : d == Direction::South ? RawInputKind::TrackballDown
             : d == Direction::East ? RawInputKind::TrackballRight : RawInputKind::TrackballLeft);
    }
    void alt_save() { key('s', true); }
    void alt_load() { key('l', true); }
    void menu_toggle() { key('m', true); }

    void set_mark() { mark = ui().transcript_size(); }
    std::vector<std::string> since_mark() {
        std::vector<std::string> out;
        for (size_t i = mark; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i)) out.push_back(b->text);
        return out;
    }
    const NpcActor *actor_with_dialog(uint8_t dialog) {
        for (size_t i = 0; i < rt->actors().count; ++i) {
            const auto &a = rt->actors().actors[i];
            if (a.location == g().position.map.location && a.schedule.dialog == dialog) return &a;
        }
        return nullptr;
    }
    bool enter(uint8_t location) {
        g().position.map = {0, 0};
        g().position.xy = {pack->location_x[location - 1], pack->location_y[location - 1]};
        key('e');
        frames(2);
        return g().position.map.location == location;
    }
    // Stand beside the NPC with `dialog` on its own floor and (T)alk toward it.
    bool talk_to(uint8_t dialog) {
        const auto *a = actor_with_dialog(dialog);
        if (!a) return false;
        g().position.map.floor = a->z;
        constexpr Direction dirs[] = {Direction::South, Direction::North, Direction::East, Direction::West};
        for (auto d : dirs) {
            const auto dd = direction_delta(d);
            const int x = a->x + dd.dx, y = a->y + dd.dy;
            const int t = ctx().terrain->effective(ctx().world, g().position.map, x, y);
            if (!is_passable(t, TransportMode::Foot).value) continue;
            bool occupied = false;
            for (size_t i = 0; i < rt->actors().count; ++i) {
                const auto &o = rt->actors().actors[i];
                if (o.location == g().position.map.location && o.z == a->z && o.x == x && o.y == y) occupied = true;
            }
            if (occupied) continue;
            g().position.xy = {uint8_t(x), uint8_t(y)};
            const Direction toward = d == Direction::South ? Direction::North : d == Direction::North ? Direction::South
                                   : d == Direction::East ? Direction::West : Direction::East;
            key('t');
            step(toward);
            return session_active();
        }
        return false;
    }
    // Chuckles, "Your interest?" armed.
    bool at_chuckles() { return enter(kLbCastle) && talk_to(kChucklesDialog) && mode() == UiMode::TextEntry; }
    // Blackthorn with the Avatar's name known (his label 0), past the greeting.
    bool at_blackthorn() {
        if (!enter(kBlackthornCastle)) return false;
        const auto *b = actor_with_dialog(kBlackthornDialog);
        if (!b) return false;
        g().npc_met[kBlackthornCastle - 1] |= uint32_t(1) << b->schedule.slot;
        return talk_to(kBlackthornDialog);
    }
    // Submit a keyword; the mark is set just before the Enter.
    int64_t said_at = 0; // the virtual ms the Enter was handled at
    void say(const char *word) {
        type(word);
        set_mark();
        said_at = now_ms();
        key('\r');
    }

    // After `ms` more of device loop: the rows appended since the mark, each
    // stamped (relative to t0) with the frame that first showed it.
    std::vector<Row> timeline(int64_t t0, int64_t ms) {
        frames(int(ms / kFrameMs));
        std::vector<Row> rows;
        for (size_t i = mark; i < log.size(); ++i) rows.push_back({log[i].at - t0, log[i].text});
        return rows;
    }
    std::vector<std::pair<std::string, int>> visible() const {
        UiRenderedLine lines[kHudTranscriptLines]{};
        const size_t count = rt->ui()->visible_lines(lines, kHudTranscriptLines, size_t(kHudTranscriptColumns));
        std::vector<std::pair<std::string, int>> out;
        for (size_t i = 0; i < count; ++i) out.emplace_back(lines[i].text, int(lines[i].channel));
        return out;
    }
};

void dump(const char *tag, const std::vector<Run::Row> &rows) {
    std::printf("    %s (%zu rows)\n", tag, rows.size());
    for (const auto &r : rows) std::printf("      t=%6lld ms | %s\n", (long long)r.at, r.text.c_str());
}
std::vector<std::string> texts(const std::vector<Run::Row> &rows) {
    std::vector<std::string> out;
    for (const auto &r : rows) out.push_back(r.text);
    return out;
}
int64_t first_at(const std::vector<Run::Row> &rows, const char *needle, size_t nth = 0) {
    for (const auto &r : rows)
        if (r.text.find(needle) != std::string::npos && nth-- == 0) return r.at;
    return -1;
}
size_t rows_before(const std::vector<Run::Row> &rows, int64_t at) {
    size_t k = 0;
    for (const auto &r : rows) k += r.at < at;
    return k;
}
// A release lands on the first RENDERED frame at or after its time; the
// render pacer can hold a frame back, so allow one BIOS tick -- the
// original's own granularity. A row is stamped with the frame that shows
// it, one frame after the key that started its pause.
bool within(int64_t v, int64_t want) { return v >= want - kFrameMs && v <= want + 55; }

// The whole of Chuckles' label 0 ("ente"), exactly as the corpus prints it.
const std::vector<std::string> kEntertain = {
    "Ho eyo he hum! ", "", "Ho eyo he hum! ", "", "Ho eyo he hum! ", "", "Bounce, bounce, bounce, bounce! ", "",
    "Didst thou enjoy that?"};

// ===========================================================================
void test_entertain() {
    std::printf("\nN1 Chuckles \"entertain\" (0x83 Pause x4)\n");
    Run r;
    if (!check(r.at_chuckles(), "N1.0", "(T)alk reaches Chuckles in Lord British's castle and arms \"Your interest?\"")) return;
    const auto routed = r.rt->routed_command_count();
    r.say("ente");
    const int64_t t0 = r.said_at;
    const auto rows = r.timeline(t0, 8000);
    dump("timeline", rows);
    const auto second = first_at(rows, "Ho eyo", 1), third = first_at(rows, "Ho eyo", 2);
    const auto bounce = first_at(rows, "Bounce"), last = first_at(rows, "Didst thou");
    check(rows_before(rows, kPauseMs) == 2, "N1.1",
          "before the first Pause expires only the first section is shown (" + n(long(rows_before(rows, kPauseMs))) +
              " rows, want 2: the verse and the Pause's blank row)");
    check(within(second, kPauseMs), "N1.2", "the second verse lands one Pause (28 x 55 ms) later: t=" + n(second));
    check(within(third, 2 * kPauseMs) && within(bounce, 3 * kPauseMs) && within(last, 4 * kPauseMs), "N1.3",
          "each later section one more Pause on: " + n(third) + " / " + n(bounce) + " / " + n(last));
    check(texts(rows) == kEntertain, "N1.4", "all nine rows, in the corpus order, none lost or doubled");
    check(r.mode() == UiMode::TextEntry && r.session_active(), "N1.5",
          "the question prompt is armed once the routine is out, the conversation still live");
    check(r.rt->routed_command_count() == routed + 1, "N1.6", "one routed command for the whole routine");
    r.set_mark();
    r.say("y");
    const auto yes = r.timeline(Run::now_ms(), 20);
    check(!yes.empty() && yes.front().text.find("I thought thou might!") != std::string::npos && yes.front().at <= kFrameMs,
          "N1.7", "the unpaused answer that follows is immediate");
}

void test_cut_by_key() {
    std::printf("\nN1C a key during a Pause\n");
    Run r;
    if (!check(r.at_chuckles(), "N1C.0", "Chuckles")) return;
    r.say("ente");
    const int64_t t0 = r.said_at;
    r.run_ms(300);
    const auto routed = r.rt->routed_command_count();
    const auto before = r.since_mark().size();
    const int64_t cut = Run::now_ms() - t0; // the key is handled at this instant
    r.key('x');
    const auto after = r.since_mark().size();
    check(before == 2 && after == 4, "N1C.1",
          "the key at t=" + n(cut) + " ends the Pause at once (rows " + n(long(before)) + " -> " + n(long(after)) + ")");
    check(r.rt->routed_command_count() == routed && r.ui().input_length() == 0, "N1C.2",
          "the key is consumed: no command routed, nothing typed into the prompt (routed +" +
              n(long(r.rt->routed_command_count() - routed)) + ", input " + n(long(r.ui().input_length())) + ")");
    const auto rows = r.timeline(t0, 7000);
    const auto third = first_at(rows, "Ho eyo", 2);
    check(within(third, cut + kPauseMs), "N1C.3", "the next Pause runs a full 28 ticks from the key: t=" + n(third));
    check(texts(rows) == kEntertain, "N1C.4", "the routine completes in order");

    // ESC (Mic) is just another key to the Pause loop: it ends the pause, it
    // does not end the conversation (in the Dialogue base mode Cancel would).
    Run m;
    if (!check(m.at_chuckles(), "N1C.5", "Chuckles again")) return;
    m.say("ente");
    const int64_t m0 = m.said_at;
    m.run_ms(300);
    const auto routed_m = m.rt->routed_command_count();
    m.mic();
    const auto after_mic = m.since_mark().size();
    const auto rest = m.timeline(m0, 6000);
    check(after_mic == 4 && m.rt->routed_command_count() == routed_m && m.session_active() &&
              texts(rest) == kEntertain && m.mode() == UiMode::TextEntry,
          "N1C.6", "Mic during a Pause ends the pause only: no EndConversation, the routine and its question follow "
                   "(rows " + n(long(after_mic)) + ", routed +" + n(long(m.rt->routed_command_count() - routed_m)) + ")");
}

void test_welcome() {
    std::printf("\nN1K Chuckles \"welcome\" (0x8F KeyWait x4)\n");
    Run r;
    if (!check(r.at_chuckles(), "N1K.0", "Chuckles")) return;
    r.say("welc");
    const auto rows = r.timeline(Run::now_ms(), 10000);
    dump("after 10 s with no key", rows);
    check(rows.size() == 2 && rows[0].text.find("Welcome, welcome, welcome.") != std::string::npos, "N1K.1",
          "a KeyWait holds: after 10 s only the first section (" + n(long(rows.size())) + " rows)");
    check(std::string(r.rt->status_overlay()) == "Enter: continue", "N1K.2",
          "the handheld shows the same key-wait cue the Blackthorn scene uses (\"" + std::string(r.rt->status_overlay()) + "\")");
    const auto routed = r.rt->routed_command_count();
    r.key(' ');
    const auto one = r.since_mark();
    check(one.size() == 4 && one[2].find("Welcome to the castle") != std::string::npos, "N1K.3",
          "one key releases exactly one section (" + n(long(one.size())) + " rows)");
    check(r.rt->routed_command_count() == routed && r.ui().input_length() == 0, "N1K.4",
          "the key is consumed (no command, nothing typed)");
    const auto x0 = r.g().position.xy.x;
    r.key('\r');
    r.ball(RawInputKind::TrackballLeft);
    r.key('q');
    r.frames(2);
    const auto all = r.since_mark();
    check(all.size() == 9 && all.back().find("That's ME!") != std::string::npos && r.mode() == UiMode::TextEntry,
          "N1K.5", "four keys of any kind release the whole speech and re-arm \"Your interest?\"");
    check(r.rt->routed_command_count() == routed && r.g().position.xy.x == x0, "N1K.6",
          "the trackball press advanced the speech and moved nothing");
}

void test_blackthorn() {
    std::printf("\nN2 Blackthorn's throne-room speech\n");
    Run r;
    const bool in = r.enter(kBlackthornCastle);
    const auto *b = r.actor_with_dialog(kBlackthornDialog);
    if (!check(in && b, "N2.0", "Blackthorn is in his castle at noon")) return;
    r.g().npc_met[kBlackthornCastle - 1] |= uint32_t(1) << b->schedule.slot;
    r.set_mark();
    const int64_t t0 = Run::now_ms();
    r.talk_to(kBlackthornDialog);
    const auto greet = r.timeline(t0, 3000);
    // (the mark counts the Talk-/north echo rows too; only the two lines are timed)
    dump("greeting", greet);
    const auto desc = first_at(greet, "Dark Lord"), hello = first_at(greet, "Greetings");
    check(desc >= 0 && hello >= 0 && within(hello - desc, kPauseMs), "N2.1",
          "the description's Pause holds \"Greetings\" for one Pause (" + n(desc) + " -> " + n(hello) + ")");
    r.say("no");
    const int64_t t1 = r.said_at;
    const bool core_done = !r.session_active();
    const UiMode during = r.mode();
    const auto rows = r.timeline(t1, 5000);
    dump("\"no\"", rows);
    const auto differ = first_at(rows, "I beg to differ"), kind = first_at(rows, "So very kind"),
               fate = first_at(rows, "Prepare now");
    check(differ >= 0 && differ <= kFrameMs && within(kind, kPauseMs) && within(fate, 2 * kPauseMs), "N2.2",
          "\"I beg to differ!\" at once, then one Pause per section: " + n(differ) + " / " + n(kind) + " / " + n(fate));
    check(core_done && during != UiMode::Exploration && r.mode() == UiMode::Exploration, "N2.3",
          "the core ends the conversation (and calls the guards) at once; the screen returns to the map only "
          "after the last line");
}

// N9. An NPC's approach queued by the same turn (TOWN's turn tail emits
// NpcInitiatesTalk while the command is still on the stack; the runtime
// drains it once the input unwinds) must wait for the speech: in 1988 TALK
// runs its pauses before the main loop runs anyone's turn. The drain refuses
// anything but the map, so a drain that ran under the paused speech would
// drop the approach for good.
void test_initiation() {
    std::printf("\nN9 an NPC approach queued behind the speech\n");
    Run r;
    if (!check(r.at_blackthorn(), "N9.0", "Blackthorn, label 0")) return;
    r.run_ms(4000); // past the greeting's Pause
    r.type("no");
    const auto *b = r.actor_with_dialog(kBlackthornDialog);
    GameEvent approach{};
    approach.kind = GameEventKind::NpcInitiatesTalk;
    approach.npc = b;
    r.ctx().events.emit(r.ctx().events.context, approach);
    r.set_mark();
    r.said_at = Run::now_ms();
    r.key('\r');
    const bool pending = r.rt->transient_probe_for_test().npc_initiation;
    const bool quiet = !r.session_active();
    const auto rows = r.timeline(r.said_at, 6000);
    const auto fate = first_at(rows, "Prepare now"), again = first_at(rows, "Dark Lord");
    size_t fate_row = rows.size(), again_row = rows.size();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (fate_row == rows.size() && rows[i].text.find("Prepare now") != std::string::npos) fate_row = i;
        if (again_row == rows.size() && rows[i].text.find("Dark Lord") != std::string::npos) again_row = i;
    }
    check(pending && quiet, "N9.1", "while the speech is paused the approach is still pending, no conversation opened");
    check(fate >= 2 * kPauseMs && again >= fate && again_row > fate_row && r.session_active() && !r.rt->transient_probe_for_test().npc_initiation, "N9.2",
          "once the last line is out the approach runs (his description at t=" + n(again) + ", after the fate line at " +
              n(fate) + ")");
}

void test_load(bool succeed) {
    const char *id = succeed ? "N3" : "N4";
    std::printf("\n%s a %s load in the middle of the routine\n", id, succeed ? "successful" : "failed");
    Run r;
    if (succeed) {
        r.alt_save();
        r.frames(2);
    }
    if (!check(r.at_chuckles(), succeed ? "N3.0" : "N4.0", "Chuckles")) return;
    r.say("ente");
    const int64_t t0 = r.said_at;
    r.run_ms(500);
    r.alt_load();
    const bool loaded = !r.session_active();
    const auto rows = r.timeline(t0, 8000);
    const auto verses = [&] {
        size_t k = 0;
        for (const auto &x : rows) k += x.text.find("Ho eyo") != std::string::npos || x.text.find("Bounce") != std::string::npos;
        return k;
    }();
    if (succeed) {
        check(loaded && verses == 1, "N3.1",
              "the rest of the routine never reaches the loaded game (" + n(long(verses)) + " verses shown)");
        check(r.mode() == UiMode::Exploration, "N3.2", "the loaded game is back on the map");
        const auto routed = r.rt->routed_command_count();
        const auto x = r.g().position.xy.x;
        r.step(Direction::East);
        r.step(Direction::West);
        check(r.rt->routed_command_count() == routed + 2, "N3.3",
              "the next keys are ordinary commands again (routed " + n(long(r.rt->routed_command_count() - routed)) +
                  ", x " + n(x) + ")");
    } else {
        auto spoken = texts(rows);
        bool refused = false;
        for (auto it = spoken.begin(); it != spoken.end();)
            if (*it == "No valid save") { refused = true; it = spoken.erase(it); } else ++it;
        if (spoken != kEntertain) dump("failed-load rows", rows);
        check(!loaded && refused && spoken == kEntertain, "N4.1",
              "a load that fails leaves the routine running to its end, in order");
        check(within(first_at(rows, "Ho eyo", 1), kPauseMs) && within(first_at(rows, "Didst"), 4 * kPauseMs), "N4.2",
              "on its own cadence");
    }
}

void test_menu() {
    std::printf("\nN5 the System Menu in the middle of the routine\n");
    Run r;
    if (!check(r.at_chuckles(), "N5.0", "Chuckles")) return;
    r.say("ente");
    const int64_t t0 = r.said_at;
    r.run_ms(500);
    r.menu_toggle();
    const bool open = r.rt->system_menu_open();
    const auto shown = r.since_mark().size();
    r.run_ms(3000);
    const auto behind = r.since_mark().size();
    check(open && shown == 2 && behind == 2, "N5.1",
          "nothing is released behind the menu (" + n(long(shown)) + " -> " + n(long(behind)) + " rows over 3 s)");
    r.menu_toggle();
    const bool closed = !r.rt->system_menu_open();
    const auto rows = r.timeline(t0, 8000);
    check(closed && texts(rows) == kEntertain, "N5.2", "after the menu closes the routine completes, in order");
}

void test_immediate() {
    std::printf("\nN6 ordinary text stays immediate\n");
    Run r;
    if (!check(r.at_chuckles(), "N6.0", "Chuckles")) return;
    r.say("cast");
    const auto rows = r.timeline(Run::now_ms(), 20);
    check(!rows.empty() && rows.front().text.find("This one.") != std::string::npos && rows.front().at <= kFrameMs &&
              r.mode() == UiMode::TextEntry,
          "N6.1", "an answer with no pause opcode arrives in the same frame, prompt re-armed");
    r.set_mark();
    r.key('\r'); // Enter on an empty keyword = bye
    const auto bye = r.timeline(Run::now_ms(), 20);
    check(!bye.empty() && bye.front().at <= kFrameMs && r.mode() == UiMode::Exploration, "N6.2",
          "the goodbye is immediate and returns to the map");
    r.set_mark();
    r.g().position.xy = {1, 1};
    r.key('t');
    r.step(Direction::North);
    const auto none = r.since_mark();
    bool funny = false;
    for (const auto &s : none) funny |= s.find("Funny, no response!") != std::string::npos;
    check(funny && r.mode() == UiMode::Exploration, "N6.3", "\"Funny, no response!\" is immediate");
}

void test_unpaced() {
    std::printf("\nN7 the unpaced harness contract\n");
    Run r(false);
    if (!check(r.at_chuckles(), "N7.0", "Chuckles")) return;
    r.say("ente");
    const auto rows = r.timeline(Run::now_ms(), 10);
    check(texts(rows) == kEntertain && rows.back().at <= kFrameMs, "N7.1",
          "with a zero cadence the routine drains in the same frame (the reference's automation rule)");
}

void test_render() {
    std::printf("\nN8 the A3-04F transcript row cache\n");
    Run r;
    if (!check(r.at_chuckles(), "N8.0", "Chuckles")) return;
    // Fill the transcript first, so every release scrolls it.
    for (int i = 0; i < 12; ++i) r.say("cast");
    r.say("ente");
    const int64_t t0 = r.said_at;
    auto before = r.visible();
    int releases = 0, exact = 0, idle_frames = 0, idle_rows = 0;
    size_t last_changed = 0, last_drawn = 0;
    std::string detail;
    for (int64_t t = 0; t < 7000; t += kFrameMs) {
        bus::reset_stats();
        r.frames(1);
        uint32_t drawn = 0;
        for (const auto &w : bus::windows()) drawn += !std::strcmp(region_of(w), "transcript");
        const auto after = r.visible();
        size_t changed = 0;
        for (size_t i = 0; i < size_t(kHudTranscriptLines); ++i) {
            const auto a = i < before.size() ? before[i] : std::pair<std::string, int>{"", 0};
            const auto b = i < after.size() ? after[i] : std::pair<std::string, int>{"", 0};
            changed += a != b;
        }
        if (changed) {
            ++releases;
            exact += drawn == changed;
            last_changed = changed;
            last_drawn = drawn;
            detail += " " + n(Run::now_ms() - t0) + "ms:" + n(long(changed)) + "/" + n(drawn);
        } else {
            ++idle_frames;
            idle_rows += int(drawn);
        }
        before = after;
    }
    std::printf("    releases (t: changed/drawn):%s\n", detail.c_str());
    // The last release also brings back the question prompt, whose context
    // bar sits in the same screen region: it may draw more, never fewer.
    check(releases == 4 && exact >= releases - 1, "N8.1",
          "each Pause release that only adds text redraws exactly the rows it changed (" + n(exact) + " of " +
              n(releases) + " releases exact)");
    check(releases == 4 && last_drawn >= last_changed, "N8.1b",
          "the release that re-arms the prompt skips no changed row (" + n(long(last_changed)) + " changed, " +
              n(long(last_drawn)) + " drawn)");
    check(idle_rows == 0, "N8.2", "the " + n(idle_frames) + " frames between releases draw no transcript row");
    const auto v = r.visible();
    // The page as wrapped at kHudTranscriptColumns: the Bounce line takes two rows.
    const std::vector<std::string> page = {"Ho eyo he hum!", "", "Ho eyo he hum!", "", "Ho eyo he hum!", "",
                                           "Bounce, bounce,", "bounce, bounce!", "", "Didst thou enjoy that?"};
    std::vector<std::string> tail;
    for (size_t i = v.size() >= page.size() ? v.size() - page.size() : 0; i < v.size(); ++i) {
        std::string t = v[i].first;
        while (!t.empty() && t.back() == ' ') t.pop_back();
        tail.push_back(t);
    }
    if (tail != page)
        for (const auto &row : v) std::printf("      | %s (channel %d)\n", row.first.c_str(), row.second);
    check(tail == page, "N8.3", "the page ends with the whole routine, scrolled in order (the repeated verse "
                                      "three times, the blank rows kept)");
}

void probe() {
    Run r;
    std::printf("probe: chuckles=%d\n", r.at_chuckles());
    r.say("ente");
    dump("probe", r.timeline(Run::now_ms(), 8000));
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
    if (argc > 2 && !std::strcmp(argv[2], "--probe")) {
        probe();
        return 0;
    }
    test_entertain();
    test_cut_by_key();
    test_welcome();
    test_blackthorn();
    test_initiation();
    test_load(true);
    test_load(false);
    test_menu();
    test_immediate();
    test_unpaced();
    test_render();
    std::printf("\nA3-HF5 dialogue pacing: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
