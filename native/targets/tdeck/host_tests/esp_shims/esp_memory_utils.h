#pragma once
// Batch 22 host-test seam. Minimal stand-in for ESP-IDF's esp_memory_utils.h,
// used only by the U5OBJ diagnostic to name the heap a pointer lives in. The
// host has one heap; "internal" is the honest answer and decides nothing.
inline bool esp_ptr_external_ram(const void *) { return false; }
inline bool esp_ptr_internal(const void *p) { return p != nullptr; }
