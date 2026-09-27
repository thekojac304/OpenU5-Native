#pragma once
// Alpha 3 A3-04G: the test side of the fake SD card (host_sd_vfs.h) and of the
// capability-aware heap_caps shim (sd_shims/esp_heap_caps.h). Never linked into
// firmware.
#include <cstddef>
#include <cstdint>
#include <string>

namespace host_sd {

struct Counters {
    uint32_t opens = 0, open_reads = 0, open_writes = 0, open_failures = 0;
    uint32_t reads = 0, writes = 0, closes = 0, renames = 0, unlinks = 0, mkdirs = 0, stats = 0, fat_info = 0;
    uint64_t bytes_read = 0, bytes_written = 0;
    int open_handles = 0, max_open_handles = 0;
    // sdlog::begin/end_storage_transaction, i.e. SdHeadroomGuard windows.
    uint32_t windows_opened = 0, windows_closed = 0;
};

// Where the card lives on the host. Every "/sd/..." path maps to root + path.
void set_root(const std::string &host_directory);
const std::string &root();
// Deletes everything under root()/sd (a blank card), keeps the counters.
void format_card();
Counters &counters();
void reset_counters();

// Faults, by path substring ("" = none). Each stays until cleared.
struct Faults {
    std::string fail_open;     // fopen returns null (errno EIO)
    std::string short_read;    // fread returns half of what was asked
    std::string fail_write;    // fwrite writes nothing
    std::string fail_rename;   // rename returns -1
    bool no_card = false;      // every call fails as with no mounted card
    uint64_t free_bytes = 8ull << 30;
};
Faults &faults();

// Called at every read and write, with the operation and the file's path.
using Probe = void (*)(const char *op, const char *path);
void set_probe(Probe);

// The card's files, as bytes (for tests that damage or compare them).
bool read_card_file(const char *sd_path, std::string &out);
bool write_card_file(const char *sd_path, const std::string &bytes);
bool remove_card_file(const char *sd_path);

} // namespace host_sd

namespace host_heap {

// heap_caps_* calls (the PSRAM scratch, the 8 KiB DMA headroom), by caps.
struct CapsCounters {
    uint32_t mallocs = 0, frees = 0, failures = 0;
    uint64_t live_bytes = 0;
    uint64_t live_dma_bytes = 0;   // requested with MALLOC_CAP_DMA
    uint64_t live_psram_bytes = 0; // requested with MALLOC_CAP_SPIRAM
};
CapsCounters &caps_counters();
// The next heap_caps allocation whose caps include `caps` fails (once).
void fail_next(uint32_t caps);

} // namespace host_heap
