// Batch 51: the Camp apparition's pacing through the REAL AlphaRuntime, raw
// keys in, frames out of the capture Board, on the host esp_timer shim's
// pinned virtual clock (the device loop's 5 ms cadence, render() each step).
//
// Physical T-Deck witness (Batch 48 image 0bbdbf5c86f5, Phase 7B rerun): every
// visual of the scene was right and the whole of it went by "way too fast".
// Batch 42's host test proved the frames and their ORDER; it could not see
// time, because every Camp event was rendered synchronously inside the
// command. This test asserts the missing axis: how long each original wait
// keeps its frame on the glass, and what input does while it does.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/rest.h"
#include "openu5/scene_timing.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
using namespace openu5;
void batch37_reset_screen();
size_t batch37_frame_count();
uint16_t batch37_frame_pixel(size_t, int, int);
int64_t batch37_frame_time_us(size_t);
int batch37_panel_draw_count();
uint16_t batch37_pixel(int, int);
namespace {
int checks = 0, failures = 0;
void check(bool good, const char *label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label);
}
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr int64_t kClockStartUs = 5'000'000;

struct Sample {
    int64_t us;
    bool inverted, key_wait, pacer;
    size_t frames;
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    std::vector<Sample> timeline;
    Run(int seed, bool paced) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        batch37_reset_screen();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.render_pixels = true;
        f.indexed_test_tiles = true;
        f.paced_scenes = paced;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.position.xy = {80, 80};
        g.time.hour = 12;
        g.time.minute = 55;
        g.food = 80;
        g.party.character_count = g.party.party_size = 2;
        g.party.active_character = 255;
        for (int i = 0; i < 2; ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "Member%d", i);
            m.status = 'G';
            m.character_class = i ? 'M' : 'F';
            m.level = 1;
            m.current_hp = 5;
            m.max_hp = 30;
            m.strength = m.dexterity = m.intelligence = 10;
        }
        g.party.characters[0].exp = 100; // levels: one Hail getkey
        g.party.characters[1].exp = 0;
        g.rng.seed(uint32_t(seed));
        rt->render(board, true);
    }
    int64_t now() const { return openu5_host_virtual_clock_us(); }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = now();
        rt->handle(e);
    }
    void key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent e{};
        e.kind = tdeck::RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = code;
        e.modifiers.alt = alt;
        raw(e);
    }
    /** A short Mic/0 tap: press then release at the physical matrix cell. */
    void mic() {
        tdeck::RawInputEvent e{};
        e.kind = tdeck::RawInputKind::Keyboard;
        e.column = tdeck::kMicrophoneKeyColumn;
        e.row = tdeck::kMicrophoneKeyRow;
        e.transition = tdeck::KeyTransition::Pressed;
        raw(e);
        e.transition = tdeck::KeyTransition::Released;
        raw(e);
    }
    void camp() { key('h'); key('1'); key('\r'); key('n'); }
    bool key_wait() const {
        return rt->ui()->mode() == UiMode::KeyWait && rt->ui()->request() == UiRequestId::CampAdvance;
    }
    void sample() {
        timeline.push_back({now(), rt->camp_scene_inverted(), key_wait(),
                            rt->narrative_pacer().active(), batch37_frame_count()});
    }
    /** The device loop: advance 5 ms, render, record -- for `ms`. */
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
            sample();
        }
    }
    bool run_to_key(int64_t limit_ms = 60000) {
        for (int64_t t = 0; t < limit_ms && !key_wait(); t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
            sample();
        }
        return key_wait();
    }
    bool transcript_has(const char *needle) const {
        for (size_t i = 0; i < rt->ui()->transcript_size(); ++i) {
            const auto *b = rt->ui()->transcript_at(i);
            if (b && std::strstr(b->text, needle)) return true;
        }
        return false;
    }
};

/** First sampled instant at which `pred` holds (in the timeline), or -1. */
template <class P> int64_t first(const Run &r, P pred) {
    for (const auto &s : r.timeline)
        if (pred(s)) return s.us;
    return -1;
}
/** Index of the first captured frame presented at or after `us`. */
size_t frame_at_or_after(int64_t us) {
    for (size_t i = 0; i < batch37_frame_count(); ++i)
        if (batch37_frame_time_us(i) >= us) return i;
    return batch37_frame_count();
}
uint16_t px(size_t frame, int col, int row) { return batch37_frame_pixel(frame, col * 16 + 8, row * 16 + 8); }
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    const auto &arena = *pack->combat_map_views[0];
    const auto first_cell = arena.starts[int(CombatDirection::South)][0];

    int seed = -1;
    for (int s = 1; s <= 512; ++s) {
        Run candidate(s, false);
        candidate.camp();
        if (candidate.rt->commands().camp_advance.phase != CommandState::CampAdvance::Phase::None) {
            seed = s;
            break;
        }
    }
    check(seed > 0, "P0: a shipped-pack raw Camp reaches the apparition");
    if (seed < 0) return 1;

    // ---- The unpaced control: Batch 42's synchronous contract, unchanged.
    {
        Run sync(seed, false);
        sync.camp();
        check(sync.key_wait() && batch37_frame_count() >= 6,
              "P1: unpaced harness still presents every Camp frame inside the command (Batch 42)");
    }

    // ---- The paced device path.
    Run h(seed, true);
    const int64_t command_us = h.now();
    const size_t frames_before = batch37_frame_count();
    h.camp();
    const auto &g = h.rt->game();
    check(h.rt->commands().camp_advance.phase == CommandState::CampAdvance::Phase::MemberKey &&
              g.party.characters[0].level == 2,
          "P2: the core has already resolved member one (the pacer only defers presentation)");
    check(!h.key_wait() && h.rt->narrative_pacer().active() && !h.rt->camp_scene_inverted(),
          "P2: the Hail getkey has NOT reached the session and the XOR is not yet up -- the scene "
          "is staged, not played inside the command");

    // Input during the materialize/arpeggio hold: swallowed, all of it.
    h.run(300);
    const auto level_before = g.party.characters[0].level;
    const auto commands_before = h.rt->routed_command_count();
    h.key('x');
    h.key('\r');
    h.mic();
    h.key('m', true); // Alt+M: system menu
    h.key('d', true); // Alt+D: Developer
    h.key('s', true); // Alt+S: save
    h.key('l', true); // Alt+L: load
    for (int i = 0; i < 5; ++i) h.key('x'); // a held/repeating key
    check(!h.key_wait() && h.rt->narrative_pacer().active() &&
              h.rt->ui()->mode() != UiMode::DebugMenu && !h.rt->system_menu_open() &&
              !h.transcript_has("Save complete") && !h.transcript_has("Save failed") &&
              !h.transcript_has("Load complete") && !h.transcript_has("No valid save") &&
              h.rt->commands().camp_advance.phase == CommandState::CampAdvance::Phase::MemberKey &&
              g.party.characters[0].level == level_before,
          "P3: while an original wait is on screen, ordinary keys, Enter, Mic, Alt+M, Alt+D, "
          "save, load and a held key are all swallowed -- no menu, no save/load, no advance");
    check(h.rt->routed_command_count() == commands_before,
          "P3: and not one of them reaches the core as a command -- nothing reads the keyboard "
          "inside tone_sweep, so no getkey stand-in may be synthesized for a Hail not yet shown");

    const bool reached = h.run_to_key();
    check(reached, "P4: the paced scene reaches the Hail getkey by itself");

    const int64_t xor_on = first(h, [](const Sample &s) { return s.inverted; });
    const int64_t xor_off = first(h, [&](const Sample &s) { return xor_on >= 0 && s.us > xor_on && !s.inverted; });
    const int64_t key_at = first(h, [](const Sample &s) { return s.key_wait; });
    const int64_t lead_ms = tone_sweep_ms(kApparitionMaterializeSamples) +
                            tone_sweep_ms(kApparitionArpeggioSamples) + kFizzleFloorMs +
                            run_n_frames_ms(kApparitionWakeFrames) + tone_sweep_ms(kApparitionChimeSamples);
    check(xor_on >= 0 && (xor_on - command_us) / 1000 >= lead_ms,
          "P5: the viewport cannot invert before materialize + arpeggio + figure + wake + chime");
    check(xor_on >= 0 && xor_off > xor_on &&
              (xor_off - xor_on) / 1000 >= int64_t(tone_sweep_ms(kApparitionChordSamples)),
          "P6: the XOR pulse stays on the glass for the whole chord (tone_sweep a2=0xea60, B-class "
          "floor) -- no longer a single-transfer flash");
    check(xor_on >= 0 && xor_off > xor_on &&
              (xor_off - xor_on) / 1000 < int64_t(tone_sweep_ms(kApparitionChordSamples)) + 20,
          "P6: and it comes down on the chord's deadline, within one device loop");
    check(key_at >= 0 && xor_off >= 0 &&
              (key_at - xor_off) / 1000 >= int64_t(run_n_frames_ms(kApparitionRestoreFrames)),
          "P7: the Hail getkey follows the three restore frames, never the XOR directly");

    // What the glass showed: order and inversion, frame by frame.
    const size_t first_inverted = frame_at_or_after(xor_on);
    const size_t first_restored = frame_at_or_after(xor_off);
    check(first_inverted > frames_before && first_inverted < batch37_frame_count() &&
              first_restored > first_inverted && first_restored < batch37_frame_count(),
          "P8: an inverted frame and a restored frame were both actually presented");
    if (first_inverted > frames_before && first_restored < batch37_frame_count()) {
        const size_t pre = first_inverted - 1;
        const uint16_t member = px(pre, first_cell.x, first_cell.y);
        bool held = true;
        for (size_t i = first_inverted; i < first_restored; ++i)
            held = held && px(i, first_cell.x, first_cell.y) == uint16_t(member ^ 0xffff);
        check(member == 0x8888,
              "P8: the frame before the pulse shows member one standing (Fighter 0x148, OUTSUBS 0x086d)");
        check(held, "P8: every frame presented during the chord is the XOR of that frame");
        check(px(first_restored, first_cell.x, first_cell.y) == member,
              "P8: the first frame after the chord restores the pre-XOR scene");
        check(batch37_frame_time_us(first_inverted) - batch37_frame_time_us(pre) >=
                  int64_t(tone_sweep_ms(kApparitionChimeSamples)) * 1000,
              "P9: the standing frame was held for the chime before the XOR replaced it");
    }
    check(g.party.characters[0].current_hp == 60 && batch37_pixel(201, 4) == 5,
          "P10: the member's new HP is resolved in the core but the paced renders still show the "
          "pre-apparition HP on the party panel until the getkey (OUTSUBS 0x07f5, Batch 39/42)");

    // Batch 41 input contract, re-verified on the paced path: once the getkey
    // is really live, a save shortcut is a scene key, not a save.
    h.key('s', true);
    check(!h.transcript_has("Save complete") &&
              h.rt->commands().camp_advance.phase == CommandState::CampAdvance::Phase::KarmaKey,
          "P11: at the live getkey Alt+S acknowledges the Hail (Batch 41) and never saves");
    check(h.rt->narrative_pacer().active() && !h.key_wait(),
          "P11: and the second member is staged again rather than presented at once");
    const bool karma = h.run_to_key();
    check(karma && h.rt->commands().camp_advance.phase == CommandState::CampAdvance::Phase::KarmaKey,
          "P12: the second member's pulse plays out and the scene waits on the karma getkey");
    h.key('\b');
    h.run(50);
    check(h.rt->ui()->mode() == UiMode::Exploration &&
              h.rt->commands().camp_advance.phase == CommandState::CampAdvance::Phase::None &&
              !h.rt->narrative_pacer().active(),
          "P13: the karma key ends the scene and commands resume");

    openu5_host_virtual_clock_us() = -1;
    std::printf("Batch 51 Camp pacing: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
