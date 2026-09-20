#pragma once

#include <cstddef>
#include <cstdint>

#include "commands.h"
#include "movement.h"
#include "presentation.h"

// Y-04 (Refuge, TrollSneak) -- the MODAL NARRATIVE SCENES, and the one minimal
// sequencer they share.
//
// Both channels were adjudicated as full staged scenes, not one-shot VFX, and
// the adjudication found they are the SAME primitive: an ORDERED queue of
// beats, each beat carrying an observable change (a console line, a sound cue,
// a visual phase) and its own DWELL, played back on a clock while input is
// swallowed, with the rest of the turn DEFERRED until the last beat has been
// seen. They differ only in the numbers and in what each beat can change --
// which is why there is one sequencer here and not two.
//
//   TrollSneak -- MAINOUT 0x1c0e-0x1ca6. The original paces the preamble with
//   kernel 0x3AE6, which is not a beep but RUN-N-FRAMES (n x [frame tick 0x5910
//   + delay 0x20fa(1)]): a MUTE pause that keeps rendering. pause(10) after
//   `Thou spieth trolls under the bridge!` (0x1c19) and pause(5) before EACH
//   dot of `$ sneaks across...` (0x1c56-0x1c65). One unit = one INT 1Ch tick
//   ~ 55 ms -- a derived tracing, not a calibration: pause(10) ~ 549 ms,
//   pause(5) ~ 275 ms, which matches the aulddragon P02 26:20 witness
//   (preamble to name ~ 700 ms, dot to dot ~ 200-300 ms). The rest of the
//   turn's events (the troll toll's `Caught!` prompt, or nothing) are DEFERRED
//   until the beats run out -- in the original all of this is synchronous
//   inside the turn, so nothing legitimate can slip in between. Input is
//   swallowed while it runs (getkey is not read during the sequence).
//
//   Refuge -- BLCKTHRN.OVL 0x0910 `party_refuge`, the death + resurrection
//   staging the original plays when the WHOLE party falls: the viewport goes
//   BLACK (0x0962), the Avatar is left ALONE in the centre, and over that
//   nothingness two flanking spectral figures and the cyan apparition arrive
//   while Lord British recites the karma speech. Blits (kernel 0x6dd8
//   `blit(tile,col,row)`): 0x0a70 `push 0x5e; push 2; push 7` -- left ghost at
//   (col2,row7); 0x0aa2 `push 0x5f; push 8; push 7` -- right ghost at
//   (col8,row7); 0x0ae9 `push 0x174; push 5; push 2` -- the cyan apparition at
//   (col5,row2). Confirmed in video-M f042 (black viewport, Avatar alone) and
//   f058 (apparition top-centre, the two spectral figures bottom left/right,
//   speech in quotes). Authority: re/notes/death-resurrection-audit.md section
//   3 plus the BLCKTHRN.OVL 0x0910 disassembly.
//
// STATE MUTATION ORDER. `check_refuge` deliberately mutates NOTHING: it emits
// the script and latches `refuge_pending`. The revive, the karma floor, the
// clock and the wake-up in Lord British's castle all live in `resolve_refuge`,
// which the OWNER calls when this pacer reports the scene complete -- so the
// roster keeps showing the FALLEN party for the whole scene (faithful to
// video-M f042/f058) and the castle is revealed only after `Vertigo...`.
// TrollSneak mutates nothing at all: the rolls already happened in
// `bridgeTrollAmbush`, and the deferred tail carries whatever the turn decided.
//
// PACING NOTE (audit Y-32). This sequencer is deliberately generic in the one
// dimension that matters -- "a beat becomes visible, and cannot be replaced
// before its dwell has elapsed" -- because the same property is what the
// separately observed scripted-event pacing gap (Y-32, the Blackthorn capture
// scene running visibly faster than the original) will need to be measured
// against. It is not a general scene framework and must not grow into one
// here; it is the smallest thing that makes staged timing assertable.
namespace openu5 {

/** run-n-frames (kernel 0x3AE6): one INT 1Ch tick. The shared scene unit. */
constexpr uint32_t kSceneFrameUnitMs = 55;

/**
 * Refuge cadence (Class C, calibrated against video-M): ms per RAW unit of the
 * original's `delay` (0x7e6a), plus a READING floor on beats that print a line
 * (so the line can actually be read) and a short one on the figure/sound
 * transitions, which in the original are nearly immediate. These are the same
 * three numbers the reference presenter uses (main.ts `runRefugeScene`).
 */
constexpr uint32_t kRefugeUnitMs = 70;
constexpr uint32_t kRefugeTextFloorMs = 900;
constexpr uint32_t kRefugeSceneFloorMs = 260;

/** Which scene is on stage. */
enum class NarrativeScene : uint8_t { None, TrollSneak, Refuge };

/**
 * VISUAL PHASE of the refuge scene (BLCKTHRN 0x0910). Each phase ACCUMULATES
 * what the renderer paints over the BLACK viewport (the "nothingness" of the
 * dream): the Avatar alone in the centre and, incrementally, the two flanking
 * spectral figures and the cyan apparition.
 */
enum class RefugePhase : uint8_t {
    None,       // no scene mounted
    Void,       // 0x0962: viewport to black, the Avatar ALONE in the centre
    GhostLeft,  // 0x0a70: + left ghost (tile 0x5e) at (col2,row7)
    GhostBoth,  // 0x0aa2: + right ghost (tile 0x5f) at (col8,row7)
    Apparition, // 0x0ae9: + the cyan apparition (0x174) at (col5,row2)
    Vertigo,    // 0x0bc4: the transition flash before the wake-up
};

/** The apparition -- "Apparation1" (0x174), cyan four-frame group. */
constexpr int16_t kRefugeApparitionTile = 0x174;
/** Left spectral figure (0x0a70 `push 0x5e`). */
constexpr int16_t kRefugeGhostLeftTile = 0x5e;
/** Right spectral figure (0x0aa2 `push 0x5f`). */
constexpr int16_t kRefugeGhostRightTile = 0x5f;
constexpr int8_t kRefugeApparitionCol = 5, kRefugeApparitionRow = 2;
constexpr int8_t kRefugeGhostLeftCol = 2, kRefugeGhostLeftRow = 7;
constexpr int8_t kRefugeGhostRightCol = 8, kRefugeGhostRightRow = 7;

/** One figure baked into the refuge's black viewport. */
struct RefugeSceneFigure {
    int8_t col = 0, row = 0;
    int16_t tile = 0;
};

/** Map a script's phase name ("void", "ghostLeft", ...) onto the enum. */
RefugePhase refuge_phase_from_name(const char *);

/**
 * Figures VISIBLE in a given phase -- CUMULATIVE: the Avatar always (centre);
 * from `GhostLeft` on the left figure; `GhostBoth` adds the right one; and
 * `Apparition`/`Vertigo` the cyan apparition top-centre. PURE: it reads no
 * external state. Returns how many figures were produced.
 */
size_t refuge_scene_figures(RefugePhase, int16_t avatar_tile,
                            RefugeSceneFigure *out, size_t capacity);

/**
 * Bake the refuge scene into the ordinary presentation window, so the existing
 * rasterizer draws it with no new code: every uncovered cell is
 * kPresentationHidden, which render_snapshot already leaves BLACK -- exactly
 * as the original blackens the window at 0x0962.
 */
PresentationSnapshot compose_refuge_presentation(RefugePhase, int16_t avatar_tile);

/** One released beat, handed to the owner for the duration of the callback. */
struct NarrativeSceneBeat {
    const char *text = nullptr; // console line, or null
    const char *sfx = nullptr;  // cue id, or null
    RefugePhase phase = RefugePhase::None; // phase this beat mounts, or None
    /** True for `messageAppend`: continues the line already on screen (the
     *  three dots of `$ sneaks across...`, 0x1c56-0x1c65). */
    bool append = false;
};

/** Where released beats go. Deferred turn events go to an ordinary EventSink. */
struct NarrativeSceneSink {
    void *context = nullptr;
    void (*beat)(void *, const NarrativeSceneBeat &) = nullptr;
};

enum class NarrativeSceneStepKind : uint8_t { Beat, Forward };

/** One entry of the deferred turn. */
struct NarrativeSceneStep {
    GameEvent event{};
    uint32_t text_offset = 0, text_length = 0;
    uint32_t sfx_offset = 0, sfx_length = 0;
    uint32_t dwell_ms = 0;
    RefugePhase phase = RefugePhase::None;
    NarrativeSceneStepKind kind = NarrativeSceneStepKind::Forward;
    bool append = false, has_text = false, has_sfx = false;
};

/** Caller-owned storage, so the pacer itself stays a small object. */
struct NarrativeScenePacerStorage {
    NarrativeSceneStep *steps = nullptr;
    size_t step_capacity = 0;
    char *text = nullptr;
    size_t text_capacity = 0;
};

enum class NarrativeScenePacerState : uint8_t { Idle, Running };

/**
 * The shared sequencer. The core emits the whole turn synchronously; this
 * takes ownership of it from the first scene event onward and hands it back a
 * beat at a time, so the narrative arrives STAGED rather than as one burst.
 *
 * It owns no game rules, no clock and no RNG. It never sleeps and never spins:
 * the owner calls `pump()` once per frame with its own monotonic millisecond
 * count, and the pacer returns immediately when the beat on screen has not yet
 * served its dwell.
 */
class NarrativeScenePacer {
  public:
    void attach(NarrativeScenePacerStorage storage) { storage_ = storage; }

    /**
     * False drains every beat synchronously (host harnesses and automated
     * digests), exactly as the reference pacers do with a zero unit. True is
     * the real cadence. Defaults to true.
     */
    void set_paced(bool paced) { paced_ = paced; }
    bool paced() const { return paced_; }

    /**
     * Offer an event. Returns true when the pacer took ownership of it (the
     * caller must NOT also forward it). A scene event always activates; while
     * a scene runs every later event of the turn is deferred too, because
     * releasing a later message before an earlier beat would put the narrative
     * back out of order.
     */
    bool enqueue(const GameEvent &);

    /** Release whatever is due at `now_ms`. Safe to call every frame. */
    void pump(uint32_t now_ms, NarrativeSceneSink beats, EventSink forward);

    /** Tear the scene down unconditionally (mode change, load, reset). */
    void cancel();

    bool active() const { return state_ != NarrativeScenePacerState::Idle; }
    /** Input other than transcript paging is swallowed while this is true. */
    bool modal() const { return active(); }
    NarrativeScene scene() const { return scene_; }
    RefugePhase phase() const { return phase_; }
    /** True while the refuge scene owns the viewport. */
    bool mounted() const { return phase_ != RefugePhase::None; }

    /**
     * The scene that has just FINISHED, reported exactly ONCE. `Refuge` is the
     * owner's cue to call `resolve_refuge()` -- the reference's own order:
     * unmount the scene first, then apply the resurrection.
     */
    NarrativeScene take_completion();

    /** Instant the beat on screen may be replaced. Pacing assertions read it. */
    uint32_t resume_at_ms() const { return resume_at_ms_; }
    bool waiting() const { return waiting_; }
    uint32_t released_steps() const { return released_; }
    uint32_t dropped_steps() const { return dropped_; }
    size_t queued_steps() const { return count_; }

  private:
    NarrativeScenePacerStorage storage_{};
    NarrativeScene scene_ = NarrativeScene::None;
    NarrativeScene completion_ = NarrativeScene::None;
    RefugePhase phase_ = RefugePhase::None;
    NarrativeScenePacerState state_ = NarrativeScenePacerState::Idle;
    size_t head_ = 0, count_ = 0, text_used_ = 0;
    uint32_t resume_at_ms_ = 0, released_ = 0, dropped_ = 0;
    bool waiting_ = false, paced_ = true;

    NarrativeSceneStep *push(NarrativeSceneStepKind);
    bool copy_text(const char *, uint32_t &offset, uint32_t &length);
    void begin(NarrativeScene);
    void reset_queue();
};

} // namespace openu5
