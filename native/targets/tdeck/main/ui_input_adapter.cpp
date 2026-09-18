#include "ui_input_adapter.h"

namespace tdeck {
namespace {
constexpr int64_t kMicHoldUs = 1100000;

bool is_movement_context(openu5::UiMode mode, bool accepts_direction) {
    return accepts_direction || mode == openu5::UiMode::Exploration ||
           mode == openu5::UiMode::Dungeon || mode == openu5::UiMode::Combat ||
           mode == openu5::UiMode::TargetSelection;
}

uint8_t ascii_lower(uint8_t code) {
    return code >= 'A' && code <= 'Z' ? uint8_t(code + ('a' - 'A')) : code;
}
} // namespace

void UiInputAdapter::toggle_movement_mode() {
    movement_mode_enabled_ = !movement_mode_enabled_;
}

bool UiInputAdapter::movement_mode_active(openu5::UiMode mode, bool accepts_direction) const {
    return movement_mode_enabled_ && is_movement_context(mode, accepts_direction);
}

bool UiInputAdapter::update(int64_t now_us, openu5::UiMode,
                            DeviceShortcut &shortcut) {
    shortcut = DeviceShortcut::None;
    if (!mic_down_ || mic_long_hold_handled_ ||
        now_us - mic_pressed_us_ < kMicHoldUs) return false;
    toggle_movement_mode();
    mic_long_hold_handled_ = true;
    shortcut = DeviceShortcut::MovementModeToggled;
    return true;
}

bool UiInputAdapter::translate(const RawInputEvent &raw, openu5::UiMode mode,
                               openu5::UiAction &action, DeviceShortcut &shortcut,
                               bool accepts_direction) {
    shortcut = DeviceShortcut::None;
    openu5::Direction direction{};
    if (directions_.normalize(raw, direction)) {
        if (raw.modifiers.shift &&
            (direction == openu5::Direction::North || direction == openu5::Direction::South)) {
            action = {};
            action.kind = direction == openu5::Direction::North
                              ? openu5::UiActionKind::PageUp
                              : openu5::UiActionKind::PageDown;
            return true;
        }
        action = {}; action.kind = openu5::UiActionKind::Direction; action.direction = direction;
        return true;
    }
    if (raw.kind == RawInputKind::KeyboardResynchronized) {
        // A release may have been lost.  Abandon the gesture; a later orphaned
        // release is ignored and can neither cancel nor toggle.
        mic_down_ = false;
        mic_long_hold_handled_ = false;
        return false;
    }
    if (raw.kind != RawInputKind::Keyboard) return false;
    // Match the physical matrix location, not the vendor '$' label.  Cancel is
    // emitted on a short release so a hold can be classified without leakage.
    if (raw.column == kMicrophoneKeyColumn && raw.row == kMicrophoneKeyRow) {
        if (raw.transition == KeyTransition::Pressed) {
            if (!mic_down_) {
                mic_down_ = true;
                mic_pressed_us_ = raw.timestamp_us;
                mic_long_hold_handled_ = false;
            }
            return false;
        }
        if (!mic_down_) return false;
        const bool already_handled = mic_long_hold_handled_;
        const bool long_hold = already_handled ||
                               raw.timestamp_us - mic_pressed_us_ >= kMicHoldUs;
        mic_down_ = false;
        if (long_hold) {
            mic_long_hold_handled_ = false;
            if (already_handled) return false;
            toggle_movement_mode();
            shortcut = DeviceShortcut::MovementModeToggled;
            return true;
        }
        action = {};
        action.kind = openu5::UiActionKind::Cancel;
        return true;
    }
    if (raw.transition != KeyTransition::Pressed || raw.modifier_key) return false;
    if (raw.code == 0) return false;
    const uint8_t lower = ascii_lower(raw.code);
    if (raw.modifiers.alt) {
        if (lower == 'm') { action={};action.kind=openu5::UiActionKind::SystemMenu;return true; }
        if (lower == 'd') shortcut = DeviceShortcut::DeveloperMenu;
        else if (lower == 's') shortcut = DeviceShortcut::Save;
        else if (lower == 'l') shortcut = DeviceShortcut::Load;
        return shortcut != DeviceShortcut::None;
    }
    if (movement_mode_active(mode, accepts_direction) && !raw.modifiers.symbol) {
        action = {};
        action.kind = openu5::UiActionKind::Direction;
        if (lower == 'w') action.direction = openu5::Direction::North;
        else if (lower == 'a') action.direction = openu5::Direction::West;
        else if (lower == 's') action.direction = openu5::Direction::South;
        else if (lower == 'd') action.direction = openu5::Direction::East;
        else action.kind = openu5::UiActionKind::Character;
        if (action.kind == openu5::UiActionKind::Direction) return true;
    }
    action = {};
    if (raw.code == '\r') action.kind = openu5::UiActionKind::Confirm;
    else if (raw.code == '\b')
        action.kind = mode == openu5::UiMode::TextEntry || mode == openu5::UiMode::NumericEntry
                          ? openu5::UiActionKind::DeleteCharacter : openu5::UiActionKind::Back;
    else { action.kind = openu5::UiActionKind::Character; action.character = char16_t(raw.code); }
    return true;
}

} // namespace tdeck
