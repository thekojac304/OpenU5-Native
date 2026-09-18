#pragma once

#include <array>
#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "input_events.h"
#include "keyboard_recovery.h"

namespace tdeck {

class InputHardware {
public:
    ~InputHardware();
    esp_err_t initialize();
    bool poll(RawInputEvent &event);
    bool keyboard_online() const { return keyboard_device_ != nullptr; }
    void log_metrics() const;

private:
    struct GpioInterruptContext {
        InputHardware *owner = nullptr;
        uint32_t notification_bit = 0;
    };

    esp_err_t start_capture_task();
    static void capture_task_entry(void *);
    static void gpio_interrupt_entry(void *);
    void capture_task();
    void configure_gpio_wakeups();
    void remove_gpio_wakeups();
    bool service_once(RawInputEvent &event);
    void queue_event(const RawInputEvent &event);
    void *i2c_bus_ = nullptr;
    void *keyboard_device_ = nullptr;
    KeyboardMatrix keyboard_matrix_{};
    std::array<KeyboardEvent, kKeyboardKeyCount> keyboard_events_{};
    size_t keyboard_event_count_ = 0;
    size_t keyboard_event_index_ = 0;
    bool trackball_levels_[4] = {true, true, true, true};
    bool keyboard_interrupt_level_ = true;
    bool keyboard_recovering_ = false;
    KeyboardRecoveryPolicy keyboard_recovery_{};
    int64_t next_keyboard_poll_us_ = 0;
    int64_t next_keyboard_error_log_us_ = 0;
    uint32_t keyboard_read_count_ = 0;
    uint32_t keyboard_error_count_ = 0;
    uint32_t keyboard_recovery_count_ = 0;
    uint32_t trace_poll_ = 0;
    uint32_t trace_mode_attempt_ = 0;
    int64_t trace_last_read_us_ = 0;
    int64_t trace_last_service_us_ = 0;
    int64_t trace_next_service_gap_log_us_ = 0;
    int64_t trace_service_gap_high_us_ = 0;
    QueueHandle_t event_queue_ = nullptr;
    TaskHandle_t capture_task_ = nullptr;
    volatile bool capture_running_ = false;
    GpioInterruptContext gpio_interrupts_[5]{};
    bool gpio_handlers_[5]{};
    uint32_t pending_gpio_edges_ = 0;
    uint32_t input_task_wakes_ = 0;
    uint32_t input_task_blocks_ = 0;
    uint32_t input_task_irq_wakes_ = 0;
    uint32_t input_task_idle_waits_ = 0;
    uint32_t input_task_recovery_steps_ = 0;
    uint32_t input_task_max_work_us_ = 0;
    uint32_t queued_event_count_ = 0;
    uint32_t consumed_event_count_ = 0;
    uint32_t dropped_event_count_ = 0;
    uint32_t queue_high_water_ = 0;
    uint32_t trackball_raw_edges_ = 0;
    int64_t consumer_last_poll_us_ = 0;
    int64_t consumer_gap_high_us_ = 0;
    int64_t consumer_next_gap_log_us_ = 0;
    void trace_snapshot(const uint8_t *bytes, esp_err_t result, int64_t start);
};

}  // namespace tdeck
