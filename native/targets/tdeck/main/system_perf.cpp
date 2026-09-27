#include "system_perf.h"

#include <cstring>

#include "esp_heap_caps.h"
#include "esp_timer.h"

namespace tdeck {
namespace {
constexpr uint32_t kInternal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, kPsram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

uint32_t permille(uint64_t part, uint64_t whole) {
    if (!whole) return 0;
    const uint64_t p = part * 1000 / whole;
    return uint32_t(p > 1000 ? 1000 : p);
}
uint64_t delta(uint64_t now, uint64_t base) { return now >= base ? now - base : 0; }
} // namespace

bool SystemPerf::begin() {
    if (!status_) status_ = static_cast<TaskStatus_t *>(heap_caps_calloc(kMaxTasks, sizeof(TaskStatus_t), kPsram));
    system_perf_reset();
    return status_ != nullptr;
}

bool SystemPerf::sample(Sample &s) {
    s = Sample{};
#if configGENERATE_RUN_TIME_STATS
    if (!status_) return false;
    configRUN_TIME_COUNTER_TYPE total = 0;
    const UBaseType_t n = uxTaskGetSystemState(status_, kMaxTasks, &total);
    if (!n) return false; // the table is too small: report nothing rather than a partial picture
    const TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCore(0), idle1 = xTaskGetIdleTaskHandleForCore(1);
    s.total = uint64_t(esp_timer_get_time());
    for (UBaseType_t i = 0; i < n; ++i) {
        const TaskStatus_t &t = status_[i];
        const uint64_t run = uint64_t(t.ulRunTimeCounter);
        // IDF stacks are counted in bytes (StackType_t is one byte wide).
        const uint32_t stack = uint32_t(t.usStackHighWaterMark * sizeof(StackType_t));
        const char *name = t.pcTaskName ? t.pcTaskName : "";
        s.all += run;
        if (t.xHandle == idle0) {
            s.idle[0] += run;
        } else if (t.xHandle == idle1) {
            s.idle[1] += run;
        } else if (std::strcmp(name, kAudioTask) == 0) {
            s.audio += run;
            s.stack_audio = stack;
        } else if (std::strcmp(name, kMainTask) == 0) {
            s.main += run;
            s.stack_main = stack;
        } else if (std::strcmp(name, kInputTask) == 0) {
            s.input += run;
            s.stack_input = stack;
        } else if (std::strcmp(name, kSdLogTask) == 0) {
            s.sdlog += run;
        }
    }
    s.valid = true;
    return true;
#else
    return false;
#endif
}

void SystemPerf::system_perf_reset() { sample(base_); }

bool SystemPerf::system_perf_snapshot(openu5::SystemPerfSnapshot &out) {
    out = openu5::SystemPerfSnapshot{};
    Sample now{};
    const bool sampled = sample(now);
    if (sampled && base_.valid && now.total > base_.total) {
        const uint64_t window = now.total - base_.total;
        out.valid = true;
        out.window_us = uint32_t(window);
        for (int c = 0; c < 2; ++c) out.core_busy_permille[c] = 1000 - permille(delta(now.idle[c], base_.idle[c]), window);
        // A task created after the reset (the audio task starts with the
        // first sound) had a zero base: its whole counter is in the window.
        out.audio_permille = permille(delta(now.audio, base_.audio), window);
        out.main_permille = permille(delta(now.main, base_.main), window);
        out.input_permille = permille(delta(now.input, base_.input), window);
        out.sdlog_permille = permille(delta(now.sdlog, base_.sdlog), window);
        const uint64_t named = delta(now.idle[0], base_.idle[0]) + delta(now.idle[1], base_.idle[1]) +
                               delta(now.audio, base_.audio) + delta(now.main, base_.main) +
                               delta(now.input, base_.input) + delta(now.sdlog, base_.sdlog);
        out.other_permille = permille(delta(delta(now.all, base_.all), named), window);
    }
    if (sampled) {
        out.stack_main_free = now.stack_main;
        out.stack_audio_free = now.stack_audio;
        out.stack_input_free = now.stack_input;
    }
    out.heap_internal_free = uint32_t(heap_caps_get_free_size(kInternal));
    out.heap_internal_min = uint32_t(heap_caps_get_minimum_free_size(kInternal));
    out.heap_psram_free = uint32_t(heap_caps_get_free_size(kPsram));
    out.heap_psram_min = uint32_t(heap_caps_get_minimum_free_size(kPsram));
    return true;
}

} // namespace tdeck
