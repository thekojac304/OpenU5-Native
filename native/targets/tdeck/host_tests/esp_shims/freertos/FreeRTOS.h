#pragma once
// Batch 11 host-test seam. Minimal stand-in for ESP-IDF's freertos/FreeRTOS.h.
// alpha_runtime.cpp's command-routing path never touches a queue, semaphore
// or task scheduler; the only FreeRTOS surface it reaches is the stack
// high-water-mark diagnostic in task.h.
#include <cstdint>

using TickType_t = uint32_t;
using StackType_t = uint32_t;
using BaseType_t = int;
#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE 1
#define pdFALSE 0
