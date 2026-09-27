// Alpha 3 A3-04G host-test seam: the fake SD card and the counting heap_caps
// shim. See host_sd_vfs.h (the force-included renames) and host_sd_card.h (the
// test API). Never linked into firmware.
#include "host_sd_card.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_map>

#include "esp_err.h"
#include "esp_heap_caps.h"

namespace fs = std::filesystem;

namespace host_sd {
namespace {
std::string g_root;
Counters g_counters;
Faults g_faults;
Probe g_probe = nullptr;
std::map<FILE *, std::string> g_open;

bool hits(const std::string &needle, const char *path) {
    return !needle.empty() && path && std::strstr(path, needle.c_str());
}
std::string real_path(const char *sd_path) { return g_root + sd_path; }
} // namespace

void set_root(const std::string &dir) { g_root = dir; }
const std::string &root() { return g_root; }
void format_card() {
    std::error_code ec;
    fs::remove_all(fs::path(g_root) / "sd", ec);
    fs::create_directories(fs::path(g_root) / "sd", ec);
}
Counters &counters() { return g_counters; }
void reset_counters() {
    const int handles = g_counters.open_handles;
    g_counters = Counters{};
    g_counters.open_handles = handles;
}
Faults &faults() { return g_faults; }
void set_probe(Probe p) { g_probe = p; }

bool read_card_file(const char *sd_path, std::string &out) {
    std::ifstream in(real_path(sd_path), std::ios::binary);
    if (!in) return false;
    std::ostringstream s;
    s << in.rdbuf();
    out = s.str();
    return true;
}
bool write_card_file(const char *sd_path, const std::string &bytes) {
    std::ofstream o(real_path(sd_path), std::ios::binary | std::ios::trunc);
    if (!o) return false;
    o.write(bytes.data(), std::streamsize(bytes.size()));
    return bool(o);
}
bool remove_card_file(const char *sd_path) {
    std::error_code ec;
    return fs::remove(real_path(sd_path), ec);
}

// Internal to the renamed calls below.
const char *path_of(FILE *f) {
    auto it = g_open.find(f);
    return it == g_open.end() ? "" : it->second.c_str();
}
void track_open(FILE *f, const char *path) { g_open[f] = path; }
void track_close(FILE *f) { g_open.erase(f); }
void probe(const char *op, const char *path) {
    if (g_probe) g_probe(op, path);
}
bool no_card() { return g_faults.no_card; }

} // namespace host_sd

using namespace host_sd;

FILE *host_sd_fopen(const char *path, const char *mode) {
    auto &c = counters();
    const bool write = std::strchr(mode, 'w') || std::strchr(mode, 'a');
    if (no_card() || hits(faults().fail_open, path)) {
        ++c.open_failures;
        errno = EIO;
        return nullptr;
    }
    FILE *f = std::fopen(real_path(path).c_str(), mode);
    if (!f) {
        ++c.open_failures;
        if (!errno) errno = ENOENT;
        return nullptr;
    }
    ++c.opens;
    ++(write ? c.open_writes : c.open_reads);
    if (++c.open_handles > c.max_open_handles) c.max_open_handles = c.open_handles;
    track_open(f, path);
    return f;
}
size_t host_sd_fread(void *out, size_t size, size_t count, FILE *f) {
    const char *path = path_of(f);
    probe("read", path);
    size_t want = count;
    if (hits(faults().short_read, path)) want = count / 2;
    const size_t got = std::fread(out, size, want, f);
    ++counters().reads;
    counters().bytes_read += uint64_t(got) * size;
    return got;
}
size_t host_sd_fwrite(const void *data, size_t size, size_t count, FILE *f) {
    const char *path = path_of(f);
    probe("write", path);
    ++counters().writes;
    if (hits(faults().fail_write, path)) {
        errno = EIO;
        return 0;
    }
    const size_t put = std::fwrite(data, size, count, f);
    counters().bytes_written += uint64_t(put) * size;
    return put;
}
int host_sd_fclose(FILE *f) {
    track_close(f);
    ++counters().closes;
    --counters().open_handles;
    return std::fclose(f);
}
int host_sd_fflush(FILE *f) { return std::fflush(f); }
int host_sd_fseek(FILE *f, long offset, int whence) { return std::fseek(f, offset, whence); }
long host_sd_ftell(FILE *f) { return std::ftell(f); }
void host_sd_rewind(FILE *f) { std::rewind(f); }
int host_sd_rename(const char *from, const char *to) {
    ++counters().renames;
    if (no_card() || hits(faults().fail_rename, from) || hits(faults().fail_rename, to)) {
        errno = EIO;
        return -1;
    }
    std::error_code ec;
    fs::rename(real_path(from), real_path(to), ec);
    if (ec) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}
int host_sd_unlink(const char *path) {
    ++counters().unlinks;
    if (no_card()) {
        errno = EIO;
        return -1;
    }
    std::error_code ec;
    if (!fs::remove(real_path(path), ec)) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}
int host_sd_mkdir(const char *path, int) {
    ++counters().mkdirs;
    if (no_card()) {
        errno = EIO;
        return -1;
    }
    std::error_code ec;
    const fs::path p = real_path(path);
    if (fs::exists(p, ec)) {
        errno = EEXIST;
        return -1;
    }
    if (!fs::create_directory(p, ec)) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}
int host_sd_stat(const char *path, struct stat *out) {
    ++counters().stats;
    if (no_card()) {
        errno = EIO;
        return -1;
    }
    return ::stat(real_path(path).c_str(), out);
}
esp_err_t esp_vfs_fat_info(const char *, uint64_t *total_bytes, uint64_t *free_bytes) {
    ++counters().fat_info;
    if (no_card()) {
        errno = EIO;
        return ESP_FAIL;
    }
    *total_bytes = 32ull << 30;
    *free_bytes = faults().free_bytes;
    return ESP_OK;
}

// sd_diagnostic_logger.h's storage transaction: the SdHeadroomGuard window.
namespace tdeck::sdlog {
bool begin_storage_transaction() {
    ++host_sd::counters().windows_opened;
    return true;
}
void end_storage_transaction() { ++host_sd::counters().windows_closed; }
} // namespace tdeck::sdlog

namespace host_heap {
namespace {
CapsCounters g_caps;
uint32_t g_fail_caps = 0;
struct Block {
    size_t size;
    uint32_t caps;
};
std::unordered_map<void *, Block> g_blocks;
} // namespace
CapsCounters &caps_counters() { return g_caps; }
void fail_next(uint32_t caps) { g_fail_caps = caps; }
} // namespace host_heap

using namespace host_heap;

void *host_heap_caps_malloc(size_t size, uint32_t caps) {
    if (g_fail_caps && (caps & g_fail_caps) == g_fail_caps) {
        g_fail_caps = 0;
        ++g_caps.failures;
        return nullptr;
    }
    void *p = std::malloc(size);
    if (!p) return nullptr;
    ++g_caps.mallocs;
    g_blocks[p] = {size, caps};
    g_caps.live_bytes += size;
    if (caps & MALLOC_CAP_DMA) g_caps.live_dma_bytes += size;
    if (caps & MALLOC_CAP_SPIRAM) g_caps.live_psram_bytes += size;
    return p;
}
void *host_heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
    void *p = host_heap_caps_malloc(n * size, caps);
    if (p) std::memset(p, 0, n * size);
    return p;
}
void host_heap_caps_free(void *p) {
    if (!p) return;
    auto it = g_blocks.find(p);
    if (it != g_blocks.end()) {
        ++g_caps.frees;
        g_caps.live_bytes -= it->second.size;
        if (it->second.caps & MALLOC_CAP_DMA) g_caps.live_dma_bytes -= it->second.size;
        if (it->second.caps & MALLOC_CAP_SPIRAM) g_caps.live_psram_bytes -= it->second.size;
        g_blocks.erase(it);
    }
    std::free(p);
}
