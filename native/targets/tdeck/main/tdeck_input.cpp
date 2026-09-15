#include "tdeck_input.h"
#include "input_trace.h"

#include <array>

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
constexpr int64_t kKeyboardPollIntervalUs = 10000;

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
    if (keyboard_device_ != nullptr) i2c_master_bus_rm_device(device_handle(keyboard_device_));
    if (i2c_bus_ != nullptr) i2c_del_master_bus(bus_handle(i2c_bus_));
}

esp_err_t InputHardware::initialize()
{
    INPUT_TRACE("INIT keyboard t=%lld", (long long)esp_timer_get_time());
    // Observe INT only. No ISR, acknowledgement, pull, or polling gate is added.
    ESP_RETURN_ON_ERROR(gpio_set_direction(pins::kKeyboardInterrupt, GPIO_MODE_INPUT), kTag, "observe keyboard INT");
    const gpio_config_t inputs = {
        .pin_bit_mask = (1ULL << pins::kTrackballUp) |
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
        ESP_LOGW(kTag, "Keyboard 0x55 probe failed: %s; trackball remains available",
                 esp_err_to_name(probe_result));
        i2c_master_bus_rm_device(device);
        keyboard_device_ = nullptr;
        return ESP_OK;
    }
    ++trace_mode_attempt_;
    const esp_err_t mode_result = i2c_master_transmit(
        device, &kKeyboardRawModeCommand, 1, kI2cTimeoutMs);
    INPUT_TRACE("MODE t=%lld attempt=%lu cmd=03 result=%s (ACK is not capability proof)",
                (long long)esp_timer_get_time(), (unsigned long)trace_mode_attempt_, esp_err_to_name(mode_result));
    if (mode_result != ESP_OK) {
        ESP_LOGW(kTag, "Keyboard raw-mode command failed: %s; trackball remains available",
                 esp_err_to_name(mode_result));
        i2c_master_bus_rm_device(device);
        keyboard_device_ = nullptr;
        return ESP_OK;
    }

    uint8_t initial_snapshot[kKeyboardColumns]{};
    const int64_t initial_start = esp_timer_get_time();
    const esp_err_t initial_result = i2c_master_receive(
        device, initial_snapshot, sizeof(initial_snapshot), kI2cTimeoutMs);
    trace_snapshot(initial_snapshot, initial_result, initial_start);
    if (initial_result != ESP_OK) {
        ESP_LOGW(kTag, "Keyboard raw snapshot failed: %s; trackball remains available",
                 esp_err_to_name(initial_result));
        i2c_master_bus_rm_device(device);
        keyboard_device_ = nullptr;
        return ESP_OK;
    }
    keyboard_matrix_.apply_snapshot(initial_snapshot, nullptr, 0);
    INPUT_TRACE("QUEUE poll=%lu count=0 reason=initial-baseline-edges-suppressed", (unsigned long)trace_poll_);
    ESP_LOGI(kTag, "Keyboard online: I2C address 0x55, SDA=%d SCL=%d INT=%d",
             pins::kI2cData, pins::kI2cClock, pins::kKeyboardInterrupt);
    ESP_LOGI(kTag, "Keyboard raw matrix mode active; press/release/modifiers tracked by snapshot");
    ESP_LOGI(kTag, "Directional movement: trackball up=%d down=%d left=%d right=%d",
             pins::kTrackballUp, pins::kTrackballDown,
             pins::kTrackballLeft, pins::kTrackballRight);
    return ESP_OK;
}

bool InputHardware::poll(RawInputEvent &event)
{
    event = {};
    const int64_t now = esp_timer_get_time();

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
        const bool falling_edge = trackball_levels_[i] && !level;
        trackball_levels_[i] = level;
        if (falling_edge) {
            INPUT_TRACE("TB t=%lld dir=%s queue=%u/%u last_read=%lld",
                        (long long)now, raw_input_name(kinds[i]),
                        (unsigned)keyboard_event_index_, (unsigned)keyboard_event_count_,
                        (long long)trace_last_read_us_);
            event = {
                .kind = kinds[i],
                .code = static_cast<uint8_t>(pins_by_direction[i]),
                .transition = KeyTransition::Pressed,
                .timestamp_us = now,
            };
            return true;
        }
    }

    if (keyboard_event_index_ < keyboard_event_count_) {
        INPUT_TRACE("DEQUEUE t=%lld poll=%lu index=%u count=%u read_deferred=queued-edge",
                    (long long)now, (unsigned long)trace_poll_,
                    (unsigned)keyboard_event_index_, (unsigned)keyboard_event_count_);
        const KeyboardEvent &key = keyboard_events_[keyboard_event_index_++];
        event = {
            .kind = RawInputKind::Keyboard,
            .code = key.code,
            .transition = key.transition,
            .modifiers = key.modifiers,
            .column = key.column,
            .row = key.row,
            .modifier_key = key.modifier_key,
            .timestamp_us = now,
        };
        return true;
    }
    keyboard_event_index_ = 0;
    keyboard_event_count_ = 0;

    if (keyboard_device_ != nullptr && now >= next_keyboard_poll_us_) {
        next_keyboard_poll_us_ = now + kKeyboardPollIntervalUs;
        uint8_t snapshot[kKeyboardColumns]{};
        const esp_err_t result = i2c_master_receive(
            device_handle(keyboard_device_), snapshot, sizeof(snapshot), kI2cTimeoutMs);
        trace_snapshot(snapshot, result, now);
        if (result == ESP_OK) {
            const bool baseline = !keyboard_matrix_.synchronized();
            keyboard_event_count_ = keyboard_matrix_.apply_snapshot(
                snapshot, keyboard_events_.data(), keyboard_events_.size());
            INPUT_TRACE("QUEUE poll=%lu count=%u reason=%s", (unsigned long)trace_poll_,
                        (unsigned)keyboard_event_count_, baseline ? "resync-baseline-edges-suppressed" :
                        keyboard_event_count_ ? "matrix-edges" : "unchanged-no-edge");
            for (size_t i = 0; i < keyboard_event_count_; ++i) {
                const auto &key = keyboard_events_[i];
                INPUT_TRACE("EDGE poll=%lu index=%u matrix=%u,%u code=%02x %s S%d A%d H%d modifier=%d",
                            (unsigned long)trace_poll_, (unsigned)i, key.column, key.row,
                            key.code, transition_name(key.transition), key.modifiers.symbol,
                            key.modifiers.alt, key.modifiers.shift, key.modifier_key);
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
                    .timestamp_us = now,
                };
                return true;
            }
        } else {
            // A failed/short snapshot is never merged with old state. Clearing
            // synchronization prevents a missed release from latching Symbol,
            // Alt, or Shift. The next good snapshot becomes a quiet baseline.
            keyboard_matrix_.desynchronize();
            INPUT_TRACE("QUEUE poll=%lu count=0 reason=read-error-desynchronize", (unsigned long)trace_poll_);
            if (now >= next_keyboard_error_log_us_) {
                next_keyboard_error_log_us_ = now + 1000000;
                ESP_LOGW(kTag, "Keyboard snapshot failed: %s; resynchronizing raw mode",
                         esp_err_to_name(result));
                ++trace_mode_attempt_;
                const esp_err_t mode_result = i2c_master_transmit(
                    device_handle(keyboard_device_), &kKeyboardRawModeCommand, 1,
                    kI2cTimeoutMs);
                INPUT_TRACE("MODE t=%lld attempt=%lu cmd=03 result=%s",
                            (long long)esp_timer_get_time(), (unsigned long)trace_mode_attempt_, esp_err_to_name(mode_result));
                if (mode_result != ESP_OK) {
                    ESP_LOGW(kTag, "Keyboard raw-mode retry failed: %s",
                             esp_err_to_name(mode_result));
                }
            }
        }
    }

    return false;
}

void InputHardware::trace_snapshot(const uint8_t *b, esp_err_t result, int64_t start)
{
    const auto *p = keyboard_matrix_.columns();
    ++trace_poll_;
    // Each hex byte encodes rows 0..6 (bit 1 = down). Preserve bit 7 in raw
    // output: the decoder masks it, but it can reveal unsupported/raw-mode data.
    INPUT_TRACE("POLL n=%lu t=%lld gap=%lld io_us=%lld result=%s requested=5 valid=%d raw=%02x,%02x,%02x,%02x,%02x prev=%02x,%02x,%02x,%02x,%02x sync=%d int=%d",
                (unsigned long)trace_poll_, (long long)start,
                (long long)(trace_last_read_us_ ? start - trace_last_read_us_ : 0),
                (long long)(esp_timer_get_time() - start), esp_err_to_name(result), result == ESP_OK,
                b[0], b[1], b[2], b[3], b[4], p[0], p[1], p[2], p[3], p[4], keyboard_matrix_.synchronized(), gpio_get_level(pins::kKeyboardInterrupt));
    if (result == ESP_OK) {
        INPUT_TRACE("MATRIX n=%lu cols=%02x,%02x,%02x,%02x,%02x press=%02x,%02x,%02x,%02x,%02x release=%02x,%02x,%02x,%02x,%02x S%d A%d L%d R%d",
                    (unsigned long)trace_poll_, b[0]&127,b[1]&127,b[2]&127,b[3]&127,b[4]&127,
                    b[0]&~p[0]&127,b[1]&~p[1]&127,b[2]&~p[2]&127,b[3]&~p[3]&127,b[4]&~p[4]&127,
                    p[0]&~b[0]&127,p[1]&~b[1]&127,p[2]&~b[2]&127,p[3]&~b[3]&127,p[4]&~b[4]&127,
                    !!(b[0]&4),!!(b[0]&16),!!(b[1]&64),!!(b[2]&8));
    }
    trace_last_read_us_ = start;
}

const char *raw_input_name(RawInputKind kind)
{
    switch (kind) {
    case RawInputKind::Keyboard: return "keyboard";
    case RawInputKind::TrackballUp: return "trackball-up";
    case RawInputKind::TrackballDown: return "trackball-down";
    case RawInputKind::TrackballLeft: return "trackball-left";
    case RawInputKind::TrackballRight: return "trackball-right";
    case RawInputKind::None: return "none";
    }
    return "unknown";
}

}  // namespace tdeck
