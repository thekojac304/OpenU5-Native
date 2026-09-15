#pragma once
#include "types.h"
namespace openu5 {
class OriginalRng {
    uint16_t state_;
public:
    explicit OriginalRng(int32_t seed = 0) : state_(uint16_t(seed)) {}
    void seed(int32_t n) { state_ = uint16_t(n); }
    uint16_t get_seed() const { return state_; }
    uint16_t next_raw16();
    // TS throws RangeError before advancing. Native returns an explicit error.
    Result<int32_t> next(int32_t lo, int32_t hi);
};
uint16_t time_hash_seed(int32_t hour, int32_t minute, int32_t second, int32_t hundredths);
}
