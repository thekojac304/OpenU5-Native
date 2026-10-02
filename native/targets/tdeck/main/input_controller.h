#pragma once

#include <cstddef>
#include <cstdint>

#include "input_events.h"
#include "native_movement.h"

namespace openu5 {

// Alpha 4 A4-ENH1 (targets/tdeck/ALPHA4_UI.md section 10). The trackball's
// press switch: one accepted press per physical press, taken at the press edge
// itself -- no hold, no delay -- however the contact bounces and however long
// the ball is held down.
class TrackballClickFilter {
public:
    // A press edge this soon after a release is the contact bouncing.
    static constexpr int64_t kBounceUs = 30000;
    // A second accepted press needs this long since the last accepted one.
    static constexpr int64_t kRepeatUs = 150000;
    bool press(int64_t now_us);   // true: accept this press
    void release(int64_t now_us);
    uint32_t presses() const { return presses_; }
    uint32_t accepted() const { return accepted_; }
    uint32_t rejected() const { return presses_ - accepted_; }
    void reset_counts() { presses_ = accepted_ = 0; }

private:
    bool down_ = false, released_once_ = false, accepted_once_ = false;
    int64_t last_release_us_ = 0, last_accept_us_ = 0;
    uint32_t presses_ = 0, accepted_ = 0;
};

// A4-ENH1. What one trackball speed level does with the pulses. The ball has
// no X/Y counter: each direction pin pulses once per few degrees of roll, so a
// level says how many pulses on one axis make a step and how close together
// two steps may come. PROVISIONAL -- tuned on hardware (section 10).
struct TrackballTuning {
    uint8_t pulses_per_step; // pulses on one axis that make one step (the remainder is kept)
    uint16_t step_gap_ms;    // the shortest gap between two steps, any direction; pulses inside it are dropped
    uint8_t window_ms;       // a same-direction pulse closer than this is contact bounce
};
/** The tuning of speed `level` (clamped to 1..10). Level 10 is the pre-A4-ENH1 100 % behaviour. */
const TrackballTuning &trackball_tuning(uint8_t level);

// A4-ENH1. One roll of the ball: its pulses from the first after a quiet gap
// of kIdleResetUs to the last before the next such gap.
struct TrackballGesture {
    uint16_t edges[4]{}; // raw GPIO pulses per direction (up, down, left, right)
    uint16_t steps[4]{}; // Direction actions emitted
    uint32_t duration_ms = 0;
    uint32_t min_gap_ms = 0; // the shortest pulse-to-pulse gap inside it
    uint8_t level = 0;       // the speed level it was read at
};

// A4-ENH1. What the trackball did since the counters were last read: the
// Developer > Diagnostics "Trackball stats (live)" report, for tuning on the
// device. Integer counters only; nothing here allocates or logs.
struct TrackballStats {
    static constexpr size_t kGapBuckets = 7; // same-direction gaps: <4 <8 <16 <32 <64 <128 >=128 ms
    static constexpr size_t kRecent = 6;
    uint32_t edges[4]{}, steps[4]{};
    uint32_t bounced = 0;       // dropped: same direction inside the level's window
    uint32_t step_gap = 0;      // dropped: inside the level's gap after a step
    uint32_t click_guarded = 0; // dropped: inside kClickGuardUs of a press-switch edge
    uint32_t idle_resets = 0;   // a partial step thrown away after kIdleResetUs without a pulse on its axis
    uint32_t reversals = 0;     // a partial step thrown away by a pulse the other way
    uint32_t gaps[kGapBuckets]{};
    uint32_t gestures = 0, longest_gesture = 0;
    TrackballGesture recent[kRecent]{}; // newest at recent_next - 1
    uint8_t recent_count = 0, recent_next = 0;
};

// The semantic trackball: raw direction pulses in, Direction actions out.
//
//   pulse -> click guard -> same-direction bounce window -> step gap ->
//   per-axis accumulator (reversal and idle reset) -> one step per
//   pulses_per_step pulses, the remainder kept
//
// Each axis (left/right, up/down) counts on its own, so a diagonal roll
// alternates the two directions. Level 10 is exactly the pre-A4-ENH1 path: a
// 12 ms same-direction window and one step per pulse.
class InputController {
public:
    // A roll ends, and its axis forgets a partial step, after this long quiet.
    static constexpr int64_t kIdleResetUs = 400000;
    static constexpr int64_t kGestureGapUs = kIdleResetUs;
    // A4-ENH1: pulses this soon after a press-switch edge are the ball
    // rocking under the finger, not a roll.
    static constexpr int64_t kClickGuardUs = 60000;

    bool normalize(const tdeck::RawInputEvent &raw, Direction &direction);
    // A4-ENH1: a TrackballClick edge. True = an accepted press.
    bool click(const tdeck::RawInputEvent &raw);
    // Closes a roll that has gone quiet (the runtime's pump calls it).
    void poll(int64_t now_us);
    // A finished roll not yet handed out (oldest first), for the serial log.
    bool take_gesture(TrackballGesture &out);
    const TrackballStats &stats() const { return stats_; }
    const TrackballClickFilter &click_filter() const { return click_; }
    void reset_stats();

    void set_trackball_level(uint8_t level);
    uint8_t trackball_level() const { return level_; }
    const TrackballTuning &tuning() const { return trackball_tuning(level_); }
    /** The partial step held on each axis: X (+ right), Y (+ down). */
    int32_t held_x() const { return acc_[0]; }
    int32_t held_y() const { return acc_[1]; }
    uint32_t trackball_raw_edges() const { return trackball_raw_edges_; }
    uint32_t trackball_accepted() const { return trackball_accepted_; }
    uint32_t trackball_suppressed() const { return trackball_suppressed_; }
    const char *decision() const { return decision_; }

private:
    void note_edge(size_t index, int64_t now_us);
    void close_gesture();
    const char *decision_ = "none";
    uint8_t level_ = 5; // openu5::kTrackballSpeedDefault; the device applies settings.json at boot
    int64_t last_trackball_us_[4]{};
    bool pulse_seen_[4]{};
    int32_t acc_[2]{};
    int64_t last_axis_us_[2]{};
    bool axis_seen_[2]{};
    int64_t last_step_us_ = 0;
    bool step_seen_ = false;
    uint32_t trackball_raw_edges_ = 0;
    uint32_t trackball_accepted_ = 0;
    uint32_t trackball_suppressed_ = 0;
    TrackballClickFilter click_{};
    int64_t click_guard_until_us_ = 0;
    TrackballStats stats_{};
    TrackballGesture gesture_{};
    bool gesture_open_ = false;
    int64_t gesture_start_us_ = 0, last_edge_us_ = 0, last_dir_edge_us_[4]{};
    bool dir_seen_[4]{};
    int64_t gesture_min_gap_us_ = 0;
    uint8_t unlogged_ = 0; // finished gestures take_gesture() has not handed out
};

// A4-ENH1. The "Trackball stats (live)" report: at most `max_lines` rows of
// `line_bytes - 1` characters into `lines` (row i at lines + i * line_bytes).
size_t format_trackball_report(const InputController &, char *lines, size_t line_bytes, size_t max_lines);

}  // namespace openu5
