#pragma once
// Batch 11 host-test seam. Minimal stand-in for ESP-IDF's esp_system.h --
// just the reset-reason surface boot_trace.h reads. A host test process was
// never "reset" by hardware, so ESP_RST_POWERON (the reference value
// boot_trace.h treats as "no retained state to trust") is the only faithful
// answer.
#include "esp_err.h"

typedef enum {
    ESP_RST_UNKNOWN = 0,
    ESP_RST_POWERON,
    ESP_RST_EXT,
    ESP_RST_SW,
    ESP_RST_PANIC,
    ESP_RST_INT_WDT,
    ESP_RST_TASK_WDT,
    ESP_RST_WDT,
    ESP_RST_DEEPSLEEP,
    ESP_RST_BROWNOUT,
    ESP_RST_SDIO,
} esp_reset_reason_t;

inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }
