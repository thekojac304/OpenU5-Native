#pragma once
// Alpha 3 A3-04D (ALPHA3_AUDIO.md section 21): the SD diagnostic log's switch,
// the ESP-IDF log hook's body and the writer's per-wake logic. They were
// device-only code in targets/tdeck/main/sd_diagnostic_logger.cpp; they live
// here so host tests drive the device's own logic against a fake card. The
// device keeps only what cannot run on a host: the FreeRTOS queue, task and
// storage mutex, stdio on FATFS, and the burst timing.
//
// Why a switch, and why OFF: the log file is on the SD card, and the card
// shares SPI2 with the TFT. ESP-IDF's sdspi driver holds that bus for a whole
// card command, the card's busy time included (sdspi_host_start_command:
// spi_device_acquire_bus ... poll_busy ... spi_device_release_bus; write
// timeout 5 s), at the card's 800 kHz. While the writer is in a command, no
// TFT row can be sent. So the log is off at boot, and Developer > Diagnostics
// > "Probe: SD diag logging" switches it on for the session. Serial logging
// never depends on it.
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace openu5 {

/** A3-04D policy: nothing is written to the card for diagnostics unless a developer asks. */
constexpr bool kSdDiagLoggingDefault = false;
constexpr size_t kSdDiagLineBytes = 768;           // one captured log call, as queued
constexpr size_t kSdDiagMaximumBytes = 512 * 1024; // beyond this the log becomes the one archive (.1)
constexpr uint64_t kSdDiagFlushPeriodMs = 2000;    // while logging: flush at least this often

/** What the Developer row, the scenario label and the report say about the log. */
enum class SdLogState : uint8_t {
    NotReported, // no logger attached (the label keeps its A3-04C form)
    Unavailable, // no card, the writer is not running, or a card error stopped it
    Off,         // A3-04D default: no diagnostic card writes
    On,
};

/** One captured ESP_LOG call: the device queue's element. */
struct SdDiagLine {
    uint64_t timestamp_ms;
    uint16_t length;
    char text[kSdDiagLineBytes];
};

/** The card side: the device's stdio-on-FATFS log file, or a host fake. */
class SdDiagFile {
  public:
    virtual ~SdDiagFile() = default;
    /** Creates the log directory and opens the log for append; `bytes` = its size. */
    virtual bool open(size_t &bytes) = 0;
    /** Closes the log, keeps it as the one archive and opens a new, empty log. */
    virtual bool rotate() = 0;
    /** Appends through the file's buffer (the card is written when the buffer fills). */
    virtual bool write(const void *data, size_t size) = 0;
    /** Pushes the buffer to the card. */
    virtual bool flush() = 0;
    /** Flushes and closes; the directory entry gets the file's size. */
    virtual void close() = 0;
};

/** The queue between the capture side and the writer. Neither call ever waits. */
class SdDiagQueue {
  public:
    virtual ~SdDiagQueue() = default;
    virtual bool push(const SdDiagLine &) = 0; // false: full
    virtual bool pop(SdDiagLine &) = 0;        // false: empty
};

class SdDiagLog {
  public:
    SdDiagLog(SdDiagFile &file, SdDiagQueue &queue) : file_(file), queue_(queue) {}

    // ---- the switch (any task) ----------------------------------------
    /** The card is mounted and the writer runs; before this the log is Unavailable. */
    void set_available(bool available) { available_.store(available); }
    bool enabled() const { return enabled_.load(); }
    /** Capture follows at once; the card follows at the writer's next wake. false: unavailable. */
    bool set_enabled(bool on);
    bool failed() const { return failed_.load(); }
    SdLogState state() const;
    /** Whether a log call is copied for the card. */
    bool capturing() const { return enabled() && available_.load() && !failed(); }

    /**
     * The ESP-IDF log hook's body. The serial copy ALWAYS goes out through
     * `serial`; the card copy is queued only while capturing (never waits;
     * a full queue counts a dropped line). Returns serial's result.
     */
    int mirror(int (*serial)(const char *, va_list), uint64_t (*clock_ms)(), const char *format, va_list args);

    // ---- the writer task ----------------------------------------------
    /** Off (or failed) and the file closed: nothing to do, the writer may sleep without a timeout. */
    bool idle() const { return failed() || (!enabled() && !open_); }

    struct Wake {
        bool burst = false;  // the card (or the stdio buffer in front of it) was touched
        bool failed = false; // a card error: the log is closed and off for good
    };
    /**
     * One wake of the writer, the storage mutex held. `line` holds the line
     * its wait returned (nullptr: the wait timed out); the rest of the queue
     * is drained through the same buffer, without waiting. Switched off with
     * the file open: writes what was captured while on, then closes.
     * Switched off and closed: touches nothing.
     */
    Wake wake(SdDiagLine *line, uint64_t now_ms);

    bool file_open() const { return open_; }
    uint32_t dropped() const { return dropped_.load(); }

    /** The lines that also force a flush (identity, input, errors, saves...). */
    static bool important(const char *text);

  private:
    bool write_bytes(const void *data, size_t size);
    bool ensure_capacity(size_t upcoming);
    bool write_line(const SdDiagLine &line);
    bool write_drop_notice(uint32_t dropped, uint64_t now_ms);
    Wake fail(Wake w);

    SdDiagFile &file_;
    SdDiagQueue &queue_;
    std::atomic<bool> enabled_{kSdDiagLoggingDefault};
    std::atomic<bool> available_{false};
    std::atomic<bool> failed_{false};
    std::atomic<uint32_t> dropped_{0};
    // Writer-task state only.
    bool open_ = false;
    size_t bytes_ = 0;
    uint64_t last_flush_ms_ = 0;
};

} // namespace openu5
