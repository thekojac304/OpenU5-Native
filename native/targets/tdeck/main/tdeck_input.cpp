#include "tdeck_input.h"
#include "input_trace.h"

#include <array>
#include <cstring>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "tdeck_pins.h"

namespace tdeck {
namespace {
constexpr char kTag[] = "TDeckInput";
constexpr uint16_t kKeyboardAddress = 0x55;
constexpr uint8_t kKeyboardRawModeCommand = 0x03;
constexpr int kI2cFrequencyHz = 100000;
constexpr int kI2cTimeoutMs = 10;
// The keyboard C3 firmware performs a full 5x7 scan with a 1 ms settle per
// cell. Reading every 10 ms can collide with that roughly 35 ms scan over and
// over, so use INT falling edges for low-latency reads and a conservative
// fallback interval because older raw-mode firmware does not assert INT.
constexpr int64_t kKeyboardPollIntervalUs = KeyboardRecoveryPolicy::kScanPeriodUs;
// A delay expressed as 1 ms becomes zero ticks with this target's 100 Hz
// scheduler. One explicit tick is the bounded fallback cadence; GPIO falling
// edges wake the task immediately between those polls.
constexpr TickType_t kInputFallbackWaitTicks = 1;
constexpr size_t kMaximumEventsPerWake = 8;
constexpr uint32_t kKeyboardNotificationBit = 1U << 0;
constexpr uint32_t kTrackballNotificationBitBase = 1U << 1;
static_assert(kInputFallbackWaitTicks > 0, "input capture must enter the Blocked state");

bool valid_raw_snapshot(const uint8_t *snapshot)
{
    for (size_t column = 0; column < kKeyboardColumns; ++column) {
        if ((snapshot[column] & 0x80U) != 0) return false;
    }
    return true;
}

i2c_master_bus_handle_t bus_handle(void *handle)
{
    return static_cast<i2c_master_bus_handle_t>(handle);
}

i2c_master_dev_handle_t device_handle(void *handle)
{
    return static_cast<i2c_master_dev_handle_t>(handle);
}
}  // namespace

InputHardware::~InputHardware()
{
    capture_running_ = false;
    remove_gpio_wakeups();
    if (capture_task_ != nullptr) xTaskNotifyGive(capture_task_);
    if (capture_task_ != nullptr) vTaskDelete(capture_task_);
    if (event_queue_ != nullptr) vQueueDelete(event_queue_);
    if (keyboard_device_ != nullptr) i2c_master_bus_rm_device(device_handle(keyboard_device_));
    if (i2c_bus_ != nullptr) i2c_del_master_bus(bus_handle(i2c_bus_));
}

esp_err_t InputHardware::initialize()
{
    INPUT_TRACE("INIT keyboard t=%lld", (long long)esp_timer_get_time());
    const gpio_config_t inputs = {
        .pin_bit_mask = (1ULL << pins::kKeyboardInterrupt) |
                        (1ULL << pins::kTrackballUp) |
                        (1ULL << pins::kTrackballDown) |
                        (1ULL << pins::kTrackballLeft) |
                        (1ULL << pins::kTrackballRight),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&inputs), kTag, "configure keyboard/trackball inputs");

    const std::array<gpio_num_t, 4> trackball = {
        pins::kTrackballUp, pins::kTrackballDown, pins::kTrackballLeft, pins::kTrackballRight};
    for (size_t i = 0; i < trackball.size(); ++i) {
        trackball_levels_[i] = gpio_get_level(trackball[i]) != 0;
    }
    keyboard_interrupt_level_ = gpio_get_level(pins::kKeyboardInterrupt) != 0;

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = pins::kI2cData,
        .scl_io_num = pins::kI2cClock,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {.enable_internal_pullup = true, .allow_pd = false},
    };
    i2c_master_bus_handle_t bus = nullptr;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), kTag, "initialize I2C bus");
    i2c_bus_ = bus;
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kKeyboardAddress,
        .scl_speed_hz = kI2cFrequencyHz,
        .scl_wait_us = 0,
        .flags = {.disable_ack_check = false},
    };
    i2c_master_dev_handle_t device = nullptr;
    const esp_err_t add_result = i2c_master_bus_add_device(bus, &device_config, &device);
    if (add_result != ESP_OK) return add_result;
    keyboard_device_ = device;

    const esp_err_t probe_result = i2c_master_probe(bus, kKeyboardAddress, kI2cTimeoutMs);
    if (probe_result != ESP_OK) {
        ESP_LOGW(kTag, "KEYBOARD_ERROR probe=%s address=0x55; trackball remains available",
                 esp_err_to_name(probe_result));
        i2c_master_bus_rm_device(device);
        keyboard_device_ = nullptr;
        return start_capture_task();
    }
    ++trace_mode_attempt_;
    const esp_err_t mode_result = i2c_master_transmit(
        device, &kKeyboardRawModeCommand, 1, kI2cTimeoutMs);
    INPUT_TRACE("MODE t=%lld attempt=%lu cmd=03 result=%s (ACK is not capability proof)",
                (long long)esp_timer_get_time(), (unsigned long)trace_mode_attempt_, esp_err_to_name(mode_result));
    if (mode_result != ESP_OK) {
        ESP_LOGW(kTag, "KEYBOARD_ERROR raw_mode_start=%s; keeping device for bounded recovery",
                 esp_err_to_name(mode_result));
        keyboard_recovering_ = true;
        const int64_t recovery_start = esp_timer_get_time();
        keyboard_recovery_.failures = 2;
        keyboard_recovery_.step = KeyboardRecoveryStep::ResetBus;
        keyboard_recovery_.due_us = recovery_start + KeyboardRecoveryPolicy::kResetSettleUs;
        next_keyboard_poll_us_ = keyboard_recovery_.due_us;
        next_keyboard_error_log_us_ = recovery_start;
        ESP_LOGI(kTag, "Keyboard recovery pending: I2C address 0x55, SDA=%d SCL=%d INT=%d",
                 pins::kI2cData, pins::kI2cClock, pins::kKeyboardInterrupt);
        return start_capture_task();
    }

    uint8_t initial_snapshot[kKeyboardColumns]{};
    const int64_t initial_start = esp_timer_get_time();
    esp_err_t initial_result = i2c_master_receive(
        device, initial_snapshot, sizeof(initial_snapshot), kI2cTimeoutMs);
    if (initial_result == ESP_OK && !valid_raw_snapshot(initial_snapshot)) {
        initial_result = ESP_ERR_INVALID_RESPONSE;
    }
    trace_snapshot(initial_snapshot, initial_result, initial_start);
    if (initial_result != ESP_OK) {
        ESP_LOGW(kTag, "KEYBOARD_ERROR initial_snapshot=%s; preserving device for recovery",
                 esp_err_to_name(initial_result));
        keyboard_matrix_.desynchronize();
        keyboard_recovering_ = true;
        keyboard_recovery_.read_failed(esp_timer_get_time());
    } else {
        keyboard_matrix_.apply_snapshot(initial_snapshot, nullptr, 0);
        INPUT_TRACE("QUEUE poll=%lu count=0 reason=initial-baseline-edges-suppressed", (unsigned long)trace_poll_);
    }
    const int64_t ready = esp_timer_get_time();
    next_keyboard_poll_us_ = ready + (initial_result == ESP_OK
                                          ? kKeyboardPollIntervalUs
                                          : KeyboardRecoveryPolicy::kScanPeriodUs);
    ESP_LOGI(kTag, "Keyboard online: I2C address 0x55, SDA=%d SCL=%d INT=%d",
             pins::kI2cData, pins::kI2cClock, pins::kKeyboardInterrupt);
    ESP_LOGI(kTag, "Keyboard raw matrix mode active; press/release/modifiers tracked by snapshot");
    ESP_LOGI(kTag, "Directional movement: trackball up=%d down=%d left=%d right=%d",
             pins::kTrackballUp, pins::kTrackballDown,
             pins::kTrackballLeft, pins::kTrackballRight);
    return start_capture_task();
}

esp_err_t InputHardware::start_capture_task()
{
    if (capture_task_ != nullptr) return ESP_OK;
    event_queue_ = xQueueCreate(64, sizeof(RawInputEvent));
    if (event_queue_ == nullptr) return ESP_ERR_NO_MEM;
    capture_running_ = true;
    if (xTaskCreatePinnedToCore(capture_task_entry, "openu5-input", 4096, this, 4,
                                &capture_task_, 0) != pdPASS) {
        capture_running_ = false;
        vQueueDelete(event_queue_);
        event_queue_ = nullptr;
        return ESP_ERR_NO_MEM;
    }
    configure_gpio_wakeups();
    ESP_LOGI(kTag,
             "INPUT_QUEUE started capacity=64 task_stack=4096 priority=4 core=0 fallback_ticks=%u fallback_ms=%u gpio_wake=falling-edge",
             unsigned(kInputFallbackWaitTicks),
             unsigned(kInputFallbackWaitTicks * portTICK_PERIOD_MS));
    return ESP_OK;
}

void InputHardware::capture_task_entry(void *context)
{
    static_cast<InputHardware *>(context)->capture_task();
}

void InputHardware::gpio_interrupt_entry(void *context)
{
    auto *interrupt = static_cast<GpioInterruptContext *>(context);
    if (interrupt == nullptr || interrupt->owner == nullptr ||
        interrupt->owner->capture_task_ == nullptr) return;
    BaseType_t higher_priority_woken = pdFALSE;
    xTaskNotifyFromISR(interrupt->owner->capture_task_, interrupt->notification_bit,
                       eSetBits, &higher_priority_woken);
    if (higher_priority_woken == pdTRUE) portYIELD_FROM_ISR();
}

void InputHardware::configure_gpio_wakeups()
{
    const esp_err_t service = gpio_install_isr_service(0);
    if (service != ESP_OK && service != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "INPUT_TASK GPIO ISR service unavailable: %s; timed fallback remains active",
                 esp_err_to_name(service));
        return;
    }
    const std::array<gpio_num_t, 5> wake_pins = {
        pins::kKeyboardInterrupt, pins::kTrackballUp, pins::kTrackballDown,
        pins::kTrackballLeft, pins::kTrackballRight};
    for (size_t i = 0; i < wake_pins.size(); ++i) {
        gpio_interrupts_[i] = {this, i == 0 ? kKeyboardNotificationBit
                                            : kTrackballNotificationBitBase << (i - 1)};
        const esp_err_t type = gpio_set_intr_type(wake_pins[i], GPIO_INTR_NEGEDGE);
        const esp_err_t add = type == ESP_OK
                                  ? gpio_isr_handler_add(wake_pins[i], gpio_interrupt_entry,
                                                         &gpio_interrupts_[i])
                                  : type;
        if (add == ESP_OK) {
            gpio_handlers_[i] = true;
            gpio_intr_enable(wake_pins[i]);
        } else {
            ESP_LOGW(kTag,
                     "INPUT_TASK GPIO wake unavailable pin=%d result=%s; timed fallback remains active",
                     int(wake_pins[i]), esp_err_to_name(add));
        }
    }
}

void InputHardware::remove_gpio_wakeups()
{
    const std::array<gpio_num_t, 5> wake_pins = {
        pins::kKeyboardInterrupt, pins::kTrackballUp, pins::kTrackballDown,
        pins::kTrackballLeft, pins::kTrackballRight};
    for (size_t i = 0; i < wake_pins.size(); ++i) {
        if (!gpio_handlers_[i]) continue;
        gpio_intr_disable(wake_pins[i]);
        gpio_isr_handler_remove(wake_pins[i]);
        gpio_handlers_[i] = false;
    }
}

void InputHardware::capture_task()
{
    while (capture_running_) {
        const int64_t work_started_us = esp_timer_get_time();
        ++input_task_wakes_;
        for (size_t emitted = 0; emitted < kMaximumEventsPerWake; ++emitted) {
            RawInputEvent event{};
            if (!service_once(event)) break;
            queue_event(event);
        }
        const uint32_t work_us = static_cast<uint32_t>(
            std::min<int64_t>(UINT32_MAX, esp_timer_get_time() - work_started_us));
        input_task_max_work_us_ = std::max(input_task_max_work_us_, work_us);

        uint32_t notifications = 0;
        ++input_task_blocks_;
        const BaseType_t notified = xTaskNotifyWait(0, UINT32_MAX, &notifications,
                                                    kInputFallbackWaitTicks);
        if (notified == pdTRUE) {
            ++input_task_irq_wakes_;
            pending_gpio_edges_ |= notifications;
        } else {
            ++input_task_idle_waits_;
        }
    }
    capture_task_ = nullptr;
    vTaskDelete(nullptr);
}

void InputHardware::queue_event(const RawInputEvent &event)
{
    if (xQueueSend(event_queue_, &event, 0) != pdTRUE) {
        ++dropped_event_count_;
        ESP_LOGE(kTag,"INPUT_QUEUE depth=%u high_water=%lu dropped=%lu raw=%s",
                 unsigned(uxQueueMessagesWaiting(event_queue_)),
                 (unsigned long)queue_high_water_,(unsigned long)dropped_event_count_,
                 raw_input_name(event.kind));
        return;
    }
    ++queued_event_count_;
    const uint32_t depth=uint32_t(uxQueueMessagesWaiting(event_queue_));
    if(depth>queue_high_water_)queue_high_water_=depth;
    ESP_LOGD(kTag,"INPUT_EVENT raw=%s queued=%lu consumed=%lu depth=%lu",
             raw_input_name(event.kind),(unsigned long)queued_event_count_,
             (unsigned long)consumed_event_count_,(unsigned long)depth);
}

bool InputHardware::poll(RawInputEvent &event)
{
    event = {};
    const int64_t now=esp_timer_get_time();
    if(consumer_last_poll_us_){
        const int64_t gap=now-consumer_last_poll_us_;
        if(gap>consumer_gap_high_us_)consumer_gap_high_us_=gap;
        if(gap>=75000&&now>=consumer_next_gap_log_us_){consumer_next_gap_log_us_=now+1000000;
            ESP_LOGW(kTag,"INPUT_SERVICE render_block_us=%lld high_us=%lld queue_depth=%u",
                     (long long)gap,(long long)consumer_gap_high_us_,
                     event_queue_?unsigned(uxQueueMessagesWaiting(event_queue_)):0U);}
    }
    consumer_last_poll_us_=now;
    if(event_queue_==nullptr||xQueueReceive(event_queue_,&event,0)!=pdTRUE)return false;
    ++consumed_event_count_;
    ESP_LOGD(kTag,"INPUT_EVENT raw=%s queued=%lu consumed=%lu depth=%u",
             raw_input_name(event.kind),(unsigned long)queued_event_count_,
             (unsigned long)consumed_event_count_,unsigned(uxQueueMessagesWaiting(event_queue_)));
    return true;
}

bool InputHardware::service_once(RawInputEvent &event)
{
    event = {};
    const int64_t now = esp_timer_get_time();
    if (trace_last_service_us_ != 0) {
        const int64_t gap = now - trace_last_service_us_;
        if (gap > trace_service_gap_high_us_) trace_service_gap_high_us_ = gap;
        if (gap >= 75000 && now >= trace_next_service_gap_log_us_) {
            trace_next_service_gap_log_us_ = now + 1000000;
            ESP_LOGW(kTag,
                     "INPUT_SERVICE_GAP gap_us=%lld high_us=%lld (main-loop delay; transport result unknown)",
                     (long long)gap, (long long)trace_service_gap_high_us_);
        }
    }
    trace_last_service_us_ = now;

    // The trackball is the primary navigation input. Sample it on every call,
    // even while keyboard edges are queued, so a short detent cannot be hidden
    // behind a multi-key matrix transition.
    const std::array<gpio_num_t, 4> pins_by_direction = {
        pins::kTrackballUp, pins::kTrackballDown, pins::kTrackballLeft, pins::kTrackballRight};
    const std::array<RawInputKind, 4> kinds = {
        RawInputKind::TrackballUp, RawInputKind::TrackballDown,
        RawInputKind::TrackballLeft, RawInputKind::TrackballRight};
    for (size_t i = 0; i < pins_by_direction.size(); ++i) {
        const bool level = gpio_get_level(pins_by_direction[i]) != 0;
        const uint32_t notification_bit = kTrackballNotificationBitBase << i;
        const bool notified_edge = (pending_gpio_edges_ & notification_bit) != 0;
        pending_gpio_edges_ &= ~notification_bit;
        const bool falling_edge = notified_edge || (trackball_levels_[i] && !level);
        trackball_levels_[i] = level;
        if (falling_edge) {
            ++trackball_raw_edges_;
            INPUT_TRACE("TB t=%lld dir=%s queue=%u/%u last_read=%lld",
                        (long long)now, raw_input_name(kinds[i]),
                        (unsigned)keyboard_event_index_, (unsigned)keyboard_event_count_,
                        (long long)trace_last_read_us_);
            event = {
                .kind = kinds[i],
                .code = static_cast<uint8_t>(pins_by_direction[i]),
                .transition = KeyTransition::Pressed,
                .modifiers = keyboard_matrix_.modifiers(),
                .timestamp_us = now,
            };
            return true;
        }
    }

    if (keyboard_event_index_ < keyboard_event_count_) {
        const KeyboardEvent &key = keyboard_events_[keyboard_event_index_++];
        event = {
            .kind = RawInputKind::Keyboard,
            .code = key.code,
            .transition = key.transition,
            .modifiers = key.modifiers,
            .column = key.column,
            .row = key.row,
            .modifier_key = key.modifier_key,
            .base_code = key.base_code,
            .symbol_code = key.symbol_code,
            .timestamp_us = now,
        };
        std::memcpy(event.snapshot,key.snapshot,sizeof(event.snapshot));
        std::memcpy(event.previous_snapshot,key.previous_snapshot,sizeof(event.previous_snapshot));
        return true;
    }
    keyboard_event_index_ = 0;
    keyboard_event_count_ = 0;

    const bool interrupt_level = gpio_get_level(pins::kKeyboardInterrupt) != 0;
    const bool notified_keyboard = (pending_gpio_edges_ & kKeyboardNotificationBit) != 0;
    pending_gpio_edges_ &= ~kKeyboardNotificationBit;
    const bool keyboard_interrupt = notified_keyboard ||
                                    (keyboard_interrupt_level_ && !interrupt_level);
    keyboard_interrupt_level_ = interrupt_level;

    // Recovery is staged across normal pump calls. This gives the C3 slave and
    // S3 master peripheral quiet time between reset, mode write and snapshot;
    // no loop can hammer 0x03 while the bus is already unhealthy.
    if(keyboard_device_!=nullptr&&keyboard_recovering_&&
       now>=keyboard_recovery_.due_us&&
       keyboard_recovery_.step!=KeyboardRecoveryStep::Read){
        if(keyboard_recovery_.step==KeyboardRecoveryStep::ResetBus){
            ++input_task_recovery_steps_;
            const esp_err_t reset=i2c_master_bus_reset(bus_handle(i2c_bus_));
            keyboard_recovery_.bus_reset_finished(esp_timer_get_time(),reset==ESP_OK);
            next_keyboard_poll_us_=keyboard_recovery_.due_us;
            ESP_LOGW(kTag,"KEYBOARD_RECOVER stage=bus-reset result=%s failures=%lu next_ms=%lld",
                     esp_err_to_name(reset),(unsigned long)keyboard_recovery_.failures,
                     (long long)((keyboard_recovery_.due_us-esp_timer_get_time())/1000));
            return false;
        }
        ++input_task_recovery_steps_;
        ++trace_mode_attempt_;
        const esp_err_t mode=i2c_master_transmit(device_handle(keyboard_device_),
                                                &kKeyboardRawModeCommand,1,kI2cTimeoutMs);
        keyboard_recovery_.raw_mode_finished(esp_timer_get_time(),mode==ESP_OK);
        next_keyboard_poll_us_=keyboard_recovery_.due_us;
        INPUT_TRACE("MODE t=%lld attempt=%lu cmd=03 result=%s recovery=staged-once",
                    (long long)esp_timer_get_time(),(unsigned long)trace_mode_attempt_,
                    esp_err_to_name(mode));
        ESP_LOGW(kTag,"KEYBOARD_RECOVER stage=raw-mode-once result=%s attempt=%lu next_ms=%lld",
                 esp_err_to_name(mode),(unsigned long)trace_mode_attempt_,
                 (long long)((keyboard_recovery_.due_us-esp_timer_get_time())/1000));
        return false;
    }
    const bool read_due=!keyboard_recovering_
        ?(keyboard_interrupt||now>=next_keyboard_poll_us_)
        :(keyboard_recovery_.step==KeyboardRecoveryStep::Read&&now>=keyboard_recovery_.due_us);
    if (keyboard_device_ != nullptr && read_due) {
        uint8_t snapshot[kKeyboardColumns]{};
        esp_err_t result = i2c_master_receive(
            device_handle(keyboard_device_), snapshot, sizeof(snapshot), kI2cTimeoutMs);
        ++keyboard_read_count_;
        if (result == ESP_OK && !valid_raw_snapshot(snapshot)) {
            result = ESP_ERR_INVALID_RESPONSE;
        }
        trace_snapshot(snapshot, result, now);
        if (result == ESP_OK) {
            const bool baseline = !keyboard_matrix_.synchronized();
            keyboard_event_count_ = keyboard_matrix_.apply_snapshot(
                snapshot, keyboard_events_.data(), keyboard_events_.size());
            if(baseline||keyboard_event_count_)
                INPUT_TRACE("QUEUE poll=%lu count=%u reason=%s", (unsigned long)trace_poll_,
                            (unsigned)keyboard_event_count_, baseline ? "resync-baseline-edges-suppressed" :
                            "matrix-edges");
            for (size_t i = 0; i < keyboard_event_count_; ++i) {
                const auto &key = keyboard_events_[i];
                INPUT_TRACE("EDGE poll=%lu index=%u matrix=%u,%u code=%02x %s S%d A%d H%d modifier=%d",
                            (unsigned long)trace_poll_, (unsigned)i, key.column, key.row,
                            key.code, transition_name(key.transition), key.modifiers.symbol,
                            key.modifiers.alt, key.modifiers.shift, key.modifier_key);
            }
            const bool recovered=keyboard_recovering_;
            keyboard_recovering_ = false;
            keyboard_recovery_.read_succeeded(now);
            next_keyboard_poll_us_ = now + kKeyboardPollIntervalUs;
            if(recovered){
                ++keyboard_recovery_count_;
                ESP_LOGW(kTag,"KEYBOARD_RECOVER stage=baseline result=usable reads=%lu errors=%lu recoveries=%lu",
                         (unsigned long)keyboard_read_count_,(unsigned long)keyboard_error_count_,
                         (unsigned long)keyboard_recovery_count_);
            }
            if (keyboard_event_count_ > 0) {
                const KeyboardEvent &key = keyboard_events_[keyboard_event_index_++];
                event = {
                    .kind = RawInputKind::Keyboard,
                    .code = key.code,
                    .transition = key.transition,
                    .modifiers = key.modifiers,
                    .column = key.column,
                    .row = key.row,
                    .modifier_key = key.modifier_key,
                    .base_code = key.base_code,
                    .symbol_code = key.symbol_code,
                    .timestamp_us = now,
                };
                std::memcpy(event.snapshot,key.snapshot,sizeof(event.snapshot));
                std::memcpy(event.previous_snapshot,key.previous_snapshot,sizeof(event.previous_snapshot));
                return true;
            }
        } else {
            ++keyboard_error_count_;
            const bool entered_recovery = !keyboard_recovering_;
            keyboard_recovering_ = true;
            if (entered_recovery) {
                // Preserve the last stable state. The first valid snapshot is
                // installed quietly, so a transport fault cannot synthesize a
                // burst of releases or leave a modifier logically latched.
                keyboard_matrix_.desynchronize();
                INPUT_TRACE("QUEUE poll=%lu count=0 reason=read-error-preserve-stable",
                            (unsigned long)trace_poll_);
            }
            keyboard_recovery_.read_failed(now);
            const int64_t retry_delay=keyboard_recovery_.due_us-now;
            next_keyboard_poll_us_ = keyboard_recovery_.due_us;
            if (now >= next_keyboard_error_log_us_) {
                next_keyboard_error_log_us_ = now + 1000000;
                ESP_LOGW(kTag, "KEYBOARD_ERROR snapshot=%s retry_ms=%lld streak=%lu poll=%lu",
                         esp_err_to_name(result), (long long)(retry_delay / 1000),
                         (unsigned long)keyboard_recovery_.failures,(unsigned long)trace_poll_);
            }
            if (entered_recovery) {
                event = {
                    .kind = RawInputKind::KeyboardResynchronized,
                    .timestamp_us = now,
                };
                return true;
            }
        }
    }

    return false;
}

void InputHardware::log_metrics() const
{
    const uint32_t per_mille=keyboard_read_count_
        ?uint32_t((uint64_t(keyboard_error_count_)*1000U)/keyboard_read_count_):0;
    ESP_LOGI(kTag,"KEYBOARD_METRICS reads=%lu errors=%lu error_per_mille=%lu recoveries=%lu service_gap_high_us=%lld recovering=%d",
             (unsigned long)keyboard_read_count_,(unsigned long)keyboard_error_count_,
             (unsigned long)per_mille,(unsigned long)keyboard_recovery_count_,
             (long long)trace_service_gap_high_us_,keyboard_recovering_);
    ESP_LOGI(kTag,"INPUT_QUEUE depth=%u high_water=%lu dropped=%lu queued=%lu consumed=%lu render_block_high_us=%lld",
             event_queue_?unsigned(uxQueueMessagesWaiting(event_queue_)):0U,
             (unsigned long)queue_high_water_,(unsigned long)dropped_event_count_,
             (unsigned long)queued_event_count_,(unsigned long)consumed_event_count_,
             (long long)consumer_gap_high_us_);
    ESP_LOGI(kTag,"TRACKBALL_INPUT raw_edges=%lu accepted=semantic-layer suppressed=semantic-layer",
             (unsigned long)trackball_raw_edges_);
    ESP_LOGI(kTag,
             "INPUT_TASK wakes=%lu blocks=%lu irq_wakes=%lu idle_waits=%lu recovery_steps=%lu max_work_us=%lu priority=4 core=0 fallback_ticks=%u",
             (unsigned long)input_task_wakes_, (unsigned long)input_task_blocks_,
             (unsigned long)input_task_irq_wakes_, (unsigned long)input_task_idle_waits_,
             (unsigned long)input_task_recovery_steps_,
             (unsigned long)input_task_max_work_us_, unsigned(kInputFallbackWaitTicks));
}

void InputHardware::trace_snapshot(const uint8_t *b, esp_err_t result, int64_t start)
{
    const auto *p = keyboard_matrix_.columns();
    ++trace_poll_;
    // Each hex byte encodes rows 0..6 (bit 1 = down). Preserve bit 7 in raw
    // output: the decoder masks it, but it can reveal unsupported/raw-mode data.
    const bool changed=result==ESP_OK&&(((b[0]^p[0])|(b[1]^p[1])|(b[2]^p[2])|(b[3]^p[3])|(b[4]^p[4]))&0x7fU);
    if(result!=ESP_OK||changed)
        INPUT_TRACE("SNAPSHOT n=%lu t=%lld io_us=%lld result=%s raw=%02x,%02x,%02x,%02x,%02x prev=%02x,%02x,%02x,%02x,%02x sync=%d int=%d",
                    (unsigned long)trace_poll_,(long long)start,
                    (long long)(esp_timer_get_time()-start),esp_err_to_name(result),
                    b[0],b[1],b[2],b[3],b[4],p[0],p[1],p[2],p[3],p[4],
                    keyboard_matrix_.synchronized(),gpio_get_level(pins::kKeyboardInterrupt));
    trace_last_read_us_ = start;
}

const char *raw_input_name(RawInputKind kind)
{
    switch (kind) {
    case RawInputKind::Keyboard: return "keyboard";
    case RawInputKind::KeyboardResynchronized: return "keyboard-resynchronized";
    case RawInputKind::TrackballUp: return "trackball-up";
    case RawInputKind::TrackballDown: return "trackball-down";
    case RawInputKind::TrackballLeft: return "trackball-left";
    case RawInputKind::TrackballRight: return "trackball-right";
    case RawInputKind::None: return "none";
    }
    return "unknown";
}

}  // namespace tdeck
