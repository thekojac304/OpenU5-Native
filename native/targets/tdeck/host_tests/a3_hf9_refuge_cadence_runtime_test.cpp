// Alpha 3 A3-HF9 (H-185 / D-42) -- the Refuge's cadence, and its karma getkey,
// on the real device path.
//
//   a3_hf9_refuge_cadence_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> [--only <name>]
//
// The player-visible defect: the Refuge ran on a Class-C clock (70 ms per raw
// delay unit plus 900 / 260 ms reading floors) that no instruction backs, and
// Lord British's karma speech left the screen by itself after ~1.5 s where the
// original waits for a key. BLCKTHRN.OVL party_refuge 0x0910 (near calls
// through base 0xA290):
//
//   0946 delay(10)                         -- the world, before any line
//   095f darkness; 0962-098c black + dissolve   09d6 refuge; 09dd delay(14)
//   09e4 no evil; 09eb delay(28)           09f2 slumber; 0a0d-0a49 260,000 sweep samples
//   0a4f the shout; 0a56 delay(6)          0a7c / 0aae two FIZZLE_INs, each + delay(4)
//   0ac9 thunder; 0acc / 0acf two screen_shake_fx   0af5 FIZZLE_IN the apparition
//   0b03-0b3b the KARMA.DAT record, quoted; 0b3e GETKEY -- no timeout, any key
//   0b45 strange words; 0b4c delay(4); one 0x7530 revival sweep per member
//   0bba vertigo; 0bc1 delay(4); 0bc4-0bfa black + dissolve; 0bfd the karma floor
//
// Everything goes through the REAL AlphaRuntime (a wiped party, one Space for
// the turn whose death check raises the scene), with the pack's KARMA.DAT,
// drawing on the REAL tdeck_board.cpp over the fake ST7789 on the host
// esp_timer virtual clock (bus timing off). The instants below are stated from
// the bytes in device units (tick 55 ms, tone_sweep 25,806 samples/s, fizzle /
// dissolve floor one tick, shake 8 x 117 ms), never read back from the model.
//
//   N1  cadence: each line and figure at the binary's instant; the world during
//       the first delay(10), the black void after the darkness line; the shakes
//   N2  the getkey: no timeout, `Enter: continue`, one key ends it, consumed --
//       Space, Enter, a letter, the trackball, Mic
//   N3  leakage: the key moves nothing, routes no command, opens no prompt;
//       a second key in the same frame and keys in the later holds skip nothing
//   N4  karma: the death karma picks the speech; nothing mutates until the end
//   N5  deferral: after the key, the rest on its own clock; transcript rows too
//   N6  the System Menu at the getkey and during the slumber
//   N7  a successful load drops the scene and its latch; a failed one keeps it
//   N8  Return to Title + Continue at the getkey
//   N9  controls: the unpaced harness; keys before the getkey are not kept
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/misc_records.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/audio.h"
#include "openu5/hud.h"
#include "openu5/narrative_scene.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
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
bool check(bool good, const std::string &id, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s %s\n", good ? "GREEN" : "RED", id.c_str(), label.c_str());
    return good;
}
std::string n(long long v) { return std::to_string(v); }
const tdeck::AlphaResourceOwners *pack = nullptr;
AudioPackInfo g_audio{};
constexpr int64_t kFrameMs = 5;
constexpr int kW = 320;
constexpr int64_t kTick = 55;
constexpr int64_t sweep_ms(int64_t samples) { return samples * 1000 / 25806; }
constexpr int64_t kSlumberMs = sweep_ms(3 * 0xc350 + 0x7530 + 2 * 0x9c40); // DS 0x372c: 10,075
constexpr int64_t kShakeMs = 8 * 117;
constexpr int64_t kFloorMs = kTick; // FIZZLE_IN / RECT_DISSOLVE have no timer: one tick
/** A hold of `want` ms, seen by a 5 ms frame loop. */
bool at(int64_t got, int64_t want) { return got >= want && got < want + 2 * kFrameMs; }

const char *const kDark = "An unending darkness engulfs thee...";
const char *const kRefuge = "Thou hast found refuge.";
const char *const kEvil = "No evil lives here, only peace and darkness.";
const char *const kSlumber = "But thy slumber is disturbed!";
const char *const kShout = "Someone shouts";
const char *const kThunder = "There is a peal of thunder!";
const char *const kWords = "Strange words are intoned.";
const char *const kVertigo = "Vertigo...";

std::string karma_text(int index) {
    const char *k = tdeck::misc_text_record(
        {pack->karma_text_offsets, pack->karma_text_records, pack->karma_text_record_count}, index);
    return k ? std::string(k) : std::string();
}

struct Recorder final : AudioBackend {
    struct Sub {
        int64_t us;
        SfxId id;
    };
    std::vector<Sub> subs;
    bool play_sfx(const SfxRequest &r) override {
        subs.push_back({openu5_host_virtual_clock_us(), r.id});
        return true;
    }
    void stop_sfx() override {}
    bool start_music(MusicSong, uint16_t) override { return true; }
    void stop_music() override {}
    void set_gain(AudioChannel, uint16_t) override {}
    int64_t first(SfxId id) const {
        for (const auto &s : subs)
            if (s.id == id) return s.us / 1000;
        return -1;
    }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    Recorder audio{};
    size_t mark = 0;
    explicit Run(bool paced = true, int members = 2, int karma = 50) {
        openu5_host_virtual_clock_us() = 5'000'000;
        bus::install();
        bus::model() = bus::Model{};
        bus::model().timed = false;
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
        rt->configure_audio(g_audio, &audio);
        auto &g = rt->game();
        g.time.hour = 12;
        g.time.minute = 0;
        g.food = 900;
        g.karma = uint8_t(karma);
        const char *names[] = {"Avatar", "Shamino", "Iolo"};
        g.party.character_count = g.party.party_size = uint8_t(members);
        g.party.active_character = 255;
        for (int i = 0; i < members; ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "%s", names[i]);
            m.status = 'G';
            m.character_class = i ? 'F' : 'A';
            m.level = 1;
            m.current_hp = m.max_hp = 300;
            m.strength = m.dexterity = m.intelligence = 20;
        }
        // Britannia on foot, in open grassland (the Developer's own surface).
        g.position.map = {0, 0};
        g.position.xy = {90, 100};
        frames(3);
    }
    GameState &g() { return rt->game(); }
    const UiSession &ui() { return *rt->ui(); }
    const NarrativeScenePacer &pacer() { return rt->narrative_pacer(); }
    static int64_t now_ms() { return openu5_host_virtual_clock_us() / 1000; }
    std::function<void()> each;
    void frames(int k) {
        for (int i = 0; i < k; ++i) {
            openu5_host_virtual_clock_us() += kFrameMs * 1000;
            rt->render(board);
            if (each) each();
        }
    }
    void run_ms(int64_t ms) { frames(int(ms / kFrameMs)); }
    bool until(const std::function<bool()> &done, int64_t max_ms) {
        for (int64_t t = 0; t < max_ms; t += kFrameMs) {
            if (done()) return true;
            frames(1);
        }
        return done();
    }
    void raw(tdeck::RawInputEvent e, bool frame = true) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        if (frame) frames(1);
    }
    static tdeck::RawInputEvent key_event(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = e.base_code = code;
        e.modifiers.alt = alt;
        return e;
    }
    void key(uint8_t code, bool alt = false) { raw(key_event(code, alt)); }
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

    /** Every member down; the next turn's death check raises the Refuge. */
    void wipe() {
        for (int i = 0; i < g().party.character_count; ++i) {
            g().party.characters[i].status = 'D';
            g().party.characters[i].current_hp = 0;
        }
    }
    /** Wipe, then one Space: returns the virtual time of that key. */
    int64_t start() {
        wipe();
        frames(2);
        set_mark();
        const int64_t t = now_ms();
        key(' ');
        return t;
    }
    bool to_getkey(int64_t max_ms = 30000) { return until([&] { return pacer().awaiting_key(); }, max_ms); }
    bool to_end(int64_t max_ms = 30000) { return until([&] { return !pacer().active(); }, max_ms); }
    std::string overlay() { return rt->status_overlay(); }

    // --- the panel -------------------------------------------------------------
    /** Viewport cell (cx,cy) is solid black in the panel's GRAM. */
    static bool cell_black(int cx, int cy) {
        const uint16_t *p = bus::gram();
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                if (p[(kHudViewportY + cy * 16 + y) * kW + kHudViewportX + cx * 16 + x] != 0) return false;
        return true;
    }
    /** Cells of rows 1..9 (clear of the sky / wind strips) that are black. */
    static int black_cells() {
        int k = 0;
        for (int y = 1; y < 10; ++y)
            for (int x = 0; x < 11; ++x) k += cell_black(x, y);
        return k;
    }
    /** FNV of the transcript panel (right, below the party). */
    static uint64_t transcript_hash() {
        const uint16_t *p = bus::gram();
        uint64_t h = 1469598103934665603ull;
        for (int y = kHudTranscriptY; y < 240; ++y)
            for (int x = kHudRightX; x < kW; ++x) h = (h ^ p[y * kW + x]) * 1099511628211ull;
        return h;
    }
    /** FNV of the viewport rows a shake moves (rows 6..8, clear of the strips). */
    static uint64_t viewport_hash() {
        const uint16_t *p = bus::gram();
        uint64_t h = 1469598103934665603ull;
        for (int y = kHudViewportY; y < kHudViewportY + 176; ++y)
            for (int x = kHudViewportX; x < kHudViewportX + 176; ++x) h = (h ^ p[y * kW + x]) * 1099511628211ull;
        return h;
    }
};

/** First time (ms) each needle and each phase reaches the screen. */
struct Timeline {
    std::vector<std::pair<std::string, int64_t>> lines;
    int64_t phase_at[6] = {-1, -1, -1, -1, -1, -1};
    int64_t getkey = -1;
    int64_t silence = -1; // first frame of the scene whose music context is Silence
    std::vector<int64_t> viewport_changes; // frames whose viewport rows differ from the previous frame
    std::vector<int64_t> quake_frames;     // frames with the device shake running
    uint64_t last_view = 0;
    int preroll_black = -1, void_black = -1;
    void watch(Run &r, const std::vector<std::string> &needles) {
        for (const auto &s : needles) lines.push_back({s, -1});
        r.each = [this, &r] {
            const auto text = r.since_mark();
            for (auto &l : lines)
                if (l.second < 0 && text.find(l.first) != std::string::npos) l.second = Run::now_ms();
            const int p = int(r.pacer().phase());
            if (p > 0 && p < 6 && phase_at[p] < 0) {
                phase_at[p] = Run::now_ms();
                if (p == int(RefugePhase::Void)) void_black = Run::black_cells();
            }
            if (r.pacer().active() && !r.pacer().mounted() && preroll_black < 0 && lines[0].second < 0)
                preroll_black = Run::black_cells();
            if (getkey < 0 && r.pacer().awaiting_key()) getkey = Run::now_ms();
            if (silence < 0 && r.pacer().active() && r.rt->audio().current_music_context() == MusicContext::Silence)
                silence = Run::now_ms();
            if (r.rt->transient_probe_for_test().quake) quake_frames.push_back(Run::now_ms());
            const auto v = Run::viewport_hash();
            if (v != last_view) viewport_changes.push_back(Run::now_ms());
            last_view = v;
        };
    }
    int64_t at_line(const std::string &s) const {
        for (const auto &l : lines)
            if (l.first == s) return l.second;
        return -1;
    }
    int64_t at_phase(RefugePhase p) const { return phase_at[int(p)]; }
};

// --- N1 / N4 / N5: the whole scene, timed ---------------------------------------------
void test_cadence() {
    std::printf("\nN1 cadence, N4 karma, N5 the deferred rest (2 members, karma 50)\n");
    Run r;
    const std::string speech = karma_text(2);
    Timeline tl;
    tl.watch(r, {kDark, kRefuge, kEvil, kSlumber, kShout, "FORTIS FORTUNA AVENTARI", kThunder,
                 speech.substr(0, 24), kWords, kVertigo});
    const int64_t key = r.start();
    const bool up = r.pacer().active() && r.pacer().scene() == NarrativeScene::Refuge;
    check(up, "N1.0", "one Space on a wiped party raises the Refuge scene");
    if (!up) return;
    const bool waited = r.to_getkey();
    const int64_t dark = tl.at_line(kDark), refuge = tl.at_line(kRefuge), evil = tl.at_line(kEvil),
                  slumber = tl.at_line(kSlumber), shout = tl.at_line(kShout),
                  fortis = tl.at_line("FORTIS FORTUNA AVENTARI"), thunder = tl.at_line(kThunder),
                  said = tl.at_line(speech.substr(0, 24));
    const int64_t left = tl.at_phase(RefugePhase::GhostLeft), both = tl.at_phase(RefugePhase::GhostBoth),
                  app = tl.at_phase(RefugePhase::Apparition), blank = tl.at_phase(RefugePhase::Void);
    std::printf("    from the key: darkness %lld, refuge %lld, evil %lld, slumber %lld, shout %lld, ghosts %lld/%lld, "
                "thunder %lld, apparition %lld, speech %lld, getkey %lld ms\n",
                (long long)(dark - key), (long long)(refuge - key), (long long)(evil - key), (long long)(slumber - key),
                (long long)(shout - key), (long long)(left - key), (long long)(both - key), (long long)(thunder - key),
                (long long)(app - key), (long long)(said - key), (long long)(tl.getkey - key));
    check(at(dark - key, 10 * kTick), "N1.1",
          "0x0946 delay(10) BEFORE the first line: the darkness line " + n(dark - key) + " ms after the key (want 550)");
    check(tl.preroll_black >= 0 && tl.preroll_black < 20, "N1.2",
          "during that delay the viewport still shows the world (" + n(tl.preroll_black) + " of 99 cells black)");
    // A4-UI4 (H-211 / D-68): 0x098f-0x09ca clear every object; the Avatar is
    // placed only at "But thy slumber is disturbed!" (0x09f5). Until A4-UI4 the
    // device kept it on the black stage (>= 98 of 99).
    check(blank == dark && tl.void_black == 99, "N1.3",
          "with the line, 0x0962: the viewport goes all black, the Avatar not yet placed (" + n(tl.void_black) + " of 99 black)");
    check(at(refuge - dark, kFloorMs), "N1.4",
          "\"Thou hast found refuge.\" after the dissolve's one-tick floor (+" + n(refuge - dark) + ", want 55)");
    check(at(evil - refuge, 14 * kTick), "N1.5", "\"No evil lives here\" after delay(14) (+" + n(evil - refuge) + ", want 770)");
    check(at(slumber - evil, 28 * kTick), "N1.6",
          "\"But thy slumber\" after delay(28) (+" + n(slumber - evil) + ", want 1540)");
    check(at(shout - slumber, kSlumberMs) && fortis == shout, "N1.7",
          "the shout, as one print, after the six slumber sweeps (+" + n(shout - slumber) + ", want 10075)");
    check(at(left - shout, 6 * kTick) && at(both - left, kFloorMs + 4 * kTick) &&
              at(thunder - both, kFloorMs + 4 * kTick),
          "N1.8", "the figures: delay(6), then fizzle + delay(4) twice (+" + n(left - shout) + ", +" + n(both - left) +
                      ", +" + n(thunder - both) + "; want 330, 275, 275)");
    check(at(app - thunder, 2 * kShakeMs), "N1.9",
          "the apparition after two 936 ms shakes (+" + n(app - thunder) + ", want 1872)");
    // The device's shake (the Quake's own primitive). A4-UI4 (H-212 / D-69):
    // every pulse is a frame now -- down and back for each of the two shakes'
    // eight pulses, 32 viewport changes, the first with the line. Until A4-UI4
    // the static stage showed one drop and one restore.
    int inside = 0;
    int64_t first = -1;
    for (auto t : tl.viewport_changes)
        if (t >= thunder && t < thunder + 2 * kShakeMs) {
            ++inside;
            if (first < 0) first = t;
        }
    const int64_t shaking = tl.quake_frames.empty() ? -1 : tl.quake_frames.back() - tl.quake_frames.front() + kFrameMs;
    check(inside == 32 && first == thunder && !tl.quake_frames.empty() && tl.quake_frames.front() == thunder &&
              at(shaking, 2 * kShakeMs),
          "N1.10", "the peal shakes the viewport: 16 pulses, " + n(inside) + " viewport changes from the line on, the device shake running " +
                       n(shaking) + " ms (screen_shake_fx x 2)");
    check(at(said - app, kFloorMs) && waited && tl.getkey == said, "N1.11",
          "the karma speech after the apparition's fizzle floor (+" + n(said - app) + "), and there it waits");
    check(r.audio.first(SfxId::RefugeSlumber) == slumber, "N1.12", "the slumber melody starts with its line");
    check(tl.silence >= 0 && tl.silence < dark && tl.silence - key <= 2 * kFrameMs, "N1.13",
          "the music stops with the scene's first beat, before the darkness line (the callers stop it before "
          "party_refuge): silent at +" + n(tl.silence - key) + " ms");

    // N4: nothing has mutated at the getkey.
    const auto &avatar = r.g().party.characters[0];
    check(r.g().karma == 50 && avatar.status == 'D' && r.g().position.map.location == 0 && !speech.empty() &&
              r.shown("\"" + speech.substr(0, 24)),
          "N4.1", "at the getkey: KARMA.DAT record 2 (karma 50 / 20), quoted; karma still 50, the party fallen, still outdoors");
    // N2 / N5: no timeout, then one key.
    const uint64_t rows = Run::transcript_hash();
    r.run_ms(60000);
    check(r.pacer().awaiting_key() && !r.shown(kWords) && Run::transcript_hash() == rows &&
              r.overlay() == "Enter: continue",
          "N2.1", "a minute later: still waiting (`Enter: continue`), no \"Strange words\", no transcript row drawn");
    const auto routed = r.rt->routed_command_count();
    const int64_t pressed = Run::now_ms();
    r.key(' ');
    const int64_t words = tl.at_line(kWords);
    check(words >= pressed && words - pressed <= kFrameMs && Run::transcript_hash() != rows, "N2.2",
          "one Space ends it: \"Strange words are intoned.\" in the key's frame, drawn to the transcript");
    check(r.rt->routed_command_count() == routed && r.g().position.xy.x == 90 && r.g().position.xy.y == 100, "N3.1",
          "the key is consumed: no command routed, the party did not move");
    check(!r.pacer().awaiting_key() && !r.shown(kVertigo) && r.g().karma == 50, "N5.1",
          "\"Vertigo...\" is still deferred, and karma is still 50 after the key");
    check(r.audio.first(SfxId::RefugeRevival) == words, "N5.2", "the revival tones start with \"Strange words\"");
    r.to_end();
    const int64_t vertigo = tl.at_line(kVertigo), flash = tl.at_phase(RefugePhase::Vertigo);
    check(at(vertigo - words, 4 * kTick + sweep_ms(2 * 0x7530)), "N5.3",
          "\"Vertigo...\" after delay(4) + two revival sweeps (+" + n(vertigo - words) + ", want 2545)");
    check(at(flash - vertigo, 4 * kTick), "N5.4", "the last dissolve after delay(4) (+" + n(flash - vertigo) + ")");
    r.frames(2);
    check(r.g().karma == 75 && avatar.status == 'G' && avatar.current_hp == avatar.max_hp &&
              r.g().position.map.location == 17 && r.g().position.map.floor == 1,
          "N4.2", "only when the scene is down: karma floored to 75, the party revived in the castle (17, floor 1)");
    const auto before = r.rt->routed_command_count();
    r.key(' ');
    check(r.rt->routed_command_count() == before + 1, "N3.2", "and the next key is ordinary play again");
}

// --- N4: the karma floor only raises; the record is the death karma's ----------------
void test_high_karma() {
    std::printf("\nN4 karma 90 (record 4; the floor does not lower it)\n");
    Run r(true, 1, 90);
    const std::string speech = karma_text(4);
    r.start();
    const bool waited = r.to_getkey();
    check(waited && r.shown("\"" + speech.substr(0, 24)) && r.g().karma == 90, "N4.3",
          "karma 90: record 4 is spoken at the getkey");
    r.key('\r');
    r.to_end();
    r.frames(2);
    check(r.g().karma == 90 && r.g().position.map.location == 17, "N4.4", "after the scene: karma still 90, in the castle");
}

// --- N2 / N3: every kind of key ends the getkey once and does nothing else ----------
void test_keys() {
    std::printf("\nN2 / N3 the key that ends the getkey\n");
    struct Kind {
        const char *name;
        std::function<void(Run &)> press;
    } kinds[] = {
        {"Enter", [](Run &r) { r.key('\r'); }},
        {"a letter (K = Klimb)", [](Run &r) { r.key('k'); }},
        {"the trackball (a move)", [](Run &r) { r.ball(RawInputKind::TrackballDown); }},
        {"Mic", [](Run &r) { r.mic(); }},
    };
    int i = 0;
    for (const auto &k : kinds) {
        ++i;
        Run r;
        r.start();
        if (!check(r.to_getkey(), "N2.k" + n(i) + ".0", std::string("the getkey for ") + k.name)) continue;
        const auto routed = r.rt->routed_command_count();
        const auto mode = r.ui().mode();
        const auto xy = r.g().position.xy;
        k.press(r);
        r.frames(2);
        const std::string after = r.since_mark();
        check(r.shown(kWords) && !r.pacer().awaiting_key(), "N2.k" + n(i),
              std::string(k.name) + " ends the getkey: \"Strange words\" follows");
        check(r.rt->routed_command_count() == routed && r.ui().mode() == mode && r.g().position.xy.x == xy.x &&
                  r.g().position.xy.y == xy.y && after.find("Klimb") == std::string::npos &&
                  after.find("what?") == std::string::npos,
              "N3.k" + n(i), std::string(k.name) + " does nothing else: no command, no prompt, no move");
        r.to_end();
        r.frames(2);
        check(r.ui().mode() == UiMode::Exploration && r.since_mark().find("Klimb") == std::string::npos &&
                  r.g().position.map.location == 17,
              "N3.e" + n(i), "and nothing of it surfaces after the scene: no prompt waiting, awake in the castle");
    }
}

// --- N3: one key never ends two waits -----------------------------------------------
void test_burst() {
    std::printf("\nN3 a burst of keys at the getkey, and keys in the later holds\n");
    Run r;
    Timeline tl;
    tl.watch(r, {kWords, kVertigo});
    r.start();
    if (!check(r.to_getkey(), "N3.b0", "the getkey")) return;
    const int64_t pressed = Run::now_ms();
    r.raw(Run::key_event(' '), false);
    r.raw(Run::key_event('\r'), false);
    r.raw(Run::key_event('k'), false);
    r.frames(1);
    // a key every 100 ms through the post-getkey holds
    for (int k = 0; k < 30 && r.pacer().active(); ++k) {
        r.key('x');
        r.run_ms(95);
    }
    r.to_end();
    const int64_t words = tl.at_line(kWords), vertigo = tl.at_line(kVertigo);
    check(words >= pressed && words - pressed <= kFrameMs && at(vertigo - words, 4 * kTick + sweep_ms(2 * 0x7530)),
          "N3.b1",
          "three keys in one frame end the one getkey; they and 30 more cut no hold (\"Vertigo...\" +" +
              n(vertigo - words) + ", want 2545)");
    r.frames(2);
    check(r.since_mark().find("Klimb") == std::string::npos && r.ui().mode() == UiMode::Exploration, "N3.b2",
          "none of them surfaces afterwards as a command or a prompt");
}

// --- N9: keys BEFORE the getkey are swallowed, not kept for it ----------------------
void test_typeahead() {
    std::printf("\nN9 keys before the getkey are not kept for it\n");
    Run r;
    r.start();
    for (int k = 0; k < 40 && !r.pacer().awaiting_key(); ++k) {
        r.key(' ');
        r.run_ms(390);
    }
    r.to_getkey();
    r.run_ms(2000);
    check(r.pacer().awaiting_key() && !r.shown(kWords), "N9.1",
          "forty keys pressed during the scene's busy loops: the getkey still waits for its own key");
}

// --- N6: the System Menu ------------------------------------------------------------
void test_menu() {
    std::printf("\nN6 the System Menu at the getkey and during the slumber\n");
    {
        Run r;
        r.start();
        if (!check(r.to_getkey(), "N6.0", "the getkey")) return;
        r.menu_toggle();
        const bool open = r.rt->system_menu_open();
        r.ball(RawInputKind::TrackballDown);
        r.ball(RawInputKind::TrackballUp);
        r.run_ms(3000);
        r.menu_toggle();
        r.run_ms(500);
        check(open && !r.rt->system_menu_open() && r.pacer().awaiting_key() && !r.shown(kWords), "N6.1",
              "the menu opens over the getkey; nothing is released behind it or by its keys; closed, it still waits");
        r.key(' ');
        check(r.shown(kWords) && !r.shown(kVertigo), "N6.2", "then one key releases the next line, and only it");
    }
    {
        Run r;
        Timeline tl;
        tl.watch(r, {kSlumber, kShout});
        r.start();
        r.until([&] { return r.shown(kSlumber); }, 10000);
        r.run_ms(2000);
        r.menu_toggle();
        r.run_ms(3000);
        const bool held = !r.shown(kShout);
        r.menu_toggle();
        r.run_ms(8000);
        const int64_t shout = tl.at_line(kShout), slumber = tl.at_line(kSlumber);
        check(held && shout - slumber >= kSlumberMs, "N6.3",
              "the menu during the slumber: nothing behind it; the shout still waits out the melody (+" +
                  n(shout - slumber) + ")");
    }
}

// --- N7: load ---------------------------------------------------------------------------
void test_load(bool succeed) {
    const std::string id = succeed ? "N7" : "N7F";
    std::printf("\n%s a %s load at the getkey\n", id.c_str(), succeed ? "successful" : "failed");
    Run r;
    if (succeed) {
        r.alt_save();
        r.frames(2);
    }
    r.start();
    if (!check(r.to_getkey(), id + ".0", "the getkey")) return;
    r.alt_load();
    r.run_ms(500);
    if (succeed) {
        check(!r.pacer().active() && !r.pacer().awaiting_key() && !r.pacer().mounted() &&
                  r.g().party.characters[0].status == 'G' && r.g().position.map.location == 0 &&
                  r.overlay() != "Enter: continue",
              "N7.1", "the loaded game: no scene, no getkey, the party as saved");
        r.run_ms(20000);
        check(!r.shown(kWords) && r.g().karma == 50, "N7.2", "nothing of the Refuge replays over it; no resurrection applied");
        // The latch went with the scene: the next wipe is a whole new Refuge.
        const int64_t key = r.start();
        const bool up = r.pacer().active();
        r.run_ms(200);
        const bool fresh = up && !r.shown(kDark) && r.g().party.characters[0].status == 'D' &&
                           r.g().position.map.location == 0;
        r.until([&] { return r.shown(kDark); }, 2000);
        check(fresh && r.shown(kDark) && r.to_getkey(), "N7.3",
              "a wipe after the load plays the whole Refuge again (not an instant resurrection); key at " + n(key));
    } else {
        check(r.shown("No valid save") && r.pacer().awaiting_key() && !r.shown(kWords), "N7F.1",
              "a load that fails leaves the getkey waiting");
        r.key(' ');
        check(r.shown(kWords), "N7F.2", "and one key goes on with the scene");
    }
}

// --- N8: Return to Title at the getkey --------------------------------------------------
void test_title() {
    std::printf("\nN8 Return to Title at the getkey, then Continue\n");
    Run r;
    r.alt_save();
    r.frames(2);
    r.start();
    if (!check(r.to_getkey(), "N8.0", "the getkey (a save was made before the wipe)")) return;
    r.return_to_title();
    r.key('x');
    r.key('j');
    r.key('\r');
    r.run_ms(600);
    check(!r.pacer().active() && !r.pacer().awaiting_key() && r.g().party.characters[0].status == 'G' &&
              r.overlay() != "Enter: continue",
          "N8.1", "Continue shows the saved game: no scene, no stale getkey");
    r.run_ms(20000);
    check(!r.shown(kWords) && !r.pacer().active(), "N8.2", "and nothing of the Refuge replays later");
}

// --- N9: the unpaced harness ---------------------------------------------------------------
void test_unpaced() {
    std::printf("\nN9 the unpaced harness contract\n");
    Run r(false);
    r.start();
    r.frames(2);
    check(!r.pacer().active() && r.shown(kWords) && r.shown(kVertigo) && r.g().position.map.location == 17 &&
              r.g().party.characters[0].status == 'G',
          "N9.2", "unpaced, the whole Refuge drains with the turn, no key needed, and resolves");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <openu5-alpha1-resources.bin> <openu5-audio.bin> [--only <name>]\n", argv[0]);
        return 2;
    }
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    g_audio = tdeck::load_audio_pack_info(argv[2]);
    const char *only = argc > 4 && std::strcmp(argv[3], "--only") == 0 ? argv[4] : nullptr;
    const std::pair<const char *, std::function<void()>> tests[] = {
        {"cadence", test_cadence},
        {"karma", test_high_karma},
        {"keys", test_keys},
        {"burst", test_burst},
        {"typeahead", test_typeahead},
        {"menu", test_menu},
        {"load", [] { test_load(true); }},
        {"load-fail", [] { test_load(false); }},
        {"title", test_title},
        {"unpaced", test_unpaced},
    };
    for (const auto &t : tests)
        if (!only || std::strcmp(only, t.first) == 0) t.second();
    std::printf("\na3_hf9_refuge_cadence_runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
