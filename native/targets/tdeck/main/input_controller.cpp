#include "input_controller.h"

namespace openu5 {

bool InputController::normalize(const tdeck::RawInputEvent &raw, Direction &direction)
{
    decision_ = "accepted";
    bool directional = true;
    switch (raw.kind) {
    case tdeck::RawInputKind::Keyboard:
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
    constexpr int64_t kTrackballDebounceUs = 65000;
    if (raw.timestamp_us - last_trackball_us_[index] < kTrackballDebounceUs) {
        decision_ = "trackball-same-direction-under-65000us";
        return false;
    }
    last_trackball_us_[index] = raw.timestamp_us;
    return true;
}

}  // namespace openu5
