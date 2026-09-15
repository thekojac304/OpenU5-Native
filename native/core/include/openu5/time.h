#pragma once
#include "types.h"
namespace openu5 {
// time.ts does NOT mask fields to bytes. Signed fields preserve negative additions.
struct GameTime { int32_t year = 0, month = 1, day = 1, hour = 0, minute = 0; };
enum class DayPhase : uint8_t { Night, Dawn, Day, Dusk };
GameTime advance_minutes(GameTime time, int32_t minutes);
DayPhase day_phase(GameTime time);
uint8_t schedule_index(const uint8_t (&times)[4], uint8_t hour);
}
