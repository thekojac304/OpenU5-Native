#include "openu5/time.h"
namespace openu5 {
GameTime advance_minutes(GameTime t, int32_t minutes) {
    // Widen BEFORE addition; positive carry only, matching TS for negative minutes.
    int64_t m = int64_t(t.minute) + minutes;
    int64_t h = t.hour, d = t.day, mo = t.month, y = t.year;
    if (m >= 60) { h += m / 60; m %= 60; }
    if (h >= 24) { d += h / 24; h %= 24; }
    if (d > 28) { mo += (d - 1) / 28; d = (d - 1) % 28 + 1; }
    if (mo > 13) { y += (mo - 1) / 13; mo = (mo - 1) % 13 + 1; }
    return {int32_t(y), int32_t(mo), int32_t(d), int32_t(h), int32_t(m)};
}
DayPhase day_phase(GameTime t) {
    if (t.hour >= 5 && t.hour < 6) return DayPhase::Dawn;
    if (t.hour >= 6 && t.hour < 20) return DayPhase::Day;
    if (t.hour >= 20 && t.hour < 21) return DayPhase::Dusk;
    return DayPhase::Night;
}
uint8_t schedule_index(const uint8_t (&times)[4], uint8_t hour) {
    uint8_t best = uint8_t(hour - times[0]), index = 0;
    for (uint8_t k = 1; k < 4; ++k) {
        const uint8_t diff = uint8_t(hour - times[k]);
        if (best > diff) { best = diff; index = k == 3 ? 1 : k; }
    }
    return index;
}
}
