#include "tdeck_board.h"
#include "boot_trace.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include "sd_protocol_defs.h"
#include "sdkconfig.h"

#include "tdeck_pins.h"
#include "idle_service.h"
#include "native_renderer.h"
#include "location_names.h"

namespace tdeck {
namespace {

constexpr char kTag[] = "TDeckBoard";
constexpr char kMountPoint[] = "/sd";
constexpr char kKnownTestPath[] = "/sd/openu5-m2-test.txt";
constexpr char kTemporaryTestPath[] = "/sd/.openu5-m2-diag.tmp";
constexpr char kTestPayload[] = "OpenU5-TDeck Milestone 2 shared SPI test\n";
constexpr int kDisplayWidth = 320;
constexpr int kDisplayHeight = 240;
constexpr int kTftClockHz = 40 * 1000 * 1000;
// LilyGO's factory UnitTest uses 800 kHz for the shared-bus SD path.
constexpr int kSdClockKhz = 800;
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kCyan = 0x07FF;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kYellow = 0xFFE0;
// Alpha 4 UI Batch 1 (ALPHA4_UI.md): the original's frame palette -- the web
// port's skin/fiel/frame.ts DEFAULT_FRAME_COLORS: the frame EGA 1 (#0000AA),
// its border EGA 15 (#FFFFFF) -- and EGA 7 (#AAAAAA) for footers.
constexpr uint16_t kChromeBand = 0x0015;
constexpr uint16_t kChromeRule = kWhite;
constexpr uint16_t kChromeDim = 0xAD55;
// bandBracket.ts BAND_BRACKET_BLUE / _WHITE: the > that closes a band's notch
// (8x8, bit 7 = left); the < is its mirror. Everything else in the cell is the
// notch (black), bar the base column, which is the band itself.
constexpr uint8_t kBracketBlue[8] = {0x00, 0x00, 0x60, 0x78, 0x78, 0x60, 0x00, 0x00};
constexpr uint8_t kBracketWhite[8] = {0x00, 0x60, 0x18, 0x04, 0x04, 0x18, 0x60, 0x00};
// roster.ts: the roster row is 15 IBM.CH cells; they start 4 px into the row.
constexpr int kRosterTextX = 188;

// Left part at cell 0, right part right-aligned to cell `cells`, spaces between.
void split_cells(char *out, size_t cap, const char *left, const char *right, size_t cells) {
    if (!cap) return;
    const size_t width = std::min(cells, cap - 1);
    for (size_t i = 0; i < width; ++i) out[i] = ' ';
    out[width] = 0;
    for (size_t i = 0; left[i] && i < width; ++i) out[i] = left[i];
    const size_t n = std::strlen(right);
    for (size_t i = 0; i < n && i < width; ++i) out[width - std::min(n, width) + i] = right[i];
}
constexpr size_t kReadLimit = 160;
constexpr uint8_t kMadctlRgb = 0x00;
constexpr uint8_t kMadctlMirrorX = 0x40;
constexpr uint8_t kMadctlSwapXy = 0x20;
constexpr uint8_t kLandscapeRotation1Rgb =
    kMadctlMirrorX | kMadctlSwapXy | kMadctlRgb;

spi_device_handle_t display_handle(void *handle)
{
    return static_cast<spi_device_handle_t>(handle);
}

esp_err_t prepare_inactive_chip_selects()
{
    // Preload every CS high before enabling its output. TFT and SD are not
    // reserved here: their drivers take sole ownership when each device is
    // attached. LoRa remains an ordinary board-owned GPIO until that future
    // peripheral is implemented.
    ESP_RETURN_ON_ERROR(gpio_set_level(pins::kTftChipSelect, 1), kTag,
                        "preload TFT CS");
    ESP_RETURN_ON_ERROR(gpio_set_level(pins::kSdChipSelect, 1), kTag,
                        "preload SD CS");
    ESP_RETURN_ON_ERROR(gpio_set_level(pins::kLoraChipSelect, 1), kTag,
                        "preload LoRa CS");
    ESP_RETURN_ON_ERROR(gpio_output_enable(pins::kTftChipSelect), kTag,
                        "enable temporary TFT CS output");
    ESP_RETURN_ON_ERROR(gpio_output_enable(pins::kSdChipSelect), kTag,
                        "enable temporary SD CS output");
    return ESP_OK;
}

std::array<uint8_t, 5> glyph(char c)
{
    switch (c) {
    case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00};
    case '!': return {0x00, 0x00, 0x5f, 0x00, 0x00};
    case '"': return {0x00, 0x07, 0x00, 0x07, 0x00};
    case '#': return {0x14, 0x7f, 0x14, 0x7f, 0x14};
    case '$': return {0x24, 0x2a, 0x7f, 0x2a, 0x12};
    case '%': return {0x23, 0x13, 0x08, 0x64, 0x62};
    case '&': return {0x36, 0x49, 0x55, 0x22, 0x50};
    case '\'': return {0x00, 0x05, 0x03, 0x00, 0x00};
    case '(': return {0x00, 0x1c, 0x22, 0x41, 0x00};
    case ')': return {0x00, 0x41, 0x22, 0x1c, 0x00};
    case '*': return {0x14, 0x08, 0x3e, 0x08, 0x14};
    case '+': return {0x08, 0x08, 0x3e, 0x08, 0x08};
    case ',': return {0x00, 0x50, 0x30, 0x00, 0x00};
    case '-': return {0x08, 0x08, 0x08, 0x08, 0x08};
    case '.': return {0x00, 0x60, 0x60, 0x00, 0x00};
    case '/': return {0x20, 0x10, 0x08, 0x04, 0x02};
    case ':': return {0x00, 0x36, 0x36, 0x00, 0x00};
    case ';': return {0x00, 0x56, 0x36, 0x00, 0x00};
    case '<': return {0x08, 0x14, 0x22, 0x41, 0x00};
    case '=': return {0x14, 0x14, 0x14, 0x14, 0x14};
    case '>': return {0x00, 0x41, 0x22, 0x14, 0x08};
    case '?': return {0x02, 0x01, 0x51, 0x09, 0x06};
    case '@': return {0x32, 0x49, 0x79, 0x41, 0x3e};
    case '0': return {0x3E, 0x51, 0x49, 0x45, 0x3E};
    case '1': return {0x00, 0x42, 0x7F, 0x40, 0x00};
    case '2': return {0x42, 0x61, 0x51, 0x49, 0x46};
    case '3': return {0x21, 0x41, 0x45, 0x4B, 0x31};
    case '4': return {0x18, 0x14, 0x12, 0x7F, 0x10};
    case '5': return {0x27, 0x45, 0x45, 0x45, 0x39};
    case '6': return {0x3C, 0x4A, 0x49, 0x49, 0x30};
    case '7': return {0x01, 0x71, 0x09, 0x05, 0x03};
    case '8': return {0x36, 0x49, 0x49, 0x49, 0x36};
    case '9': return {0x06, 0x49, 0x49, 0x29, 0x1E};
    case 'A': return {0x7E, 0x11, 0x11, 0x11, 0x7E};
    case 'B': return {0x7F, 0x49, 0x49, 0x49, 0x36};
    case 'C': return {0x3E, 0x41, 0x41, 0x41, 0x22};
    case 'D': return {0x7F, 0x41, 0x41, 0x22, 0x1C};
    case 'E': return {0x7F, 0x49, 0x49, 0x49, 0x41};
    case 'F': return {0x7F, 0x09, 0x09, 0x09, 0x01};
    case 'G': return {0x3E, 0x41, 0x49, 0x49, 0x7A};
    case 'H': return {0x7F, 0x08, 0x08, 0x08, 0x7F};
    case 'I': return {0x00, 0x41, 0x7F, 0x41, 0x00};
    case 'J': return {0x20, 0x40, 0x41, 0x3F, 0x01};
    case 'K': return {0x7F, 0x08, 0x14, 0x22, 0x41};
    case 'L': return {0x7F, 0x40, 0x40, 0x40, 0x40};
    case 'M': return {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    case 'N': return {0x7F, 0x04, 0x08, 0x10, 0x7F};
    case 'O': return {0x3E, 0x41, 0x41, 0x41, 0x3E};
    case 'P': return {0x7F, 0x09, 0x09, 0x09, 0x06};
    case 'Q': return {0x3E, 0x41, 0x51, 0x21, 0x5E};
    case 'R': return {0x7F, 0x09, 0x19, 0x29, 0x46};
    case 'S': return {0x46, 0x49, 0x49, 0x49, 0x31};
    case 'T': return {0x01, 0x01, 0x7F, 0x01, 0x01};
    case 'U': return {0x3F, 0x40, 0x40, 0x40, 0x3F};
    case 'V': return {0x1F, 0x20, 0x40, 0x20, 0x1F};
    case 'W': return {0x3F, 0x40, 0x38, 0x40, 0x3F};
    case 'X': return {0x63, 0x14, 0x08, 0x14, 0x63};
    case 'Y': return {0x07, 0x08, 0x70, 0x08, 0x07};
    case 'Z': return {0x61, 0x51, 0x49, 0x45, 0x43};
    case '[': return {0x00, 0x7f, 0x41, 0x41, 0x00};
    case '\\': return {0x02, 0x04, 0x08, 0x10, 0x20};
    case ']': return {0x00, 0x41, 0x41, 0x7f, 0x00};
    case '_': return {0x40, 0x40, 0x40, 0x40, 0x40};
    case '|': return {0x00, 0x00, 0x7f, 0x00, 0x00};
    case 'a': return {0x20, 0x54, 0x54, 0x54, 0x78};
    case 'b': return {0x7f, 0x48, 0x44, 0x44, 0x38};
    case 'c': return {0x38, 0x44, 0x44, 0x44, 0x20};
    case 'd': return {0x38, 0x44, 0x44, 0x48, 0x7f};
    case 'e': return {0x38, 0x54, 0x54, 0x54, 0x18};
    case 'f': return {0x08, 0x7e, 0x09, 0x01, 0x02};
    case 'g': return {0x0c, 0x52, 0x52, 0x52, 0x3e};
    case 'h': return {0x7F, 0x08, 0x04, 0x04, 0x78};
    case 'i': return {0x00, 0x44, 0x7D, 0x40, 0x00};
    case 'j': return {0x20, 0x40, 0x44, 0x3d, 0x00};
    case 'k': return {0x7F, 0x10, 0x28, 0x44, 0x00};
    case 'l': return {0x00, 0x41, 0x7F, 0x40, 0x00};
    case 'm': return {0x7c, 0x04, 0x18, 0x04, 0x78};
    case 'n': return {0x7C, 0x08, 0x04, 0x04, 0x78};
    case 'o': return {0x38, 0x44, 0x44, 0x44, 0x38};
    case 'p': return {0x7C, 0x14, 0x14, 0x14, 0x08};
    case 'q': return {0x08, 0x14, 0x14, 0x18, 0x7c};
    case 'r': return {0x7c, 0x08, 0x04, 0x04, 0x08};
    case 's': return {0x48, 0x54, 0x54, 0x54, 0x20};
    case 't': return {0x04, 0x3F, 0x44, 0x40, 0x20};
    case 'u': return {0x3c, 0x40, 0x40, 0x20, 0x7c};
    case 'v': return {0x1c, 0x20, 0x40, 0x20, 0x1c};
    case 'w': return {0x3c, 0x40, 0x30, 0x40, 0x3c};
    case 'x': return {0x44, 0x28, 0x10, 0x28, 0x44};
    case 'y': return {0x0c, 0x50, 0x50, 0x50, 0x3c};
    case 'z': return {0x44, 0x64, 0x54, 0x4c, 0x44};
    default: return {0x02, 0x01, 0x51, 0x09, 0x06}; // visible '?' for unsupported bytes
    }
}

esp_err_t read_prefix(const char *path, std::array<char, kReadLimit> &buffer, size_t &length)
{
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        ESP_LOGE(kTag, "Cannot open %s for reading: errno=%d", path, errno);
        return ESP_FAIL;
    }
    length = std::fread(buffer.data(), 1, buffer.size(), file);
    const bool read_error = std::ferror(file) != 0;
    std::fclose(file);
    if (read_error) {
        ESP_LOGE(kTag, "Read failed for %s", path);
        return ESP_FAIL;
    }
    return ESP_OK;
}

}  // namespace

esp_err_t Board::initialize_shared_spi()
{
    INPUT_TRACE("INIT shared-spi t=%lld already=%d", (long long)esp_timer_get_time(), shared_spi_initialized_);
    if (shared_spi_initialized_) {
        return ESP_OK;
    }

    const gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << pins::kPowerEnable) |
                        (1ULL << pins::kLoraChipSelect) |
                        (1ULL << pins::kTftDataCommand) |
                        (1ULL << pins::kTftBacklight),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    // Establish safe inactive levels before powering or clocking any shared device.
    ESP_RETURN_ON_ERROR(prepare_inactive_chip_selects(), kTag,
                        "prepare shared SPI chip selects");
    ESP_RETURN_ON_ERROR(gpio_config(&output_config), kTag, "configure board outputs");
    gpio_set_level(pins::kTftBacklight, 0);
    gpio_set_level(pins::kTftDataCommand, 0);
    gpio_set_level(pins::kPowerEnable, 1);
    gpio_set_pull_mode(pins::kSpiMiso, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(500));

    const spi_bus_config_t bus_config = {
        .mosi_io_num = pins::kSpiMosi,
        .miso_io_num = pins::kSpiMiso,
        .sclk_io_num = pins::kSpiClock,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .data4_io_num = GPIO_NUM_NC,
        .data5_io_num = GPIO_NUM_NC,
        .data6_io_num = GPIO_NUM_NC,
        .data7_io_num = GPIO_NUM_NC,
        .data_io_default_level = 0,
        .max_transfer_sz = kDisplayWidth * 2,
        .dma_burst_size = 0,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
        .intr_flags = 0,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(pins::kSharedSpiHost, &bus_config,
                                            SPI_DMA_CH_AUTO),
                        kTag, "initialize shared SPI2 bus");
    shared_spi_initialized_ = true;
    ESP_LOGI(kTag, "Shared SPI2 initialized: SCK=%d MOSI=%d MISO=%d",
             pins::kSpiClock, pins::kSpiMosi, pins::kSpiMiso);
    return ESP_OK;
}

esp_err_t Board::write_display_command(uint8_t command, const uint8_t *data,
                                       size_t data_length)
{
    spi_transaction_t transaction{};
    transaction.flags = SPI_TRANS_USE_TXDATA;
    transaction.length = 8;
    transaction.tx_data[0] = command;
    gpio_set_level(pins::kTftDataCommand, 0);
    ESP_RETURN_ON_ERROR(tft_command(transaction), kTag, "send TFT command 0x%02x", command);
    if (data_length == 0) {
        return ESP_OK;
    }
    spi_transaction_t payload{};
    payload.length = data_length * 8;
    payload.tx_buffer = data;
    gpio_set_level(pins::kTftDataCommand, 1);
    return tft_command(payload);
}

// ---------------------------------------------------------------------------
// Alpha 3 A3-04C (ALPHA3_AUDIO.md section 20): the timed TFT transaction.
// Core-0 cycle counts (the game loop is pinned there): two CCOUNT reads per
// transaction and two loads of the audio task's flag -- no call, no lock.
// The transactions themselves are exactly what they were.
// ---------------------------------------------------------------------------
Board::RowMark Board::row_mark() const
{
    return RowMark{uint32_t(esp_cpu_get_cycle_count()), audio_running()};
}

esp_err_t Board::tft_row(spi_transaction_t &transaction, RowMark start)
{
    if (endgame_capturing_ && transaction.tx_buffer)
        mirror_pixels(static_cast<const uint8_t *>(transaction.tx_buffer), transaction.length / 16);
    return tft_transmit(transaction, &start);
}

// A4-END1. Every pixel transaction follows a window (set_display_window) and
// fills it row by row; the copy follows the same cursor, so after a full
// repaint it holds exactly what the panel shows.
void Board::mirror_pixels(const uint8_t *bytes, size_t count)
{
    if (!endgame_shadow_ || window_w_ <= 0) return;
    for (size_t i = 0; i < count; ++i) {
        if (cursor_y_ >= window_y_ + window_h_) return;
        if (cursor_x_ >= 0 && cursor_x_ < kDisplayWidth && cursor_y_ >= 0 && cursor_y_ < kDisplayHeight)
            endgame_shadow_[cursor_y_ * kDisplayWidth + cursor_x_] = uint16_t(bytes[i * 2] << 8 | bytes[i * 2 + 1]);
        if (++cursor_x_ >= window_x_ + window_w_) { cursor_x_ = window_x_; ++cursor_y_; }
    }
}

esp_err_t Board::tft_command(spi_transaction_t &transaction)
{
    return tft_transmit(transaction, nullptr);
}

esp_err_t Board::tft_transmit(spi_transaction_t &transaction, const RowMark *start)
{
    const bool busy0 = audio_running();
    const bool sd0 = sd_log_burst();
    const uint32_t c0 = uint32_t(esp_cpu_get_cycle_count());
    const esp_err_t result = spi_device_transmit(display_handle(display_device_), &transaction);
    const uint32_t c1 = uint32_t(esp_cpu_get_cycle_count());
    const bool busy1 = audio_running();
    const bool sd1 = sd_log_burst();
    const uint32_t xfer = c1 - c0;
    auto &t = tft_timing_;
    ++t.transactions;
    t.xfer_cycles += xfer;
    if (xfer > t.xfer_max_cycles) t.xfer_max_cycles = xfer;
    if (xfer > openu5::kSlowXferUs * tft_cpu_mhz_) {
        ++t.slow_xfers;
        if (busy0 || busy1) ++t.slow_xfers_busy;
        // A3-04D: the SD-log writer was in a burst -- a card command holds this bus.
        if (sd0 || sd1) {
            ++t.slow_xfers_sd;
            if (xfer > t.xfer_max_sd_cycles) t.xfer_max_sd_cycles = xfer;
        }
    }
    if (start) {
        ++t.rows;
        const uint32_t fill = c0 - start->cycles;
        // Classified only when the audio task did not change state during the row.
        if (start->busy && busy0 && busy1) {
            ++t.rows_busy;
            t.fill_cycles_busy += fill;
            t.xfer_cycles_busy += xfer;
        } else if (!start->busy && !busy0 && !busy1) {
            ++t.rows_idle;
            t.fill_cycles_idle += fill;
            t.xfer_cycles_idle += xfer;
        }
    }
    return result;
}

void Board::tft_yield()
{
    const uint32_t c0 = uint32_t(esp_cpu_get_cycle_count());
    // A3-04E (ALPHA3_AUDIO.md section 22): a reschedule, not a sleep. Alpha
    // 2.0's vTaskDelay(1) waited for the next 10 ms tick at every call, and the
    // 16 rows between two calls take ~2 ms: a 158-row viewport spent ~90 ms
    // asleep, the menu-exit repaint ~370 ms. The sleep stays behind Developer >
    // Diagnostics > "Probe: legacy TFT pacing".
    // A3-04E.1 (section 23): a yield never hands core 0 to the lower-priority
    // idle task, and hardware showed the rows' own blocks do not reliably let
    // it finish a pass either (task watchdog on IDLE0). So the yield first asks
    // the idle-service guard, which sleeps one tick when -- and only when --
    // the idle loop has not run for 200 ms.
    if (const uint32_t ticks = openu5::tft_pause_ticks(tft_pacing_)) vTaskDelay(ticks);
    else if (!(idle_ && idle_->enforce())) taskYIELD();
    const uint32_t cycles = uint32_t(esp_cpu_get_cycle_count()) - c0;
    auto &t = tft_timing_;
    ++t.yields;
    t.yield_cycles += cycles;
    if (cycles > t.yield_max_cycles) t.yield_max_cycles = cycles;
    if (cycles > openu5::kLateYieldUs * tft_cpu_mhz_) ++t.late_yields;
}

esp_err_t Board::initialize_display()
{
    debug51::stage(4, "display-initialize-entry");
    ESP_RETURN_ON_ERROR(initialize_shared_spi(), kTag, "shared SPI setup failed");

    const spi_device_interface_config_t display_config = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,
        .clock_source = SPI_CLK_SRC_DEFAULT,
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = kTftClockHz,
        .input_delay_ns = 0,
        .sample_point = SPI_SAMPLING_POINT_PHASE_0,
        .spics_io_num = pins::kTftChipSelect,
        .flags = SPI_DEVICE_NO_DUMMY,
        .queue_size = 1,
        .pre_cb = nullptr,
        .post_cb = nullptr,
    };
    spi_device_handle_t handle = nullptr;
    ESP_RETURN_ON_ERROR(spi_bus_add_device(pins::kSharedSpiHost, &display_config, &handle),
                        kTag, "attach ST7789");
    display_device_ = handle;
    tft_cpu_mhz_ = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ; // A3-04C: CCOUNT -> microseconds (no DFS here)

    struct InitCommand {
        uint8_t command;
        uint8_t length;
        std::array<uint8_t, 14> data;
        uint16_t delay_ms;
    };
    // LilyGO's corrected ST7789 sequence (T-Deck repository, 2024-07-26 update).
    static constexpr InitCommand kInit[] = {
        {0x01, 0, {}, 120},
        {0x11, 0, {}, 120},
        {0x13, 0, {}, 0},
        {0x36, 1, {kMadctlRgb}, 0},
        {0x3A, 1, {0x55}, 10},
        {0xB2, 5, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 0},
        {0xB7, 1, {0x75}, 0},
        {0xBB, 1, {0x1A}, 0},
        {0xC0, 1, {0x2C}, 0},
        {0xC2, 1, {0x01}, 0},
        {0xC3, 1, {0x13}, 0},
        {0xC4, 1, {0x20}, 0},
        {0xC6, 1, {0x0F}, 0},
        {0xD0, 2, {0xA4, 0xA1}, 0},
        {0xD6, 1, {0xA1}, 0},
        {0xE0, 14, {0xD0, 0x0D, 0x14, 0x0D, 0x0D, 0x09, 0x38, 0x44,
                     0x4E, 0x3A, 0x17, 0x18, 0x2F, 0x30}, 0},
        {0xE1, 14, {0xD0, 0x09, 0x0F, 0x08, 0x07, 0x14, 0x37, 0x44,
                     0x4D, 0x38, 0x15, 0x16, 0x2C, 0x3E}, 0},
        {0x21, 0, {}, 0},
        // LilyGO Setup210 selects TFT_RGB_ORDER=TFT_RGB. Its setRotation(1)
        // writes MX | MV | RGB = 0x60; setting BGR here swaps red and blue.
        {0x36, 1, {kLandscapeRotation1Rgb}, 0},
        {0x29, 0, {}, 120},
    };
    for (const auto &entry : kInit) {
        ESP_RETURN_ON_ERROR(write_display_command(entry.command, entry.data.data(), entry.length),
                            kTag, "ST7789 initialization failed");
        if (entry.delay_ms != 0) {
            vTaskDelay(pdMS_TO_TICKS(entry.delay_ms));
        }
    }
    display_initialized_ = true;
    ESP_RETURN_ON_ERROR(fill_rect(0, 0, kDisplayWidth, kDisplayHeight, kBlack),
                        kTag, "clear display");
    // Assemble the first complete frame while the backlight is still dark.
    // This avoids exposing controller reset pixels or a half-drawn boot screen.
    ESP_RETURN_ON_ERROR(draw_text_box(0,54,kDisplayWidth,24,"OpenU5-TDeck",kCyan,3,3),
                        kTag,"draw coherent boot title");
    ESP_RETURN_ON_ERROR(draw_text_box(0,88,kDisplayWidth,16,"Alpha 2.0",kWhite,2,2),
                        kTag,"draw coherent boot version");
    ESP_RETURN_ON_ERROR(draw_text_box(0,124,kDisplayWidth,10,"Starting...",kGreen),
                        kTag,"draw coherent boot status");
    ESP_RETURN_ON_ERROR(set_brightness(80), kTag, "enable PWM backlight");
    ESP_LOGI(kTag,
             "ST7789 initialized at 320x240 landscape, RGB order, MADCTL=0x%02x, SPI clock %d Hz",
             kLandscapeRotation1Rgb, kTftClockHz);
    return ESP_OK;
}

esp_err_t Board::set_display_window(int x, int y, int width, int height)
{
    window_x_ = int16_t(x); window_y_ = int16_t(y); window_w_ = int16_t(width); window_h_ = int16_t(height);
    cursor_x_ = int16_t(x); cursor_y_ = int16_t(y);
    const uint16_t x_end = static_cast<uint16_t>(x + width - 1);
    const uint16_t y_end = static_cast<uint16_t>(y + height - 1);
    const uint8_t columns[] = {static_cast<uint8_t>(x >> 8), static_cast<uint8_t>(x),
                               static_cast<uint8_t>(x_end >> 8), static_cast<uint8_t>(x_end)};
    const uint8_t rows[] = {static_cast<uint8_t>(y >> 8), static_cast<uint8_t>(y),
                            static_cast<uint8_t>(y_end >> 8), static_cast<uint8_t>(y_end)};
    ESP_RETURN_ON_ERROR(write_display_command(0x2A, columns, sizeof(columns)), kTag,
                        "set TFT columns");
    ESP_RETURN_ON_ERROR(write_display_command(0x2B, rows, sizeof(rows)), kTag,
                        "set TFT rows");
    return write_display_command(0x2C);
}

esp_err_t Board::fill_rect(int x, int y, int width, int height, uint16_t color)
{
    if (!display_initialized_ || width <= 0 || height <= 0 || x < 0 || y < 0 ||
        x + width > kDisplayWidth || y + height > kDisplayHeight) {
        return ESP_ERR_INVALID_ARG;
    }
    ++draw_calls_.fill_rects;
    ESP_RETURN_ON_ERROR(set_display_window(x, y, width, height), kTag, "set fill window");
    auto &pixels = transfer_row_;
    static_assert(sizeof(transfer_row_) == kDisplayWidth * 2);
    // A3-04F (ALPHA3_AUDIO.md section 26): a chunk is the whole buffer, not one
    // row of the rectangle -- the window wraps each row into the next, so the
    // pixel stream is the same. A 1-2 px frame line was one 2-4 byte
    // transaction per row (180 for a viewport side); it is now one or two.
    const size_t pixels_per_chunk = kDisplayWidth;
    for (size_t index = 0; index < pixels_per_chunk; ++index) {
        pixels[index * 2] = static_cast<uint8_t>(color >> 8);
        pixels[index * 2 + 1] = static_cast<uint8_t>(color);
    }
    gpio_set_level(pins::kTftDataCommand, 1);
    int remaining = width * height;
    int chunks=0;
    while (remaining > 0) {
        const RowMark mark = row_mark();
        const int count = std::min(remaining, static_cast<int>(pixels_per_chunk));
        spi_transaction_t transaction{};
        transaction.length = count * 16;
        transaction.tx_buffer = pixels.data();
        ESP_RETURN_ON_ERROR(tft_row(transaction, mark), kTag, "write TFT pixels");
        remaining -= count;
        if(openu5::tft_chunk_yield_due(++chunks))tft_yield();
    }
    return ESP_OK;
}

esp_err_t Board::draw_rgb565(int x, int y, int width, int height, const uint16_t *pixels)
{
    return draw_rgb565_strided(x,y,width,height,pixels,width);
}

esp_err_t Board::set_brightness(uint8_t percent)
{
    percent=std::clamp<uint8_t>(percent,10,100);
    if(!backlight_pwm_initialized_){
        const ledc_timer_config_t timer={
            .speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_10_BIT,
            .timer_num=LEDC_TIMER_0,.freq_hz=5000,.clk_cfg=LEDC_AUTO_CLK,
            .deconfigure=false};
        ESP_RETURN_ON_ERROR(ledc_timer_config(&timer),kTag,"configure backlight PWM timer");
        const ledc_channel_config_t channel={
            .gpio_num=pins::kTftBacklight,.speed_mode=LEDC_LOW_SPEED_MODE,
            .channel=LEDC_CHANNEL_0,.intr_type=LEDC_INTR_DISABLE,
            .timer_sel=LEDC_TIMER_0,.duty=0,.hpoint=0,.sleep_mode=LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
            .flags={.output_invert=0},.deconfigure=false};
        ESP_RETURN_ON_ERROR(ledc_channel_config(&channel),kTag,"configure backlight PWM channel");
        backlight_pwm_initialized_=true;
    }
    const uint32_t duty=uint32_t(percent)*1023U/100U;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,duty),kTag,"set backlight duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);
}

esp_err_t Board::draw_rgb565_strided(int x,int y,int width,int height,
                                     const uint16_t *pixels,int stride)
{
    if (!display_initialized_ || pixels == nullptr || width <= 0 || height <= 0 ||
        stride < width || width > kDisplayWidth || x < 0 || y < 0 || x + width > kDisplayWidth ||
        y + height > kDisplayHeight) {
        return ESP_ERR_INVALID_ARG;
    }
    ++draw_calls_.rgb565;
    ESP_RETURN_ON_ERROR(set_display_window(x, y, width, height), kTag, "set RGB565 window");
    auto &row_bytes = transfer_row_;
    gpio_set_level(pins::kTftDataCommand, 1);
    const size_t row_length = size_t(width) * 2;
    size_t used = 0; // A3-04F: bytes of whole rows waiting in transfer_row_
    RowMark mark{};
    for (int row = 0; row < height; ++row) {
        if (used == 0) mark = row_mark();
        uint8_t *out = row_bytes.data() + used;
        for (int col = 0; col < width; ++col) {
            const uint16_t pixel = pixels[row * stride + col];
            out[col * 2] = static_cast<uint8_t>(pixel >> 8);
            out[col * 2 + 1] = static_cast<uint8_t>(pixel);
        }
        used += row_length;
        if (!row_batch_ends(used, row_length, row, height)) continue; // not a pause row either
        spi_transaction_t transaction{};
        transaction.length = used * 8;
        transaction.tx_buffer = row_bytes.data();
        used = 0;
        ESP_RETURN_ON_ERROR(tft_row(transaction, mark), kTag, "write RGB565 row");
        if(openu5::tft_row_yield_due(row))tft_yield();
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// A4-END1 (ALPHA4_UI.md section 7): ENDGAME.OVL's full-screen frames.
// ---------------------------------------------------------------------------
namespace {
constexpr int kEndgameTop = 20; // the 320 x 200 picture's first row on the 240-row panel
}

esp_err_t Board::show_endgame_page(const uint8_t *page, const uint16_t *palette,
                                   const openu5::EndgameScrollCell *cells, const uint8_t *normal_font,
                                   const uint8_t *rune_font, int key, bool force)
{
    if (!display_initialized_ || !page || !palette) return ESP_ERR_INVALID_ARG;
    if (!force && key == endgame_page_key_) return ESP_OK;
    ++tft_timing_.full_screen;
    ESP_RETURN_ON_ERROR(set_display_window(0, 0, kDisplayWidth, kDisplayHeight), kTag, "set ending window");
    gpio_set_level(pins::kTftDataCommand, 1);
    auto &row_bytes = transfer_row_;
    for (int row = 0; row < kDisplayHeight; ++row) {
        const RowMark mark = row_mark();
        const int py = row - kEndgameTop;
        for (int x = 0; x < kDisplayWidth; ++x) {
            uint16_t pixel = kBlack;
            if (py >= 0 && py < 200) {
                const uint8_t pair = page[py * (kDisplayWidth / 2) + (x >> 1)];
                pixel = palette[(x & 1) ? (pair & 0x0f) : (pair >> 4)];
                const auto *cell = cells ? &cells[(py >> 3) * openu5::kEndgameScrollCols + (x >> 3)] : nullptr;
                if (cell && (cell->flags & openu5::kEndgameCellPrinted)) {
                    // put_char: an opaque 8x8 cell, glyph bits in the white
                    // foreground, the rest black; reverse video swaps them.
                    const uint8_t *font = (cell->flags & openu5::kEndgameCellRunes) ? rune_font : normal_font;
                    const bool ink = font && ((font[cell->ch * 8 + (py & 7)] >> (7 - (x & 7))) & 1);
                    pixel = ink != ((cell->flags & openu5::kEndgameCellInverse) != 0) ? palette[15] : palette[0];
                }
            }
            row_bytes[x * 2] = static_cast<uint8_t>(pixel >> 8);
            row_bytes[x * 2 + 1] = static_cast<uint8_t>(pixel);
        }
        spi_transaction_t transaction{};
        transaction.length = kDisplayWidth * 16;
        transaction.tx_buffer = row_bytes.data();
        ESP_RETURN_ON_ERROR(tft_row(transaction, mark), kTag, "write ending row");
        if(openu5::tft_row_yield_due(row))tft_yield();
    }
    // The whole panel was painted over: every other screen redraws from scratch.
    endgame_page_key_ = key;
    alpha_drawn_ = false; frontend_cache_valid_ = false; debug_cache_valid_ = false;
    return ESP_OK;
}

bool Board::begin_endgame_capture()
{
    if (!endgame_shadow_)
        endgame_shadow_ = static_cast<uint16_t *>(heap_caps_malloc(size_t(kDisplayWidth) * kDisplayHeight * 2, MALLOC_CAP_SPIRAM));
    if (!endgame_shadow_) return false;
    std::fill(endgame_shadow_, endgame_shadow_ + kDisplayWidth * kDisplayHeight, kBlack);
    endgame_capturing_ = true;
    alpha_drawn_ = false; // the next show_alpha sends every pixel
    return true;
}

void Board::release_endgame_capture()
{
    endgame_capturing_ = false;
    if (endgame_shadow_) heap_caps_free(endgame_shadow_);
    endgame_shadow_ = nullptr;
}

esp_err_t Board::fizzle_endgame(openu5::EndgameFizzle &picture, openu5::EndgameFizzle &bands, uint32_t pixels)
{
    if (!display_initialized_ || !endgame_shadow_) return ESP_ERR_INVALID_STATE;
    endgame_capturing_ = false;
    uint16_t x = 0, y = 0;
    for (uint32_t i = 0; i < pixels && picture.next(x, y); ++i)
        endgame_shadow_[(y + kEndgameTop) * kDisplayWidth + x] = kBlack;
    // The bands above and below the picture are the device's own: fn34 over
    // their 320 x 40 rect, kept in step with the picture's progress.
    const uint64_t band_target = uint64_t(picture.produced()) * bands.total() / picture.total();
    while (bands.produced() < band_target && bands.next(x, y))
        endgame_shadow_[(y < kEndgameTop ? y : y + 200) * kDisplayWidth + x] = kBlack;
    endgame_page_key_ = -1;
    alpha_drawn_ = false; frontend_cache_valid_ = false; debug_cache_valid_ = false;
    ++tft_timing_.full_screen;
    return draw_rgb565(0, 0, kDisplayWidth, kDisplayHeight, endgame_shadow_);
}

esp_err_t Board::draw_text(int x, int y, const char *text, uint16_t color, int scale)
{
    for (const char *cursor = text; *cursor != '\0'; ++cursor, x += 6 * scale) {
        const auto bitmap = glyph(*cursor);
        for (int column = 0; column < 5; ++column) {
            const uint8_t bits = bitmap[column];
            for (int row = 0; row < 7; ++row) {
                if ((bits & (1U << row)) != 0) {
                    ESP_RETURN_ON_ERROR(fill_rect(x + column * scale, y + row * scale,
                                                  scale, scale, color),
                                        kTag, "draw glyph");
                }
            }
        }
    }
    return ESP_OK;
}

void Board::show_diagnostics(bool sd_ok)
{
    debug51::stage(6, "startup-screen-clear-draw");
    if (!display_initialized_) {
        return;
    }
    if (fill_rect(0, 0, kDisplayWidth, kDisplayHeight, kBlack) != ESP_OK ||
        draw_text(18, 14, "OpenU5-TDeck", kCyan, 3) != ESP_OK ||
        draw_text(18, 50, "Alpha 2.0 Debug", kWhite, 2) != ESP_OK ||
        draw_text(18, 82, "ESP32-S3", kWhite, 2) != ESP_OK ||
        draw_text(18, 108, "16 MB Flash", kWhite, 2) != ESP_OK ||
        draw_text(18, 134, "8 MB PSRAM", kWhite, 2) != ESP_OK ||
        draw_text(18, 174, sd_ok ? "SD: OK" : "SD: FAIL", sd_ok ? kGreen : kRed, 3) != ESP_OK) {
        ESP_LOGE(kTag, "Failed to draw diagnostic screen");
    }
}

void Board::show_runtime_identity(const char *firmware,const char *git_commit,
                                  const char *build_timestamp,const char *alpha_identity,
                                  const char *asset_identity,bool packs_match)
{
    if(!display_initialized_)return;
    if(fill_rect(0,0,kDisplayWidth,kDisplayHeight,kBlack)!=ESP_OK||
       draw_text(8,8,"OpenU5 HARDWARE-TRUTH",kCyan,2)!=ESP_OK||
       draw_text(8,34,firmware?firmware:"firmware unknown",kWhite,1)!=ESP_OK||
       draw_text(8,50,git_commit?git_commit:"git unknown",kWhite,1)!=ESP_OK||
       draw_text(8,66,build_timestamp?build_timestamp:"build unknown",kWhite,1)!=ESP_OK||
       draw_text(8,92,alpha_identity?alpha_identity:"resources missing",packs_match?kGreen:kRed,1)!=ESP_OK||
       draw_text(8,108,asset_identity?asset_identity:"assets missing",packs_match?kGreen:kRed,1)!=ESP_OK||
       draw_text(8,142,packs_match?"PACKS MATCH FIRMWARE":"RESOURCE PACK MISMATCH",packs_match?kGreen:kRed,2)!=ESP_OK||
       draw_text(8,184,packs_match?"Starting diagnostic runtime":"Startup blocked; update SD packs",kWhite,1)!=ESP_OK)
        ESP_LOGE(kTag,"Failed to draw runtime identity screen");
}

esp_err_t Board::show_view(const uint16_t *pixels, int width, int height,
                          const char *coordinates, const char *location, bool first_draw)
{
    if (first_draw) debug51::stage(11, "game-screen-transition");
    if (!display_initialized_) return ESP_ERR_INVALID_STATE;
    if (first_draw) {
        ESP_RETURN_ON_ERROR(fill_rect(0, 0, kDisplayWidth, kDisplayHeight, kBlack), kTag,
                            "clear legacy screen");
    } else {
        ESP_RETURN_ON_ERROR(fill_rect(198, 76, 120, 18, kBlack), kTag,
                            "clear live coordinates");
    }
    ESP_RETURN_ON_ERROR(draw_rgb565(8, 8, width, height, pixels), kTag,
                        "draw live viewport");
    if (first_draw) {
        ESP_RETURN_ON_ERROR(draw_text(200, 16, "OpenU5", kCyan, 2), kTag, "draw title");
        ESP_RETURN_ON_ERROR(draw_text(200, 48, "Alpha 2.0", kWhite, 1), kTag,
                            "draw version");
    }
    ESP_RETURN_ON_ERROR(draw_text(200, 78, coordinates, kWhite, 1), kTag,
                        "draw coordinates");
    ESP_RETURN_ON_ERROR(draw_text(200, 96, location, kWhite, 1), kTag,
                        "draw location");
    ESP_RETURN_ON_ERROR(draw_text(200, 126, "SD: OK", kGreen, 1), kTag,
                        "draw SD status");
    return ESP_OK;
}

esp_err_t Board::show_camp_viewport(const uint16_t *pixels,uint32_t)
{
    if(!display_initialized_||!alpha_drawn_||!pixels)return ESP_ERR_INVALID_STATE;
    const auto result=draw_rgb565(openu5::kHudViewportX,openu5::kHudViewportY,
                                  openu5::kViewportPixels,openu5::kViewportPixels,pixels);
    if(result==ESP_OK){viewport_cache_valid_=false;full_square_active_=true;}
    return result;
}

esp_err_t Board::fill_bed_viewport()
{
    // Original (8,8)-(183,183) is the entire 176x176 gameplay window.
    // Native's sky and wind strips live inside that window, so preserve them.
    const auto r=bed_viewport_rect();
    const auto result=fill_rect(r.x,r.y,r.width,r.height,kBlack);
    if(result==ESP_OK)viewport_cache_valid_=false;
    return result;
}

esp_err_t Board::draw_panel_row(size_t slot,int y,const char *text,uint16_t color,bool invert,bool force,
                                ChromeFont font,size_t accent_from,uint16_t accent) {
    // A3-04F (ALPHA3_AUDIO.md section 26): up to A3-HF2.1 all nine rows were
    // rewritten on every frame that was not an animation tick -- 117 SPI
    // transactions (~7 ms of transfers) when nothing in them had changed.
    auto &cached=panel_rows_[slot];
    if(!force&&cached.valid&&cached.color==color&&cached.invert==invert&&std::strcmp(cached.text,text)==0)return ESP_OK;
    // Alpha 4 UI Batch 1: 134 px, one short of kHudRightW -- the 135th column
    // (x=318) is the boxes' right rule, which every row used to paint black.
    ESP_RETURN_ON_ERROR(draw_cells(openu5::kHudRightX,y,openu5::kHudRightW-1,8,
                                   font==ChromeFont::Ibm?kRosterTextX:openu5::kHudRightX,text,color,
                                   accent_from,accent,invert,font),kTag,"draw panel row");
    std::snprintf(cached.text,sizeof(cached.text),"%s",text);cached.color=color;cached.invert=invert;cached.valid=true;
    return ESP_OK;
}

esp_err_t Board::draw_party_rows(const openu5::GameState &game,DevicePartyHighlight party_highlight,bool force) {
    const auto members=openu5::party_members(game.party);
    // Y-04 (#213): `damage_flash` puts ONE row in reverse video -- the binary's
    // 0x2a28, an XOR of the row's rectangle, shared with the picker cursor and
    // the combat hit. It is the top of openu5::roster_invert_row's precedence.
    // Alpha 4 UI Batch 1: the original's roster row (web port roster.ts, kernel
    // draw_roster_row 0x27ab): the name padded to 9, the -> (IBM.CH 0x1a) on the
    // ACTIVE member unless asleep or dead, HP right-aligned in 4, the status
    // letter -- in IBM.CH. The picker / combat-actor tints are today's.
    for(size_t row=0;row<6;++row){char line[24]{};uint16_t color=kWhite;bool invert=false;if(row<members.count){const auto index=members.indices[row];const auto&a=game.party.characters[index];const bool selected=index==party_highlight.selected,actor=index==party_highlight.actor;const char status=a.status?char(a.status):'G';const bool arrow=index==game.party.active_character&&status!='D'&&status!='S';std::snprintf(line,sizeof(line),"%-9.9s%c%4u%c",a.name,arrow?'\x1a':' ',unsigned(std::min<uint16_t>(a.current_hp,9999)),status);color=selected?kGreen:actor?kCyan:kWhite;invert=index==party_highlight.damage_flash;}ESP_RETURN_ON_ERROR(draw_panel_row(row,4+int(row)*8,line,color,invert,force,ChromeFont::Ibm),kTag,"draw party row");}
    return ESP_OK;
}

esp_err_t Board::refresh_bed_status_panel(const openu5::GameState &game,DevicePartyHighlight party_highlight) {
    if(!display_initialized_||!alpha_drawn_)return ESP_ERR_INVALID_STATE;
    // Drawn unconditionally, as before; the rows' caches learn what is shown.
    ESP_RETURN_ON_ERROR(draw_party_rows(game,party_highlight,true),kTag,"refresh bed party rows");
    // Alpha 4 UI Batch 1: the same three status rows show_alpha draws, with
    // the Movement Mode it last showed.
    const char *name=hud_location_caption(game.position.map.location,game.position.map.floor,false,0);
    return draw_status_rows(game,name,status_move_,true);
}

esp_err_t Board::draw_status_rows(const openu5::GameState &game,const char *place,bool move,bool force) {
    // Row A (compact): the native location caption; in Movement Mode it is
    // clipped to 17 characters and MOVE (green) takes cells 18-21.
    char location[24]{};
    if(move)std::snprintf(location,sizeof(location),"%-17.17s MOVE",place);
    else std::snprintf(location,sizeof(location),"%.22s",place);
    ESP_RETURN_ON_ERROR(draw_panel_row(6,58,location,kCyan,false,force,ChromeFont::Compact,move?size_t(18):SIZE_MAX,kGreen),
                        kTag,"draw location caption");
    // Rows B and C (IBM.CH): the original's food/gold/date box (web port
    // panel.ts) -- F: left, G: right; the date month-day-year, with the
    // digital clock kept on its right.
    char left[48]{},right[48]{},line[24]{};
    std::snprintf(left,sizeof(left),"F:%u",unsigned(game.food));
    std::snprintf(right,sizeof(right),"G:%u",unsigned(game.gold));
    split_cells(line,sizeof(line),left,right,15);
    ESP_RETURN_ON_ERROR(draw_panel_row(7,68,line,kWhite,false,force,ChromeFont::Ibm),kTag,"draw food and gold");
    std::snprintf(left,sizeof(left),"%ld-%ld-%ld",long(game.time.month),long(game.time.day),long(game.time.year));
    std::snprintf(right,sizeof(right),"%02ld:%02ld",long(game.time.hour),long(game.time.minute));
    split_cells(line,sizeof(line),left,right,15);
    return draw_panel_row(8,78,line,kWhite,false,force,ChromeFont::Ibm);
}

esp_err_t Board::show_alpha(const uint16_t *pixels,const openu5::UiSession &ui,
                            const openu5::GameState &game,const openu5::TurnState &turn,
                            const openu5::HudWorldState &hud,const uint8_t *runes_font,const char *overlay,
                            const uint8_t *animated_cells,bool animation_only,
                            const DeviceDebugScreen *debug,bool movement_mode,
                            uint8_t ui_size,const DeviceShopView *shop,const DeviceSelectionView *selection,
                            const DeviceContextActionBar *context_bar,DevicePartyHighlight party_highlight,
                            uint32_t viewport_crc,const openu5::HudDungeonBands *dungeon_bands,
                            bool full_square_viewport,bool preserve_party_panel)
{
    // R-05.  The T-Deck's 176x176 viewport has no 8 px margin to put the
    // original's dungeon bands in, so they are drawn over the same two 9 px
    // strips the overworld uses for sky and wind -- a documented platform
    // divergence in POSITION only.  Their CONTENT is the original's, and
    // without them a turn in place gave the player no feedback whatsoever in a
    // corridor whose two directions look alike.
    const bool bands_active=dungeon_bands&&dungeon_bands->active;
    if(!display_initialized_||!pixels)return ESP_ERR_INVALID_STATE;
    (void)turn;(void)overlay;
    debug_last_full_redraw_=false;debug_last_dirty_regions_=0;debug_last_pixels_=0;
    endgame_page_key_=-1;
    if(!alpha_drawn_||frontend_drawn_){++tft_timing_.full_screen;ESP_RETURN_ON_ERROR(fill_rect(0,0,kDisplayWidth,kDisplayHeight,kChromeBand),kTag,"initialize Alpha 2.0 game screen");alpha_drawn_=true;frontend_drawn_=false;alpha_ui_cache_valid_=false;alpha_ui_size_cache_=0xff;viewport_cache_valid_=false;sky_bar_cache_valid_=false;shop_cache_valid_=false;selection_cache_valid_=false;context_cache_valid_=false;animation_only=false;debug_last_full_redraw_=true;debug_last_pixels_=kDisplayWidth*kDisplayHeight;}
    if(debug){
        const bool full=!debug_drawn_||!debug_cache_valid_;
        debug_last_full_redraw_=full;debug_last_dirty_regions_=0;debug_last_pixels_=0;
        if(full){
            ESP_RETURN_ON_ERROR(fill_rect(0,0,kDisplayWidth,kDisplayHeight,kBlack),kTag,"enter developer screen");
            debug_last_pixels_+=kDisplayWidth*kDisplayHeight;
            ESP_RETURN_ON_ERROR(draw_text_box(8,6,304,16,"Developer",kCyan,2,2),kTag,"draw developer title");
            ESP_RETURN_ON_ERROR(draw_text_box(8,216,304,11,"Trackball: Move  Enter: Select",kWhite),kTag,"draw developer controls");
            ESP_RETURN_ON_ERROR(draw_text_box(8,229,304,11,"Mic: Back",kWhite),kTag,"draw developer back control");
            debug_last_dirty_regions_+=3;debug_last_pixels_+=304*(16+11+11);
        }
        if(full||std::strcmp(debug_cache_.breadcrumb,debug->breadcrumb)!=0){
            ESP_RETURN_ON_ERROR(draw_text_box(8,25,244,11,debug->breadcrumb,kWhite),kTag,"update developer breadcrumb");
            ++debug_last_dirty_regions_;debug_last_pixels_+=244*11;
        }
        if(full||std::strcmp(debug_cache_.position,debug->position)!=0){
            ESP_RETURN_ON_ERROR(draw_text_box(256,25,64,11,debug->position,kGreen),kTag,"update developer position");
            ++debug_last_dirty_regions_;debug_last_pixels_+=64*11;
        }
        const size_t row_limit=std::max(debug->row_count,debug_cache_.row_count);
        for(size_t i=0;i<row_limit&&i<kDebugScreenRows;++i){
            const bool current=i<debug->row_count;
            const bool cached=i<debug_cache_.row_count;
            const bool changed=full||current!=cached||
                (current&&std::strcmp(debug_cache_.rows[i],debug->rows[i])!=0)||
                i==debug->selected_row||i==debug_cache_.selected_row;
            if(!changed)continue;
            char line[54]{};
            if(current)std::snprintf(line,sizeof(line),"%c %.49s",i==debug->selected_row?'>':' ',debug->rows[i]);
            ESP_RETURN_ON_ERROR(draw_text_box(8,43+int(i)*17,312,17,line,
                                current&&i==debug->selected_row?kGreen:kWhite),kTag,"update developer item");
            ++debug_last_dirty_regions_;debug_last_pixels_+=312*17;
        }
        if(full||std::strcmp(debug_cache_.status,debug->status)!=0){
            ESP_RETURN_ON_ERROR(draw_text_box(8,199,304,11,debug->status,kCyan),kTag,"update developer status");
            ++debug_last_dirty_regions_;debug_last_pixels_+=304*11;
        }
        debug_cache_=*debug;debug_cache_valid_=true;debug_drawn_=true;
        return ESP_OK;
    }
    if(debug_drawn_){++tft_timing_.full_screen;ESP_RETURN_ON_ERROR(fill_rect(0,0,kDisplayWidth,kDisplayHeight,kChromeBand),kTag,"leave developer screen");debug_drawn_=false;debug_cache_valid_=false;alpha_ui_cache_valid_=false;alpha_ui_size_cache_=0xff;viewport_cache_valid_=false;sky_bar_cache_valid_=false;shop_cache_valid_=false;selection_cache_valid_=false;context_cache_valid_=false;animation_only=false;}
    if((!shop||!shop->active)&&shop_cache_valid_){
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,0,kDisplayWidth-openu5::kHudPartyFrameX,kDisplayHeight,kBlack),kTag,"leave shop panel");
        shop_cache_valid_=false;context_cache_valid_=false;alpha_ui_cache_valid_=false;
    }
    if((!selection||!selection->active)&&selection_cache_valid_){
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,0,kDisplayWidth-openu5::kHudPartyFrameX,kDisplayHeight,kBlack),kTag,"leave compact selector");
        selection_cache_valid_=false;context_cache_valid_=false;alpha_ui_cache_valid_=false;
    }
    // Alpha 4 UI Batch 1 (ALPHA4_UI.md): the two 9 px strips keep their
    // rectangles and become frame -- 8 rows of band with the original's notch
    // and >< ends (bandBracket.ts), plus the white rule that bounds the map
    // (the sky strip's last row, the wind strip's first). The sky track sits in
    // the notch as before; the wind and the dungeon's level / facing are
    // captions in IBM.CH, as the original's band text is.
    auto draw_sky_bar=[&]()->esp_err_t{
        if(bands_active)
            return draw_band_strip(openu5::kHudSkyBarX,openu5::kHudSkyBarY,openu5::kHudSkyBarW,openu5::kHudSkyBarH,
                                   openu5::kHudSkyBarH-1,dungeon_bands->level,nullptr,nullptr);
        if(!runes_font)return ESP_ERR_INVALID_ARG;
        ++draw_calls_.sky_strips;
        return draw_band_strip(openu5::kHudSkyBarX,openu5::kHudSkyBarY,openu5::kHudSkyBarW,openu5::kHudSkyBarH,
                               openu5::kHudSkyBarH-1,nullptr,&hud,runes_font);
    };
    auto draw_wind_bar=[&]()->esp_err_t{
        const char *caption=bands_active?dungeon_bands->direction:hud.wind_visible?hud.wind:"";
        return draw_band_strip(openu5::kHudWindBarX,openu5::kHudWindBarY,openu5::kHudWindBarW,openu5::kHudWindBarH,
                               0,caption,nullptr,nullptr);
    };
    if(animation_only&&animated_cells){
        // A3-04F (ALPHA3_AUDIO.md section 26): a run of adjacent animated cells
        // in one tile row is one window over the same composed pixels -- one
        // window setup (five transactions) per run, not per cell.
        for(int row=0;row<openu5::kViewportTiles;++row)for(int col=0;col<openu5::kViewportTiles;){const int i=row*openu5::kViewportTiles+col;if(!animated_cells[i]){++col;continue;}
            int end=col+1;while(end<openu5::kViewportTiles&&animated_cells[row*openu5::kViewportTiles+end])++end;
            const int clip_top=row==0?openu5::kHudSkyBarH:0;
            const int clip_bottom=row==openu5::kViewportTiles-1?openu5::kHudWindBarH:0;
            const int height=openu5::kTilePixels-clip_top-clip_bottom;if(height<=0){col=end;continue;}
            ESP_RETURN_ON_ERROR(draw_rgb565_strided(openu5::kHudViewportX+col*openu5::kTilePixels,openu5::kHudViewportY+row*openu5::kTilePixels+clip_top,(end-col)*openu5::kTilePixels,height,
                                pixels+(row*openu5::kTilePixels+clip_top)*openu5::kViewportPixels+col*openu5::kTilePixels,openu5::kViewportPixels),kTag,"draw clipped animated Alpha cells");
            col=end;}
        return ESP_OK;
    }
    // R-17/Y-14: the gem view is a full-square 176x176 composition, not the
    // world/dungeon3d viewport that legitimately cedes its top/bottom 9 px to
    // the sky and wind strips (or, for a mounted dungeon, their band-caption
    // replacements). Toggling in or out of that mode must not leave a stale
    // bar fragment or a stale clipped-rectangle behind, so force both caches
    // on the transition either way.
    if(full_square_viewport!=full_square_active_){viewport_cache_valid_=false;sky_bar_cache_valid_=false;full_square_active_=full_square_viewport;}
    // The band captions take part in the SAME cache signatures as the strips
    // they replace, so a Klimb (level) or a turn (direction) repaints its strip
    // on the very frame that produced it, and nothing else repaints.
    uint32_t sky_signature=bands_active?0x9dU:hud.sky_visible?0x51U:0x17U;
    if(bands_active)for(const char *c=dungeon_bands->level;*c;++c)sky_signature=sky_signature*16777619U^uint32_t(uint8_t(*c));
    else for(size_t i=0;i<hud.mark_count;++i)sky_signature=sky_signature*16777619U^(uint32_t(hud.marks[i].cell)<<16|uint32_t(hud.marks[i].glyph)<<8|uint32_t(hud.marks[i].sun));
    char wind_text[34]{};
    if(bands_active)std::snprintf(wind_text,sizeof(wind_text),"%.30s",dungeon_bands->direction);
    else std::snprintf(wind_text,sizeof(wind_text),"%.30s",hud.wind_visible?hud.wind:"");
    const bool viewport_changed=!viewport_cache_valid_||viewport_crc_!=viewport_crc;
    if(viewport_changed){
        ++tft_timing_.viewport_full; // A3-04C: a whole-viewport frame
        const int top=full_square_viewport?0:openu5::kHudSkyBarH;
        const int height=full_square_viewport?openu5::kViewportPixels:openu5::kViewportPixels-openu5::kHudSkyBarH-openu5::kHudWindBarH;
        ESP_RETURN_ON_ERROR(draw_rgb565_strided(openu5::kHudViewportX,openu5::kHudViewportY+top,openu5::kViewportPixels,height,pixels+top*openu5::kViewportPixels,openu5::kViewportPixels),kTag,"draw Alpha viewport");
        debug_last_pixels_+=openu5::kViewportPixels*height;++debug_last_dirty_regions_;
    }
    if(!full_square_viewport){
        if(!sky_bar_cache_valid_||sky_bar_signature_!=sky_signature){ESP_RETURN_ON_ERROR(draw_sky_bar(),kTag,"compose authentic sky strip");sky_bar_signature_=sky_signature;sky_bar_cache_valid_=true;debug_last_pixels_+=openu5::kHudSkyBarW*openu5::kHudSkyBarH;++debug_last_dirty_regions_;}
        if(!viewport_cache_valid_||std::strcmp(wind_bar_cache_,wind_text)!=0){ESP_RETURN_ON_ERROR(draw_wind_bar(),kTag,"compose lower strip");std::snprintf(wind_bar_cache_,sizeof(wind_bar_cache_),"%s",wind_text);debug_last_pixels_+=openu5::kHudWindBarW*openu5::kHudWindBarH;++debug_last_dirty_regions_;}
    }
    viewport_crc_=viewport_crc;viewport_cache_valid_=true;
    if((!shop||!shop->active)&&(!selection||!selection->active)&&alpha_ui_size_cache_!=ui_size){
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,0,kDisplayWidth-openu5::kHudPartyFrameX,kDisplayHeight,kBlack),kTag,"reflow gameplay UI scale");
        alpha_ui_cache_valid_=false;alpha_ui_size_cache_=ui_size;
        std::memset(transcript_cache_,0,sizeof(transcript_cache_));
    }
    if(!alpha_ui_cache_valid_){
        // Alpha 4 UI Batch 1 (ALPHA4_UI.md): the original's frame structure
        // (web port frame.ts) inside the unchanged HUD rectangles. Everything
        // that is not a content window is band (the full clears paint it; the
        // touch reserve stays band); the play window gets a 1 px white rule on
        // its sides (the strips carry its top and bottom rows); the roster and
        // status boxes are white-ruled and joined by a 2 px blue bar; the
        // console below them is unboxed and runs to the glass, as the
        // original's does. The right column's interior is black already: every
        // path that clears this cache has filled x=182..319 black first.
        const int top=openu5::kHudViewportY+openu5::kHudSkyBarH-1,bottom=openu5::kHudWindBarY;
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudViewportX-1,top,1,bottom-top+1,kChromeRule),kTag,"play window rule left");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudViewportX+openu5::kHudViewportW,top,1,bottom-top+1,kChromeRule),kTag,"play window rule right");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,0,kDisplayWidth-openu5::kHudPartyFrameX,openu5::kHudPartyFrameY,kChromeBand),kTag,"band above the boxes");
        ESP_RETURN_ON_ERROR(fill_rect(kDisplayWidth-1,openu5::kHudPartyFrameY,1,openu5::kHudTranscriptSeparatorY-openu5::kHudPartyFrameY+1,kChromeBand),kTag,"band right of the boxes");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudWorldFrameX,openu5::kHudWorldFrameY,openu5::kHudWorldFrameW,2,kChromeBand),kTag,"bar between the boxes");
        ESP_RETURN_ON_ERROR(fill_rect(kDisplayWidth-1,openu5::kHudTranscriptSeparatorY+1,1,kDisplayHeight-openu5::kHudTranscriptSeparatorY-1,kBlack),kTag,"console to the glass");
        const int box_right=openu5::kHudPartyFrameX+openu5::kHudPartyFrameW-1,status_top=openu5::kHudWorldFrameY+2;
        for(const auto &r:std::array<std::array<int,4>,9>{{
                {{openu5::kHudPartyFrameX,openu5::kHudPartyFrameY,openu5::kHudPartyFrameW,1}},
                {{openu5::kHudPartyFrameX,openu5::kHudPartyFrameY+openu5::kHudPartyFrameH-1,openu5::kHudPartyFrameW,1}},
                {{openu5::kHudPartyFrameX,openu5::kHudPartyFrameY,1,openu5::kHudPartyFrameH}},
                {{box_right,openu5::kHudPartyFrameY,1,openu5::kHudPartyFrameH}},
                {{openu5::kHudWorldFrameX,status_top,openu5::kHudWorldFrameW,1}},
                {{openu5::kHudPartyFrameX,openu5::kHudTranscriptSeparatorY,openu5::kHudPartyFrameW,1}},
                {{openu5::kHudWorldFrameX,status_top,1,openu5::kHudTranscriptSeparatorY-status_top}},
                {{box_right,status_top,1,openu5::kHudTranscriptSeparatorY-status_top}},
                {{openu5::kHudPartyFrameX,openu5::kHudTranscriptSeparatorY,1,kDisplayHeight-openu5::kHudTranscriptSeparatorY}}}})
            ESP_RETURN_ON_ERROR(fill_rect(r[0],r[1],r[2],r[3],kChromeRule),kTag,"draw box rule");
    }
    auto draw_context_bar=[&](const DeviceContextActionBar &bar,int x,int width,bool first)->esp_err_t{
        if(first||!context_cache_valid_){
            ESP_RETURN_ON_ERROR(fill_rect(x,kContextBarTop,width,1,kChromeDim),kTag,"context action separator");
        }
        if(first||!context_cache_valid_||std::strcmp(context_cache_.status,bar.status)!=0)
            ESP_RETURN_ON_ERROR(draw_text_box(x+2,kContextBarStatusY,width-4,kContextBarStatusH,bar.status,kCyan),kTag,"context active status");
        if(first||!context_cache_valid_||std::strcmp(context_cache_.actions,bar.actions)!=0)
            ESP_RETURN_ON_ERROR(draw_text_box(x+2,kContextBarActionsY,width-4,kContextBarActionsH,bar.actions,kGreen),kTag,"context valid actions");
        context_cache_=bar;context_cache_valid_=true;return ESP_OK;
    };
    // A3-04F: the visible transcript lines, built each frame -- a local on the
    // main task's stack (it was a Board member in internal .data).
    openu5::UiRenderedLine transcript_lines[kAlphaTranscriptLines]{};
    if(shop&&shop->active){
        const bool first=!shop_cache_valid_;
        auto account=[&](size_t pixels){++debug_last_dirty_regions_;debug_last_pixels_+=pixels;};
        if(first){
            ESP_RETURN_ON_ERROR(fill_rect(182,0,138,240,kBlack),kTag,"initialize U5 shop panel");account(138*240);
            context_cache_valid_=false;
            for(const auto &r:std::array<std::array<int,4>,8>{{{{182,2,137,1}},{{182,33,137,1}},{{182,35,137,1}},{{182,124,137,1}},{{182,149,137,1}},{{182,239,137,1}},{{182,2,1,238}},{{318,2,1,238}}}})
                {ESP_RETURN_ON_ERROR(fill_rect(r[0],r[1],r[2],r[3],kChromeRule),kTag,"draw shop separator");account(size_t(r[2]*r[3]));}
        }
        auto changed=[&](const char*a,const char*b){return first||std::strcmp(a,b)!=0;};
        if(changed(shop->title,shop_cache_.title)){ESP_RETURN_ON_ERROR(draw_text_box(184,4,134,8,shop->title,kCyan),kTag,"shop title");account(134*8);}
        if(changed(shop->keeper,shop_cache_.keeper)){ESP_RETURN_ON_ERROR(draw_text_box(184,13,134,8,shop->keeper,kWhite),kTag,"shop keeper");account(134*8);}
        if(changed(shop->phase,shop_cache_.phase)){ESP_RETURN_ON_ERROR(draw_text_box(184,23,134,8,shop->phase,kGreen),kTag,"shop phase/page");account(134*8);}
        const size_t rows=std::max(shop->row_count,shop_cache_.row_count);
        for(size_t i=0;i<rows&&i<kShopVisibleRows;++i){
            const bool current=i<shop->row_count;
            const bool row_changed=shop_row_needs_redraw(*shop,shop_cache_,i,first);
            if(!row_changed)continue;
            char line[24]{};if(current){
                if(shop->rows[i].quantity>1)std::snprintf(line,sizeof(line),"%c%-12.12s %3ldg",i==shop->selected_row?'>':' ',shop->rows[i].name,long(shop->rows[i].price));
                else std::snprintf(line,sizeof(line),"%c%-14.14s %3ldg",i==shop->selected_row?'>':' ',shop->rows[i].name,long(shop->rows[i].price));
            }
            ESP_RETURN_ON_ERROR(draw_text_box(184,39+int(i)*14,134,12,line,current&&i==shop->selected_row?kGreen:kWhite),kTag,"shop offer row");account(134*12);
        }
        if(first||shop->gold!=shop_cache_.gold){char line[24]{};std::snprintf(line,sizeof(line),"Gold: %ld",long(shop->gold));ESP_RETURN_ON_ERROR(draw_text_box(184,127,134,8,line,kWhite),kTag,"shop gold");account(134*8);}
        std::fill(std::begin(transcript_lines),std::end(transcript_lines),openu5::UiRenderedLine{});
        const auto count=ui.visible_lines(transcript_lines,kShopLogRows,openu5::kHudTranscriptColumns);
        for(size_t i=0;i<kShopLogRows;++i){const char*text=i<count?transcript_lines[i].text:"";const uint32_t seq=i<count?transcript_lines[i].sequence:0;const uint16_t color=i<count&&transcript_lines[i].channel==openu5::UiTextChannel::Shop?kCyan:kWhite;auto&cached=transcript_cache_[i];if(first||cached.sequence!=seq||cached.color!=color||std::strcmp(cached.text,text)!=0){ESP_RETURN_ON_ERROR(draw_text_box(184,152+int(i)*8,134,8,text,color),kTag,"shop transcript row");account(134*8);cached.sequence=seq;cached.color=color;std::snprintf(cached.text,sizeof(cached.text),"%s",text);}}
        ESP_RETURN_ON_ERROR(draw_context_bar(shop->context,182,137,first),kTag,"shop context action bar");account(137*25);
        shop_cache_=*shop;shop_cache_valid_=true;alpha_ui_cache_valid_=true;
        return ESP_OK;
    }
    if(selection&&selection->active){
        const bool first=!selection_cache_valid_;
        auto account=[&](size_t count){++debug_last_dirty_regions_;debug_last_pixels_+=count;};
        if(first){
            ESP_RETURN_ON_ERROR(fill_rect(182,0,138,240,kBlack),kTag,"initialize compact selector");account(138*240);
            context_cache_valid_=false;
            for(const auto&r:std::array<std::array<int,4>,7>{{{{182,2,137,1}},{{182,23,137,1}},{{182,43,137,1}},{{182,160,137,1}},{{182,213,137,1}},{{182,2,1,238}},{{318,2,1,238}}}}){
                ESP_RETURN_ON_ERROR(fill_rect(r[0],r[1],r[2],r[3],kChromeRule),kTag,"draw selector separator");account(size_t(r[2]*r[3]));}
        }
        auto changed=[&](const char*a,const char*b){return first||std::strcmp(a,b)!=0;};
        if(changed(selection->title,selection_cache_.title)){ESP_RETURN_ON_ERROR(draw_text_box(184,5,134,14,selection->title,kCyan),kTag,"selector title");account(134*14);}
        if(changed(selection->detail,selection_cache_.detail)){ESP_RETURN_ON_ERROR(draw_text_box(184,26,134,8,selection->detail,kWhite),kTag,"selector detail row one");account(134*8);}
        if(changed(selection->detail2,selection_cache_.detail2)){ESP_RETURN_ON_ERROR(draw_text_box(184,35,134,8,selection->detail2,kWhite),kTag,"selector detail row two");account(134*8);}
        const size_t rows=std::max(selection->row_count,selection_cache_.row_count);
        for(size_t i=0;i<rows&&i<kSelectionVisibleRows;++i){if(!selection_row_needs_redraw(*selection,selection_cache_,i,first))continue;char line[24]{};if(i<selection->row_count)std::snprintf(line,sizeof(line),"%c%.21s",i==selection->selected_row?'>':' ',selection->rows[i]);ESP_RETURN_ON_ERROR(draw_text_box(184,46+int(i)*14,134,12,line,i<selection->row_count&&i==selection->selected_row?kGreen:kWhite),kTag,"selector row");account(134*12);}
        std::fill(std::begin(transcript_lines),std::end(transcript_lines),openu5::UiRenderedLine{});const auto count=ui.visible_lines(transcript_lines,kSelectorLogRows,openu5::kHudTranscriptColumns);
        const bool show_transcript=selection_uses_transcript(*selection);
        for(size_t i=0;i<kSelectorLogRows;++i){const char*text=show_transcript&&i<count?transcript_lines[i].text:"";const uint32_t seq=show_transcript&&i<count?transcript_lines[i].sequence:0;const uint16_t color=show_transcript&&i<count&&transcript_lines[i].channel==openu5::UiTextChannel::Combat?kRed:kWhite;auto&cached=transcript_cache_[i];if(first||cached.sequence!=seq||cached.color!=color||std::strcmp(cached.text,text)!=0){ESP_RETURN_ON_ERROR(draw_text_box(184,172+int(i)*8,134,8,text,color),kTag,"selector transcript");account(134*8);cached.sequence=seq;cached.color=color;std::snprintf(cached.text,sizeof(cached.text),"%s",text);}}
        ESP_RETURN_ON_ERROR(draw_context_bar(selection->context,182,137,first),kTag,"selector context action bar");account(137*25);
        selection_cache_=*selection;selection_cache_valid_=true;alpha_ui_cache_valid_=true;return ESP_OK;
    }
    if(!preserve_party_panel) {
        ESP_RETURN_ON_ERROR(draw_party_rows(game,party_highlight,!alpha_ui_cache_valid_),kTag,"draw party rows");
    }
    // Batch 9B.  While a dungeon session is mounted the caption is the DUNGEON's
    // name, not game.position's -- that field holds the surface RETURN context
    // for the whole descent and is stale by design (see hud_location_caption()).
    const char*name=hud_location_caption(game.position.map.location,game.position.map.floor,bands_active,bands_active?dungeon_bands->dungeon_id:uint8_t(0));
    if(!preserve_party_panel){
        status_move_=movement_mode;
        ESP_RETURN_ON_ERROR(draw_status_rows(game,name,movement_mode,!alpha_ui_cache_valid_),kTag,"draw world status");
    }

    // Transcript history and active modal state have separate retained regions.
    // Every user size maps to distinct, aspect-correct raster metrics.
    const auto text_metrics=ui_text_metrics(ui_size);
    const bool context_active=context_bar&&context_bar->active;
    if(context_active!=context_cache_valid_){
        // Alpha 4 UI Batch 1: inside the console's rules (x=182, y=86), so
        // neither is erased; x=319 stays the console's black edge.
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX+1,openu5::kHudTranscriptSeparatorY+1,
                            kDisplayWidth-openu5::kHudPartyFrameX-1,kDisplayHeight-openu5::kHudTranscriptSeparatorY-1,kBlack),
                            kTag,"reflow transcript context region");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,openu5::kHudTranscriptSeparatorY,
                            openu5::kHudPartyFrameW,1,kChromeRule),kTag,"restore transcript separator");
        std::memset(transcript_cache_,0,sizeof(transcript_cache_));alpha_ui_cache_valid_=false;
        if(!context_active)context_cache_valid_=false;
    }
    size_t transcript_columns=0,transcript_rows=0;
    world_transcript_geometry(ui_size,context_active,transcript_columns,transcript_rows);
    std::fill(std::begin(transcript_lines),std::end(transcript_lines),openu5::UiRenderedLine{});
    const auto count=ui.visible_lines(transcript_lines,transcript_rows,transcript_columns);
    // A3-04F (ALPHA3_AUDIO.md section 26): a row is redrawn when its text or
    // colour changed. A row's pixels are a function of those two and the text
    // metrics (whose change clears the cache above); the line's sequence number
    // is not drawn, and keying on it redrew every row of each scroll even
    // where the same text landed on it again ("Pass" under "Pass").
    for(size_t i=0;i<transcript_rows;++i){const char*text="";uint32_t seq=0;uint16_t color=kWhite;if(i<count){text=transcript_lines[i].text;seq=transcript_lines[i].sequence;color=transcript_lines[i].channel==openu5::UiTextChannel::Prompt?kCyan:transcript_lines[i].channel==openu5::UiTextChannel::Combat?kRed:kWhite;}auto&cached=transcript_cache_[i];if(!alpha_ui_cache_valid_||cached.color!=color||std::strcmp(cached.text,text)!=0){ESP_RETURN_ON_ERROR(draw_text_box_metrics(openu5::kHudRightX,openu5::kHudTranscriptY+int(i)*text_metrics.line_height,openu5::kHudRightW,text_metrics.line_height,text,color,text_metrics),kTag,"draw running log row");cached.sequence=seq;cached.color=color;std::snprintf(cached.text,sizeof(cached.text),"%s",text);}}
    if(context_active)ESP_RETURN_ON_ERROR(draw_context_bar(*context_bar,openu5::kHudPartyFrameX,openu5::kHudPartyFrameW,!context_cache_valid_),kTag,"gameplay context action bar");
    alpha_ui_cache_valid_=true;
    return ESP_OK;
}

esp_err_t Board::draw_rgb565_scaled(int x,int y,int width,int height,
                                    const uint16_t *pixels,int source_width,
                                    int source_height,int source_stride)
{
    if(!display_initialized_||!pixels||width<=0||height<=0||source_width<=0||
       source_height<=0||source_stride<source_width||width>kDisplayWidth||
       x<0||y<0||x+width>kDisplayWidth||y+height>kDisplayHeight)
        return ESP_ERR_INVALID_ARG;
    ++draw_calls_.rgb565;
    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set scaled RGB565 window");
    auto &row_bytes=transfer_row_;gpio_set_level(pins::kTftDataCommand,1);
    for(int row=0;row<height;++row){
        const RowMark mark=row_mark();
        const int source_y=row*source_height/height;
        for(int col=0;col<width;++col){
            const int source_x=col*source_width/width;
            const uint16_t pixel=pixels[source_y*source_stride+source_x];
            row_bytes[col*2]=uint8_t(pixel>>8);row_bytes[col*2+1]=uint8_t(pixel);
        }
        spi_transaction_t transaction{};transaction.length=width*16;transaction.tx_buffer=row_bytes.data();
        ESP_RETURN_ON_ERROR(tft_row(transaction,mark),kTag,"write scaled RGB565 row");
        if(openu5::tft_row_yield_due(row))tft_yield();
    }
    return ESP_OK;
}

esp_err_t Board::show_frontend(const openu5::FrontendView&in,const uint16_t*preview,const uint16_t*title_art,const uint16_t*panel_art,const uint16_t*creation_art,uint8_t ui_size){
    endgame_page_key_=-1;
    if(!display_initialized_)return ESP_ERR_INVALID_STATE;
    // Alpha 4 A4-UI3 (ALPHA4_UI.md section 6): a slot page is two rows a slot,
    // its line then its dimmer detail row, and the selection covers both.
    const bool paired=in.details[0]&&in.line_count<=size_t(openu5::kSaveSlotCount);
    openu5::FrontendView expanded{};
    if(paired){expanded=in;expanded.line_count=0;
        for(size_t i=0;i<in.line_count;++i){expanded.lines[expanded.line_count++]=in.lines[i];expanded.lines[expanded.line_count++]=in.details[i]?in.details[i]:"";}
        expanded.selected_line=in.selected_line<0?-1:in.selected_line*2;}
    const auto&v=paired?expanded:in;const int span=paired?2:1;
    auto selected_row=[&](int i){return v.selected_line>=0&&i>=v.selected_line&&i<v.selected_line+span;};
    auto row_color=[&](int i){return paired&&(i&1)&&!selected_row(i)?kChromeDim:kWhite;};
    const bool first=!frontend_drawn_||!frontend_cache_valid_;
    // Alpha 4 UI Batch 1 (ALPHA4_UI.md): every screen with a text title is
    // drawn in the band shell (>Title< in the top band, a white-ruled window).
    const bool shell=!title_art&&!creation_art;
    const bool title_credits=title_art&&v.kind==openu5::FrontendViewKind::TitleCredits;
    const bool layout_changed=first||frontend_cache_.state!=v.state||frontend_cache_.kind!=v.kind||ui_scale_requires_full_layout(frontend_cache_.ui_size,ui_size)||
                               (frontend_cache_.title_art!=nullptr)!=(title_art!=nullptr)||
                               frontend_cache_.panel!=(panel_art!=nullptr)||
                               frontend_cache_.creation_art!=(creation_art!=nullptr)||
                               frontend_cache_.shell!=shell;
    debug_last_full_redraw_=first;debug_last_dirty_regions_=0;debug_last_pixels_=0;
    auto account=[&](size_t pixels){++debug_last_dirty_regions_;debug_last_pixels_+=pixels;};
    if(first){
        ESP_RETURN_ON_ERROR(fill_rect(0,0,kDisplayWidth,kDisplayHeight,kBlack),kTag,"initialize frontend");
        account(kDisplayWidth*kDisplayHeight);
    }
    frontend_drawn_=true;alpha_drawn_=false;alpha_ui_cache_valid_=false;

    if(creation_art){
        ESP_RETURN_ON_ERROR(draw_rgb565(0,0,320,152,creation_art),kTag,"draw original FONT.OVL creation art");
        account(320*152);
    }else if(title_art){
        if(first||!frontend_cache_.title_art){
            if(!first&&frontend_cache_.shell){ESP_RETURN_ON_ERROR(fill_rect(0,0,kDisplayWidth,kDisplayHeight,kBlack),kTag,"leave frontend shell");account(kDisplayWidth*kDisplayHeight);}
            ESP_RETURN_ON_ERROR(draw_rgb565(0,kCharacterTitleArtY,320,kCharacterTitleArtH,title_art),kTag,"draw original Ultima V title art");
            account(320*110);
        }else if(frontend_cache_.title_art!=title_art){
            // The logo is identical in all four resources. Only the extracted
            // 288x49 Warriors-of-Destiny fire strip changes between frames.
            ESP_RETURN_ON_ERROR(draw_rgb565_strided(16,65,288,49,title_art+61*320+16,320),
                                kTag,"update title fire strip");
            account(288*49);
        }
    }else{
        if(first||!frontend_cache_.shell){
            if(!first){ESP_RETURN_ON_ERROR(fill_rect(0,0,kDisplayWidth,kDisplayHeight,kBlack),kTag,"replace frontend title art");account(kDisplayWidth*kDisplayHeight);}
            ESP_RETURN_ON_ERROR(draw_frontend_shell(v.title?v.title:""),kTag,"frontend shell");account(320*12+2*228*2+316*2+4*316);
            ESP_RETURN_ON_ERROR(draw_text_box(8,15,304,8,v.subtitle?v.subtitle:"",kChromeDim),kTag,"frontend subtitle");account(304*8);
        }else{
            if(std::strcmp(frontend_cache_.title,v.title?v.title:"")!=0){ESP_RETURN_ON_ERROR(draw_band_strip(0,2,kDisplayWidth,8,-1,v.title?v.title:"",nullptr,nullptr),kTag,"update frontend title");account(320*8);}
            if(std::strcmp(frontend_cache_.subtitle,v.subtitle?v.subtitle:"")!=0){ESP_RETURN_ON_ERROR(draw_text_box(8,15,304,8,v.subtitle?v.subtitle:"",kChromeDim),kTag,"update frontend subtitle");account(304*8);}
        }
    }

    const bool sized_menu=v.kind==openu5::FrontendViewKind::Menu||v.kind==openu5::FrontendViewKind::Settings||v.kind==openu5::FrontendViewKind::SystemMenu;
    const auto menu_metrics=ui_text_metrics(ui_size);
    auto draw_generic_body=[&]()->esp_err_t{
        int y=title_art?116:24;
        for(size_t i=0;i<v.line_count&&y<218;++i){
            const char*source=v.lines[i]?v.lines[i]:"";const bool selected=selected_row(int(i));size_t at=0;
            do{
                char line[51]{};size_t n=std::min<size_t>(selected&&at==0?48:50,std::strlen(source+at));
                if(source[at+n]&&n==(selected&&at==0?48U:50U)){size_t cut=n;while(cut>20&&source[at+cut]!=' ')--cut;if(cut>20)n=cut;}
                // Alpha 4 UI Batch 1: the selection is reverse video (kernel
                // 0x2a28's look), not a green '>'; the gutter cell stays.
                if(v.selected_line>=0&&at==0){line[0]=sized_menu?' ':selected?'>':' ';line[1]=' ';std::memcpy(line+2,source+at,n);line[n+2]=0;}else{std::memcpy(line,source+at,n);line[n]=0;}
                if(sized_menu)ESP_RETURN_ON_ERROR(draw_text_box_metrics(8,y-1,304,menu_metrics.line_height+2,line,row_color(int(i)),menu_metrics,selected,1),kTag,"frontend sized line");
                else ESP_RETURN_ON_ERROR(draw_text_box(20,y,280,9,line,selected?kGreen:kWhite),kTag,"frontend line");
                account((sized_menu?304:280)*(sized_menu?menu_metrics.line_height+2:9));
                at+=n;while(source[at]==' ')++at;y+=sized_menu?menu_metrics.line_height+3:11;
            }while(source[at]&&y<218);
        }
        return ESP_OK;
    };

    if(creation_art){
        ESP_RETURN_ON_ERROR(fill_rect(0,152,320,88,kBlack),kTag,"prepare creation question band");account(320*88);
        const char*source=v.line_count>1&&v.lines[1]?v.lines[1]:v.line_count&&v.lines[0]?v.lines[0]:"";
        int y=154;size_t at=0;do{char line[51]{};size_t n=std::min<size_t>(50,std::strlen(source+at));if(source[at+n]&&n==50){size_t cut=n;while(cut>20&&source[at+cut]!=' ')--cut;if(cut>20)n=cut;}std::memcpy(line,source+at,n);line[n]=0;ESP_RETURN_ON_ERROR(draw_text_box(8,y,304,9,line,kWhite),kTag,"draw creation question");account(304*9);at+=n;while(source[at]==' ')++at;y+=11;}while(source[at]&&y<228);
    }else if(v.kind==openu5::FrontendViewKind::Attract){
        if(layout_changed){
            ESP_RETURN_ON_ERROR(fill_rect(0,114,320,110,kBlack),kTag,"prepare attract safe area");account(320*110);
            ESP_RETURN_ON_ERROR(draw_text_box(8,116,304,14,v.subtitle?v.subtitle:"The Summoning",kCyan,2,2),kTag,"attract scene title");account(304*14);
            ESP_RETURN_ON_ERROR(fill_rect(6,134,308,2,kCyan),kTag,"attract frame top");account(308*2);
            ESP_RETURN_ON_ERROR(fill_rect(6,200,308,2,kCyan),kTag,"attract frame bottom");account(308*2);
            ESP_RETURN_ON_ERROR(fill_rect(6,136,2,64,kCyan),kTag,"attract frame left");account(2*64);
            ESP_RETURN_ON_ERROR(fill_rect(312,136,2,64,kCyan),kTag,"attract frame right");account(2*64);
            ESP_RETURN_ON_ERROR(draw_text_box(8,207,304,9,"Original FONT.OVL View script",kGreen),kTag,"attract status");account(304*9);
        }
        else if(std::strcmp(frontend_cache_.subtitle,v.subtitle?v.subtitle:"")!=0){ESP_RETURN_ON_ERROR(draw_text_box(8,116,304,14,v.subtitle?v.subtitle:"The View",kCyan,2,2),kTag,"update attract scene title");account(304*14);}
        if(preview){ESP_RETURN_ON_ERROR(draw_rgb565(8,136,304,64,preview),kTag,"draw scripted attract band");account(304*64);}
    }else if(title_credits){
        // Alpha 4 UI Batch 2 (ALPHA4_UI.md section 2.1): the credit lines in
        // IBM.CH, each centred under the art, and the prompt centred beneath
        // them in the footers' grey. Redrawn only when the screen or its text
        // changes, so a fire-animation frame still sends the strip alone.
        bool changed=layout_changed||v.line_count!=frontend_cache_.line_count;
        for(size_t i=0;!changed&&i<v.line_count;++i)changed=std::strcmp(v.lines[i]?v.lines[i]:"",frontend_cache_.lines[i])!=0;
        if(changed){
            ESP_RETURN_ON_ERROR(fill_rect(0,114,320,110,kBlack),kTag,"prepare title credits");account(320*110);
            auto centred=[&](int y,const char*text,uint16_t color)->esp_err_t{
                const int width=int(std::min<size_t>(std::strlen(text),40))*8;if(!width)return ESP_OK;
                const int x=(kDisplayWidth-width)/2;account(size_t(width)*8);
                return draw_cells(x,y,width,8,x,text,color,SIZE_MAX,color,false,ChromeFont::Ibm);
            };
            for(size_t i=0;i<v.line_count&&i<2;++i)
                ESP_RETURN_ON_ERROR(centred(kTitleCreditsY+int(i)*kTitleCreditsStep,v.lines[i]?v.lines[i]:"",kWhite),kTag,"title credit line");
            ESP_RETURN_ON_ERROR(centred(kTitlePromptY,v.footer?v.footer:"",kChromeDim),kTag,"title prompt");
        }
    }else if(panel_art){
        if(layout_changed){ESP_RETURN_ON_ERROR(fill_rect(shell?3:0,50,shell?314:320,174,kBlack),kTag,"prepare acknowledgements panel");account(320*174);}
        if(layout_changed||!frontend_cache_.panel){ESP_RETURN_ON_ERROR(draw_rgb565(16,54,288,137,panel_art),kTag,"draw original acknowledgements panel");account(288*137);}
    }else if(!layout_changed&&(v.kind==openu5::FrontendViewKind::Menu||v.kind==openu5::FrontendViewKind::Settings||v.kind==openu5::FrontendViewKind::SystemMenu)){
        const size_t count=std::max(v.line_count,frontend_cache_.line_count);
        const int row_y=title_art?116:24;const int row_step=int(menu_metrics.line_height)+3;const int row_height=menu_metrics.line_height;
        for(size_t i=0;i<count&&i<12;++i){
            const char*current=i<v.line_count&&v.lines[i]?v.lines[i]:"";
            const char*cached=i<frontend_cache_.line_count?frontend_cache_.lines[i]:"";
            const int was=frontend_cache_.selected_line;const bool was_selected=was>=0&&int(i)>=was&&int(i)<was+frontend_cache_.selected_span;
            if(std::strcmp(current,cached)==0&&!selected_row(int(i))&&!was_selected)continue;
            char line[64]{};if(i<v.line_count)std::snprintf(line,sizeof(line),"  %.48s",current);
            ESP_RETURN_ON_ERROR(draw_text_box_metrics(8,row_y+int(i)*row_step-1,304,row_height+2,line,row_color(int(i)),menu_metrics,
                                i<v.line_count&&selected_row(int(i)),1),kTag,"update retained frontend row");account(304*(row_height+2));
        }
    }else if(!layout_changed&&v.kind==openu5::FrontendViewKind::CharacterName){
        for(size_t i=0;i<2;++i){const char*current=i<v.line_count&&v.lines[i]?v.lines[i]:"";const char*cached=i<frontend_cache_.line_count?frontend_cache_.lines[i]:"";if(std::strcmp(current,cached)==0)continue;
            ESP_RETURN_ON_ERROR(draw_text_box(20,kCharacterTextY+int(i)*11,280,kCharacterTextRowH,current,kWhite),kTag,"update character name row");account(280*kCharacterTextRowH);}
    }else{
        bool body_changed=layout_changed||v.line_count!=frontend_cache_.line_count||v.selected_line!=frontend_cache_.selected_line;
        for(size_t i=0;!body_changed&&i<v.line_count;++i)body_changed=std::strcmp(v.lines[i]?v.lines[i]:"",frontend_cache_.lines[i])!=0;
        if(body_changed){
            // Alpha 4 UI Batch 2: the shell's body starts under the subtitle
            // (y=15..22); clearing from y=20 cut its last glyph rows.
            const int body_y=title_art?114:23;
            ESP_RETURN_ON_ERROR(fill_rect(shell?3:0,body_y,shell?314:320,224-body_y,kBlack),kTag,"prepare frontend body");account(320*(224-body_y));
            ESP_RETURN_ON_ERROR(draw_generic_body(),kTag,"draw frontend body");
        }
    }
    if(v.kind==openu5::FrontendViewKind::Settings&&(layout_changed||first)){
        ESP_RETURN_ON_ERROR(fill_rect(8,209,304,16,kBlack),kTag,"clear settings type preview");account(304*16);
        ESP_RETURN_ON_ERROR(draw_text_box_metrics(12,211,296,menu_metrics.line_height,
                            "Sample: The Avatar",kCyan,menu_metrics),kTag,"draw settings type preview");
        account(296*menu_metrics.line_height);
    }
    // The title's prompt is drawn with its credit block, not in the footer row.
    const char*footer=creation_art||title_credits?"":v.footer?v.footer:"";
    if(first||layout_changed||std::strcmp(frontend_cache_.footer,footer)!=0){ESP_RETURN_ON_ERROR(draw_text_box(8,228,304,9,footer,kChromeDim),kTag,"frontend footer");account(304*9);}

    frontend_cache_={};frontend_cache_.state=v.state;frontend_cache_.kind=v.kind;
    std::snprintf(frontend_cache_.title,sizeof(frontend_cache_.title),"%s",v.title?v.title:"");
    std::snprintf(frontend_cache_.subtitle,sizeof(frontend_cache_.subtitle),"%s",v.subtitle?v.subtitle:"");
    frontend_cache_.line_count=std::min<size_t>(v.line_count,12);frontend_cache_.selected_line=v.selected_line;frontend_cache_.selected_span=uint8_t(span);
    for(size_t i=0;i<frontend_cache_.line_count;++i)std::snprintf(frontend_cache_.lines[i],sizeof(frontend_cache_.lines[i]),"%s",v.lines[i]?v.lines[i]:"");
    std::snprintf(frontend_cache_.footer,sizeof(frontend_cache_.footer),"%s",footer);
    frontend_cache_.title_art=title_art;frontend_cache_.preview=preview!=nullptr;frontend_cache_.panel=panel_art!=nullptr;frontend_cache_.creation_art=creation_art!=nullptr;frontend_cache_.shell=shell;frontend_cache_.ui_size=ui_size;frontend_cache_valid_=true;
    return ESP_OK;
}

esp_err_t Board::draw_text_box(int x,int y,int width,int height,const char *text,
                               uint16_t color,int scale_x,int scale_y,bool invert)
{
    if(!display_initialized_||!text||width<=0||height<=0||width>kDisplayWidth||
       x<0||y<0||x+width>kDisplayWidth||y+height>kDisplayHeight||scale_x<=0||scale_y<=0)
        return ESP_ERR_INVALID_ARG;
    ++draw_calls_.text_boxes;
    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set coherent text window");
    auto &row_bytes=transfer_row_;gpio_set_level(pins::kTftDataCommand,1);
    const int cell_width=6*scale_x;
    const size_t text_length=std::strlen(text);
    const size_t row_length=size_t(width)*2;
    size_t used=0; // A3-04F: whole rows share a transaction (row_batch_ends)
    RowMark mark{};
    for(int row=0;row<height;++row){
        if(used==0)mark=row_mark();
        const int glyph_row=row/scale_y;
        uint8_t *out=row_bytes.data()+used;
        for(int col=0;col<width;++col){
            // Reverse video swaps the two: the glyph is punched out of a
            // filled row, which is what an XOR of the row's rectangle looks
            // like on a two-colour row (kernel 0x2a28).
            uint16_t pixel=invert?color:kBlack;const size_t char_index=size_t(col/cell_width);
            const int glyph_col=(col%cell_width)/scale_x;
            if(glyph_row<7&&glyph_col<5&&char_index<text_length){
                const auto bitmap=glyph(text[char_index]);
                if(bitmap[glyph_col]&(1U<<glyph_row))pixel=invert?kBlack:color;
            }
            out[col*2]=uint8_t(pixel>>8);out[col*2+1]=uint8_t(pixel);
        }
        used+=row_length;
        if(!row_batch_ends(used,row_length,row,height))continue; // not a pause row either
        spi_transaction_t transaction{};transaction.length=used*8;transaction.tx_buffer=row_bytes.data();used=0;
        ESP_RETURN_ON_ERROR(tft_row(transaction,mark),kTag,"write coherent text row");
        if(openu5::tft_row_yield_due(row))tft_yield();
    }
    return ESP_OK;
}

esp_err_t Board::draw_text_box_metrics(int x,int y,int width,int height,const char *text,
                                       uint16_t color,DeviceTextMetrics metrics,bool invert,int top_pad)
{
    if(!display_initialized_||!text||width<=0||height<=0||x<0||y<0||
       x+width>kDisplayWidth||y+height>kDisplayHeight||!metrics.glyph_width||
       !metrics.glyph_height||metrics.cell_width<metrics.glyph_width||
       metrics.line_height<metrics.glyph_height)return ESP_ERR_INVALID_ARG;
    ++draw_calls_.metric_text_boxes;
    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set metric text window");
    auto &row_bytes=transfer_row_;gpio_set_level(pins::kTftDataCommand,1);
    const size_t text_length=std::strlen(text);
    const size_t row_length=size_t(width)*2;
    size_t used=0; // A3-04F: whole rows share a transaction (row_batch_ends)
    RowMark mark{};
    for(int row=0;row<height;++row){
        if(used==0)mark=row_mark();
        uint8_t *out=row_bytes.data()+used;
        for(int col=0;col<width;++col){
            uint16_t pixel=invert?color:kBlack;const size_t char_index=size_t(col/metrics.cell_width);
            const int within_x=col%metrics.cell_width,glyph_y=row-top_pad;
            if(glyph_y>=0&&glyph_y<metrics.glyph_height&&within_x<metrics.glyph_width&&char_index<text_length){
                const int glyph_col=within_x*5/metrics.glyph_width;
                const int glyph_row=glyph_y*7/metrics.glyph_height;
                const auto bitmap=glyph(text[char_index]);
                if(bitmap[glyph_col]&(1U<<glyph_row))pixel=invert?kBlack:color;
            }
            out[col*2]=uint8_t(pixel>>8);out[col*2+1]=uint8_t(pixel);
        }
        used+=row_length;
        if(!row_batch_ends(used,row_length,row,height))continue;
        spi_transaction_t transaction{};transaction.length=used*8;transaction.tx_buffer=row_bytes.data();used=0;
        ESP_RETURN_ON_ERROR(tft_row(transaction,mark),kTag,"write metric text row");
    }
    return ESP_OK;
}

bool Board::chrome_glyph_bit(char c,int row,int col) const
{
    if(row<0||row>=8||col<0||col>=8)return false;
    // IBM.CH: 8 bytes a glyph, bit 7 = left (as runes.ch). Without the font
    // (a stubbed pack) the 5x7 glyph stands in, one column in from the left.
    if(ibm_font_)return (ibm_font_[size_t(uint8_t(c)&0x7fU)*8+size_t(row)]&(0x80U>>unsigned(col)))!=0;
    if(row>=7||col<1||col>5)return false;
    return (glyph(c)[size_t(col-1)]&(1U<<unsigned(row)))!=0;
}

esp_err_t Board::draw_cells(int x,int y,int width,int height,int text_x,const char *text,uint16_t color,
                            size_t accent_from,uint16_t accent,bool invert,ChromeFont font)
{
    if(!display_initialized_||!text||width<=0||height<=0||x<0||y<0||
       x+width>kDisplayWidth||y+height>kDisplayHeight)return ESP_ERR_INVALID_ARG;
    ++draw_calls_.text_boxes;
    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set chrome row window");
    gpio_set_level(pins::kTftDataCommand,1);
    const bool ibm=font==ChromeFont::Ibm;const int cell=ibm?8:6;
    const size_t text_length=std::strlen(text);
    const size_t row_length=size_t(width)*2;
    size_t used=0; // A3-04F: whole rows share a transaction (row_batch_ends)
    RowMark mark{};
    for(int row=0;row<height;++row){
        if(used==0)mark=row_mark();
        uint8_t *out=transfer_row_.data()+used;
        for(int col=0;col<width;++col){
            const int dx=x+col-text_x;bool on=false;uint16_t ink=color;
            if(dx>=0){
                const size_t index=size_t(dx/cell);const int gx=dx%cell;
                if(index<text_length){
                    if(index>=accent_from)ink=accent;
                    on=ibm?chrome_glyph_bit(text[index],row,gx):row<7&&gx<5&&(glyph(text[index])[size_t(gx)]&(1U<<unsigned(row)))!=0;
                }
            }
            // Reverse video as draw_text_box: the glyph punched out of the row.
            const uint16_t pixel=invert?(on?kBlack:ink):(on?ink:kBlack);
            out[col*2]=uint8_t(pixel>>8);out[col*2+1]=uint8_t(pixel);
        }
        used+=row_length;
        if(!row_batch_ends(used,row_length,row,height))continue;
        spi_transaction_t transaction{};transaction.length=used*8;transaction.tx_buffer=transfer_row_.data();used=0;
        ESP_RETURN_ON_ERROR(tft_row(transaction,mark),kTag,"write chrome row");
    }
    return ESP_OK;
}

esp_err_t Board::draw_band_strip(int x,int y,int width,int height,int rule_row,const char *caption,
                                 const openu5::HudWorldState *sky,const uint8_t *runes)
{
    if(!display_initialized_||width<=0||height<=0||x<0||y<0||
       x+width>kDisplayWidth||y+height>kDisplayHeight)return ESP_ERR_INVALID_ARG;
    const size_t length=caption?std::strlen(caption):0;
    const bool track=sky&&sky->sky_visible&&runes;
    int notch0=-1,notch1=-1;
    if(track){notch0=(width-12*8)/2;notch1=notch0+12*8;}
    else if(length){const int w=int(length)*8+4;notch0=(width-w)/2;notch1=notch0+w;}
    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set band strip window");
    gpio_set_level(pins::kTftDataCommand,1);
    const size_t row_length=size_t(width)*2;
    size_t used=0;
    RowMark mark{};
    int band_row=0;
    for(int row=0;row<height;++row){
        if(used==0)mark=row_mark();
        uint8_t *out=transfer_row_.data()+used;
        const bool rule=row==rule_row;
        for(int col=0;col<width;++col){
            uint16_t pixel=rule?kChromeRule:kChromeBand;
            if(!rule&&notch0>=0&&band_row<8){
                if(col>=notch0&&col<notch1){
                    pixel=kBlack;
                    if(track){
                        const int cell=(col-notch0)/8,gx=(col-notch0)%8;
                        for(size_t i=0;i<sky->mark_count;++i){
                            if(sky->marks[i].cell!=cell)continue;
                            const uint8_t code=sky->marks[i].sun?0x2a:sky->marks[i].glyph;
                            if(runes[size_t(code&0x7f)*8+size_t(band_row)]&(0x80U>>unsigned(gx)))pixel=sky->marks[i].sun?kYellow:kWhite;
                        }
                    }else{
                        const int cx=col-notch0-2;
                        if(cx>=0&&size_t(cx/8)<length&&chrome_glyph_bit(caption[cx/8],band_row,cx%8))pixel=kWhite;
                    }
                }else if(col>=notch0-8&&col<notch0){
                    const int i=col-(notch0-8);const uint8_t bit=uint8_t(0x80U>>unsigned(i));
                    pixel=(kBracketWhite[band_row]&bit)?kChromeRule:(kBracketBlue[band_row]&bit)?kChromeBand:i>0?kBlack:kChromeBand;
                }else if(col>=notch1&&col<notch1+8){
                    const int i=col-notch1;const uint8_t bit=uint8_t(0x80U>>unsigned(7-i));
                    pixel=(kBracketWhite[band_row]&bit)?kChromeRule:(kBracketBlue[band_row]&bit)?kChromeBand:i<7?kBlack:kChromeBand;
                }
            }
            out[col*2]=uint8_t(pixel>>8);out[col*2+1]=uint8_t(pixel);
        }
        if(!rule)++band_row;
        used+=row_length;
        if(!row_batch_ends(used,row_length,row,height))continue;
        spi_transaction_t transaction{};transaction.length=used*8;transaction.tx_buffer=transfer_row_.data();used=0;
        ESP_RETURN_ON_ERROR(tft_row(transaction,mark),kTag,"write band strip rows");
    }
    return ESP_OK;
}

esp_err_t Board::draw_frontend_shell(const char *title)
{
    // The band everywhere outside the content window, a white rule around it,
    // and the >title< caption in the top band (rows 2-9).
    ESP_RETURN_ON_ERROR(fill_rect(0,0,kDisplayWidth,12,kChromeBand),kTag,"shell top band");
    ESP_RETURN_ON_ERROR(fill_rect(0,12,2,kDisplayHeight-12,kChromeBand),kTag,"shell left band");
    ESP_RETURN_ON_ERROR(fill_rect(kDisplayWidth-2,12,2,kDisplayHeight-12,kChromeBand),kTag,"shell right band");
    ESP_RETURN_ON_ERROR(fill_rect(2,kDisplayHeight-2,kDisplayWidth-4,2,kChromeBand),kTag,"shell bottom band");
    for(const auto &r:std::array<std::array<int,4>,4>{{{{2,12,316,1}},{{2,237,316,1}},{{2,13,1,224}},{{317,13,1,224}}}})
        ESP_RETURN_ON_ERROR(fill_rect(r[0],r[1],r[2],r[3],kChromeRule),kTag,"shell rule");
    return draw_band_strip(0,2,kDisplayWidth,8,-1,title,nullptr,nullptr);
}

esp_err_t Board::draw_shared_bus_marker(int pass)
{
    const uint16_t color = (pass % 2 == 0) ? kCyan : kGreen;
    return fill_rect(286 + pass * 8, 220, 6, 6, color);
}

SdStatus Board::initialize_and_test_sd()
{
    debug51::stage(5, "sd-initialize-entry");
    SdStatus status{};
    if (!shared_spi_initialized_) {
        status.error = ESP_ERR_INVALID_STATE;
        return status;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = pins::kSharedSpiHost;
    host.max_freq_khz = kSdClockKhz;
    const sdspi_device_config_t slot_config = {
        .host_id = pins::kSharedSpiHost,
        .gpio_cs = pins::kSdChipSelect,
        .gpio_cd = SDSPI_SLOT_NO_CD,
        .gpio_wp = SDSPI_SLOT_NO_WP,
        .gpio_int = SDSPI_SLOT_NO_INT,
        .gpio_wp_polarity = SDSPI_IO_ACTIVE_LOW,
        .duty_cycle_pos = 0,
        .wait_for_miso = 0,
    };
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        // Two resource packs and the diagnostic log may overlap briefly with
        // transactional save files during startup. Keep a bounded reserve.
        .max_files = 6,
        .allocation_unit_size = 0,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };
    sdmmc_card_t *card = nullptr;
    status.error = esp_vfs_fat_sdspi_mount(kMountPoint, &host, &slot_config,
                                           &mount_config, &card);
    if (status.error != ESP_OK) {
        ESP_LOGE(kTag, "SD initialization/mount failed: %s", esp_err_to_name(status.error));
        return status;
    }

    status.capacity_bytes = static_cast<uint64_t>(card->csd.capacity) * card->csd.sector_size;
    status.type = card->is_mmc ? "MMC" : ((card->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(kTag, "SD card initialized: type=%s size=%llu bytes (%llu MiB), bus=%d kHz",
             status.type, status.capacity_bytes, status.capacity_bytes / (1024ULL * 1024ULL),
             static_cast<int>(card->real_freq_khz));
    sdmmc_card_print_info(stdout, card);

    struct stat file_info{};
    const bool known_file_present = stat(kKnownTestPath, &file_info) == 0;
    const char *test_path = known_file_present ? kKnownTestPath : kTemporaryTestPath;
    status.used_existing_test_file = known_file_present;
    if (!known_file_present) {
        FILE *file = std::fopen(test_path, "wb");
        if (file == nullptr) {
            ESP_LOGE(kTag, "Cannot create temporary SD diagnostic file: errno=%d", errno);
            status.error = ESP_FAIL;
            return status;
        }
        bool write_failed =
            std::fwrite(kTestPayload, 1, sizeof(kTestPayload) - 1, file) != sizeof(kTestPayload) - 1;
        write_failed = std::fflush(file) != 0 || write_failed;
        write_failed = fsync(fileno(file)) != 0 || write_failed;
        write_failed = std::fclose(file) != 0 || write_failed;
        if (write_failed) {
            ESP_LOGE(kTag, "Temporary SD diagnostic write failed: errno=%d", errno);
            status.error = ESP_FAIL;
            unlink(test_path);
            return status;
        }
        ESP_LOGI(kTag, "No %s found; created temporary diagnostic file", kKnownTestPath);
    }

    std::array<char, kReadLimit> expected{};
    size_t expected_length = 0;
    status.error = read_prefix(test_path, expected, expected_length);
    if (status.error != ESP_OK || expected_length == 0 ||
        (!known_file_present &&
         (expected_length != sizeof(kTestPayload) - 1 ||
          std::memcmp(expected.data(), kTestPayload, expected_length) != 0))) {
        ESP_LOGE(kTag, "SD diagnostic read-back verification failed");
        status.error = ESP_FAIL;
        if (!known_file_present) {
            unlink(test_path);
        }
        return status;
    }
    ESP_LOGI(kTag, "Read %zu byte(s) from %s", expected_length, test_path);

    // Alternate SD reads and TFT writes on the one SPI2 bus. The ESP-IDF SPI
    // driver owns arbitration; each device has its own CS and clock setting.
    for (int pass = 0; pass < 3; ++pass) {
        std::array<char, kReadLimit> observed{};
        size_t observed_length = 0;
        status.error = read_prefix(test_path, observed, observed_length);
        if (status.error != ESP_OK || observed_length != expected_length ||
            std::memcmp(observed.data(), expected.data(), expected_length) != 0 ||
            draw_shared_bus_marker(pass) != ESP_OK) {
            ESP_LOGE(kTag, "Shared SPI interleave test failed on pass %d", pass + 1);
            status.error = ESP_FAIL;
            if (!known_file_present) {
                unlink(test_path);
            }
            return status;
        }
    }
    status.shared_bus_verified = true;
    ESP_LOGI(kTag, "Shared SPI interleave test passed: 3 SD reads + 3 TFT writes");

    if (!known_file_present) {
        if (unlink(test_path) != 0) {
            ESP_LOGE(kTag, "Could not remove temporary diagnostic file: errno=%d", errno);
            status.error = ESP_FAIL;
            return status;
        }
        ESP_LOGI(kTag, "Temporary diagnostic file verified and removed");
    }
    status.error = ESP_OK;
    status.ok = true;
    return status;
}

}  // namespace tdeck
