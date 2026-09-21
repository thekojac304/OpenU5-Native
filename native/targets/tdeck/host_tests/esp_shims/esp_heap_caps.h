#pragma once
// Batch 11 host-test seam. Minimal stand-in for ESP-IDF's esp_heap_caps.h.
// The real header steers allocations to specific memory regions (PSRAM vs
// internal RAM); host tests never call AlphaRuntime::initialize() (the only
// caller of these), but the whole translation unit still needs the symbols
// to compile and link, so plain malloc/calloc/free are a harmless substitute
// -- capability selection has no gameplay meaning, only a device memory-
// budget one.
#include <cstdlib>
#include <cstddef>
#include <cstdint>

#define MALLOC_CAP_SPIRAM (1 << 0)
#define MALLOC_CAP_INTERNAL (1 << 1)
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_DMA (1 << 3)
#define MALLOC_CAP_DEFAULT (1 << 4)

inline void *heap_caps_malloc(size_t size, uint32_t /*caps*/) { return std::malloc(size); }
inline void *heap_caps_calloc(size_t n, size_t size, uint32_t /*caps*/) { return std::calloc(n, size); }
inline void heap_caps_free(void *p) { std::free(p); }
inline size_t heap_caps_get_free_size(uint32_t /*caps*/) { return size_t(64) * 1024 * 1024; }
