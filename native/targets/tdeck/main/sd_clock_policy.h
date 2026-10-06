#pragma once
// BOOT2: the SD card's clock policy. Pure constants and decisions, no ESP-IDF
// types, so a host test can pin them.
//
// The card shares SPI2 with the ST7789. Up to Alpha 4 it ran at LilyGO's
// factory-UnitTest 800 kHz (~100 KB/s), and reading the 2.27 MB resource pack
// twice (validation, then load) was ~40 s of a 58 s boot. The board now mounts
// at kSdFastKhz and falls back to kSdSafeKhz when the card cannot run there.
//
// 16 MHz (the 80 MHz / 5 divider) is from the BOOT2 hardware sweep of the
// T-Deck's card: 4, 10, 16 and 20 MHz all validated the pack on every cold boot;
// 26.7 and 40 MHz fail at mount (ESP_ERR_INVALID_RESPONSE reading the CSD), so
// 20 MHz sits one divider below the cliff. 16 MHz is 0.47 s slower than 20 MHz
// to the first frame (throughput saturates well below the clock) and keeps a
// wider timing margin on the shared bus.

#ifndef OPENU5_SD_FAST_KHZ
#define OPENU5_SD_FAST_KHZ 16000
#endif

namespace openu5 {

constexpr int kSdSafeKhz = 800;
constexpr int kSdFastKhz = OPENU5_SD_FAST_KHZ;

// The clock to retry at after `failed_khz` did not work, or 0 when nothing
// slower is left (the safe clock itself failed: the card is genuinely absent
// or broken, and a slower clock is not on offer).
constexpr int sd_fallback_khz(int failed_khz) { return failed_khz > kSdSafeKhz ? kSdSafeKhz : 0; }

}  // namespace openu5
