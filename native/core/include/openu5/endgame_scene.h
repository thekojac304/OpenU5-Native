#pragma once

#include <cstddef>
#include <cstdint>

#include "presentation.h"
#include "rng.h"

namespace openu5 {

struct GameState;

// ---------------------------------------------------------------------------
// A4-END1 -- ENDGAME.OVL, the whole ending, as a pure sequencer.
//
// Read from the shipped overlay instruction by instruction
// (re/tools/a4_end1_endgame_listing.py, re/notes/a4-end1-ending-reconstruction.md):
// endgame_main 0x0648, story_screens 0x0000 and endgame_datestamp 0x0326, with
// the kernel routines they call (run_n_frames 0x3ae6, getkey 0x266c, the actor
// pose selector 0x51b8, print_string 0x1850, put_char 0x16ba, tone_sweep 0x2192,
// the fn34 fizzle EGA.DRV 0x2570). No clock and no I/O here: advance() runs the
// overlay up to its next wait and says what that wait is; the device paces it,
// draws the state this class exposes, and feeds back the keys the original's
// getkey loops read. The original never leaves the overlay (both branches end
// in a loop), so neither does this: there is no "finished" state.
// ---------------------------------------------------------------------------

constexpr int kEndgameRoomSide = 11;
constexpr int kEndgameRoomCells = kEndgameRoomSide * kEndgameRoomSide;
constexpr int kEndgameActorSlots = 32;
constexpr int kEndgameStoryPages = 6;
constexpr int kEndgameScrollCols = 40;
constexpr int kEndgameScrollRows = 25;
/** The moongate cell, DS 0xad99 = 0xad14 + 4 x 0x20 + 5 (0x0992). */
constexpr int kEndgameGateCol = 5, kEndgameGateRow = 4;
/** EGA.DRV fn36 ax=4 (0x2c4e), the tileset recolor 0x0658 asks for: the LUT (0x2d4d). */
extern const uint8_t kEndgameRecolorLut[16];
/** True for the 22 tiles in fn36's hardcoded list (0x2cb0-0x2d33). */
bool endgame_recolored_tile(int32_t tile);

enum class EndgamePhase : uint8_t {
    Inactive,
    Throne,   // the green throne room in the viewport, text in the console
    Dissolve, // 0x004b: the fn34 fizzle of the whole screen to black
    Story,    // story_page(): one of END.DAT's six pages
    Scroll,   // ENDSC.16 and the proclamation (endgame_datestamp)
    Stranded  // the "pull up a chair" room, its wander loop forever
};

enum class EndgameWait : uint8_t {
    Frames,   // run_n_frames(wait_amount()): ticks of 55 ms, the scene redrawn each tick
    Hold,     // a busy loop that redraws nothing (tone_sweep, the footstep): wait_amount() ms
    Key,      // getkey_with_redraw, or story_screens' poll: any key
    YesNo,    // the box questions (0x0852 / 0x088b): only Y or N
    Dissolve, // the fizzle: the device reports dissolve_done()
    Forever   // the victory loop 0x04f9: nothing more happens
};

/** The speaker calls, by site. */
enum class EndgameCue : uint8_t {
    Footstep,    // sprite_step_redraw 0x0505 -> kernel 0x433e
    ReviveSweep, // 0x078f tone_sweep(1, 0x1388, 0x9c40, 1, 0x2260), " lives!"
    OrbSweep     // 0x0987 tone_sweep(1, 0x2710, 0xc350, 1, 0x1450), the orb
};

/** The Exodus Project patch's selector calls (mid.drv); stock DOS has none. */
enum class EndgameMusic : uint8_t {
    Reunion,    // 0x0aff: sel 0x15 at entry (Joyous Reunion, Rule Britannia after it)
    StoryScene, // 0x0aee: sel 0x18 with BL = the page story_screens is about to show
    Finale      // 0x0b18: sel 0x1b in both terminal loops (Rule Britannia)
};

struct EndgameSink {
    void *context = nullptr;
    /** A console print. `first` is the overlay's first print; every later one continues the stream. */
    void (*text)(void *, const char *, bool first) = nullptr;
    void (*cue)(void *, EndgameCue) = nullptr;
    void (*music)(void *, EndgameMusic, uint8_t scene) = nullptr;
};

struct EndgameActor {
    uint8_t tile = 0; // the record's byte 1 (0x0000 bank); drawn | 0x100
    uint8_t col = 0, row = 0;
    bool active = false;
};

/** One cell of the 40x25 text screen endgame_datestamp prints on. */
struct EndgameScrollCell {
    uint8_t ch = 0;
    uint8_t flags = 0; // kEndgameCellPrinted | kEndgameCellRunes | kEndgameCellInverse
};
constexpr uint8_t kEndgameCellPrinted = 1, kEndgameCellRunes = 2, kEndgameCellInverse = 4;

/** Playtime, endgame_datestamp 0x0407: the game date minus Year 139, Month 4, Day 5 (13 x 28). */
struct EndgamePlaytime { int years = 0, months = 0, days = 0; };
EndgamePlaytime endgame_scroll_playtime(int year, int month, int day);

/**
 * EGA.DRV fn34, carry clear (0x2570): the fizzle's pixel order over a w x h
 * rect. A Galois LFSR (taps cs:[0x254d + (bits - 2) * 2], bits = bit width of
 * w*h - 1), seeded 1, visits every position once; pos = state, x = pos % w,
 * y = pos / w, rows past h skipped; (0,0) last (0x2600). For the endgame's
 * 320x200 the taps are 0xB400. PURE.
 */
class EndgameFizzle {
  public:
    explicit EndgameFizzle(uint16_t width = 320, uint16_t height = 200);
    /** The next pixel; false once every pixel has been produced. */
    bool next(uint16_t &x, uint16_t &y);
    uint32_t produced() const { return produced_; }
    uint32_t total() const { return uint32_t(width_) * height_; }

  private:
    uint32_t state_ = 1, taps_ = 0, produced_ = 0;
    uint16_t width_, height_;
    bool done_ = false;
};

class EndgameScene {
  public:
    /**
     * Mount the overlay over the live game. `room` is MISCMAPS.DAT[0x210] as
     * 11 rows of 11 tiles; `records` are ENDMSG.DAT's 11 records in file order.
     * The revive (0x075a-0x0765) writes the live roster, as the original does.
     */
    bool start(GameState &, const uint8_t *room, const char *const *records, size_t record_count);
    void cancel();
    bool active() const { return phase_ != EndgamePhase::Inactive; }

    /** Run to the next wait. Calling it again before the wait is satisfied re-runs nothing. */
    EndgameWait advance(const EndgameSink &);
    EndgameWait wait() const { return wait_; }
    uint32_t wait_amount() const { return amount_; }
    /** True while advance() must be called (the last wait was satisfied). */
    bool ready() const { return ready_; }
    /** The device: a Frames/Hold wait has elapsed. */
    void elapsed();
    /** The device: the fizzle reached its last pixel. */
    void dissolve_done();
    /**
     * A key during a Key or YesNo wait. True when it ended the wait. Y/N are
     * case-folded (the loops compare the getkey result with 'Y' / 'N' after
     * the kernel's upper-casing); during YesNo every other key is re-read.
     */
    bool key(char16_t ch, const EndgameSink &);

    EndgamePhase phase() const { return phase_; }
    uint8_t story_page() const { return page_; }
    /** 0x0778: the revive's XOR of the viewport, up until the next redraw. */
    bool inverted() const { return inverted_; }
    /** [0x5887], read by the arena present 0x56ac: 1..15 draw a partial gate. */
    uint8_t gate_stage() const { return gate_stage_; }
    bool victory() const { return victory_; }
    const EndgameActor &actor(int slot) const { return actors_[slot]; }
    const uint8_t *room() const { return room_; }
    /** 0x0a45: after the gate closes the floor is blitted over the gate cell. */
    bool gate_cell_restored() const { return gate_floor_; }
    const EndgameScrollCell *scroll() const { return scroll_; }

    /**
     * The throne room's 11x11 window, as the arena compositor 0x5394 / 0x56ac
     * draws it: the room (copied into the visible buffer every frame, 0x59f8),
     * then slots 31..0 over it, each through the town pose selector 0x51b8
     * (a mirror, a bed, the chairs). `pose_rng` is 0x51a0's rand(0, 3).
     */
    void compose(PresentationSnapshot &, OriginalRng &pose_rng) const;
    /** The wander loop's draws (0x05c5 / 0x05e7) use this RNG. */
    OriginalRng &rng() { return rng_; }

  private:
    enum class Pc : uint8_t;
    struct Walk { uint8_t slot, col, row; };
    EndgameWait frames(uint32_t n);
    EndgameWait hold(uint32_t ms);
    EndgameWait wait_for(EndgameWait w);
    bool step_toward(uint8_t slot, uint8_t col, uint8_t row);
    void print(const EndgameSink &, const char *);
    void print_record(const EndgameSink &, size_t index);
    // endgame_datestamp's text screen.
    void put_char(uint8_t c);
    void scroll_print(const char *s);
    void accum(const char *s);
    void accum_cardinal(int n);
    void accum_ordinal(int n);
    void compose_scroll();
    bool wander(uint8_t slot);
    uint8_t tile_at(int col, int row) const;

    GameState *game_ = nullptr;
    const char *const *records_ = nullptr;
    size_t record_count_ = 0;
    uint8_t room_[kEndgameRoomCells]{};
    EndgameActor actors_[kEndgameActorSlots]{};
    OriginalRng rng_{};
    EndgamePhase phase_ = EndgamePhase::Inactive;
    EndgameWait wait_ = EndgameWait::Frames;
    Pc pc_{};
    uint32_t amount_ = 0;
    bool ready_ = false, first_print_ = true, victory_ = false, inverted_ = false, gate_floor_ = false;
    uint8_t member_ = 0, page_ = 0, gate_stage_ = 0, step_ = 0, answer_ = 0, wander_index_ = 0;
    bool moved_ = false;
    Walk walk_{};
    // endgame_datestamp: the cursor, the window 0 flags and text_accum_char's buffer.
    EndgameScrollCell scroll_[kEndgameScrollCols * kEndgameScrollRows]{};
    uint8_t cursor_col_ = 0, cursor_row_ = 0, font_ = 0;
    bool scroll_inverse_ = false, scroll_center_ = false;
    char accum_[0x28]{};
    uint8_t accum_length_ = 0;
};

} // namespace openu5
