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

// Alpha 3 A3-04E (ALPHA3_AUDIO.md section 22): the scheduler calls of the
// game thread, observable. Both are no-ops unless a test installs a hook --
// a3_04e_pacing_runtime does, to put the REAL tdeck_board.cpp's draw-loop
// pauses on its model of the 100 Hz tick (host_tests/board_shims).
struct OpenU5HostScheduler {
    void (*delay)(TickType_t ticks) = nullptr;
    void (*yield)() = nullptr;
};
inline OpenU5HostScheduler &openu5_host_scheduler() {
    static OpenU5HostScheduler hooks;
    return hooks;
}
inline void vTaskDelay(TickType_t ticks) {
    if (auto hook = openu5_host_scheduler().delay) hook(ticks);
}
inline void openu5_host_task_yield() {
    if (auto hook = openu5_host_scheduler().yield) hook();
}
#define taskYIELD() openu5_host_task_yield()
