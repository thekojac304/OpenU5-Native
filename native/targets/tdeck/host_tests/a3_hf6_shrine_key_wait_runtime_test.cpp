// Alpha 3 A3-HF6 (H-183 / D-40) -- the shrine rite's and the Codex's key waits
// reach the screen.
//
//   a3_hf6_shrine_key_wait_runtime <openu5-alpha1-resources.bin> [--probe]
//
// The player-visible defect: after the mantra, "The Altar speaks and a Quest
// is ordained!", the Codex page and "Return again..." arrived in one frame;
// the Codex's four pages (nine with the ceremony) likewise. CAST2.OVL waits
// for a key between each of them (derivation: ALPHA3_AUDIO.md 32, the
// comment in openu5/dialogue_pacer.h):
//
//   shrine_visit, quest branch  0x0a9b, 0x0abc     call 0x448c
//   Codex handler 0x0d24        0x0d2b 0x0d35 0x0d3f 0x0d9f
//                  ceremony     0x0df8 0x0e16 0x0e2d 0x0e44 0x0e5b
//
// Every one resolves (CAST2 base 0xE1E0) to kernel 0x266c getkey_with_redraw
// -- the SAME blocking getkey as the TLK KeyWait: any key, discarded, no
// timeout, no flush before or after, nothing ticks while it waits. The
// "WELL DONE!" branch (0x0c18-0x0d1a) has none.
//
// The core has emitted GameEventKind::ShrineKeyWait at each of those points
// since the shrine port (shrine.cpp); outside a mounted Blackthorn scene
// nothing on the device read it.
//
// Everything below goes through the REAL AlphaRuntime -- raw keys in, the
// shipped pack's shrine table and MISCMSG records (the production
// bind_shrine_services(), A3-HF6), the pack's overworld -- drawing on the REAL
// tdeck_board.cpp over the fake ST7789, on the host esp_timer virtual clock.
//
//   N1  ordained: the altar's first wait holds the rest of the rite
//   N2  one key releases one wait; it is consumed (no command, no movement,
//       no typed character), Mic and the trackball press too; no timeout
//   N3  the Codex ceremony: nine waits, one key each, a burst of keys handled
//       in one frame releases one section per key
//   N4  the key after the mantra's Enter never becomes text or a new prompt
//   N5  a key that would be a command ('e' on the Codex, 'y') starts nothing
//   N6  the order: the transcript equals the unpaced run's; the quakes land
//       with their section; the core's state is complete at the Enter
//   N7  the System Menu mid-rite: nothing released behind it
//   N8  a successful load drops the rest of the rite; a failed one keeps it
//   N9  no wait survives the rite, Return to Title or a New Journey
//   N10 unchanged: WELL DONE / donation / unfocused stay immediate, the
//       unpaced harness drains synchronously, a Blackthorn scene keeps its
//       own getkeys
//   N11 the A3-04F transcript row cache: a release draws only transcript rows
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/blackthorn_scene.h"
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
constexpr int64_t kFrameMs = 5;
constexpr int kCodexTile = 17, kShrineTile = 25;

std::string ascii(TalkText t) {
    std::string s;
    for (auto c : t) s += char(c);
    return s;
}

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
        g.karma = 50;
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.character_class = 'A';
        m.level = 1;
        m.current_hp = m.max_hp = 300;
        m.strength = m.dexterity = m.intelligence = 20;
        g.quest.shrine_quest = g.quest.shrine_visited = 0;
        frames(3);
    }
    GameState &g() { return rt->game(); }
    const UiSession &ui() { return *rt->ui(); }
    UiMode mode() { return ui().mode(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const DialoguePacer &pacer() { return rt->dialogue_pacer(); }
    bool waiting() { return pacer().awaiting_key(); }
    std::string overlay() { return rt->status_overlay(); }
    static int64_t now_ms() { return openu5_host_virtual_clock_us() / 1000; }
    void frames(int k) {
        for (int i = 0; i < k; ++i) {
            openu5_host_virtual_clock_us() += kFrameMs * 1000;
            rt->render(board);
        }
    }
    void run_ms(int64_t ms) { frames(int(ms / kFrameMs)); }
    void raw(tdeck::RawInputEvent e, bool frame = true) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        if (frame) frames(1);
    }
    tdeck::RawInputEvent key_event(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = e.base_code = code;
        e.modifiers.alt = alt;
        return e;
    }
    void key(uint8_t code, bool alt = false) { raw(key_event(code, alt)); }
    void type(const std::string &s) { for (char c : s) key(uint8_t(c)); }
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
    void alt_save() { key('s', true); }
    void alt_load() { key('l', true); }
    void menu_toggle() { key('m', true); }
    void return_to_title() { key('m', true); ball(RawInputKind::TrackballUp); key('\r'); }

    void set_mark() { mark = ui().transcript_size(); }
    std::string since_mark() {
        std::string out;
        for (size_t i = mark; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i)) out += b->text;
        return out;
    }
    bool shown(const std::string &needle) { return since_mark().find(needle) != std::string::npos; }
    size_t count(const std::string &needle) {
        const auto all = since_mark();
        size_t k = 0;
        for (size_t at = all.find(needle); at != std::string::npos; at = all.find(needle, at + 1)) ++k;
        return k;
    }

    // --- the shrines, straight from the pack ---------------------------------
    const ShrineData &shrines() { return pack->shrine_data; }
    int tile_at(int x, int y) { return ctx().terrain->effective(ctx().world, MapId{0, 0}, x, y); }
    bool stand(int x, int y) {
        g().position.map = {0, 0};
        g().position.xy = {uint8_t(x), uint8_t(y)};
        frames(1);
        return true;
    }
    bool codex_xy(int &x, int &y) {
        for (y = 0; y < 256; ++y)
            for (x = 0; x < 256; ++x)
                if (tile_at(x, y) == kCodexTile) return true;
        return false;
    }
    // (E)nter the shrine of virtue `v`, accept "Visit?", then the virtue and
    // the mantra. The mark is set just before the mantra's Enter; true when
    // that Enter was handled with the text-entry prompt it belongs to.
    bool rite(int v, const std::string &virtue_override = "", const std::string &mantra_override = "") {
        if (v < 0 || v >= shrines().count) return false;
        stand(shrines().x[v], shrines().y[v]);
        if (tile_at(shrines().x[v], shrines().y[v]) != kShrineTile) return false;
        key('e');
        if (mode() != UiMode::YesNo) return false;
        key('y');
        if (mode() != UiMode::TextEntry) return false;
        type(virtue_override.empty() ? ascii(shrines().virtues[v]) : virtue_override);
        key('\r');
        if (mode() != UiMode::TextEntry) return false;
        type(mantra_override.empty() ? ascii(shrines().mantras[v]) : mantra_override);
        set_mark();
        key('\r');
        return true;
    }
    // Stand on the Codex and (E)nter it. The mark is set just before the key.
    bool codex() {
        int x = 0, y = 0;
        if (!codex_xy(x, y)) return false;
        stand(x, y);
        set_mark();
        key('e');
        return shown("Codex of Ultimate Wisdom");
    }
    std::vector<std::pair<std::string, int>> visible() const {
        UiRenderedLine lines[kHudTranscriptLines]{};
        const size_t k = rt->ui()->visible_lines(lines, kHudTranscriptLines, size_t(kHudTranscriptColumns));
        std::vector<std::pair<std::string, int>> out;
        for (size_t i = 0; i < k; ++i) out.emplace_back(lines[i].text, int(lines[i].channel));
        return out;
    }
};

// The four Codex sections before the ceremony, keyed by a line each prints.
const char *const kCodexSections[] = {"Codex of Ultimate Wisdom", "The book is open", "Upon the hallowed page",
                                      "\"\n\n" /* the page itself, closed by its quote */};

// ===========================================================================
void test_ordained() {
    std::printf("\nN1/N2 the altar ordains a Quest (CAST2 0x0a9b, 0x0abc)\n");
    Run r;
    const auto routed0 = r.rt->routed_command_count();
    if (!check(r.rite(0), "N1.0", "(E)nter the shrine of " + ascii(r.shrines().virtues[0]) + ", Visit, virtue, mantra "
                                  "-- the mantra's Enter handled"))
        return;
    const auto routed = r.rt->routed_command_count();
    const bool first = r.shown("The Altar speaks and a Quest is ordained!");
    r.run_ms(10000);
    const bool page = r.shown("'Tis now thy sacred Quest"), ret = r.shown("Return again");
    check(first && !page && !ret, "N1.1",
          "the first getkey holds: 10 s after the Enter only \"...a Quest is ordained!\" is shown (page " +
              n(page) + ", return " + n(ret) + ")");
    check(r.waiting() && r.overlay() == "Enter: continue", "N1.2",
          "no timeout, and the handheld shows the key-wait cue (\"" + r.overlay() + "\")");
    check((r.g().quest.shrine_quest & 1) != 0, "N1.3",
          "the Quest bit is already set (CAST2 0x0a88 writes it before the print and the first getkey)");
    const auto xy = r.g().position.xy;
    r.key('m'); // the mantra's own letter
    check(r.shown("'Tis now thy sacred Quest") && !r.shown("Return again") && r.waiting(), "N2.1",
          "one key releases exactly one section and stops at the second getkey");
    check(r.rt->routed_command_count() == routed && r.ui().input_length() == 0 && r.mode() != UiMode::TextEntry,
          "N2.2", "the key is consumed: no command routed, nothing typed, no prompt re-opened (routed +" +
                      n(long(r.rt->routed_command_count() - routed)) + ")");
    r.ball(RawInputKind::TrackballLeft);
    check(r.shown("Return again") && !r.waiting() && r.g().position.xy.x == xy.x && r.g().position.xy.y == xy.y &&
              r.rt->routed_command_count() == routed,
          "N2.3", "a trackball step ends the second wait and moves nothing");
    check(r.count("Return again") == 1 && r.count("'Tis now") == 1 && r.count("ordained") == 1, "N2.4",
          "each section shown exactly once");
    r.set_mark();
    // A4-UI4: a second roll the same way within the trackball's 12 ms debounce
    // (InputController) is the same detent. This roll used to land after the
    // previous frame's ~22 ms of modelled bus time; the console frame is
    // shorter, so a player's pause is spelled out.
    r.run_ms(50);
    r.ball(RawInputKind::TrackballLeft);
    check(r.rt->routed_command_count() == routed + 1, "N2.5",
          "after the last wait the next input is an ordinary command again (a step west is routed)");
    (void)routed0;

    // Mic is a key to getkey 0x266c like any other.
    Run m;
    if (!check(m.rite(0), "N2.6", "the rite again")) return;
    const auto routed_m = m.rt->routed_command_count();
    m.mic();
    check(m.shown("'Tis now thy sacred Quest") && !m.shown("Return again") && m.waiting() &&
              m.rt->routed_command_count() == routed_m,
          "N2.7", "Mic ends one wait, consumed");
}

// The Codex with every shrine visited: the reading and the ceremony.
void test_codex_ceremony() {
    std::printf("\nN3/N5/N6 the Codex ceremony (0x0d2b ... 0x0e5b, nine getkeys)\n");
    Run r;
    r.g().quest.shrine_quest = 1;       // the Preset: Shrine state
    r.g().quest.shrine_visited = 0xff;
    const auto before_quest = r.g().quest;
    if (!check(r.codex(), "N3.0", "(E)nter the Codex")) return;
    const auto routed = r.rt->routed_command_count();
    const auto after_quest = r.g().quest;
    r.run_ms(10000);
    check(!r.shown("The book is open") && r.waiting(), "N3.1",
          "the first getkey (0x0d2b) holds for 10 s: only \"...lies before thee...\" is shown");
    check(after_quest.shrine_visited == 0xff && (after_quest.optional_present & 2) && before_quest.shrine_quest == 1,
          "N6.1", "the core's Codex state is complete at the Enter (presentation-only pacing)");
    // Each later key: exactly the next section, nothing more.
    // 0x0d35 0x0d3f 0x0d9f, then the ceremony: the quakes + record 40 (0x0df8),
    // "Thou dost read:" + record 41 (0x0e16), records 42-44 (0x0e2d 0x0e44 0x0e5b).
    const char *const sections[] = {"The book is open", "Upon the hallowed page", "\"\n\n", "STRANGE WIND",
                                    "Thou dost read:", nullptr, nullptr, nullptr};
    int exact = 0;
    std::string detail;
    bool quake_early = false, quake_on_time = false;
    for (int k = 0; k < 8; ++k) {
        const auto before = r.since_mark();
        if (k == 3) quake_early = r.rt->transient_probe_for_test().quake;
        r.key(k % 2 ? 'e' : 'y'); // command letters: must start nothing
        if (k == 3) {
            quake_on_time = r.rt->transient_probe_for_test().quake;
            // A3-HF7 (H-184): CAST2 prints "A STRANGE WIND..." (0x0df1) only
            // after the three bracketed shakes (0x0dc0/0x0dd7/0x0dee) and the
            // getkey's first idle pass; a key inside them is swallowed. This
            // expectation read the section in the key's own frame (HF6).
            r.run_ms(3 * kQuakeDurationMs + 2 * int64_t(kSceneTickMs));
        }
        const auto after = r.since_mark();
        const bool grew = after.size() > before.size();
        const bool right = !sections[k] || after.find(sections[k], before.size() > 4 ? before.size() - 4 : 0) != std::string::npos;
        const bool held = r.waiting();
        exact += grew && right && held;
        detail += " " + n(long(after.size() - before.size())) + (held ? "h" : "-");
    }
    std::printf("    released bytes per key:%s\n", detail.c_str());
    check(exact == 8, "N3.2", "each of the first eight keys releases one more section and stops at the next getkey (" +
                                  n(exact) + " of 8)");
    const auto before_last = r.since_mark();
    r.key('y');
    check(!r.waiting() && r.since_mark() == before_last, "N3.3",
          "the ninth key ends the last getkey (0x0e5b), with nothing left to show");
    check(r.rt->routed_command_count() == routed && r.count("Codex of Ultimate Wisdom") == 1 &&
              r.ui().input_length() == 0 && r.mode() == UiMode::Exploration,
          "N5.1", "the nine keys ('y'/'e' on the Codex tile) started nothing: no command, no second Codex, no prompt");
    check(!quake_early && quake_on_time, "N6.2",
          "the ceremony's quakes (0x0dbd-0x0dee) land with the key that ends the page's getkey, not before");
    Run u(false);
    u.g().quest.shrine_quest = 1;
    u.g().quest.shrine_visited = 0xff;
    u.codex();
    check(u.since_mark() == r.since_mark(), "N6.3",
          "the paced transcript is byte-identical to the unpaced run's (same text, same order)");
    check(r.pacer().collapsed() == 0, "N6.4", "the whole ceremony fits the pacer's queue (no collapse)");
}

// Three keys handled inside one frame (a burst the device can queue) release
// three sections -- one per key, the way 1988's typeahead fed one getkey each.
void test_burst() {
    std::printf("\nN3B a burst of keys between two frames\n");
    Run r;
    r.g().quest.shrine_quest = 1;
    r.g().quest.shrine_visited = 0xff;
    if (!check(r.codex(), "N3B.0", "the Codex")) return;
    r.raw(r.key_event(' '), false);
    r.raw(r.key_event(' '), false);
    r.frames(1);
    check(r.shown("Upon the hallowed page") && !r.shown("\"\n\n") && r.waiting(), "N3B.1",
          "two keys release two sections and the third getkey still holds");
}

void test_mantra_adjacent() {
    std::printf("\nN4 the key after the mantra\n");
    Run r;
    if (!check(r.rite(1), "N4.0", "the rite of " + ascii(r.shrines().virtues[1]))) return;
    check(r.waiting() && r.mode() != UiMode::TextEntry, "N4.1",
          "the mantra's Enter closed the text entry and did not itself end the first getkey");
    const auto mantra = ascii(r.shrines().mantras[1]);
    r.key(uint8_t(mantra[0] | 0x20)); // the player keeps typing the mantra
    r.key('\r');
    check(r.shown("Return again") && !r.waiting() && r.ui().input_length() == 0 && r.mode() != UiMode::TextEntry,
          "N4.2", "a letter and an Enter end the two getkeys; neither became text or opened a prompt");
    r.set_mark();
    r.key('e'); // an ordinary Enter command afterwards is still one
    check(r.mode() == UiMode::YesNo, "N4.3", "the next (E)nter on the altar asks \"Visit?\" as before");
}

void test_menu() {
    std::printf("\nN7 the System Menu at a getkey\n");
    Run r;
    r.g().quest.shrine_quest = 1;
    if (!check(r.codex(), "N7.0", "the Codex (no ceremony: four getkeys)")) return;
    r.menu_toggle();
    const bool open = r.rt->system_menu_open();
    r.ball(RawInputKind::TrackballDown);
    r.run_ms(3000);
    const bool behind = r.shown("The book is open");
    r.menu_toggle();
    check(open && !behind && r.waiting() && !r.rt->system_menu_open(), "N7.1",
          "nothing is released behind the menu; closing it leaves the getkey waiting");
    r.key(' ');
    check(r.shown("The book is open") && !r.shown("Upon the hallowed") && r.waiting(), "N7.2",
          "after the menu one key releases one section, as before");
}

void test_load(bool succeed) {
    const char *id = succeed ? "N8" : "N8F";
    std::printf("\n%s a %s load at a getkey\n", id, succeed ? "successful" : "failed");
    Run r;
    if (succeed) {
        r.alt_save();
        r.frames(2);
    }
    r.g().quest.shrine_quest = 1;
    if (!check(r.codex(), succeed ? "N8.0" : "N8F.0", "the Codex")) return;
    r.alt_load();
    if (succeed) {
        r.run_ms(3000);
        check(!r.waiting() && !r.rt->transient_probe_for_test().dialogue_pause && !r.shown("The book is open"), "N8.1",
              "the rest of the reading never reaches the loaded game");
        const auto routed = r.rt->routed_command_count();
        r.ball(RawInputKind::TrackballDown);
        check(r.rt->routed_command_count() == routed + 1, "N8.2", "the next input is an ordinary command");
    } else {
        check(r.shown("No valid save") && r.waiting() && !r.shown("The book is open"), "N8F.1",
              "a load that fails leaves the getkey waiting");
        r.key(' ');
        r.key(' ');
        r.key(' ');
        const bool page = r.shown("Upon the hallowed page") && r.shown("\"\n\n") && r.waiting();
        r.key(' '); // 0x0d9f, the getkey after the page
        check(page && !r.waiting(), "N8F.2", "and the reading completes, one key per getkey (four without the ceremony)");
    }
}

void test_lifecycle() {
    std::printf("\nN9 no wait outlives the rite\n");
    {
        Run r;
        r.alt_save();
        r.frames(2);
        if (!check(r.rite(2), "N9.0", "the rite")) return;
        r.return_to_title();
        const bool title = !r.rt->system_menu_open();
        r.key('x');
        r.key('j');
        r.key('\r'); // the title's Continue: loads the save
        r.frames(3);
        const auto routed = r.rt->routed_command_count();
        r.ball(RawInputKind::TrackballDown);
        check(title && !r.rt->transient_probe_for_test().dialogue_pause && !r.shown("'Tis now") &&
                  r.rt->routed_command_count() == routed + 1,
              "N9.1", "Return to Title, then Continue: the old rite's getkey and its text are gone, input is play");
    }
    {
        Run r;
        r.g().quest.shrine_quest = 1;
        if (!r.codex()) return;
        for (int k = 0; k < 4; ++k) r.key(' ');
        r.frames(2);
        const auto routed = r.rt->routed_command_count();
        r.ball(RawInputKind::TrackballDown);
        check(!r.waiting() && r.overlay() != "Enter: continue" && r.rt->routed_command_count() == routed + 1, "N9.2",
              "after the reading's last getkey the cue is gone and the next input steps off the Codex");
    }
}

void test_unchanged() {
    std::printf("\nN10 what must not change\n");
    {
        Run r;
        r.g().quest.shrine_visited = 1;
        r.g().quest.shrine_quest = 1;
        if (!check(r.rite(0), "N10.0", "the rite with the Quest done")) return;
        check(r.shown("WELL DONE!") && !r.waiting() && r.overlay() != "Enter: continue", "N10.1",
              "\"WELL DONE!\" and its rewards arrive at once (0x0c18-0x0d1a has no getkey)");
    }
    {
        Run r;
        r.g().quest.shrine_visited = 1;
        if (!r.rite(0)) return;
        check(!r.waiting() && r.mode() == UiMode::NumericEntry, "N10.2",
              "an already visited shrine asks \"How many cycles?\" at once");
    }
    {
        Run r;
        if (!r.rite(0, "", "xx")) return;
        check(r.shown("unfocused") && !r.waiting(), "N10.3", "a wrong mantra is answered at once");
    }
    {
        Run u(false);
        u.g().quest.shrine_quest = 1;
        u.codex();
        check(u.shown("\"\n\n") && !u.waiting(), "N10.4",
              "the unpaced harness (the reference's automation rule) drains the Codex synchronously");
    }
    {
        // A Blackthorn capture scene keeps its own getkeys: the ShrineKeyWait
        // inside a mounted scene belongs to the scene pacer, never to this one.
        Run r;
        static BlackthornSceneScript script{};
        script.count = 1;
        script.beats[0] = BlackthornBeat{};
        auto emit = [&](GameEvent e) { r.ctx().events.emit(r.ctx().events.context, e); };
        r.set_mark();
        GameEvent scene{};
        scene.kind = GameEventKind::BlackthornScene;
        scene.blackthorn_scene = &script;
        emit(scene);
        GameEvent wait{};
        wait.kind = GameEventKind::ShrineKeyWait;
        emit(wait);
        GameEvent after{};
        after.kind = GameEventKind::Message;
        after.text = "after the capture getkey";
        emit(after);
        r.run_ms(2000);
        const bool scene_waits = r.rt->blackthorn_pacer().awaiting_key() && !r.pacer().holding();
        r.key('\r');
        r.run_ms(500);
        check(scene_waits && r.shown("after the capture getkey") && !r.pacer().holding(), "N10.5",
              "inside a Blackthorn scene the getkey is the scene pacer's: one Enter releases it");
    }
}

void test_render() {
    std::printf("\nN11 the A3-04F transcript row cache\n");
    Run r;
    r.g().quest.shrine_quest = 1;
    if (!check(r.codex(), "N11.0", "the Codex")) return;
    r.frames(2);
    const auto held = r.visible();
    std::string screen; // the visible rows, joined: a section may wrap mid-phrase
    for (const auto &l : held) screen += l.first + " ";
    const bool page_hidden = screen.find("lies before") != std::string::npos && screen.find("book") == std::string::npos;
    int idle_rows = 0, full = 0, releases = 0, drew_text = 0;
    for (int k = 0; k < 3; ++k) {
        for (int f = 0; f < 40; ++f) {
            bus::reset_stats();
            r.frames(1);
            for (const auto &w : bus::windows()) idle_rows += !std::strcmp(region_of(w), "transcript");
        }
        bus::reset_stats();
        r.key(' ');
        ++releases;
        bool text = false;
        for (const auto &w : bus::windows()) {
            full += !std::strcmp(region_of(w), "full-clear");
            text |= !std::strcmp(region_of(w), "transcript");
        }
        drew_text += text;
    }
    check(page_hidden, "N11.1", "while the first getkey holds, the next section is not on the screen");
    check(idle_rows == 0, "N11.2", "frames at a getkey draw no transcript row (" + n(idle_rows) + ")");
    check(drew_text == releases && full == 0, "N11.3",
          "each release draws transcript rows and never clears the screen (" + n(drew_text) + "/" + n(releases) +
              ", full clears " + n(full) + ")");
}

void probe() {
    Run r;
    for (int v = 0; v < r.shrines().count; ++v)
        std::printf("shrine %d %s (%d,%d) tile=%d mantra=%s\n", v, ascii(r.shrines().virtues[v]).c_str(),
                    r.shrines().x[v], r.shrines().y[v], r.tile_at(r.shrines().x[v], r.shrines().y[v]),
                    ascii(r.shrines().mantras[v]).c_str());
    int x = 0, y = 0;
    std::printf("codex found=%d at (%d,%d)\n", r.codex_xy(x, y), x, y);
    Run c;
    c.g().quest.shrine_quest = 1;
    c.g().quest.shrine_visited = 0xff;
    c.codex();
    std::printf("---- codex transcript after 1 s ----\n%s\n----\n", (c.run_ms(1000), c.since_mark()).c_str());
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
    // --only <name> runs one scenario (the mutation driver's quick re-runs).
    const char *only = argc > 3 && !std::strcmp(argv[2], "--only") ? argv[3] : nullptr;
    const struct { const char *name; void (*run)(); } tests[] = {
        {"ordained", test_ordained},   {"ceremony", test_codex_ceremony},
        {"burst", test_burst},         {"mantra", test_mantra_adjacent},
        {"menu", test_menu},           {"load", [] { test_load(true); }},
        {"failed-load", [] { test_load(false); }},
        {"lifecycle", test_lifecycle}, {"unchanged", test_unchanged},
        {"render", test_render}};
    for (const auto &t : tests)
        if (!only || !std::strcmp(only, t.name)) t.run();
    std::printf("\nA3-HF6 shrine key waits: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
