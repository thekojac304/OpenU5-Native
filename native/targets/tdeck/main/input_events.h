#pragma once

#include <stdint.h>

#include "keyboard_matrix.h"

namespace tdeck {

enum class RawInputKind : uint8_t {
    None,
    Keyboard,
    TrackballUp,
    TrackballDown,
    TrackballLeft,
    TrackballRight,
};

struct RawInputEvent {
    RawInputKind kind = RawInputKind::None;
    uint8_t code = 0;
    KeyTransition transition = KeyTransition::Released;
    KeyboardModifiers modifiers{};
    uint8_t column = 0;
    uint8_t row = 0;
    bool modifier_key = false;
    int64_t timestamp_us = 0;
};

const char *raw_input_name(RawInputKind kind);

}  // namespace tdeck
