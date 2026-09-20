#include "openu5/poison_tick.h"

// Mirror of the reference `PoisonTick` (game/src/ui/poison-tick.ts). The
// derivation lives in the header; only the state machine lives here.
namespace openu5 {

void PoisonFlashPacer::run(const uint8_t *slots, uint8_t count, uint32_t now_ms) {
    cancel();
    if (!slots || !count) return;
    if (count > kPoisonMaxSlots) count = kPoisonMaxSlots;
    if (!blip_ms_) {
        // Drain-synchronously mode: every cue at once, no timers, no flash
        // left on -- byte-identical to the previous flow apart from the cues.
        cues_ += count;
        return;
    }
    for (uint8_t i = 0; i < count; ++i) slots_[i] = slots[i];
    count_ = count;
    index_ = 0;
    active_ = true;
    next_at_ = now_ms; // 0x2a59 of the FIRST member fires straight away
    pump(now_ms);
}

bool PoisonFlashPacer::pump(uint32_t now_ms) {
    if (!active_) return false;
    if (int32_t(now_ms - next_at_) < 0) return false;
    if (index_ >= count_) {
        // 0x2a6e of the LAST member: the row goes back to normal.
        flash_ = -1;
        active_ = false;
        return true;
    }
    flash_ = int8_t(slots_[index_++]); // 0x2a59 -- invert THIS member's row
    ++cues_;                            // 0x2a68 -- noise_burst(10,1600,2000)
    next_at_ = now_ms + blip_ms_;
    return true;
}

void PoisonFlashPacer::cancel() {
    count_ = 0;
    index_ = 0;
    flash_ = -1;
    active_ = false;
    next_at_ = 0;
}

} // namespace openu5
