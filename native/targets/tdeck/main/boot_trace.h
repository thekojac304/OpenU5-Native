#pragma once
#include "esp_attr.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input_trace.h"

namespace debug51 {
struct BootRecord { uint32_t magic; uint32_t count; uint32_t stage; };
// No flash writes. Best-effort retention only; cold power/Launcher can clear RTC.
inline RTC_NOINIT_ATTR BootRecord retained;
inline void stack_checkpoint(const char *name) {
    // ESP-IDF reports bytes (not upstream FreeRTOS words). Historical minimum.
    const unsigned remaining = uxTaskGetStackHighWaterMark(nullptr);
    INPUT_TRACE("STACK stage=%s configured=%d free_min_bytes=%u target_min=4096",
                name, CONFIG_ESP_MAIN_TASK_STACK_SIZE, remaining);
    if (remaining < 4096) {
        ESP_LOGW("M51", "STACK LOW: safety margin below 4096 bytes at %s", name);
    }
}
inline const char *reset_name(esp_reset_reason_t reason) {
    switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-watchdog";
    case ESP_RST_TASK_WDT: return "task-watchdog";
    case ESP_RST_WDT: return "other-watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "other-see-numeric-code";
    }
}
inline void begin() {
    const auto reason = esp_reset_reason();
    const bool valid = retained.magic == 0x4d353144 && reason != ESP_RST_POWERON;
    const uint32_t prior = valid ? retained.stage : 0;
    retained.count = valid ? retained.count + 1 : 1;
    retained.magic = 0x4d353144;
    retained.stage = 1;
    INPUT_TRACE("BOOT app_main-entry t=%lld reset=%s(%d) rtc_valid=%d count=%lu prior_stage=%lu",
        (long long)esp_timer_get_time(), reset_name(reason), (int)reason, valid,
        (unsigned long)retained.count, (unsigned long)prior);
    stack_checkpoint("app_main-entry");
}
inline void stage(uint32_t id, const char *name) {
    retained.stage = id;
    INPUT_TRACE("STAGE t=%lld id=%lu name=%s boot=%lu stack_free_min_bytes=%u",
        (long long)esp_timer_get_time(), (unsigned long)id, name,
        (unsigned long)retained.count, (unsigned)uxTaskGetStackHighWaterMark(nullptr));
}
}
