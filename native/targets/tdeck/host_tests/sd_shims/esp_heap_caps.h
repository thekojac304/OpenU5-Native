#pragma once
// Alpha 3 A3-04G host-test seam: esp_heap_caps.h for the a3_04g_storage_runtime
// target, found before esp_shims/esp_heap_caps.h. Same constants; the calls go
// through host_sd_vfs.cpp, which counts them by capability and can fail one on
// request (host_heap::fail_next), so a test reaches the PSRAM-scratch failure
// path of alpha_save.cpp. Free-size queries report a fixed large value, as the
// esp_shims version does: the host has no regions to measure.
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#define MALLOC_CAP_SPIRAM (1 << 0)
#define MALLOC_CAP_INTERNAL (1 << 1)
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_DMA (1 << 3)
#define MALLOC_CAP_DEFAULT (1 << 4)

void *host_heap_caps_malloc(size_t size, uint32_t caps);
void *host_heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void host_heap_caps_free(void *p);

inline void *heap_caps_malloc(size_t size, uint32_t caps) { return host_heap_caps_malloc(size, caps); }
inline void *heap_caps_calloc(size_t n, size_t size, uint32_t caps) { return host_heap_caps_calloc(n, size, caps); }
inline void heap_caps_free(void *p) { host_heap_caps_free(p); }
inline size_t heap_caps_get_free_size(uint32_t /*caps*/) { return size_t(64) * 1024 * 1024; }
inline size_t heap_caps_get_largest_free_block(uint32_t /*caps*/) { return size_t(64) * 1024 * 1024; }
inline size_t heap_caps_get_minimum_free_size(uint32_t /*caps*/) { return size_t(64) * 1024 * 1024; }
