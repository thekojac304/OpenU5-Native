#include "input_controller.h"

#include <cstdarg>
#include <cstdio>

namespace openu5 {

namespace {
constexpr char kDirectionLetters[4] = {'U', 'D', 'L', 'R'};

uint32_t to_ms(int64_t us) { return us <= 0 ? 0U : uint32_t(us / 1000); }

struct ReportLines {
    char *lines;
    size_t line_bytes, max_lines, count = 0;
};
__attribute__((format(printf, 2, 3))) void put(ReportLines &r, const char *format, ...) {
    if (r.count >= r.max_lines || !r.line_bytes) return;
    va_list args;
    va_start(args, format);
    std::vsnprintf(r.lines + r.count++ * r.line_bytes, r.line_bytes, format, args);
    va_end(args);
}

size_t gap_bucket(int64_t gap_us) {
    static constexpr int64_t kLimits[TrackballStats::kGapBuckets - 1] = {4000, 8000, 16000, 32000, 64000, 128000};
    for (size_t i = 0; i < TrackballStats::kGapBuckets - 1; ++i)
        if (gap_us < kLimits[i]) return i;
    return TrackballStats::kGapBuckets - 1;
}
} // namespace

bool TrackballClickFilter::press(int64_t now_us)
{
    ++presses_;
    const bool bounce = down_ || (released_once_ && now_us - last_release_us_ < kBounceUs) ||
                        (accepted_once_ && now_us - last_accept_us_ < kRepeatUs);
    down_ = true;
    if (bounce) return false;
    accepted_once_ = true;
    last_accept_us_ = now_us;
    ++accepted_;
    return true;
}

void TrackballClickFilter::release(int64_t now_us)
{
    down_ = false;
    released_once_ = true;
    last_release_us_ = now_us;
}

int64_t InputController::trackball_debounce_for_percent(uint16_t percent)
{
    if (percent < 25) percent = 25;
    if (percent > 300) percent = 300;
    // Inverse scaling is deterministic: a faster percentage accepts physical
    // detents closer together. 100% restores the earlier 12 ms timing.
    return 1200000 / percent;
}

void InputController::set_trackball_speed_percent(uint16_t percent)
{
    speed_percent_ = percent;
    trackball_debounce_us_ = trackball_debounce_for_percent(percent);
}

void InputController::close_gesture()
{
    if (!gesture_open_) return;
    gesture_open_ = false;
    gesture_.duration_ms = to_ms(last_edge_us_ - gesture_start_us_);
    gesture_.min_gap_ms = to_ms(gesture_min_gap_us_);
    uint32_t edges = 0;
    for (auto e : gesture_.edges) edges += e;
    ++stats_.gestures;
    if (edges > stats_.longest_gesture) stats_.longest_gesture = edges;
    stats_.recent[stats_.recent_next] = gesture_;
    stats_.recent_next = uint8_t((stats_.recent_next + 1) % TrackballStats::kRecent);
    if (stats_.recent_count < TrackballStats::kRecent) ++stats_.recent_count;
    if (unlogged_ < TrackballStats::kRecent) ++unlogged_;
    gesture_ = {};
}

void InputController::poll(int64_t now_us)
{
    if (gesture_open_ && now_us - last_edge_us_ >= kGestureGapUs) close_gesture();
}

bool InputController::take_gesture(TrackballGesture &out)
{
    if (!unlogged_ || unlogged_ > stats_.recent_count) {
        unlogged_ = 0;
        return false;
    }
    const size_t slot = (stats_.recent_next + TrackballStats::kRecent - unlogged_) % TrackballStats::kRecent;
    out = stats_.recent[slot];
    --unlogged_;
    return true;
}

void InputController::reset_stats()
{
    // A new window: a roll still in progress belongs to the one just read.
    stats_ = {};
    unlogged_ = 0;
    gesture_open_ = false;
    gesture_ = {};
    click_.reset_counts();
}

void InputController::note_edge(size_t index, int64_t now_us)
{
    if (gesture_open_ && now_us - last_edge_us_ >= kGestureGapUs) close_gesture();
    if (!gesture_open_) {
        gesture_open_ = true;
        gesture_start_us_ = now_us;
        gesture_min_gap_us_ = 0;
    } else {
        const int64_t gap = now_us - last_edge_us_;
        if (gesture_min_gap_us_ == 0 || gap < gesture_min_gap_us_) gesture_min_gap_us_ = gap;
    }
    if (dir_seen_[index]) ++stats_.gaps[gap_bucket(now_us - last_dir_edge_us_[index])];
    dir_seen_[index] = true;
    last_dir_edge_us_[index] = now_us;
    last_edge_us_ = now_us;
    ++stats_.edges[index];
    if (gesture_.edges[index] < UINT16_MAX) ++gesture_.edges[index];
}

bool InputController::click(const tdeck::RawInputEvent &raw)
{
    // Either edge of the press switch rocks the ball a little: the pulses
    // that follow it at once are not a roll.
    click_guard_until_us_ = raw.timestamp_us + kClickGuardUs;
    if (raw.transition == tdeck::KeyTransition::Released) {
        click_.release(raw.timestamp_us);
        decision_ = "click-release";
        return false;
    }
    const bool accepted = click_.press(raw.timestamp_us);
    decision_ = accepted ? "click-accepted" : "click-bounce";
    return accepted;
}

bool InputController::normalize(const tdeck::RawInputEvent &raw, Direction &direction)
{
    decision_ = "accepted";
    bool directional = true;
    switch (raw.kind) {
    case tdeck::RawInputKind::Keyboard:
    case tdeck::RawInputKind::KeyboardResynchronized:
        // Preserve every raw keyboard edge for the future command/text layer.
        // Direction normalization intentionally consumes trackball events only.
        decision_ = "keyboard-reserved-for-command-text";
        return false;
    case tdeck::RawInputKind::TrackballUp: direction = Direction::North; break;
    case tdeck::RawInputKind::TrackballDown: direction = Direction::South; break;
    case tdeck::RawInputKind::TrackballRight: direction = Direction::East; break;
    case tdeck::RawInputKind::TrackballLeft: direction = Direction::West; break;
    case tdeck::RawInputKind::TrackballClick:
    case tdeck::RawInputKind::None: directional = false; break;
    }
    if (!directional) {
        decision_ = "not-direction-code";
        return false;
    }

    const size_t index = static_cast<size_t>(raw.kind) -
                         static_cast<size_t>(tdeck::RawInputKind::TrackballUp);
    ++trackball_raw_edges_;
    note_edge(index, raw.timestamp_us);
    if (raw.timestamp_us < click_guard_until_us_) {
        ++trackball_suppressed_;
        ++stats_.click_guarded;
        decision_ = "trackball-inside-click-guard";
        return false;
    }
    // The GPIO layer already emits falling edges only.  This window rejects a
    // contact's short re-close while preserving deliberate rapid detents.
    if (raw.timestamp_us - last_trackball_us_[index] < trackball_debounce_us_) {
        ++trackball_suppressed_;
        ++stats_.bounced;
        decision_ = "trackball-same-direction-under-configured-window";
        return false;
    }
    last_trackball_us_[index] = raw.timestamp_us;
    ++trackball_accepted_;
    ++stats_.steps[index];
    if (gesture_.steps[index] < UINT16_MAX) ++gesture_.steps[index];
    return true;
}

size_t format_trackball_report(const InputController &in, char *lines, size_t line_bytes, size_t max_lines)
{
    ReportLines r{lines, line_bytes, max_lines};
    const auto &s = in.stats();
    const auto &c = in.click_filter();
    put(r, "TRACKBALL STATS  since last read");
    put(r, "Setting: Trackball %u%% (window %lu ms)", unsigned(in.trackball_speed_percent()),
        (unsigned long)(in.trackball_debounce_us() / 1000));
    put(r, "Pulses  U %lu  D %lu  L %lu  R %lu", (unsigned long)s.edges[0], (unsigned long)s.edges[1],
        (unsigned long)s.edges[2], (unsigned long)s.edges[3]);
    put(r, "Steps   U %lu  D %lu  L %lu  R %lu", (unsigned long)s.steps[0], (unsigned long)s.steps[1],
        (unsigned long)s.steps[2], (unsigned long)s.steps[3]);
    put(r, "Dropped bounce %lu  after-click %lu", (unsigned long)s.bounced, (unsigned long)s.click_guarded);
    put(r, "Gap ms <4:%lu <8:%lu <16:%lu <32:%lu <64:%lu", (unsigned long)s.gaps[0], (unsigned long)s.gaps[1],
        (unsigned long)s.gaps[2], (unsigned long)s.gaps[3], (unsigned long)s.gaps[4]);
    put(r, "       <128:%lu >=128:%lu (same direction)", (unsigned long)s.gaps[5], (unsigned long)s.gaps[6]);
    put(r, "Clicks  pressed %lu  toggled %lu  bounce %lu", (unsigned long)c.presses(), (unsigned long)c.accepted(),
        (unsigned long)c.rejected());
    put(r, "Rolls   %lu, longest %lu pulses", (unsigned long)s.gestures, (unsigned long)s.longest_gesture);
    if (s.recent_count) put(r, "Last rolls, newest first:");
    for (size_t k = 0; k < s.recent_count; ++k) {
        const auto &g = s.recent[(s.recent_next + TrackballStats::kRecent - 1 - k) % TrackballStats::kRecent];
        char text[40]{};
        size_t at = 0;
        for (size_t d = 0; d < 4; ++d)
            if (g.edges[d] && at + 1 < sizeof(text)) {
                const int w = std::snprintf(text + at, sizeof(text) - at, "%c%u>%u ", kDirectionLetters[d],
                                            unsigned(g.edges[d]), unsigned(g.steps[d]));
                if (w > 0) at += size_t(w);
            }
        put(r, " %s%lu ms, min gap %lu ms", text, (unsigned long)g.duration_ms, (unsigned long)g.min_gap_ms);
    }
    put(r, "Pulse = one GPIO edge; R9>3 = 9 pulses, 3 steps.");
    put(r, "Reading this report starts a new window.");
    return r.count;
}

}  // namespace openu5
