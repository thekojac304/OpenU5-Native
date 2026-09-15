#pragma once

#include <cstdint>

#include "input_events.h"
#include "native_movement.h"

namespace openu5 {

class InputController {
public:
    bool normalize(const tdeck::RawInputEvent &raw, Direction &direction);
    const char *decision() const { return decision_; }

private:
    const char *decision_ = "none";
    int64_t last_trackball_us_[4]{};
};

}  // namespace openu5
