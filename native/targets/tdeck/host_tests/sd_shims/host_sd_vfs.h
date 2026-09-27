#pragma once
// Alpha 3 A3-04G host-test seam: the fake SD card under the REAL alpha_save.cpp.
//
// alpha_save.cpp talks to the card through stdio/POSIX calls on "/sd/..."
// paths (ESP-IDF's VFS). This header is force-included (-include) into that
// one translation unit only, in the a3_04g_storage_runtime target. It pulls in
// every standard header alpha_save.cpp uses FIRST, and only then renames the
// file calls, so no library declaration is rewritten. The production source is
// compiled verbatim.
//
// What the fake card does (host_sd_vfs.cpp):
//   * "/sd/..." is redirected under a per-run host directory; the files are
//     real, so fread/fwrite/fseek/ftell/rename behave as a filesystem does;
//   * every open, read, write, rename, unlink, mkdir and stat is counted,
//     with bytes, and a probe runs at each read/write (the census asks what
//     is alive in RAM while the card is busy);
//   * faults are injected by path substring: open failure, short read, write
//     failure, rename failure, or no card at all;
//   * esp_vfs_fat_info() reports a configurable free space.
// fsync() stays the board shim's no-op (mingw has none).
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <utility>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_err.h"

FILE *host_sd_fopen(const char *path, const char *mode);
size_t host_sd_fread(void *out, size_t size, size_t count, FILE *f);
size_t host_sd_fwrite(const void *data, size_t size, size_t count, FILE *f);
int host_sd_fclose(FILE *f);
int host_sd_fflush(FILE *f);
int host_sd_fseek(FILE *f, long offset, int whence);
long host_sd_ftell(FILE *f);
void host_sd_rewind(FILE *f);
int host_sd_rename(const char *from, const char *to);
int host_sd_unlink(const char *path);
int host_sd_mkdir(const char *path, int mode);
int host_sd_stat(const char *path, struct stat *out);
esp_err_t esp_vfs_fat_info(const char *base_path, uint64_t *total_bytes, uint64_t *free_bytes);

namespace std {
using ::host_sd_fclose;
using ::host_sd_fflush;
using ::host_sd_fopen;
using ::host_sd_fread;
using ::host_sd_fseek;
using ::host_sd_ftell;
using ::host_sd_fwrite;
using ::host_sd_rewind;
} // namespace std

#define fopen host_sd_fopen
#define fread host_sd_fread
#define fwrite host_sd_fwrite
#define fclose host_sd_fclose
#define fflush host_sd_fflush
#define fseek host_sd_fseek
#define ftell host_sd_ftell
#define rewind host_sd_rewind
#define rename host_sd_rename
#define unlink host_sd_unlink
#define mkdir(p, m) host_sd_mkdir(p, m)
#define stat(p, b) host_sd_stat(p, b)
