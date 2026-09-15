#include "openu5/rng.h"
namespace openu5 {
uint16_t OriginalRng::next_raw16() {
    uint32_t x = (uint32_t(state_) + 0x9248U) & 0xffffU;
    x = ((x >> 3) | (x << 13)) & 0xffffU;
    state_ = uint16_t((x ^ 0x9248U) + 0x11U);
    return state_;
}
Result<int32_t> OriginalRng::next(int32_t lo, int32_t hi) {
    const int64_t span = int64_t(hi) - lo + 1;
    if (span <= 0) return {0, Error::InvalidRange};
    return {int32_t(int64_t(lo) + (next_raw16() & 0x7fff) % span), Error::None};
}
uint16_t time_hash_seed(int32_t hour, int32_t minute, int32_t second, int32_t hundredths) {
    const uint32_t cx = (((uint32_t(hour) * 2) & 255) << 8) | ((uint32_t(minute) * 4) & 255);
    const uint32_t dx = (((uint32_t(second) * 8) & 255) << 8) | (uint32_t(hundredths) & 255);
    return uint16_t((((dx + cx) & 0xffff) ^ 0x91eb) & 0xfff);
}
}
