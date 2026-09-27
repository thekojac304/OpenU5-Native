#pragma once

#include "openu5/perf_report.h"

namespace tdeck::sdlog {

constexpr const char *kCardLogPath = "/ultima5/logs/alpha20-frontend-debug.log";

// Installs a transparent ESP-IDF log sink which always forwards to the
// existing serial sink and queues a best-effort copy for the SD card.
bool begin_capture();

// Called after the board has mounted /sd. Creates/rotates/opens the log but
// leaves physical writes paused while boot-time resource I/O is in progress.
bool initialize_storage();

// Starts the low-priority writer after resource initialization is complete.
bool start_writer();

// Serializes FAT/SD access with the background diagnostic writer. Serial log
// capture continues while a persistence transaction owns the card.
bool begin_storage_transaction();
void end_storage_transaction();

// Alpha 3 A3-04C (ALPHA3_AUDIO.md section 20): the writer's storage bursts
// since the last reset -- each wake that wrote or flushed, timed from taking
// the storage mutex to giving it back. The SD card sits on the TFT's SPI bus,
// so a long burst is time a TFT row may have waited. Any thread; never waits.
bool perf_snapshot(openu5::SdLogPerf &out);
void perf_reset();

}  // namespace tdeck::sdlog
