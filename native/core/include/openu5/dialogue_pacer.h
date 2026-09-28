#pragma once

#include <cstddef>
#include <cstdint>

#include "dialogue.h"
#include "dialogue_orchestration.h"
#include "movement.h"
#include "scene_timing.h"

// A3-HF5 -- the TLK interpreter's two pause opcodes, on the handheld.
//
// TALK.OVL prints a conversation character by character with no timer of its
// own (0x0574 -> kernel putchar, no delay/run_n_frames on that path). Every
// wait a player sees inside a conversation is one of two script opcodes, read
// from the 1988 binary for this batch (re/tools/dis16.py; TALK's kernel calls
// resolve through base 0xBF80: 0x617a -> 0x20fa delay, 0x5dde -> 0x1d5e key
// poll, 0x5b96 -> 0x1b16 keyboard flush, 0x66ec -> 0x266c getkey):
//
//   0x83 Pause    TALK 0x0f92-0x0fb3. si = 0; loop { compositor 0x5910;
//                 key = 0x1d5e (int 16h ah=1 peek, then int 21h ah=6 READS it);
//                 if key: flush 0x1b24, continue the script;
//                 delay(1); if ++si >= 0x1c: flush, continue }.
//                 = run_n_frames(28): 28 BIOS ticks (1540 ms at the port's
//                 55 ms tick) unless a key ends it first. The key is consumed,
//                 and both exits empty the BIOS keyboard buffer.       [A]
//   0x8F KeyWait  TALK 0x1010 `call 0x66ec` = getkey_with_redraw 0x266c:
//                 blocks for ANY key, the key is discarded.           [A]
//
// The core has marked both on the output line they follow since the dialogue
// port (Conversation::flush -> DialogueOutput::pause), exactly as the
// reference's core does; the reference's UI layer (game/src/ui/talk-console.ts
// TalkConsole, TALK_PAUSE_MS 1538) parks the rest of the script there. Native
// never read the mark, so every scripted pause collapsed to zero and a whole
// routine -- Chuckles' song, Blackthorn's welcome -- landed in one frame.
//
// DialoguePacer is that parking, as a presentation queue. It owns no game
// rule: the core has already run the conversation (effects, karma, the
// guards' alarm) when the events reach it; it only decides WHEN the session
// sees them. From the first released line that carries a pause, every later
// event of the turn is copied into caller-owned storage and handed back, in
// order, when the pause ends: a Timed pause on its own clock or at a key, a
// Key pause only at a key. Nothing is dropped: an event the queue cannot own
// (a borrowed payload other than the dialogue's own, or no room left)
// releases everything already queued, in order, and is then forwarded by the
// caller -- the text loses its cadence, never its content or order.
namespace openu5 {

/** TALK 0x0fae `cmp si,0x1c`: the Pause opcode's 28 ticks. [A] */
constexpr uint32_t kTalkPauseTicks = 28;
/** The Pause as the device times it: run_n_frames(28). [A] */
constexpr uint32_t kTalkPauseMs = run_n_frames_ms(kTalkPauseTicks);

enum class DialoguePacerState : uint8_t {
    Idle,  // nothing parked; events pass straight through
    Timed, // a 0x83 Pause is running: ends on its clock or at a key
    Key,   // a 0x8F KeyWait: ends only at a key
};

enum class DialoguePacerStepKind : uint8_t { Line, Prompt, Effect, EffectMessage, Ended, Handoff, Event };

/** One deferred event. Text lives in the storage arena, never in the event. */
struct DialoguePacerStep {
    GameEvent event{}; // a value copy with every borrowed pointer cleared
    DialoguePacerStepKind kind = DialoguePacerStepKind::Event;
    DialoguePause pause = DialoguePause::None;
    bool rune = false, question = false, has_text = false;
    DialogueHandoff handoff = DialogueHandoff::None;
    DialogueEffect effect{}; // an Effect output's rule payload, already applied by the core
    uint8_t location = 0, slot = 0;
    uint32_t text_offset = 0, text_bytes = 0;
};

/** Caller-owned storage, so the pacer itself stays a small object. */
struct DialoguePacerStorage {
    DialoguePacerStep *steps = nullptr;
    size_t step_capacity = 0;
    char *text = nullptr;
    size_t text_capacity = 0;
};

class DialoguePacer {
  public:
    void attach(DialoguePacerStorage storage) { storage_ = storage; reset(); }
    /** Length of a Timed pause. 0 = every pause drains synchronously: the
     *  reference's automation rule (TalkConsole `instant`), and every host
     *  harness that does not ask for the device's cadence. */
    void set_pause_ms(uint32_t ms) { pause_ms_ = ms; }
    uint32_t pause_ms() const { return pause_ms_; }

    /**
     * Offer an event at `now_ms`. Returns true when the pacer took it -- it
     * was delivered to `out` or queued -- and the caller must NOT also forward
     * it. Returns false when the caller must forward it itself.
     */
    bool offer(const GameEvent &e, uint32_t now_ms, EventSink out);
    /** Release a Timed pause whose clock has run out. */
    void pump(uint32_t now_ms, EventSink out);
    /** A key: ends the current pause of either kind. False when idle. */
    bool advance_key(uint32_t now_ms, EventSink out);
    /** Drop everything (a load, a new game): nothing queued is shown. */
    void cancel() { reset(); }

    DialoguePacerState state() const { return state_; }
    bool holding() const { return state_ != DialoguePacerState::Idle; }
    bool awaiting_key() const { return state_ == DialoguePacerState::Key; }
    size_t queued() const { return count_; }
    uint32_t resume_at_ms() const { return resume_at_ms_; }
    uint32_t released() const { return released_; }
    uint32_t collapsed() const { return collapsed_; }

  private:
    DialoguePacerStorage storage_{};
    uint32_t pause_ms_ = 0, resume_at_ms_ = 0, released_ = 0, collapsed_ = 0;
    size_t head_ = 0, count_ = 0, text_used_ = 0;
    DialoguePacerState state_ = DialoguePacerState::Idle;

    void reset();
    bool push(const GameEvent &e);
    void deliver(const DialoguePacerStep &step, EventSink out) const;
    void hold(DialoguePause pause, uint32_t now_ms);
    void release(uint32_t now_ms, EventSink out);
    void collapse(EventSink out);
};

/** Whether `e` is a dialogue line that carries a TLK pause (and which). */
DialoguePause dialogue_event_pause(const GameEvent &e);

} // namespace openu5
