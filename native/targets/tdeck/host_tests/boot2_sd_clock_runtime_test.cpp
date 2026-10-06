// BOOT2: the SD clock policy and the REAL Board's mount/fallback loop.
//
// main/sd_clock_policy.h names the preferred (fast) clock and the safe (800 kHz)
// fallback. tdeck_board.cpp's initialize_and_test_sd() must mount at the fast
// clock first and, when the card does not mount or fails the read/write/read-back
// test, unmount and retry once at the safe clock -- and never retry at the safe
// clock itself. The fake bus records every esp_vfs_fat_sdspi_mount request.
//
// The host has no /sd, so a "mounted" card always fails the read-back test: that
// exercises the unmount-then-retry path. A passing fast mount is hardware-only.
#include "../main/sd_clock_policy.h"
#include "../main/tdeck_board.h"
#include "fake_tdeck_bus.h"

#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <vector>

namespace bus = openu5_host_bus;

namespace {
int g_failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}
bool is(const std::vector<int> &v, std::initializer_list<int> want) {
    return v.size() == want.size() && std::equal(v.begin(), v.end(), want.begin());
}
}  // namespace

// The policy itself, at compile time.
static_assert(openu5::kSdSafeKhz == 800, "the conservative clock is LilyGO's 800 kHz");
static_assert(openu5::kSdFastKhz > openu5::kSdSafeKhz, "the preferred clock must be faster than the safe one");
static_assert(openu5::sd_fallback_khz(openu5::kSdFastKhz) == openu5::kSdSafeKhz, "a fast clock falls back to the safe one");
static_assert(openu5::sd_fallback_khz(openu5::kSdSafeKhz) == 0, "the safe clock has no further fallback");
static_assert(openu5::sd_fallback_khz(0) == 0, "no clock yet: nothing to fall back from");

int main() {
    bus::install();
    bus::model().timed = false;
    tdeck::Board board{};
    check(board.initialize_display() == ESP_OK, "display initialises on the fake bus");
    check(board.sd_clock_khz() == 0, "no SD clock before a mount");

    // A: the card never mounts. Fast first, then safe once, then give up.
    bus::sd_reset(false);
    auto s = board.initialize_and_test_sd();
    check(!s.ok, "A: a card that never mounts reports failure");
    check(is(bus::sd_mount_khz(), {openu5::kSdFastKhz, openu5::kSdSafeKhz}), "A: mount requests are fast then safe, nothing more");
    check(board.sd_clock_khz() == openu5::kSdSafeKhz, "A: the last clock tried is the safe one");
    check(bus::sd_unmounts() == 0, "A: a failed mount leaves nothing to unmount");
    check(!board.fall_back_sd_clock(), "A: already at the safe clock: no further fallback offered");
    check(bus::sd_mount_khz().size() == 2, "A: a refused fallback does not mount again");

    // B: the card mounts but fails the read-back test (no /sd on the host). The
    // fast mount is torn down before the safe remount; the final one is left to the caller.
    bus::sd_reset(true);
    s = board.initialize_and_test_sd();
    check(!s.ok, "B: read-back failure on both clocks reports failure");
    check(is(bus::sd_mount_khz(), {openu5::kSdFastKhz, openu5::kSdSafeKhz}), "B: mount requests are fast then safe");
    check(bus::sd_unmounts() == 1, "B: exactly the fast mount is unmounted before the retry");
    check(board.sd_clock_khz() == openu5::kSdSafeKhz, "B: ends on the safe clock");

    // C: a fresh board has no clock, so the pack-validation fallback has nothing to retreat from.
    tdeck::Board fresh{};
    bus::sd_reset(true);
    check(!fresh.fall_back_sd_clock(), "C: no fallback before any mount");
    check(bus::sd_mount_khz().empty(), "C: and it mounts nothing");

    if (g_failures) {
        std::printf("boot2_sd_clock_runtime: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("boot2_sd_clock_runtime: all checks passed (fast=%d kHz, safe=%d kHz)\n", openu5::kSdFastKhz,
                openu5::kSdSafeKhz);
    return 0;
}
