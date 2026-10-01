// Alpha 4 A4-END1 (targets/tdeck/ALPHA4_UI.md section 7) -- the ending, played,
// through the REAL AlphaRuntime on the REAL tdeck_board.cpp over the fake
// ST7789 (the A4-UI2 harness: bus untimed, the host esp_timer's virtual clock,
// patterned tiles so a pixel names its tile), from the Phase 7E-A Developer
// route into Doom's final room to the scroll and to the stranded room.
//
//   T  the trigger: the absorption mounts ENDGAME.OVL (openu5::EndgameScene),
//      not Batch 53's transcript dump, and the session is in its Ending
//   V  the throne room on the viewport: MISCMAPS.DAT[0x210] as the pack holds
//      it, EGA.DRV fn36's recolor on exactly its 22 tiles, the revive's XOR,
//      the rising gate, and the composed viewport on the panel
//   E  the console text in the overlay's order, from the pack's ENDMSG.DAT
//   W  the waits on the scene clock: 40 frames, the step cadence, the revive
//      holds, timed from the key that ended the last getkey; a getkey never
//      times out; the System Menu stops the clock
//   K  the keys: one key ends one getkey; a held key is one key; the trackball
//      and keys outside a getkey are nothing (no type-ahead); only Y / N at the
//      box questions; Mic and Backspace are keys, never an abort; no saving
//   D  the dissolve: the post-gate frame captured whole, fn34's order at the
//      measured rate, the bands with it, the first page after it
//   P  the six story pages and the scroll on the panel, pixel for pixel, each
//      drawn once
//   M  the music (the patch's selectors 0x15 / 0x18 / 0x1b) and the sounds
//      (footsteps, the revive and orb sweeps); the mutes obey the settings; the
//      timing does not depend on the audio
//   F  the last screens: the scroll forever, the stranded wander; only the
//      System Menu answers; Load and Return to Title leave
//   N  without the pack's ending the parity path (Batch 53's narration) runs
//
// No ENDMSG text is printed by this test (EA's text stays in the pack).
//
//   a4_end1_ending_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> [--dump <dir>]
#include "a4_ui2_harness.h"
#include "../main/misc_records.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/debug_map_picker.h"
#include "openu5/dungeon.h"
#include "openu5/endgame_scene.h"
#include "openu5/hud.h"
#include "openu5/presentation.h"
#include "openu5/quest_state.h"
#include "openu5/save_json.h"
#include "openu5/scene_timing.h"
#include "openu5/ui_session.h"

#include <cctype>
#include <climits>

namespace tdeck {
bool host_memory_save_edit_for_test(bool newest, void (*edit)(openu5::save::Json &game_state));
} // namespace tdeck

using namespace a4_ui2;

namespace {
AudioPackInfo g_audio{};

struct Hero {
    const char *name;
    char cls;
    char status;
};
// The Avatar and three fallen companions: three revivals, four class tiles.
const std::vector<Hero> kHeroes = {{"Avatar", 'A', 'G'}, {"Iolo", 'B', 'D'}, {"Mariah", 'M', 'D'}, {"Dupre", 'F', 'D'}};
const std::vector<Hero> kAlone = {{"Avatar", 'A', 'G'}};

constexpr int64_t kStepMs = run_n_frames_ms(2) + tone_sweep_ms(555) + run_n_frames_ms(3);
constexpr int64_t kReviveMs = tone_sweep_ms(0x9c40);

// --- the recording audio backend, with the parsed song length the chain reads --
struct Backend final : AudioBackend {
    struct Call {
        std::string what;
        int value = 0;
        int32_t param = 0;
        int64_t us = 0;
    };
    std::vector<Call> calls;
    uint32_t reunion_ms = 0;
    bool play_sfx(const SfxRequest &r) override {
        calls.push_back({"sfx", int(r.id), r.param, Run::now()});
        return true;
    }
    void stop_sfx() override { calls.push_back({"stop_sfx", 0, 0, Run::now()}); }
    bool start_music(MusicSong s, uint16_t) override {
        calls.push_back({"music", int(s), 0, Run::now()});
        return true;
    }
    void stop_music() override { calls.push_back({"stop_music", 0, 0, Run::now()}); }
    void set_gain(AudioChannel, uint16_t) override {}
    uint32_t music_length_ms(MusicSong s) const override { return s == MusicSong::Reunion ? reunion_ms : 0; }
    size_t count(const char *what, int64_t from, int64_t to = INT64_MAX) const {
        size_t k = 0;
        for (const auto &c : calls) k += c.what == what && c.us >= from && c.us < to;
        return k;
    }
    size_t sfx(SfxId id, int param, int64_t from, int64_t to = INT64_MAX) const {
        size_t k = 0;
        for (const auto &c : calls)
            k += c.what == "sfx" && c.value == int(id) && (param < 0 || c.param == param) && c.us >= from && c.us < to;
        return k;
    }
    /** The songs started since `from`, in order. */
    std::vector<MusicSong> songs(int64_t from) const {
        std::vector<MusicSong> out;
        for (const auto &c : calls)
            if (c.what == "music" && c.us >= from) out.push_back(MusicSong(c.value));
        return out;
    }
    int64_t first_start(MusicSong s, int64_t from) const {
        for (const auto &c : calls)
            if (c.what == "music" && c.value == int(s) && c.us >= from) return c.us;
        return -1;
    }
};

// --- the pack -----------------------------------------------------------------
std::string record(int i) {
    const char *r = tdeck::misc_text_record({pack->end_text_offsets, pack->end_text_records, pack->end_text_record_count}, i);
    return r ? r : "";
}
std::string squeeze(const std::string &s) {
    std::string o;
    for (const char c : s)
        if (!std::isspace(static_cast<unsigned char>(c))) o += c;
    return o;
}

// --- the runtime ----------------------------------------------------------------
CommandContext &ctx(Run &h) { return h.rt->command_context_for_test(); }
const EndgameScene *sc(Run &h) { return h.rt->endgame_scene(); }
bool live(Run &h) { return sc(h) && sc(h)->active(); }
void frame(Run &h, int ms = 5) {
    openu5_host_virtual_clock_us() += int64_t(ms) * 1000;
    h.render();
}
template <class P> bool until(Run &h, P done, int64_t max_ms, int step = 5) {
    for (int64_t t = 0; !done(); t += step) {
        if (t > max_ms) return false;
        frame(h, step);
    }
    return true;
}
bool at_getkey(Run &h) {
    const auto *s = sc(h);
    return s && s->active() && !s->ready() && s->wait() == EndgameWait::Key;
}
bool at_yes_no(Run &h) {
    const auto *s = sc(h);
    return s && s->active() && !s->ready() && s->wait() == EndgameWait::YesNo;
}
void key_release(Run &h, uint8_t code) {
    openu5_host_virtual_clock_us() += 100000;
    tdeck::RawInputEvent e{};
    e.kind = RawInputKind::Keyboard;
    e.transition = tdeck::KeyTransition::Released;
    e.code = e.base_code = code;
    h.raw(e);
    h.render();
}
void key_press_only(Run &h, uint8_t code) {
    openu5_host_virtual_clock_us() += 100000;
    tdeck::RawInputEvent e{};
    e.kind = RawInputKind::Keyboard;
    e.transition = tdeck::KeyTransition::Pressed;
    e.code = e.base_code = code;
    h.raw(e);
    h.render();
}

uint32_t last_seq(Run &h) {
    const size_t n = h.rt->ui()->transcript_size();
    const auto *b = n ? h.rt->ui()->transcript_at(n - 1) : nullptr;
    return b ? b->sequence : 0;
}
/** The transcript written after `seq`, joined; Message channel only unless `all`. */
std::string since(Run &h, uint32_t seq, bool all = false) {
    std::string s;
    for (size_t i = 0; i < h.rt->ui()->transcript_size(); ++i) {
        const auto *b = h.rt->ui()->transcript_at(i);
        if (!b || b->sequence <= seq || (!all && b->channel != UiTextChannel::Message)) continue;
        if (!s.empty() && !(b->flags & UiTextContinuesBefore)) s += '\n';
        s += b->text;
    }
    return s;
}
/** A block after `seq` that is exactly the core's internal ending token ("victory" / "stranded", D-57). */
bool token_line(Run &h, uint32_t seq) {
    for (size_t i = 0; i < h.rt->ui()->transcript_size(); ++i) {
        const auto *b = h.rt->ui()->transcript_at(i);
        if (b && b->sequence > seq && (!std::strcmp(b->text, "victory") || !std::strcmp(b->text, "stranded"))) return true;
    }
    return false;
}
size_t occurrences(const std::string &hay, const std::string &needle) {
    size_t k = 0;
    for (size_t at = hay.find(needle); !needle.empty() && at != std::string::npos; at = hay.find(needle, at + 1)) ++k;
    return k;
}

// Phase 7E-A: Developer -> Preset: Endgame, the party, Doom Level 6 (4,7),
// face East, one step into the pit (batch53a_ending_terminal's route).
bool to_final_room(Run &h, bool box, const std::vector<Hero> &heroes, bool save_in_arena) {
    apply_debug_preset(ctx(h), DebugPreset::Endgame);
    auto &g = h.rt->game();
    g.wooden_box = box;
    g.party.character_count = g.party.party_size = uint8_t(heroes.size());
    for (size_t i = 0; i < heroes.size(); ++i) {
        auto &m = g.party.characters[i];
        std::snprintf(m.name, sizeof(m.name), "%s", heroes[i].name);
        m.character_class = uint8_t(heroes[i].cls);
        m.status = uint8_t(heroes[i].status);
        m.party_status = 0;
        m.max_hp = 240;
        m.current_hp = heroes[i].status == 'D' ? 0 : 240;
    }
    g.party.active_character = 0;
    DebugTeleportRequest r;
    r.kind = DebugDestinationKind::Dungeon;
    r.location = 40;
    r.floor = 6;
    r.x = 4;
    r.y = 7;
    r.standard_entry = false;
    if (apply_debug_teleport(ctx(h), r).status != DebugTeleportStatus::Applied) return false;
    h.rt->dungeon_state_for_test().pos.facing = DungeonFacing::East;
    h.up();
    const bool in_room = ctx(h).combat && h.rt->combat_state().initialized;
    if (in_room && save_in_arena) h.key('s', true); // a pre-ending save, for Load
    return in_room;
}
// North x4 in the arena, one input per enemy beat, then idle beats until the
// absorption mounts the ending. The virtual time it mounted, or -1.
int64_t walk_to_soul(Run &h) {
    int64_t started = -1;
    for (int i = 0; i < 4 && ctx(h).combat && started < 0; ++i) {
        openu5_host_virtual_clock_us() += 500000;
        h.up();
        if (live(h)) started = Run::now();
    }
    for (int i = 0; i < 6 && started < 0; ++i) {
        openu5_host_virtual_clock_us() += 500000;
        h.raw(tdeck::RawInputEvent{});
        if (live(h)) started = Run::now();
        h.render();
    }
    return started;
}
/** Answers the box questions with `answers` in turn and presses Space at every getkey, to the last screen. */
bool drive_to_end(Run &h, const char *answers) {
    for (int guard = 0; guard < 200; ++guard) {
        const auto *s = sc(h);
        if (!s || !s->active()) return false;
        if (s->phase() == EndgamePhase::Scroll || s->phase() == EndgamePhase::Stranded) return true;
        if (at_yes_no(h)) h.key(uint8_t(*answers ? *answers++ : 'y'));
        else if (at_getkey(h)) h.key(' ');
        else until(h, [&] { return at_getkey(h) || at_yes_no(h) || sc(h)->phase() == EndgamePhase::Scroll ||
                                   sc(h)->phase() == EndgamePhase::Stranded || sc(h)->wait() == EndgameWait::Dissolve; },
                   120000);
        if (sc(h)->wait() == EndgameWait::Dissolve) until(h, [&] { return sc(h)->wait() != EndgameWait::Dissolve; }, 10000);
    }
    return false;
}

// --- the panel ----------------------------------------------------------------
// The patterned fixture (alpha_runtime_host_fixture.cpp): byte b of tile t is
// t*37 + b*11 + (b>>3)*7, the high nibble the left pixel; index i is i*0x1111.
uint8_t pattern(int tile, int x, int y) {
    const int b = y * 8 + x / 2;
    const auto byte = uint8_t(tile * 37 + b * 11 + (b >> 3) * 7);
    return (x & 1) ? uint8_t(byte & 15) : uint8_t(byte >> 4);
}
uint16_t pal(int i) { return uint16_t(i * 0x1111); }
/** Pixels of composed-viewport cell (col,row) that are not `index(x,y)`. */
template <class F> int cell_mismatch(const uint16_t *vp, int col, int row, F index) {
    int bad = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) bad += vp[(row * 16 + y) * 176 + col * 16 + x] != pal(index(x, y));
    return bad;
}
// EGA.DRV 0x2d4d and the 22 tiles 0x2cb0-0x2d33 recolour, held here apart from
// the production tables so that a wrong entry there is never also the oracle.
constexpr uint8_t kLut[16] = {0x0, 0x5, 0x4, 0x4, 0x2, 0x1, 0x2, 0x7, 0x8, 0xc, 0xc, 0xc, 0xa, 0x9, 0xe, 0xf};
constexpr int kListed[22] = {0x44, 0x5c, 0x5d, 0x90, 0x92, 0x94, 0x96, 0x9b, 0xab, 0xac, 0xaf,
                             0xb0, 0xb1, 0xbf, 0xdc, 0x108, 0x10e, 0x11a, 0x138, 0x139, 0x13a, 0x13b};
bool listed(int tile) {
    for (const int t : kListed)
        if (t == tile) return true;
    return false;
}
uint8_t recolored(int tile, int x, int y) {
    const uint8_t i = pattern(tile, x, y);
    return listed(tile) ? kLut[i] : i;
}
/** The room's static, empty cells against the pattern, recolored exactly when fn36 lists the tile. */
struct RoomCheck {
    int cells = 0, recolor_cells = 0, plain_cells = 0, bad = 0, first_static = -1;
};
RoomCheck check_room(Run &h, uint8_t xor_mask = 0) {
    RoomCheck r;
    PresentationSnapshot snap;
    OriginalRng rng(1);
    sc(h)->compose(snap, rng);
    const uint16_t *vp = h.rt->composed_viewport();
    for (int i = 0; i < kEndgameRoomCells && vp; ++i) {
        const int tile = snap.tiles[i];
        const bool gate = snap.gate_rows && i == snap.gate_y * kEndgameRoomSide + snap.gate_x;
        if (tile < 0 || tile >= 0x100 || gate || tile_animation_kind(tile) != TileAnimationKind::Static) continue;
        ++r.cells;
        (listed(tile) ? r.recolor_cells : r.plain_cells) += 1;
        if (r.first_static < 0) r.first_static = i;
        r.bad += cell_mismatch(vp, i % kEndgameRoomSide, i / kEndgameRoomSide,
                               [&](int x, int y) { return recolored(tile, x, y) ^ xor_mask; });
    }
    return r;
}
int viewport_on_panel_mismatch(Run &h) {
    const uint16_t *vp = h.rt->composed_viewport();
    if (!vp) return -1;
    int bad = 0;
    // Rows 0-8 and 167-175 lie under the HUD's two bands (A3-04F's comparison).
    for (int y = 9; y < 167; ++y)
        for (int x = 0; x < 176; ++x) bad += px(kHudViewportX + x, kHudViewportY + y) != vp[y * 176 + x];
    return bad;
}
uint8_t page_index(int page, int x, int y) {
    const uint8_t pair = pack->endgame_pages[size_t(page) * 32000 + size_t(y) * 160 + size_t(x / 2)];
    return (x & 1) ? uint8_t(pair & 15) : uint8_t(pair >> 4);
}
/** The panel against page `page` at y 20 on black, with the scroll's cells over it when given. */
int page_mismatch(int page, const EndgameScrollCell *cells) {
    int bad = 0;
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x) {
            const int py = y - 20;
            uint16_t want = kBlack;
            if (py >= 0 && py < 200) {
                int i = page_index(page, x, py);
                if (cells) {
                    const auto &c = cells[(py / 8) * kEndgameScrollCols + x / 8];
                    if (c.flags & kEndgameCellPrinted) {
                        const uint8_t *font = (c.flags & kEndgameCellRunes) ? pack->runes_font : pack->ibm_font;
                        const bool ink = (font[size_t(c.ch) * 8 + size_t(py & 7)] >> (7 - (x & 7))) & 1;
                        i = ink != ((c.flags & kEndgameCellInverse) != 0) ? 15 : 0;
                    }
                }
                want = pal(i);
            }
            bad += px(x, y) != want;
        }
    return bad;
}
std::vector<uint16_t> gram_copy() { return std::vector<uint16_t>(bus::gram(), bus::gram() + 320 * 240); }
/** The panel `produced` pixels into the fizzle: fn34's first pixels black, the rest as captured. */
int fizzle_mismatch(const std::vector<uint16_t> &captured, uint32_t produced, int &black_count) {
    std::vector<uint8_t> black(320 * 240, 0);
    EndgameFizzle picture, bands(320, 40);
    uint16_t x = 0, y = 0;
    for (uint32_t i = 0; i < produced && picture.next(x, y); ++i) black[size_t(y + 20) * 320 + x] = 1;
    const uint64_t band_target = uint64_t(picture.produced()) * bands.total() / picture.total();
    while (bands.produced() < band_target && bands.next(x, y)) black[size_t(y < 20 ? y : y + 200) * 320 + x] = 1;
    int bad = 0;
    black_count = 0;
    for (int i = 0; i < 320 * 240; ++i) {
        black_count += black[size_t(i)];
        bad += bus::gram()[i] != (black[size_t(i)] ? kBlack : captured[size_t(i)]);
    }
    return bad;
}

/** Steps frame by frame until `slot` reaches (col,row) or moves `moves` times; the times it moved. */
std::vector<int64_t> watch_steps(Run &h, int slot, int moves, int64_t max_ms) {
    std::vector<int64_t> at;
    int col = sc(h)->actor(slot).col, row = sc(h)->actor(slot).row;
    until(h, [&] {
        const auto &a = sc(h)->actor(slot);
        if (a.col != col || a.row != row) { at.push_back(Run::now()); col = a.col; row = a.row; }
        return int(at.size()) >= moves;
    }, max_ms);
    return at;
}

// ===========================================================================
// The victory: the box, Yes, the gate, the dissolve, six pages, the scroll.
// ===========================================================================
struct VictoryTimes {
    int64_t greet_ms = -1; // ENDGAME_START -> the greeting's getkey
};

VictoryTimes test_victory() {
    VictoryTimes out;
    std::printf("T  the 7E-A route: the absorption mounts ENDGAME.OVL\n");
    Run h({{"Avatar", 'G', 240}}, false, nullptr, true);
    Backend audio;
    audio.reunion_ms = 30000; // the backend's parsed Joyous Reunion, for the 0x15 chain
    h.rt->configure_audio(g_audio, &audio);
    const bool room = to_final_room(h, true, kHeroes, true);
    const uint32_t mark = last_seq(h);
    const int64_t start = walk_to_soul(h);
    const uint32_t start_seq = last_seq(h);
    check(room && start >= 0 && live(h) && sc(h)->phase() == EndgamePhase::Throne,
          "T1 Doom L6 (4,7) east into the pit, north x4: absorbed, and EndgameScene is mounted in its throne room");
    check(since(h, mark, true).find("is absorbed!") != std::string::npos && quest_flag(h.rt->game().quest, QuestFlag::GameWon) &&
              h.rt->ui()->mode() == UiMode::Ending && !ctx(h).combat,
          "T2 game-won, the arena torn down, the session in its terminal Ending (Batch 53A)");
    bool none = true;
    const std::string at_start = squeeze(since(h, mark, true));
    for (int i = 0; i < 11; ++i) none = none && !record(i).empty() && at_start.find(squeeze(record(i))) == std::string::npos;
    check(none && !token_line(h, mark),
          "T3 Batch 53's one-shot dump is gone: at the absorption the console holds no ENDMSG record and no internal "
          "ending token (D-57)");
    if (!live(h)) return out;

    // V: the throne room on the viewport.
    h.render(true);
    const bool same_room = std::memcmp(sc(h)->room(), pack->endgame_room, 121) == 0;
    const RoomCheck rc = check_room(h);
    check(same_room && rc.cells >= 40 && rc.recolor_cells > 0 && rc.plain_cells > 0 && rc.bad == 0,
          "V1 the viewport is MISCMAPS.DAT[0x210] from the pack: " + n(rc.cells) + " static cells, " + n(rc.recolor_cells) +
              " through fn36's recolor LUT and " + n(rc.plain_cells) + " untouched, " + n(rc.bad) + " pixels off");
    check(viewport_on_panel_mismatch(h) == 0,
          "V2 the composed throne room is what the panel shows (the HUD viewport, between its two bands)");
    check(sc(h)->actor(31).active && sc(h)->actor(31).col == 5 && sc(h)->actor(31).row == 8 && sc(h)->actor(31).tile == 0x7c,
          "V3 Lord British (slot 31, tile 0x7c) stands at (5,8)");
    dump("end1-throne");

    // W: 40 frames, then Lord British's five steps north -- with three keys
    // pressed between his second and third step, while no getkey is open.
    auto lb = watch_steps(h, 31, 2, 6000);
    for (uint8_t k : {uint8_t(' '), uint8_t('\r'), uint8_t('y')}) h.key(k, false, false, 0);
    const auto rest = watch_steps(h, 31, 3, 6000);
    lb.insert(lb.end(), rest.begin(), rest.end());
    bool cadence = lb.size() == 5 && lb[0] - start >= 2200000 && lb[0] - start <= 2205000;
    // Each wait ends on its due time exactly; a 5 ms frame sees it up to 5 ms late.
    for (size_t i = 1; i < lb.size(); ++i) cadence = cadence && lb[i] - lb[i - 1] >= kStepMs * 1000 - 5000 && lb[i] - lb[i - 1] <= kStepMs * 1000 + 5000;
    check(cadence && sc(h)->actor(31).row == 3,
          "W1 Lord British waits 40 frames (" + n(lb.empty() ? -1 : (lb[0] - start) / 1000) + " ms), then steps to (5,3) every " +
              n(kStepMs) + " ms (run_n_frames(2), the footstep, run_n_frames(3))");

    // The three revivals: the XOR, the sweep that holds it, the roster.
    bool revives = true, inverted_ok = false;
    int64_t revive_sweeps = 0;
    for (int member = 1; member <= 3; ++member) {
        if (!until(h, [&] { return sc(h)->inverted(); }, 20000)) { revives = false; break; }
        const int64_t rise = Run::now();
        const bool dead_during = h.rt->game().party.characters[member].status == 'D';
        frame(h);
        if (member == 1) {
            const RoomCheck x = check_room(h, 15);
            inverted_ok = x.cells > 0 && x.bad == 0;
            dump("end1-revive-xor");
        }
        until(h, [&] { return !sc(h)->inverted(); }, 5000);
        const int64_t fall = Run::now();
        const auto &ch = h.rt->game().party.characters[member];
        revive_sweeps += int64_t(audio.sfx(SfxId::EndgameOrb, 1, rise - 1, rise + 1));
        revives = revives && dead_during && ch.status == 'G' && ch.current_hp == ch.max_hp && fall - rise >= kReviveMs * 1000 - 5000 &&
                  fall - rise <= kReviveMs * 1000 + 5000;
    }
    check(revives, "W2 each fallen member: \" lives!\", the viewport XOR held " + n(kReviveMs) +
                       " ms (the sweep), then status G at full HP (0x0792's redraw)");
    check(inverted_ok, "V4 during the hold the room is drawn XOR 15 ([0x13b0], ENDGAME 0x0767-0x0778)");
    check(revive_sweeps == 3, "M2 the revive sweep (endgame-orb, param 1) at each \" lives!\" (" + n(revive_sweeps) + "/3)");

    // The greeting's getkey.
    const bool greeted = until(h, [&] { return at_getkey(h); }, 30000);
    out.greet_ms = (Run::now() - start) / 1000;
    frame(h);
    check(greeted && at_getkey(h) && squeeze(since(h, start_seq)).find("Yes") == std::string::npos,
          "K0 Space, Enter and Y pressed while he walked were nothing: no step hurried (W1), none kept to end the greeting "
          "or answer the box (no type-ahead)");
    size_t lb_steps = 0;
    for (const auto t : lb) lb_steps += audio.sfx(SfxId::MoveStep, -1, t + run_n_frames_ms(2) * 1000 - 5000, t + run_n_frames_ms(2) * 1000 + 5000);
    check(lb_steps == 5, "M1 a footstep (move-step) two frames into each of Lord British's steps (" + n(long(lb_steps)) + "/5)");
    const std::string greet = squeeze(since(h, start_seq));
    std::string want = squeeze("\nIolo lives!\n\nMariah lives!\n\nDupre lives!\n" + record(0) + "Avatar" + "!\"\n\n");
    const bool victory_prefix = greet.rfind("VICTORY!", 0) == 0;
    check(at_getkey(h) && (greet == want || greet == "VICTORY!" + want),
          std::string("E1 to the greeting, in order: three \" lives!\", ENDMSG 0x00, the Avatar's name, \"!\\\"\" -- nothing else") +
              (victory_prefix ? " (the arena's own VICTORY! precedes it: ledger D-72)" : ""));
    // (The Avatar's last step in the arena sounds at the absorption's own instant.)
    const size_t footsteps = audio.sfx(SfxId::MoveStep, -1, start + 1, Run::now() + 1);
    check(footsteps == 21, "M3 21 footsteps to the greeting: his 5 and the lineup's 4 + 4 + 4 + 4 from the mirror (" +
                               n(long(footsteps)) + ")");
    check(std::string(h.rt->status_overlay()) == "Enter: continue", "K1 a getkey wears the device's cue (\"Enter: continue\")");

    // A getkey never times out; the trackball is no key.
    const uint32_t seq = last_seq(h);
    for (auto k : {RawInputKind::TrackballUp, RawInputKind::TrackballDown, RawInputKind::TrackballLeft, RawInputKind::TrackballRight}) h.ball(k);
    const int64_t wait_from = Run::now();
    until(h, [] { return false; }, 120000, 50);
    check(at_getkey(h) && last_seq(h) == seq && audio.count("sfx", wait_from, INT64_MAX) == 0,
          "W3 two minutes and four trackball rolls later the greeting still waits; nothing printed, nothing sounded");
    const int64_t chain = audio.first_start(MusicSong::RuleBritannia, start);
    check(audio.first_start(MusicSong::Reunion, start) == start && chain >= start + 30000000 && chain <= start + 30050000,
          "M4 Joyous Reunion from the absorption (0x15); when it ends Rule Britannia follows by itself, with no key (" +
              n((chain - start) / 1000) + " ms)");

    // A held key is one key: the press ends the getkey; holding it and letting go do nothing more.
    key_press_only(h, 'x');
    const bool asked = at_yes_no(h);
    frame(h, 2000);
    key_release(h, 'x');
    check(asked && at_yes_no(h) && occurrences(squeeze(since(h, start_seq)), squeeze(record(1))) == 1,
          "K2 one press ends the greeting; ENDMSG 0x01 asks once; holding the key 2 s and releasing it ends nothing");
    // Only Y / N leave the box question; every other key is read again.
    const uint32_t q = last_seq(h);
    h.key(' ');
    h.key('\r');
    h.key('a');
    h.mic();
    h.key('\b');
    check(at_yes_no(h) && last_seq(h) == q && std::string(h.rt->status_overlay()) == "Y / N" && live(h),
          "K3 Space, Enter, A, Mic and Backspace leave the box question waiting (0x0852 reads again); its cue is \"Y / N\"");
    // The answer, 30 s later: what follows is timed from the key.
    frame(h, 30000);
    h.key('y');
    const int64_t answered = Run::now();
    const auto steps = watch_steps(h, 0, 2, 3000);
    check(steps.size() == 2 && steps[0] - answered >= run_n_frames_ms(8) * 1000 && steps[0] - answered <= run_n_frames_ms(8) * 1000 + 5000 &&
              steps[1] - steps[0] >= kStepMs * 1000 - 5000 && steps[1] - steps[0] <= kStepMs * 1000 + 5000,
          "W4 answered after 30 s: the Avatar's first step comes 8 frames after the key (" +
              n(steps.empty() ? -1 : (steps[0] - answered) / 1000) + " ms), the next one step later -- not at once");

    until(h, [&] { return at_getkey(h); }, 30000);
    check(sc(h)->actor(6).active && sc(h)->actor(6).tile == 0x0e && sc(h)->actor(6).col == 5 && sc(h)->actor(6).row == 4 &&
              sc(h)->victory(),
          "V5 Yes with the box: the Avatar steps forward and back, the box (0x0e) lies at (5,4)");
    // Mic and Backspace at a getkey are keys (ESC / BS at 0x266c), not an abort.
    h.mic();
    const bool mic_page = at_getkey(h) && live(h) && sc(h)->phase() == EndgamePhase::Throne && !h.rt->frontend_open() &&
                          !h.rt->system_menu_open();
    h.key('\b');
    check(mic_page && at_getkey(h) && live(h) && !h.rt->frontend_open(),
          "K4 Mic and Backspace each end one getkey of Lord British's speech and the ending goes on");
    h.key('s', true);
    check(since(h, start_seq, true).find("Save unavailable") != std::string::npos &&
              since(h, start_seq, true).find("Save complete") == std::string::npos,
          "K5 Alt+S during the ending saves nothing (\"Save unavailable\")");
    while (at_getkey(h) && occurrences(squeeze(since(h, start_seq)), squeeze(record(9))) == 0) h.key('\r');
    check(at_getkey(h) && sc(h)->actor(6).tile == 0x08, "V6 \"FOLLOW!\" (ENDMSG 0x09): the orb (0x08) in the box's place");

    const std::string text = squeeze(since(h, start_seq));
    std::string all = want + squeeze(record(1)) + "Yes" + squeeze(record(3)) + "Hesays:";
    for (int i = 4; i <= 9; ++i) all += squeeze(record(i));
    check(text == all || text == "VICTORY!" + all,
          "E2 the whole console in the overlay's order: ... ENDMSG 0x01, \"Yes\", 0x03, \"He says:\", 0x04-0x09 -- once each");

    // The orb, the gate.
    const int64_t orb_key = Run::now() + 100000;
    h.key(' ');
    check(audio.sfx(SfxId::EndgameOrb, 0, orb_key - 1, orb_key + 1) == 1, "M5 the orb's sweep (endgame-orb, param 0) at the key");
    until(h, [&] { return sc(h)->gate_stage() == 5 && !sc(h)->ready(); }, 10000);
    const int stage = sc(h)->gate_stage();
    frame(h);
    const uint16_t *vp = h.rt->composed_viewport();
    const int gate_bad = cell_mismatch(vp, 5, 4, [&](int x, int y) {
        return y >= 16 - stage ? recolored(0xdc, x, y - (16 - stage)) : recolored(0x44, x, y);
    });
    check(stage == 5 && gate_bad == 0,
          "V7 the gate rises: at stage 5 the cell is the floor with its bottom 5 rows the top of 0xdc, both recolored (" +
              n(gate_bad) + " px off)");
    dump("end1-gate");
    // The System Menu stops the scene clock.
    const int64_t menu_at = Run::now();
    h.key('m', true);
    const bool menu = h.rt->system_menu_open();
    frame(h, 5000);
    const int frozen = sc(h)->gate_stage();
    h.key('m', true);
    until(h, [&] { return sc(h)->gate_stage() != frozen; }, 1000);
    check(menu && frozen == stage && !h.rt->system_menu_open() && sc(h)->gate_stage() == stage + 1 &&
              Run::now() - menu_at > 5000000,
          "W5 the System Menu over the ending stops its clock (the gate holds 5 s at stage 5) and closing it resumes");

    // D: the dissolve.
    const bool capture = until(h, [&] { return h.board.endgame_capture_for_test() != nullptr; }, 30000, 5);
    const int64_t captured_at = Run::now();
    std::vector<uint16_t> captured = gram_copy();
    int capture_bad = -1;
    if (const uint16_t *c = h.board.endgame_capture_for_test()) {
        capture_bad = 0;
        for (int i = 0; i < 320 * 240; ++i) capture_bad += c[i] != captured[size_t(i)];
    }
    const RoomCheck after = check_room(h);
    check(capture && sc(h)->phase() == EndgamePhase::Dissolve && sc(h)->gate_cell_restored() && capture_bad == 0 &&
              h.rt->endgame_fizzled() == 0 && after.bad == 0,
          "D1 the gate closed and the floor blitted back (0x0a45): that frame is drawn whole and the Board's copy of it is "
          "the panel exactly (" + n(capture_bad) + " px off)");
    dump("end1-captured");
    frame(h, 1000);
    int blacks = 0;
    const uint32_t produced = h.rt->endgame_fizzled();
    const int fizzle_bad = fizzle_mismatch(captured, produced, blacks);
    check(produced == 25601 && fizzle_bad == 0,
          "D2 one second in: " + n(produced) + " of 64,000 pixels (25,600/s) in fn34's order (taps 0xB400 from (1,0)), "
          "the bands' 320x40 with them, everything else still the captured frame (" + n(fizzle_bad) + " px off)");
    dump("end1-dissolve");
    until(h, [&] { return sc(h)->phase() != EndgamePhase::Dissolve; }, 5000);
    const int64_t dissolved = Run::now() - captured_at;
    check(sc(h)->phase() == EndgamePhase::Story && sc(h)->story_page() == 0 && dissolved >= 2495000 && dissolved <= 2500000 &&
              h.board.endgame_capture_for_test() == nullptr,
          "D3 the last pixel at " + n(dissolved / 1000) + " ms ends the dissolve; page 0 follows; the copy is released");

    // P: the six pages, one key each, each drawn once.
    bool pages = true, once = true, music = true;
    std::string page_report;
    for (int page = 0; page < kEndgameStoryPages; ++page) {
        const int bad = page_mismatch(page, nullptr);
        page_report += " " + n(bad);
        pages = pages && sc(h)->phase() == EndgamePhase::Story && sc(h)->story_page() == page && at_getkey(h) && bad == 0;
        const auto bytes = bus::stats().pixel_bytes;
        frame(h, 2000);
        once = once && bus::stats().pixel_bytes == bytes;
        const MusicSong song = h.rt->audio().current_song();
        music = music && song == (page < 3 ? MusicSong::Stones : MusicSong::LadyNan);
        dump(("end1-page" + n(page)).c_str());
        h.key(' ');
    }
    check(pages, "P1 the six pages (The Homecoming ... the gate) on the panel pixel for pixel, picture at y 20 on black "
                 "(mismatches:" + page_report + ")");
    check(once, "P2 a page is drawn once and left alone while it waits (no pixel sent in 2 s)");
    check(music, "M6 the patch's 0x18 per page: Stones for pages 0-2, Dream of Lady Nan for 3-5");

    // The scroll.
    const auto *scene = sc(h);
    const int scroll_bad = scene ? page_mismatch(kEndgameStoryPages, scene->scroll()) : -1;
    check(since(h, mark, true).find("The quest is complete.") == std::string::npos && !token_line(h, mark),
          "F0 the device line (A-15) never comes over the victory ending, nor any ending token");
    check(scene && scene->phase() == EndgamePhase::Scroll && scene->wait() == EndgameWait::Forever && scroll_bad == 0,
          "P3 the scroll: ENDSC.16 with endgame_datestamp's 40x25 cells over it (IBM.CH, RUNES.CH, reverse video), "
          "pixel for pixel (" + n(scroll_bad) + " px off)");
    check(h.rt->audio().current_song() == MusicSong::RuleBritannia, "M7 the scroll's 0x1b: Rule Britannia");
    dump("end1-scroll");

    // F: forever.
    const auto bytes = bus::stats().pixel_bytes;
    const uint32_t scroll_seq = last_seq(h);
    until(h, [] { return false; }, 600000, 100);
    check(live(h) && sc(h)->phase() == EndgamePhase::Scroll && bus::stats().pixel_bytes == bytes && last_seq(h) == scroll_seq,
          "F1 ten minutes later the scroll is still up, untouched (the 1988 loop at 0x04f9 never returns)");
    const auto routed = h.rt->routed_command_count();
    for (uint8_t k : {uint8_t(' '), uint8_t('\r'), uint8_t('q'), uint8_t('\b')}) h.key(k);
    h.mic();
    h.up();
    h.down();
    check(page_mismatch(kEndgameStoryPages, sc(h)->scroll()) == 0 && h.rt->routed_command_count() == routed &&
              last_seq(h) == scroll_seq && live(h),
          "F2 Space, Enter, Q, Backspace, Mic and the trackball change nothing on the scroll");
    h.key('m', true);
    const bool over = h.rt->system_menu_open();
    h.key('m', true);
    check(over && !h.rt->system_menu_open() && page_mismatch(kEndgameStoryPages, sc(h)->scroll()) == 0,
          "F3 Alt+M opens the System Menu over the scroll; closing it puts the scroll back");
    // A won save (Batch 53 allowed saving after the ending) loaded from the scroll:
    // the overlay stops and the loaded game is the bare Ending, not the old scene over it.
    tdeck::host_memory_save_edit_for_test(true, [](save::Json &gs) { gs["questFlags"]["game-won"] = save::Json(true); });
    const uint32_t won_seq = last_seq(h);
    h.key('l', true);
    check(!live(h) && h.rt->ui()->mode() == UiMode::Ending && quest_flag(h.rt->game().quest, QuestFlag::GameWon) &&
              occurrences(since(h, won_seq, true), "The quest is complete.") == 1 && page_mismatch(kEndgameStoryPages, nullptr) > 0,
          "F4 Alt+L of a won save on the scroll: the overlay stops; the loaded game is the bare Ending (A-15), not the scroll");
    tdeck::host_memory_save_edit_for_test(true, [](save::Json &gs) { gs["questFlags"]["game-won"] = save::Json(false); });
    h.key('l', true);
    const auto before = h.rt->routed_command_count();
    const bool left = !live(h) && h.rt->ui()->mode() != UiMode::Ending && !quest_flag(h.rt->game().quest, QuestFlag::GameWon);
    h.up();
    check(left && h.rt->routed_command_count() > before,
          "F4b Alt+L of the pre-ending save: the next key plays again");
    return out;
}

// ===========================================================================
// The stranded endings: No, No (with the box), and Yes without the box.
// ===========================================================================
void test_stranded() {
    std::printf("S  No, No: \"I see...\", the chairs and the bed, the endless wander\n");
    Run h({{"Avatar", 'G', 240}}, false, nullptr, true);
    Backend audio;
    audio.reunion_ms = 60000;
    h.rt->configure_audio(g_audio, &audio);
    const bool room = to_final_room(h, true, kHeroes, false);
    const uint32_t mark = last_seq(h);
    const int64_t start = walk_to_soul(h);
    const uint32_t start_seq = last_seq(h);
    until(h, [&] { return at_getkey(h); }, 30000);
    h.key(' ');
    h.key('n');
    const bool second = at_yes_no(h);
    h.key('N');
    until(h, [&] { return sc(h)->phase() == EndgamePhase::Stranded; }, 60000);
    const int64_t wander_at = Run::now();
    const std::string text = squeeze(since(h, start_seq));
    const std::string want = squeeze("\nIolo lives!\n\nMariah lives!\n\nDupre lives!\n" + record(0) + "Avatar" + "!\"\n\n" +
                                     record(1) + "No\n\n" + record(2) + "No\n\n" + "\"I see...\n" + record(10));
    check(room && start >= 0 && second && (text == want || text == "VICTORY!" + want) && text.find("FOLLOW!") == std::string::npos,
          "S1 No, then No to ENDMSG 0x02: \"No\" twice, \"I see...\", ENDMSG 0x0a -- in order, once each");
    const auto &a2 = sc(h)->actor(2), &lb = sc(h)->actor(31), &a0 = sc(h)->actor(0);
    check(sc(h)->phase() == EndgamePhase::Stranded && a2.col == 8 && a2.row == 6 && lb.col == 4 && lb.row == 1 && a0.col == 8 &&
              a0.row == 4,
          "S2 member 2 to the table (8,6), Lord British to his bed (4,1), the Avatar to the other chair (8,4)");
    check(occurrences(since(h, mark, true), "The quest is complete. Alt+M: System Menu") == 1 && !token_line(h, mark),
          "S3 once the wander begins the device line says which key still answers (A-15), once; no ending token");
    check(wander_at < start + 60000000 && h.rt->audio().current_song() == MusicSong::Reunion,
          "M8 the wander's 0x1b waits for a Joyous Reunion still playing (" + n((wander_at - start) / 1000) + " ms in)");
    std::vector<std::pair<int, int>> from = {{sc(h)->actor(1).col, sc(h)->actor(1).row}, {sc(h)->actor(3).col, sc(h)->actor(3).row}};
    bool on_floor = true, apart = true;
    int moved1 = 0, moved3 = 0;
    for (int t = 0; t < 1000; ++t) { // 50 s: past the Reunion's end
        frame(h, 50);
        for (int s : {1, 3}) {
            const auto &a = sc(h)->actor(s);
            on_floor = on_floor && sc(h)->room()[a.row * kEndgameRoomSide + a.col] == 0x44;
            for (int o = 0; o < kEndgameActorSlots; ++o)
                if (o != s && sc(h)->actor(o).active && sc(h)->actor(o).col == a.col && sc(h)->actor(o).row == a.row) apart = false;
        }
        moved1 += sc(h)->actor(1).col != from[0].first || sc(h)->actor(1).row != from[0].second;
        moved3 += sc(h)->actor(3).col != from[1].first || sc(h)->actor(3).row != from[1].second;
    }
    const int64_t chain = audio.first_start(MusicSong::RuleBritannia, start);
    check(chain >= start + 60000000 && chain <= start + 60060000,
          "M9 and Rule Britannia starts when the Reunion ends (" + n((chain - start) / 1000) + " ms)");
    check(moved1 > 0 && moved3 > 0 && on_floor && apart && sc(h)->actor(31).col == 4 && sc(h)->actor(0).col == 8 &&
              sc(h)->actor(2).col == 8 && live(h),
          "F5 the wander (0x0ac9): members 1 and 3 drift over the floor, never onto anyone; the seated stay seated -- forever");
    dump("end1-stranded");
    const uint32_t seq = last_seq(h);
    const auto routed = h.rt->routed_command_count();
    h.key(' ');
    h.key('\r');
    h.mic();
    h.up();
    h.key('s', true);
    check(last_seq(h) >= seq && h.rt->routed_command_count() == routed && sc(h)->phase() == EndgamePhase::Stranded &&
              since(h, seq, true).find("Save complete") == std::string::npos,
          "F6 keys and the trackball do nothing in the stranded room; Alt+S saves nothing");
    h.return_to_title();
    check(h.rt->frontend_open() && !live(h), "F7 System Menu -> Return to Title leaves it: the title, the scene gone");

    std::printf("S  Yes without the box\n");
    Run y({{"Avatar", 'G', 240}}, false, nullptr, true);
    const bool room2 = to_final_room(y, false, kAlone, false);
    const int64_t start2 = walk_to_soul(y);
    const uint32_t seq2 = last_seq(y);
    const bool ended = start2 >= 0 && drive_to_end(y, "y");
    const std::string text2 = squeeze(since(y, seq2));
    const std::string want2 = squeeze(record(0) + "Avatar" + "!\"\n\n" + record(1) + "Yes\n\n" + "\"I see...\n" + record(10));
    check(room2 && ended && sc(y)->phase() == EndgamePhase::Stranded && !sc(y)->victory() && (text2 == want2 || text2 == "VICTORY!" + want2),
          "S4 Yes without the wooden box: \"Yes\", then \"I see...\" (0x08b9 needs both), the stranded room");
}

// ===========================================================================
// The audio: the mutes, and the same timeline without any audio.
// ===========================================================================
void test_audio(const VictoryTimes &v) {
    std::printf("M  the mutes and the stock (no audio pack) ending\n");
    Run h({{"Avatar", 'G', 240}}, false, nullptr, true);
    Backend audio;
    h.rt->configure_audio(g_audio, &audio);
    h.key('m', true, true); // Alt+Shift+M
    h.key('s', true, true); // Alt+Shift+S
    const bool room = to_final_room(h, true, kHeroes, false);
    const int64_t start = walk_to_soul(h);
    until(h, [&] { return at_getkey(h); }, 30000);
    const int64_t greet_ms = (Run::now() - start) / 1000;
    const bool ended = drive_to_end(h, "y");
    check(room && ended && sc(h)->phase() == EndgamePhase::Scroll && audio.count("music", start) == 0 && audio.count("sfx", start) == 0 &&
              h.rt->audio().music_muted() && h.rt->audio().sfx_muted(),
          "M10 with Alt+Shift+M and Alt+Shift+S the whole ending plays to the scroll silent: no song, no sound");
    h.key('m', true, true);
    check(!h.rt->audio().music_muted() && h.rt->audio().current_song() == MusicSong::RuleBritannia &&
              audio.first_start(MusicSong::RuleBritannia, start) >= 0,
          "M11 unmuting on the scroll starts its song (Rule Britannia), not the throne room's");
    check(greet_ms == v.greet_ms, "M12 muted, the greeting comes at the same moment (" + n(greet_ms) + " ms vs " + n(v.greet_ms) + ")");

    Run s({{"Avatar", 'G', 240}}, false, nullptr, true); // no configure_audio: stock assets, no audio pack
    const bool room2 = to_final_room(s, true, kHeroes, false);
    const int64_t start2 = walk_to_soul(s);
    until(s, [&] { return at_getkey(s); }, 30000);
    const int64_t greet2 = (Run::now() - start2) / 1000;
    const bool ended2 = drive_to_end(s, "y");
    check(room2 && ended2 && sc(s)->phase() == EndgamePhase::Scroll && greet2 == v.greet_ms && !s.rt->audio().has_music(),
          "M13 without the audio pack (stock DOS assets: no music) the same ending plays on the same clock (" + n(greet2) +
              " ms to the greeting)");
}

// ===========================================================================
// Without the pack's ending the device keeps the parity path.
// ===========================================================================
void test_no_pack() {
    std::printf("N  a pack without the ending's screens\n");
    static tdeck::AlphaResourceOwners partial;
    partial = *pack;
    partial.endgame_pages = nullptr;
    const auto *full = pack;
    pack = &partial;
    Run h({{"Avatar", 'G', 240}}, false, nullptr, true);
    pack = full;
    const bool room = to_final_room(h, true, kAlone, false);
    const uint32_t mark = last_seq(h);
    walk_to_soul(h);
    const std::string text = since(h, mark, true);
    check(room && !live(h) && quest_flag(h.rt->game().quest, QuestFlag::GameWon) && h.rt->ui()->mode() == UiMode::Ending &&
              occurrences(text, "\"FOLLOW!\" cries Lord British") == 1 && occurrences(text, "The quest is complete.") == 1,
          "N1 no scene: rescue_events' narration once (the quest_parity path) and the bare Ending (Batch 53A)");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a4_end1_ending_runtime <pack> <openu5-audio.bin> [--dump <dir>]\n");
        return 2;
    }
    g_dump = arg_after(argc, argv, "--dump");
    if (!load_pack(argv[1]) || !pack->endgame_pages || !pack->endgame_room) {
        std::printf("RED the pack does not load or has no ending -- the A4-END1 checks cannot run\n");
        return 1;
    }
    g_audio = tdeck::load_audio_pack_info(argv[2]);
    if (g_audio.state != AudioPackState::Valid) {
        std::printf("RED the audio pack is not valid -- run `npm run pack:audio`\n");
        return 1;
    }
    // Doom's room 15 is a dungeon room: the fixture needs the arenas (batch53a's table).
    static std::vector<DungeonArena> arenas;
    for (size_t i = 0; i < pack->combat_map_count; ++i) arenas.push_back({pack->combat_map_views[i], pack->combat_sprites + i * 16});
    g_arenas = arenas.data();
    g_arena_count = arenas.size();
    const VictoryTimes v = test_victory();
    test_stranded();
    test_audio(v);
    test_no_pack();
    std::printf("A4-END1 ending runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
