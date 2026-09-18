#pragma once

#include "input_controller.h"
#include "openu5/ui_session.h"

namespace tdeck {

enum class DeviceShortcut : uint8_t { None, DeveloperMenu, Save, Load, MovementModeToggled };

class UiInputAdapter {
  public:
    bool translate(const RawInputEvent &, openu5::UiMode, openu5::UiAction &,
                   DeviceShortcut &, bool accepts_direction = false);
    // Called from the regular runtime pump so a long hold toggles at the
    // threshold instead of waiting for the physical key to be released.
    bool update(int64_t now_us, openu5::UiMode, DeviceShortcut &);
    bool movement_mode_enabled() const { return movement_mode_enabled_; }
    void set_movement_mode_enabled(bool value) { movement_mode_enabled_ = value; }
    void set_trackball_responsiveness(uint16_t value) {
        directions_.set_trackball_speed_percent(value);
    }
    bool movement_mode_active(openu5::UiMode mode, bool accepts_direction = false) const;
    const openu5::InputController &direction_metrics() const { return directions_; }
  private:
    void toggle_movement_mode();
    openu5::InputController directions_{};
    int64_t mic_pressed_us_ = 0;
    bool mic_down_ = false;
    bool mic_long_hold_handled_ = false;
    bool movement_mode_enabled_ = false;
};

} // namespace tdeck
