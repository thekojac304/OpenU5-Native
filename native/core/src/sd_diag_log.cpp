#include "openu5/sd_diag_log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace openu5 {

// The writer's logic below is the device's A3-04C writer (sd_diagnostic_logger.cpp)
// moved unchanged -- the same prefix on every physical line, the same rotation, the
// same flush triggers and drop notice -- plus the switch: nothing touches the card
// while logging is off and the file is closed.

bool SdDiagLog::set_enabled(bool on) {
    if (on && (!available_.load() || failed())) return false;
    enabled_.store(on);
    return true;
}

SdLogState SdDiagLog::state() const {
    if (!available_.load() || failed()) return SdLogState::Unavailable;
    return enabled() ? SdLogState::On : SdLogState::Off;
}

int SdDiagLog::mirror(int (*serial)(const char *, va_list), uint64_t (*clock_ms)(), const char *format,
                      va_list args) {
    va_list serial_arguments;
    va_copy(serial_arguments, args);
    const int serial_result = serial ? serial(format, serial_arguments) : 0;
    va_end(serial_arguments);

    if (!capturing()) return serial_result;
    SdDiagLine line{};
    line.timestamp_ms = clock_ms ? clock_ms() : 0;
    va_list file_arguments;
    va_copy(file_arguments, args);
    const int formatted = std::vsnprintf(line.text, sizeof(line.text), format, file_arguments);
    va_end(file_arguments);
    if (formatted <= 0) return serial_result;
    line.length = static_cast<uint16_t>(std::min<size_t>(static_cast<size_t>(formatted), sizeof(line.text) - 1));
    if (!queue_.push(line)) dropped_.fetch_add(1, std::memory_order_relaxed);
    return serial_result;
}

bool SdDiagLog::important(const char *text) {
    constexpr const char *needles[] = {
        "IDENTITY ", "RESOURCE_MISMATCH", "Required hardware", "INPUT_EDGE",
        "INPUT_HOLD", "INPUT_RESYNC", "DEBUG_OPEN", "DEBUG_ACTION", "TELEPORT",
        "ENEMY_ID", "save generation=", "load generation=", "failed", "FAILED",
        "FRONTEND_STATE", "FRONTEND_RENDER", "CHAR_NAME", "CHAR_GENDER",
        "CHAR_CREATE intent=", "KEYBOARD_ERROR", "KEYBOARD_RECOVER", "KEYBOARD_METRICS",
        "INPUT_SERVICE_GAP", "FRONTEND_INTENT", "DEVELOPER_ENTRY", "NEWGAME_SAVE",
        " E (", " W (",
    };
    for (const char *needle : needles) {
        if (std::strstr(text, needle)) return true;
    }
    return false;
}

bool SdDiagLog::write_bytes(const void *data, size_t size) {
    if (!size) return true;
    if (!file_.write(data, size)) return false;
    bytes_ += size;
    return true;
}

bool SdDiagLog::ensure_capacity(size_t upcoming) {
    if (bytes_ + upcoming <= kSdDiagMaximumBytes) return true;
    if (!file_.rotate()) return false;
    bytes_ = 0;
    return true;
}

bool SdDiagLog::write_line(const SdDiagLine &line) {
    // Prefix every physical output line, including multi-line ESP log calls.
    size_t offset = 0;
    do {
        size_t end = offset;
        while (end < line.length && line.text[end] != '\n') ++end;
        const bool had_newline = end < line.length;
        char prefix[32]{};
        const int prefix_length = std::snprintf(prefix, sizeof(prefix), "[%010llu] ",
                                                static_cast<unsigned long long>(line.timestamp_ms));
        const size_t body_length = end - offset;
        const size_t total = static_cast<size_t>(prefix_length) + body_length + 1;
        if (!ensure_capacity(total) || !write_bytes(prefix, static_cast<size_t>(prefix_length)) ||
            !write_bytes(line.text + offset, body_length) || !write_bytes("\n", 1)) {
            return false;
        }
        offset = had_newline ? end + 1 : line.length;
    } while (offset < line.length);
    return true;
}

bool SdDiagLog::write_drop_notice(uint32_t dropped, uint64_t now_ms) {
    SdDiagLine line{};
    line.timestamp_ms = now_ms;
    const int length = std::snprintf(line.text, sizeof(line.text), "SD_LOG_QUEUE_DROPPED count=%lu",
                                     static_cast<unsigned long>(dropped));
    line.length = static_cast<uint16_t>(std::min<size_t>(size_t(std::max(0, length)), sizeof(line.text) - 1));
    return write_line(line);
}

SdDiagLog::Wake SdDiagLog::fail(Wake w) {
    failed_.store(true);
    enabled_.store(false);
    if (open_) file_.close();
    open_ = false;
    w.burst = true;
    w.failed = true;
    return w;
}

SdDiagLog::Wake SdDiagLog::wake(SdDiagLine *line, uint64_t now_ms) {
    Wake w{};
    if (failed()) return w;
    const bool on = enabled();
    if (!on && !open_) return w; // off and closed: the card is not touched
    if (!open_) {                // switched on: open (a full log becomes the archive) before writing
        size_t bytes = 0;
        w.burst = true;
        if (!file_.open(bytes)) return fail(w);
        open_ = true;
        bytes_ = bytes;
        if (bytes_ >= kSdDiagMaximumBytes) {
            if (!file_.rotate()) return fail(w);
            bytes_ = 0;
        }
        last_flush_ms_ = now_ms;
    }
    bool flush_now = false;
    if (line) { // the rest of the queue goes through the same buffer (the writer's stack is 4 KiB)
        do {
            if (!write_line(*line)) return fail(w);
            flush_now = flush_now || important(line->text);
        } while (queue_.pop(*line));
        w.burst = true;
    }
    const uint32_t dropped = dropped_.exchange(0, std::memory_order_acq_rel);
    if (dropped && !write_drop_notice(dropped, now_ms)) return fail(w);
    if (!on) { // switched off: what was captured while on is written, then the file is closed
        file_.close();
        open_ = false;
        w.burst = true;
        return w;
    }
    flush_now = flush_now || dropped || now_ms - last_flush_ms_ >= kSdDiagFlushPeriodMs;
    if (flush_now) {
        if (!file_.flush()) return fail(w);
        last_flush_ms_ = now_ms;
        w.burst = true;
    }
    return w;
}

} // namespace openu5
