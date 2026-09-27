// Alpha 3 A3-04E.1 -- see idle_service.h and ALPHA3_AUDIO.md section 23.
#include "idle_service.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace tdeck {
namespace {
// Written only by core 0's idle task, read by the game thread on the same core.
volatile uint32_t g_core0_idle_passes = 0;
} // namespace

bool IdleService::core0_hook() {
    g_core0_idle_passes = g_core0_idle_passes + 1;
    return true; // the core may still wait for an interrupt (waiti) after the hooks
}

const volatile uint32_t *IdleService::core0_count() { return &g_core0_idle_passes; }

void IdleService::attach(const volatile uint32_t *count) {
    count_ = count;
    if (count_) guard_.reset(*count_, uint64_t(esp_timer_get_time()));
}

bool IdleService::enforce() {
    if (!count_ || !guard_.due(*count_, uint64_t(esp_timer_get_time()))) return false;
    // A tick sleep blocks this task until the next tick: the idle task has
    // core 0 meanwhile. The first may end a microsecond later, so repeat
    // (bounded) until the idle loop is seen to have run.
    const int64_t start = esp_timer_get_time();
    uint32_t sleeps = 0;
    bool serviced = false;
    while (sleeps < openu5::kIdleServiceMaxSleeps) {
        vTaskDelay(1);
        ++sleeps;
        if (!guard_.due(*count_, uint64_t(esp_timer_get_time()))) {
            serviced = true;
            break;
        }
    }
    guard_.note_enforcement(sleeps, uint32_t(esp_timer_get_time() - start), serviced);
    return true;
}

} // namespace tdeck
