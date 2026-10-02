#pragma once

#include <cstdint>

namespace tdeck {

// Alpha 4 A4-POLISH3 (ALPHA4_UI.md section 13). The keyboard backlight is not
// an S3 GPIO: it belongs to the keyboard's own ESP32-C3. LilyGO's
// Keyboard_ESP32C3.ino (T-Deck master 12f12f8c; revision "2024-12-25 : Added
// keyboard backlight control") drives its pin 9 from LEDC channel 0 at 1 kHz,
// 8 bits, and takes two commands at I2C 0x55, the address the raw matrix is
// read from:
//   0x01 <duty>  set the duty now, 0..255 (0 = off)
//   0x02 <duty>  Alt+B's duty while the set duty is 0 (kept only above 30)
// The sketch boots dark (KB_BRIGHTNESS_BOOT_DUTY 0), stores nothing across a
// reset and has no read-back: a raw-mode read returns the five matrix bytes.
// Its case 0x01 falls through into case 0x02's own Wire.read(), so the frame
// is exactly two bytes -- a third byte would become Alt+B's duty.
// Raw mode (0x03, revision 2025-06-12) is newer than these commands in the
// same sketch, so every keyboard OpenU5 can read has them. Alt+B stays the
// C3's own toggle (between 0 and the set duty); OpenU5 neither sees its
// result nor fights it.
inline constexpr uint8_t kKeyboardBacklightCommand = 0x01;
// The Settings levels Off, Low, Medium, High, Max (openu5::kKeyboardBacklightNames).
// Medium is the sketch's own Alt+B default (KB_BRIGHTNESS_DEFAULT_DUTY 127);
// Low is just above the 30 the sketch treats as its dimmest useful duty.
// PROVISIONAL: tuned on hardware.
inline constexpr uint8_t kKeyboardBacklightDuty[] = {0, 32, 127, 191, 255};
inline constexpr uint8_t kKeyboardBacklightLevelCount = sizeof(kKeyboardBacklightDuty);

inline void keyboard_backlight_frame(uint8_t level, uint8_t frame[2]) {
    frame[0] = kKeyboardBacklightCommand;
    frame[1] = kKeyboardBacklightDuty[level < kKeyboardBacklightLevelCount ? level : 0];
}

// The write policy of InputHardware's capture task (the only I2C user), pure so
// the host tests run it. `desired` is the level the runtime last asked for. A
// level is written once; a failed write is retried on the next service passes
// up to kMaxAttempts times, then left alone until the level changes or the
// keyboard is reinitialized (raw mode re-entered after a bus recovery: the C3
// may have reset, and it resets dark).
struct KeyboardBacklightPolicy {
    static constexpr uint8_t kNone = 0xff;
    static constexpr uint8_t kMaxAttempts = 3;
    uint8_t applied = kNone;
    uint8_t failed = kNone;
    uint8_t attempts = 0;

    bool write_due(uint8_t desired) const {
        if (desired >= kKeyboardBacklightLevelCount || desired == applied) return false;
        return desired != failed || attempts < kMaxAttempts;
    }
    void write_finished(uint8_t level, bool ok) {
        if (ok) {
            applied = level;
            failed = kNone;
            attempts = 0;
            return;
        }
        applied = kNone;
        if (failed != level) {
            failed = level;
            attempts = 0;
        }
        ++attempts;
    }
    void keyboard_reinitialized() {
        applied = kNone;
        failed = kNone;
        attempts = 0;
    }
};

} // namespace tdeck
