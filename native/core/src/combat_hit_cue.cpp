#include "openu5/combat_hit_cue.h"

#include <cstring>

namespace openu5 {
namespace {
bool ends_with(const char *text, const char *suffix) {
    const size_t n = std::strlen(text), k = std::strlen(suffix);
    return n >= k && std::memcmp(text + n - k, suffix, k) == 0;
}
} // namespace

bool combat_status_hit(const CombatEvent &e) {
    // combat.cpp's own literals: poison() "%s is poisoned!", strike() / the
    // In Zu branch "%s slept!". Both are emitted with the struck combatant as
    // the target; " passes out!" (a charm ending) and field sleep carry none.
    return e.kind == CombatEventKind::Message && e.target >= 0 && e.text &&
           (ends_with(e.text, " is poisoned!") || ends_with(e.text, " slept!"));
}

int32_t combat_hit_cue_target(const CombatEvent &e) {
    if (e.kind == CombatEventKind::Attacked && e.hit > 0) return e.target;
    if (combat_status_hit(e)) return e.target;
    return -1;
}

bool CombatHitCuePacer::push(const CombatHitCue &cue, uint32_t now_ms) {
    if (count_ >= kCombatHitCueSlots) {
        ++dropped_;
        return false;
    }
    queue_[(head_ + count_) % kCombatHitCueSlots] = cue;
    ++count_;
    pump(now_ms);
    return true;
}

bool CombatHitCuePacer::pump(uint32_t now_ms) {
    bool changed = false;
    if (showing_ && int32_t(now_ms - ends_at_) >= 0) {
        // 0x2a28 again and 0x5910: the row and the cell are restored.
        showing_ = false;
        restoring_ = true;
        ready_at_ = ends_at_ + kCombatHitCueGapMs;
        changed = true;
    }
    if (!showing_ && count_ && (!restoring_ || int32_t(now_ms - ready_at_) >= 0)) {
        on_ = queue_[head_];
        head_ = uint8_t((head_ + 1) % kCombatHitCueSlots);
        --count_;
        showing_ = true;
        restoring_ = false;
        ends_at_ = now_ms + kCombatHitCueMs;
        ++shown_;
        changed = true;
    }
    return changed;
}

void CombatHitCuePacer::cancel() {
    head_ = count_ = 0;
    showing_ = restoring_ = false;
    on_ = CombatHitCue{};
}

} // namespace openu5
