#pragma once

#include <cstdint>

namespace tdeck {

enum class KeyboardRecoveryStep : uint8_t { Read, ResetBus, EnterRawMode };

// Pure timing policy shared by the ESP transport and host fault-injection
// tests. A transient gets one scan-period retry. Repeated failures reset the
// master bus, issue raw mode once after a quiet interval, and then wait one
// complete keyboard scan before accepting a new baseline.
struct KeyboardRecoveryPolicy {
    static constexpr int64_t kScanPeriodUs = 40000;
    static constexpr int64_t kResetSettleUs = 10000;
    static constexpr int64_t kMaximumBackoffUs = 250000;

    uint32_t failures = 0;
    KeyboardRecoveryStep step = KeyboardRecoveryStep::Read;
    int64_t due_us = 0;

    void read_succeeded(int64_t now) {
        failures = 0; step = KeyboardRecoveryStep::Read; due_us = now + kScanPeriodUs;
    }
    void read_failed(int64_t now) {
        ++failures;
        step = failures == 1 ? KeyboardRecoveryStep::Read : KeyboardRecoveryStep::ResetBus;
        due_us = now + (failures == 1 ? kScanPeriodUs : kResetSettleUs);
    }
    void bus_reset_finished(int64_t now, bool ok) {
        step = ok ? KeyboardRecoveryStep::EnterRawMode : KeyboardRecoveryStep::ResetBus;
        due_us = now + (ok ? kResetSettleUs : kMaximumBackoffUs);
    }
    void raw_mode_finished(int64_t now, bool ok) {
        step = ok ? KeyboardRecoveryStep::Read : KeyboardRecoveryStep::ResetBus;
        due_us = now + (ok ? kScanPeriodUs : kMaximumBackoffUs);
    }
};

}  // namespace tdeck
