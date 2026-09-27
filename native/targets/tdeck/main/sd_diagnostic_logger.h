#pragma once

#include "openu5/perf_report.h"
#include "openu5/sd_diag_log.h"

namespace tdeck::sdlog {

constexpr const char *kCardLogPath = "/ultima5/logs/alpha20-frontend-debug.log";

// Installs a transparent ESP-IDF log sink which always forwards to the
// existing serial sink and, while SD diagnostic logging is on, queues a
// best-effort copy for the SD card.
bool begin_capture();

// Called after the board has mounted /sd: the log may now be switched on.
// A3-04D: the log is no longer opened here -- the writer opens it when
// logging is switched on (it is off at boot, openu5::kSdDiagLoggingDefault).
bool initialize_storage();

// Starts the low-priority writer after resource initialization is complete.
// While logging is off the writer sleeps without a timeout.
bool start_writer();

// Alpha 3 A3-04D (ALPHA3_AUDIO.md section 21): Developer > Diagnostics >
// "Probe: SD diag logging". Off at boot, never saved. Switching off writes
// what was captured, closes the log (its directory entry then holds the
// size) and stops every diagnostic card access; serial logging is unchanged.
// set_enabled(true) is refused (false) when there is no card or writer.
openu5::SdLogState state();
bool set_enabled(bool on);

// 1 while the writer holds the storage mutex for a wake (it may be in a card
// command, which holds the TFT's SPI bus). The Board reads it at both ends
// of every TFT transaction.
const volatile uint32_t *burst_flag();

// Serializes FAT/SD access with the background diagnostic writer. Serial log
// capture continues while a persistence transaction owns the card.
bool begin_storage_transaction();
void end_storage_transaction();

// Alpha 3 A3-04C (ALPHA3_AUDIO.md section 20): the writer's storage bursts
// since the last reset -- each wake that wrote or flushed, timed from taking
// the storage mutex to giving it back. The SD card sits on the TFT's SPI bus,
// so a long burst is time a TFT row may have waited. Any thread; never waits.
// A3-04D: `off` says the log is switched off (no card access in that state).
bool perf_snapshot(openu5::SdLogPerf &out);
void perf_reset();

}  // namespace tdeck::sdlog
