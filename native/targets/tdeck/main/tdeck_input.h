#pragma once

#include <array>
#include <cstdint>

#include "esp_err.h"
#include "input_events.h"

namespace tdeck {

class InputHardware {
public:
    ~InputHardware();
    esp_err_t initialize();
    bool poll(RawInputEvent &event);
    bool keyboard_online() const { return keyboard_device_ != nullptr; }

private:
    void *i2c_bus_ = nullptr;
    void *keyboard_device_ = nullptr;
    KeyboardMatrix keyboard_matrix_{};
    std::array<KeyboardEvent, kKeyboardKeyCount> keyboard_events_{};
    size_t keyboard_event_count_ = 0;
    size_t keyboard_event_index_ = 0;
    bool trackball_levels_[4] = {true, true, true, true};
    int64_t next_keyboard_poll_us_ = 0;
    int64_t next_keyboard_error_log_us_ = 0;
    uint32_t trace_poll_ = 0;
    uint32_t trace_mode_attempt_ = 0;
    int64_t trace_last_read_us_ = 0;
    void trace_snapshot(const uint8_t *bytes, esp_err_t result, int64_t start);
};

}  // namespace tdeck
