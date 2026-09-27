#pragma once

#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "openu5/perf_report.h"

namespace tdeck {

// Alpha 3 A3-04B (ALPHA3_AUDIO.md section 19.4): the whole machine's view of
// a measurement window, from the FreeRTOS run-time statistics
// (CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS, clocked by esp_timer, 64-bit
// counters -- sdkconfig.defaults): how busy each core was (1 - its idle
// task's share), how much of that was the audio task, the game loop (the
// main task), the keyboard task and the SD-log writer, plus heap and stack
// headroom. It is what tells "the music synth is starving the renderer"
// apart from "the renderer's own core is saturated" on the device.
//
// Tasks are found by the names they are created with ("main" is ESP-IDF's,
// the others are this firmware's), so a task that starts mid-window -- the
// audio task starts with the first sound -- is simply counted from zero.
//
// Game thread only. reset() and snapshot() read the kernel's task list
// (uxTaskGetSystemState suspends the scheduler for the copy, a few tens of
// microseconds); they run only when the Developer report or the 5 s
// heartbeat asks, never per frame. The task table lives in PSRAM.
class SystemPerf final : public openu5::SystemPerfSource {
  public:
    static constexpr const char *kMainTask = "main";
    static constexpr const char *kAudioTask = "openu5-audio";
    static constexpr const char *kInputTask = "openu5-input";
    static constexpr const char *kSdLogTask = "alpha20-sd-log";

    /** Boot: allocate the PSRAM task table and start the first window. */
    bool begin();

    void system_perf_reset() override;
    bool system_perf_snapshot(openu5::SystemPerfSnapshot &) override;

  private:
    static constexpr size_t kMaxTasks = 24;
    struct Sample {
        bool valid = false;
        uint64_t total = 0; // the run-time clock (esp_timer microseconds)
        uint64_t idle[2]{};
        uint64_t audio = 0, main = 0, input = 0, sdlog = 0, all = 0;
        uint32_t stack_main = 0, stack_audio = 0, stack_input = 0;
    };
    bool sample(Sample &);

    Sample base_{};
    TaskStatus_t *status_ = nullptr; // kMaxTasks entries, PSRAM
};

} // namespace tdeck
