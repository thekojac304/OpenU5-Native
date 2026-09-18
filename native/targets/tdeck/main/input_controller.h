#pragma once

#include <cstdint>

#include "input_events.h"
#include "native_movement.h"

namespace openu5 {

class InputController {
public:
    bool normalize(const tdeck::RawInputEvent &raw, Direction &direction);
    void set_trackball_debounce_us(int64_t value) { trackball_debounce_us_ = value; }
    void set_trackball_speed_percent(uint16_t percent);
    static int64_t trackball_debounce_for_percent(uint16_t percent);
    int64_t trackball_debounce_us() const { return trackball_debounce_us_; }
    uint32_t trackball_raw_edges() const { return trackball_raw_edges_; }
    uint32_t trackball_accepted() const { return trackball_accepted_; }
    uint32_t trackball_suppressed() const { return trackball_suppressed_; }
    const char *decision() const { return decision_; }

private:
    const char *decision_ = "none";
    int64_t last_trackball_us_[4]{};
    int64_t trackball_debounce_us_ = 12000;
    uint32_t trackball_raw_edges_ = 0;
    uint32_t trackball_accepted_ = 0;
    uint32_t trackball_suppressed_ = 0;
};

}  // namespace openu5
