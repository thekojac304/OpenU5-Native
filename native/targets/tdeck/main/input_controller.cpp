#include "input_controller.h"

namespace openu5 {

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
    trackball_debounce_us_ = trackball_debounce_for_percent(percent);
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
    case tdeck::RawInputKind::None: directional = false; break;
    }
    if (!directional) {
        decision_ = "not-direction-code";
        return false;
    }

    const size_t index = static_cast<size_t>(raw.kind) -
                         static_cast<size_t>(tdeck::RawInputKind::TrackballUp);
    ++trackball_raw_edges_;
    // The GPIO layer already emits falling edges only.  This window rejects a
    // contact's short re-close while preserving deliberate rapid detents.
    if (raw.timestamp_us - last_trackball_us_[index] < trackball_debounce_us_) {
        ++trackball_suppressed_;
        decision_ = "trackball-same-direction-under-configured-window";
        return false;
    }
    last_trackball_us_[index] = raw.timestamp_us;
    ++trackball_accepted_;
    return true;
}

}  // namespace openu5
