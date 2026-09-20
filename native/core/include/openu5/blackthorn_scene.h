#pragma once

#include <cstddef>
#include <cstdint>

#include "movement.h"
#include "presentation.h"

// Blackthorn CAPTURE SCENE (#324 / audit R-32) -- the staged half of
// BLCKTHRN.OVL 0x060e, the part the native port previously did not have at
// all: the blindfold blackout, the private throne room loaded from
// MISCMAPS.DAT record 0, the chained party, the two guards marching in, the
// fizzle-in of Blackthorn, the companion dragged to the torture table, the
// hourglass, the sacrifice burst and the escorted exit to the cells.
//
// Derivation is the repository's own reverse-engineering material
// (re/notes/blackthorn-escena-324.md, re/notes/blackthorn.md) and the
// TypeScript reference that already ports it:
//   game/src/core/world/blackthorn-scene.ts    (the five anim_vm scripts)
//   game/src/core/world/blackthorn-capture.ts  (which script runs between
//                                               which prints)
//   game/src/ui/blackthorn-scene-pacer.ts      (wall-clock presentation)
// This header is the native mirror of the first of those three, plus the
// pacer that the second and third imply. Everything here is PRESENTATION:
// no GameState, no RNG, no clock. blackthorn.cpp's rules are untouched.
//
// anim_vm's time unit is run-n-frames (kernel 0x3AE6), one INT 1Ch tick,
// which the port calibrates at 55 ms -- the same PAUSE_UNIT_MS the shrine
// and endgame pacers use, and the same 55 000 us tick AlphaRuntime::render()
// already derives for tile animation.
namespace openu5 {

/** The throne room is the ordinary 11x11 presentation window. */
constexpr int kBlackthornSceneCols = kPresentationWindow;
constexpr int kBlackthornSceneRows = kPresentationWindow;
constexpr int kBlackthornSceneCells = kBlackthornSceneCols * kBlackthornSceneRows;

/** Binary object slots the scripts address: 0-5 party, 6/7 guards, 8 Blackthorn. */
constexpr int kBlackthornSlots = 9;

/** Guards (0x07f4-0x0821): tile 0x70 -> 0x170 Guard1, in through the south door. */
constexpr int16_t kBlackthornGuardTile = 0x170;
/** Blackthorn's fizzle-in (0x0842/0x0857/0x0863): holy circle, then Blackthorn1. */
constexpr int16_t kBlackthornHolySymbolTile = 0x116;
constexpr int16_t kBlackthornTile = 0x178;
/** The hourglass at (5,9) -- full on the warning, then the escalation states. */
constexpr int16_t kBlackthornHourglassFullTile = 0xe9;  // Hourglass2
constexpr int16_t kBlackthornHourglassRound1Tile = 0xeb; // Hourglass4 (interrogate 0x05da)
constexpr int16_t kBlackthornHourglassRound2Tile = 0xe8; // Hourglass1, drained (0x05e2)
/** The torture table at (5,7): occupied, then empty after the blade. */
constexpr int16_t kBlackthornTortureBodyTile = 0x82;  // TortureTableWithBody1
constexpr int16_t kBlackthornTortureAfterTile = 0x80; // TortureChair1 (0x0429)
/** The west secret door at (0,4): opens to floor for the escort, then shuts. */
constexpr int16_t kBlackthornWestDoorOpenTile = 0x44; // BrickFloor
constexpr int16_t kBlackthornWestDoorShutTile = 0xbb; // LockedDoorView

/** A step of the VM costs beep_delay(1) = footstep + run-n-frames(2). */
constexpr int16_t kBlackthornStepFrames = 2;
/** Each blindfold drag is delay_ticks_int1c(5) (kernel 0x20fa, 0x069b/0x06d5). */
constexpr int16_t kBlackthornDragFrames = 5;

/** One object slot as the scene currently stages it. */
struct BlackthornStageSlot {
    int8_t x = 0, y = 0;
    int16_t tile = 0;
    // `visible=false` with `present=true` is anim_vm op 9: the tiles are
    // cleared but x/y are RETAINED, because the sacrifice's explosion_fx
    // aims at slot 1's last coordinates whether or not it is still drawn
    // (0x0414-0x0426).
    bool visible = false, present = false;
};

/** The whole cast, as of one beat. */
struct BlackthornStage {
    BlackthornStageSlot slots[kBlackthornSlots]{};
};

/** A map patch (anim_vm op 6 / a direct write to the room buffer 0xad14). */
struct BlackthornPatch {
    int8_t x = -1, y = -1;
    int16_t tile = 0;
    bool valid = false;
};

/** Point sound cues of the scene (tone_sweep 0x2192 families). */
enum class BlackthornSfx : uint8_t { None, Materialize, ShardSweep };

/**
 * One observable beat. `frames` is the pause that follows it, in run-n-frames
 * units. `stage` is the COMPLETE cast after the beat (the reference builders
 * snapshot rather than diff, so a presenter needs no VM of its own).
 */
struct BlackthornBeat {
    BlackthornStage stage{};
    BlackthornPatch patch{};
    /** Room grid to mount on this beat (the packed MISCMAPS record). */
    const int16_t *tiles = nullptr;
    int16_t frames = 0;
    BlackthornSfx sfx = BlackthornSfx::None;
    bool has_stage = false;
    bool footstep = false;
    /** The room replaces the blindfold blackout from here on (0x06e5-0x07c9). */
    bool mount = false;
    /** The segment runs over the blacked-out viewport (0x0676 + 0x0689). */
    bool blackout = false;
    /** The deposit (0x08e7) restores the screen: the scene comes down. */
    bool dismount = false;
};

/**
 * Beats of the longest script (the finale, anim_vm 0x369e) plus headroom.
 * Counted, not guessed: 0x369e expands to 41 beats, 0x36da to 30.
 */
constexpr size_t kBlackthornMaxBeats = 48;

/** A segment of the scene. Text and key waits are sibling events, not beats. */
struct BlackthornSceneScript {
    BlackthornBeat beats[kBlackthornMaxBeats]{};
    uint8_t count = 0;
};

/** Live scene state between segments (the room positions the VM mutates). */
struct BlackthornSceneState {
    BlackthornStageSlot objects[kBlackthornSlots]{};
};

/**
 * What blackthorn.cpp needs to stage the scene at all. `capture_tiles` is the
 * packed 11x11 MISCMAPS.DAT record 0 (`blackthorn-scene.bin`); when it is null
 * the capture degrades to exactly the previous text-only stream, which is what
 * every existing parity fixture and pure harness observes. `script` is scratch
 * the emitter refills per segment -- the event borrows it for the duration of
 * the synchronous delivery only, like every other GameEvent payload.
 */
struct BlackthornSceneServices {
    const int16_t *capture_tiles = nullptr;
    BlackthornSceneState *state = nullptr;
    BlackthornSceneScript *script = nullptr;
};

/** Seat the first `living` roster members in their manacles (0x075f-0x07c9). */
void init_capture_scene(BlackthornSceneState &, const char *classes, int living);

/** SEGMENT 1 -- pause(2) then five blindfold drags, over the blackout. */
void build_blackout_intro_script(BlackthornSceneScript &);
/** SEGMENT 2 -- eighteen more drags, then the room mounts with the party seated. */
void build_throne_mount_script(BlackthornSceneState &, const int16_t *tiles,
                               BlackthornSceneScript &);
/** SEGMENT 3 -- pause(0x32) between "chained and manacled!" and "Footsteps!". */
void build_chained_pause_script(BlackthornSceneScript &);
/** SEGMENT 4 -- approaching steps, the guards (anim_vm 0x3702), Blackthorn's fizzle. */
void build_blackthorn_entry_script(BlackthornSceneState &, BlackthornSceneScript &);
/** SEGMENT 5 -- anim_vm 0x370e: guard A starts towards the Avatar, then "Wait!". */
void build_guard_release_script(BlackthornSceneState &, BlackthornSceneScript &);
/** WARNING -- anim_vm 0x36da: the companion to the table, the hourglass planted. */
void build_warning_script(BlackthornSceneState &, BlackthornSceneScript &);
/** ESCALATION -- the sand falls (rounds 1 and 2 only); false when the round has none. */
bool build_hourglass_script(int round, BlackthornSceneScript &);
/** SACRIFICE -- pause, siren, the victim goes dark, the table is left empty. */
void build_sacrifice_script(BlackthornSceneState &, BlackthornSceneScript &);
/** FINALE -- anim_vm 0x369e: the west door, the escorted Avatar, everyone leaves. */
void build_finale_script(BlackthornSceneState &, BlackthornSceneScript &);
/** Blackthorn's own exit after the pendulum -- anim_vm 0x3716. */
void build_blackthorn_exit_script(BlackthornSceneState &, BlackthornSceneScript &);
/** Is Blackthorn still on stage? (the gate at 0x08d9 over slot 8's tile). */
bool blackthorn_on_stage(const BlackthornSceneState &);
/** Slot 1's last coordinates -- where the sacrifice's explosion_fx lands. */
void sacrifice_victim_cell(const BlackthornSceneState &, int &x, int &y);

// ---------------------------------------------------------------------------
// Presentation
// ---------------------------------------------------------------------------

enum class BlackthornScenePhase : uint8_t { Inactive, Blackout, Throne };

/** What a renderer needs, and nothing else. */
struct BlackthornSceneView {
    BlackthornScenePhase phase = BlackthornScenePhase::Inactive;
    /** Live room grid with every accumulated patch; null during the blackout. */
    const int16_t *tiles = nullptr;
    BlackthornStage stage{};
};

/**
 * Bake the scene into the ordinary presentation window, so the existing
 * rasterizer draws it with no new code: the blackout is a window of
 * kPresentationHidden (which render_snapshot already leaves black), the throne
 * room is the grid with the cast composited on top.
 */
PresentationSnapshot compose_blackthorn_presentation(const BlackthornSceneView &);

/** Where a paced step came from, for the owner's logging and assertions. */
enum class BlackthornStepKind : uint8_t { Beat, Message, Prompt, KeyWait, Forward };

/** One entry of the deferred turn. */
struct BlackthornSceneStep {
    GameEvent event{};
    BlackthornBeat beat{};
    uint32_t text_offset = 0, text_length = 0;
    BlackthornStepKind kind = BlackthornStepKind::Forward;
};

/** Caller-owned storage, so the pacer itself stays a small object. */
struct BlackthornPacerStorage {
    BlackthornSceneStep *steps = nullptr;
    size_t step_capacity = 0;
    char *text = nullptr;
    size_t text_capacity = 0;
    int16_t *grid = nullptr; // kBlackthornSceneCells, the live patched room.
};

/** What the pacer is currently doing. */
enum class BlackthornPacerState : uint8_t {
    Idle,       // no scene
    Running,    // draining beats/messages; input is swallowed (as the reference does)
    AwaitingKey,// a getkey_with_redraw point (0x0894/0x08cd/0x053f/0x04f6/0x0510)
    Prompt,     // queue drained on an interrogation prompt; the scene stays up
};

/**
 * BlackthornScenePacer -- the native counterpart of the reference's
 * `BlackthornScenePacer`. blackthorn.cpp emits the whole capture turn
 * synchronously; this defers everything from the first scene event onward and
 * releases it beat by beat, so the narrative arrives paced instead of as one
 * burst, and the room stays mounted across the interrogation prompts.
 *
 * It owns no game rules. Released events go straight back out through an
 * EventSink -- normally UiSession's -- in the original order.
 */
class BlackthornScenePacer {
  public:
    void attach(BlackthornPacerStorage storage) { storage_ = storage; }
    /** ms per run-n-frames unit; 0 drains synchronously (host harnesses). */
    void set_unit_ms(uint32_t ms) { unit_ms_ = ms; }

    /**
     * Offer an event. Returns true when the pacer took ownership of it (the
     * caller must NOT also forward it). A scene event always activates; while
     * a scene is mounted every subsequent event of the turn is deferred too,
     * because releasing a later message before an earlier beat would put the
     * narrative back out of order.
     */
    bool enqueue(const GameEvent &);

    /** Release whatever is due at `now_ms`. Safe to call every frame. */
    void pump(uint32_t now_ms, EventSink out);

    /** A key was pressed at a getkey point. Returns true when it advanced one. */
    bool advance_key();

    /** Tear the scene down unconditionally (mode change, load, reset). */
    void cancel();

    bool active() const { return state_ != BlackthornPacerState::Idle; }
    /** True while input other than transcript paging must be swallowed. */
    bool modal() const {
        return state_ == BlackthornPacerState::Running ||
               state_ == BlackthornPacerState::AwaitingKey;
    }
    bool awaiting_key() const { return state_ == BlackthornPacerState::AwaitingKey; }
    BlackthornPacerState state() const { return state_; }
    bool mounted() const { return phase_ != BlackthornScenePhase::Inactive; }
    BlackthornSceneView view() const;
    /** Beats/steps released so far -- the pacing assertions read these. */
    uint32_t released_steps() const { return released_; }
    uint32_t dropped_steps() const { return dropped_; }

  private:
    BlackthornPacerStorage storage_{};
    BlackthornStage stage_{};
    BlackthornScenePhase phase_ = BlackthornScenePhase::Inactive;
    BlackthornPacerState state_ = BlackthornPacerState::Idle;
    size_t head_ = 0, count_ = 0, text_used_ = 0;
    uint32_t unit_ms_ = 55, resume_at_ms_ = 0, released_ = 0, dropped_ = 0;
    bool waiting_ = false, saw_dismount_ = false, awaiting_prompt_ = false;

    bool push(const GameEvent &, BlackthornStepKind, const BlackthornBeat *);
    void apply(const BlackthornBeat &);
    void reset_queue();
    void tear_down_stage();
};

} // namespace openu5
