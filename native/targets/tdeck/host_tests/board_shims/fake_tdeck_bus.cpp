// Alpha 3 A3-04E host-test seam -- see fake_tdeck_bus.h.
#include "fake_tdeck_bus.h"

#include <array>
#include <cstring>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_cpu.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/task.h"
#include "tdeck_pins.h"

struct openu5_host_spi_device {
    int id = 0;
};

namespace openu5_host_bus {
namespace {
constexpr int kWidth = 320, kHeight = 240;
constexpr uint64_t kFnvBasis = 1469598103934665603ull, kFnvPrime = 1099511628211ull;

Model g_model{};
Stats g_stats{};
std::array<uint16_t, kWidth * kHeight> g_gram{};
uint64_t g_hash = kFnvBasis;
uint32_t g_ns_remainder = 0; // < 1000: the part of a microsecond the clock has not taken yet
int g_dc = 0;                // the TFT's D/C pin: 0 = command, 1 = data
uint8_t g_command = 0;
uint8_t g_params[4]{};
int g_param_count = 0;
int g_x0 = 0, g_x1 = kWidth - 1, g_y0 = 0, g_y1 = kHeight - 1, g_x = 0, g_y = 0;
openu5_host_spi_device g_display{1};
std::vector<Pause> g_pauses;

void hash_byte(uint8_t b) {
    g_hash ^= b;
    g_hash *= kFnvPrime;
}

void on_delay(TickType_t ticks) {
    int64_t &now = openu5_host_virtual_clock_us();
    if (ticks == 0) {
        ++g_stats.zero_delays;
        return;
    }
    g_pauses.push_back({g_x0, g_y0, g_x1, g_y1, true});
    if (now < 0 || !g_model.timed) {
        ++g_stats.tick_sleeps;
        return; // real clock or an untimed run: counted, not modelled
    }
    const int64_t tick = int64_t(g_model.tick_us);
    const int64_t wake = (now / tick + int64_t(ticks)) * tick;
    ++g_stats.tick_sleeps;
    g_stats.sleep_us += uint64_t(wake - now);
    now = wake;
    g_ns_remainder = 0;
}

void on_yield() {
    ++g_stats.yields;
    g_pauses.push_back({g_x0, g_y0, g_x1, g_y1, false});
    advance_ns(g_model.yield_ns);
}

void pixel(uint16_t value) {
    if (g_x < 0 || g_x >= kWidth || g_y < 0 || g_y >= kHeight) {
        ++g_stats.malformed;
    } else {
        g_gram[size_t(g_y) * kWidth + size_t(g_x)] = value;
    }
    if (++g_x > g_x1) {
        g_x = g_x0;
        if (++g_y > g_y1) g_y = g_y0;
    }
}

// The ST7789's side of one transaction: a command byte, window parameters or pixels.
void decode(const uint8_t *data, size_t bytes) {
    if (g_dc == 0) {
        if (bytes != 1) ++g_stats.malformed;
        g_command = bytes ? data[0] : 0;
        g_param_count = 0;
        if (g_command == 0x2C) { // RAMWR: the write pointer returns to the window's start
            g_x = g_x0;
            g_y = g_y0;
        }
        return;
    }
    if (g_command == 0x2A || g_command == 0x2B) { // CASET / RASET: start and end, big-endian
        for (size_t i = 0; i < bytes && g_param_count < 4; ++i) g_params[g_param_count++] = data[i];
        if (g_param_count == 4) {
            const int start = g_params[0] << 8 | g_params[1], end = g_params[2] << 8 | g_params[3];
            if (g_command == 0x2A) {
                g_x0 = start;
                g_x1 = end;
            } else {
                g_y0 = start;
                g_y1 = end;
            }
        }
        return;
    }
    if (g_command == 0x2C) {
        if (bytes % 2) ++g_stats.malformed; // RGB565: whole pixels only
        for (size_t i = 0; i + 1 < bytes; i += 2) pixel(uint16_t(data[i] << 8 | data[i + 1]));
    }
}
} // namespace

Model &model() { return g_model; }
Stats &stats() { return g_stats; }
const uint16_t *gram() { return g_gram.data(); }
uint64_t stream_hash() { return g_hash; }

void reset_stats() {
    g_stats = Stats{};
    g_pauses.clear();
}
const std::vector<Pause> &pauses() { return g_pauses; }
void restart_stream() {
    reset_stats();
    g_hash = kFnvBasis;
}

void install() {
    openu5_host_scheduler().delay = on_delay;
    openu5_host_scheduler().yield = on_yield;
    reset_stats();
    g_gram.fill(0x1234);
    g_hash = kFnvBasis;
    g_ns_remainder = 0;
    g_dc = 0;
    g_command = 0;
    g_param_count = 0;
    g_x0 = g_x = 0;
    g_y0 = g_y = 0;
    g_x1 = kWidth - 1;
    g_y1 = kHeight - 1;
}

void advance_ns(uint64_t ns) {
    int64_t &now = openu5_host_virtual_clock_us();
    if (now < 0 || !g_model.timed) return;
    const uint64_t total = g_ns_remainder + ns;
    now += int64_t(total / 1000);
    g_ns_remainder = uint32_t(total % 1000);
}

} // namespace openu5_host_bus

using namespace openu5_host_bus;

esp_cpu_cycle_count_t esp_cpu_get_cycle_count() {
    const int64_t now = openu5_host_virtual_clock_us();
    const uint64_t ns = uint64_t(now < 0 ? 0 : now) * 1000u + g_ns_remainder;
    return esp_cpu_cycle_count_t(ns * g_model.cpu_mhz / 1000u);
}

esp_err_t gpio_config(const gpio_config_t *) { return ESP_OK; }
esp_err_t gpio_set_level(gpio_num_t gpio, uint32_t level) {
    if (gpio == tdeck::pins::kTftDataCommand) g_dc = level ? 1 : 0;
    return ESP_OK;
}
esp_err_t gpio_output_enable(gpio_num_t) { return ESP_OK; }
esp_err_t gpio_set_pull_mode(gpio_num_t, gpio_pull_mode_t) { return ESP_OK; }

esp_err_t ledc_timer_config(const ledc_timer_config_t *) { return ESP_OK; }
esp_err_t ledc_channel_config(const ledc_channel_config_t *) { return ESP_OK; }
esp_err_t ledc_set_duty(ledc_mode_t, ledc_channel_t, uint32_t) { return ESP_OK; }
esp_err_t ledc_update_duty(ledc_mode_t, ledc_channel_t) { return ESP_OK; }

esp_err_t spi_bus_initialize(spi_host_device_t, const spi_bus_config_t *, spi_dma_chan_t) { return ESP_OK; }
esp_err_t spi_bus_add_device(spi_host_device_t, const spi_device_interface_config_t *, spi_device_handle_t *handle) {
    *handle = &g_display;
    return ESP_OK;
}

esp_err_t spi_device_transmit(spi_device_handle_t handle, spi_transaction_t *t) {
    if (!handle || !t) return ESP_ERR_INVALID_ARG;
    const bool txdata = (t->flags & SPI_TRANS_USE_TXDATA) != 0;
    const size_t bytes = t->length / 8;
    const uint8_t *data = txdata ? t->tx_data : static_cast<const uint8_t *>(t->tx_buffer);
    if (t->length % 8 || (bytes && !data)) ++g_stats.malformed;
    hash_byte(uint8_t(g_dc));
    for (int shift = 0; shift < 32; shift += 8) hash_byte(uint8_t(bytes >> shift));
    for (size_t i = 0; data && i < bytes; ++i) hash_byte(data[i]);
    if (data) decode(data, bytes);
    const uint64_t ns = (txdata ? g_model.cpu_transaction_ns : g_model.dma_transaction_ns) +
                        uint64_t(t->length) * 1000000000ull / g_model.spi_hz;
    ++g_stats.transactions;
    g_stats.bytes += bytes;
    g_stats.xfer_ns += ns;
    advance_ns(ns);
    return ESP_OK;
}

esp_err_t esp_vfs_fat_sdspi_mount(const char *, const sdmmc_host_t *, const sdspi_device_config_t *,
                                  const esp_vfs_fat_sdmmc_mount_config_t *, sdmmc_card_t **) {
    return ESP_FAIL;
}
void sdmmc_card_print_info(FILE *, const sdmmc_card_t *) {}
