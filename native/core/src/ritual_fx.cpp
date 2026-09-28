#include "openu5/ritual_fx.h"

#include <cstring>

namespace openu5 {

namespace {
bool cue(const GameEvent &e, const char *id) {
    return e.kind == GameEventKind::Sfx && e.text && std::strcmp(e.text, id) == 0;
}
} // namespace

bool RitualFx::present(const GameEvent &e) {
    const auto before = mask_;
    if (e.kind == GameEventKind::RitualInvert) {
        // 0x0bcd / 0x0c41: one loose XOR of the viewport with [0x13b0].
        mask_ ^= kRitualInvertMask;
        loose_ = true;
    } else if (codex_pulse_quake(e)) {
        // 0x0dbd / 0x0dd4 / 0x0deb: the XOR lands on the picture the previous
        // pulse left, since the shake between them redraws nothing.
        mask_ ^= uint8_t(e.note);
        ++pulses_;
    } else if (e.kind == GameEventKind::PartyChanged && loose_) {
        // The rite's last event: 0x0d1a run_n_frames(10), whose first frame
        // redraws the viewport.
        clear();
    } else if (e.kind == GameEventKind::ShrineKeyWait && pulses_) {
        // 0x0df8: the getkey's idle pass redraws the viewport.
        clear();
    }
    return mask_ != before;
}

uint32_t RitualFx::hold_after_ms(const GameEvent &e) const {
    // The two mirrored sweep loops play with the viewport already negative.
    if (cue(e, kWellDoneCue)) return tone_sweep_ms(kWellDoneSweepSamples);
    if (cue(e, kDonationCue)) return tone_sweep_ms(kDonationSweepSamples);
    // screen_shake_fx blocks until its bands are done: WELL DONE's 0x0c88
    // inside the negative, and each of the Codex's bracketed shakes.
    if (e.kind == GameEventKind::Quake && (codex_pulse_quake(e) || loose_)) return uint32_t(kQuakeDurationMs);
    // run_n_frames(10): the redraw, then nine more frames of the same picture.
    if (e.kind == GameEventKind::PartyChanged && loose_) return run_n_frames_ms(kRitualRestoreFrames);
    // "A STRANGE WIND..." stays over the negative until the getkey's first
    // idle pass redraws (0x1b38 delay(1), 0x269a).
    if (e.kind == GameEventKind::Message && pulses_ && !loose_) return run_n_frames_ms(kGetkeyRedrawFrames);
    return 0;
}

} // namespace openu5
