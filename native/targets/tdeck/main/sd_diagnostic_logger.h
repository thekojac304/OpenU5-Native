#pragma once

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

}  // namespace tdeck::sdlog
