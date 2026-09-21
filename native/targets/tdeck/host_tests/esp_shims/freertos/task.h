#pragma once
// Batch 11 host-test seam. Minimal stand-in for ESP-IDF's freertos/task.h.
// Used only for the stack-margin diagnostic (log text, never a gameplay
// decision); a host test process has no bounded FreeRTOS task stack, so a
// constant well above every LOW-margin warning threshold in the production
// code is a harmless, honest stand-in.
#include "FreeRTOS.h"

using TaskHandle_t = void *;
#ifndef CONFIG_ESP_MAIN_TASK_STACK_SIZE
#define CONFIG_ESP_MAIN_TASK_STACK_SIZE 8192
#endif

inline unsigned uxTaskGetStackHighWaterMark(TaskHandle_t) { return 8192; }
inline void vTaskDelay(TickType_t) {}
