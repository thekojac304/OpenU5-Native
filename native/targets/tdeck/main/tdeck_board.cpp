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
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include "sd_protocol_defs.h"

#include "tdeck_pins.h"
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
    ESP_RETURN_ON_ERROR(spi_device_transmit(display_handle(display_device_), &transaction),
                        kTag, "send TFT command 0x%02x", command);
    if (data_length == 0) {
        return ESP_OK;
    }
    spi_transaction_t payload{};
    payload.length = data_length * 8;
    payload.tx_buffer = data;
    gpio_set_level(pins::kTftDataCommand, 1);
    return spi_device_transmit(display_handle(display_device_), &payload);
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
    ESP_RETURN_ON_ERROR(set_display_window(x, y, width, height), kTag, "set fill window");
    auto &pixels = transfer_row_;
    static_assert(sizeof(transfer_row_) == kDisplayWidth * 2);
    const size_t pixels_per_chunk = std::min(width, kDisplayWidth);
    for (size_t index = 0; index < pixels_per_chunk; ++index) {
        pixels[index * 2] = static_cast<uint8_t>(color >> 8);
        pixels[index * 2 + 1] = static_cast<uint8_t>(color);
    }
    gpio_set_level(pins::kTftDataCommand, 1);
    int remaining = width * height;
    int chunks=0;
    while (remaining > 0) {
        const int count = std::min(remaining, static_cast<int>(pixels_per_chunk));
        spi_transaction_t transaction{};
        transaction.length = count * 16;
        transaction.tx_buffer = pixels.data();
        ESP_RETURN_ON_ERROR(spi_device_transmit(display_handle(display_device_), &transaction),
                            kTag, "write TFT pixels");
        remaining -= count;
        if((++chunks&31)==0)vTaskDelay(1);
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
    ESP_RETURN_ON_ERROR(set_display_window(x, y, width, height), kTag, "set RGB565 window");
    auto &row_bytes = transfer_row_;
    gpio_set_level(pins::kTftDataCommand, 1);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const uint16_t pixel = pixels[row * stride + col];
            row_bytes[col * 2] = static_cast<uint8_t>(pixel >> 8);
            row_bytes[col * 2 + 1] = static_cast<uint8_t>(pixel);
        }
        spi_transaction_t transaction{};
        transaction.length = width * 16;
        transaction.tx_buffer = row_bytes.data();
        ESP_RETURN_ON_ERROR(spi_device_transmit(display_handle(display_device_), &transaction),
                            kTag, "write RGB565 row");
        if(row>0&&(row&15)==0)vTaskDelay(1);
    }
    return ESP_OK;
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

esp_err_t Board::show_alpha(const uint16_t *pixels,const openu5::UiSession &ui,
                            const openu5::GameState &game,const openu5::TurnState &turn,
                            const openu5::HudWorldState &hud,const uint8_t *runes_font,const char *overlay,
                            const uint8_t *animated_cells,bool animation_only,
                            const DeviceDebugScreen *debug,bool movement_mode,
                            uint8_t ui_size,const DeviceShopView *shop,const DeviceSelectionView *selection,
                            const DeviceContextActionBar *context_bar,DevicePartyHighlight party_highlight,
                            uint32_t viewport_crc,const openu5::HudDungeonBands *dungeon_bands)
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
    if(!alpha_drawn_||frontend_drawn_){ESP_RETURN_ON_ERROR(fill_rect(0,0,kDisplayWidth,kDisplayHeight,kBlack),kTag,"initialize Alpha 2.0 game screen");alpha_drawn_=true;frontend_drawn_=false;alpha_ui_cache_valid_=false;alpha_ui_size_cache_=0xff;viewport_cache_valid_=false;sky_bar_cache_valid_=false;shop_cache_valid_=false;selection_cache_valid_=false;context_cache_valid_=false;animation_only=false;debug_last_full_redraw_=true;debug_last_pixels_=kDisplayWidth*kDisplayHeight;}
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
    if(debug_drawn_){ESP_RETURN_ON_ERROR(fill_rect(0,0,kDisplayWidth,kDisplayHeight,kBlack),kTag,"leave developer screen");debug_drawn_=false;debug_cache_valid_=false;alpha_ui_cache_valid_=false;alpha_ui_size_cache_=0xff;viewport_cache_valid_=false;sky_bar_cache_valid_=false;shop_cache_valid_=false;selection_cache_valid_=false;context_cache_valid_=false;animation_only=false;}
    if((!shop||!shop->active)&&shop_cache_valid_){
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,0,kDisplayWidth-openu5::kHudPartyFrameX,kDisplayHeight,kBlack),kTag,"leave shop panel");
        shop_cache_valid_=false;context_cache_valid_=false;alpha_ui_cache_valid_=false;
    }
    if((!selection||!selection->active)&&selection_cache_valid_){
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,0,kDisplayWidth-openu5::kHudPartyFrameX,kDisplayHeight,kBlack),kTag,"leave compact selector");
        selection_cache_valid_=false;context_cache_valid_=false;alpha_ui_cache_valid_=false;
    }
    // Centre a band caption in the 9 px strip, the T-Deck stand-in for the
    // original's bracketed centred band (skin.ts drawCenteredBand).
    auto band_text=[](char *out,size_t cap,const char *text){
        const int columns=openu5::kHudSkyBarW/openu5::kHudCellWidth;
        int n=0;while(text[n])++n;
        int pad=(columns-n)/2;if(pad<0)pad=0;
        size_t at=0;
        for(int i=0;i<pad&&at+1<cap;++i)out[at++]=' ';
        for(int i=0;i<n&&at+1<cap;++i)out[at++]=text[i];
        out[at]=0;
    };
    auto draw_sky_bar=[&]()->esp_err_t{
        if(bands_active){
            char line[34]{};band_text(line,sizeof(line),dungeon_bands->level);
            ESP_RETURN_ON_ERROR(draw_text_box(openu5::kHudSkyBarX,openu5::kHudSkyBarY,
                                openu5::kHudSkyBarW,openu5::kHudSkyBarH,line,kWhite),kTag,"draw dungeon level band");
            return ESP_OK;
        }
        if(!runes_font)return ESP_ERR_INVALID_ARG;
        ESP_RETURN_ON_ERROR(set_display_window(openu5::kHudSkyBarX,openu5::kHudSkyBarY,
                            openu5::kHudSkyBarW,openu5::kHudSkyBarH),kTag,"set U5 sky window");
        gpio_set_level(pins::kTftDataCommand,1);
        constexpr int origin=(openu5::kHudSkyBarW-12*8)/2;
        for(int row=0;row<openu5::kHudSkyBarH;++row){
            for(int x=0;x<openu5::kHudSkyBarW;++x){uint16_t color=kBlack;
                if(row<8&&hud.sky_visible)for(size_t i=0;i<hud.mark_count;++i){const int gx=origin+int(hud.marks[i].cell)*8;if(x>=gx&&x<gx+8){const uint8_t code=hud.marks[i].sun?0x2a:hud.marks[i].glyph;const uint8_t bits=runes_font[size_t(code&0x7f)*8+size_t(row)];if(bits&(0x80U>>unsigned(x-gx)))color=hud.marks[i].sun?kYellow:kWhite;}}
                transfer_row_[size_t(x)*2]=uint8_t(color>>8);transfer_row_[size_t(x)*2+1]=uint8_t(color);}
            spi_transaction_t transaction{};transaction.length=openu5::kHudSkyBarW*16;transaction.tx_buffer=transfer_row_.data();
            ESP_RETURN_ON_ERROR(spi_device_transmit(display_handle(display_device_),&transaction),kTag,"write authentic U5 sky row");
        }
        return ESP_OK;
    };
    auto draw_wind_bar=[&]()->esp_err_t{
        char line[34]{};
        if(bands_active)band_text(line,sizeof(line),dungeon_bands->direction);
        else std::snprintf(line,sizeof(line)," Wind: %-16.16s",hud.wind_visible?hud.wind:"--");
        ESP_RETURN_ON_ERROR(draw_text_box(openu5::kHudWindBarX,openu5::kHudWindBarY,
                            openu5::kHudWindBarW,openu5::kHudWindBarH,line,kWhite),kTag,"draw lower strip");
        return ESP_OK;
    };
    if(animation_only&&animated_cells){
        for(int row=0;row<openu5::kViewportTiles;++row)for(int col=0;col<openu5::kViewportTiles;++col){const int i=row*openu5::kViewportTiles+col;if(!animated_cells[i])continue;
            const int clip_top=row==0?openu5::kHudSkyBarH:0;
            const int clip_bottom=row==openu5::kViewportTiles-1?openu5::kHudWindBarH:0;
            const int height=openu5::kTilePixels-clip_top-clip_bottom;if(height<=0)continue;
            ESP_RETURN_ON_ERROR(draw_rgb565_strided(openu5::kHudViewportX+col*openu5::kTilePixels,openu5::kHudViewportY+row*openu5::kTilePixels+clip_top,openu5::kTilePixels,height,
                                pixels+(row*openu5::kTilePixels+clip_top)*openu5::kViewportPixels+col*openu5::kTilePixels,openu5::kViewportPixels),kTag,"draw clipped animated Alpha cell");}
        return ESP_OK;
    }
    // The band captions take part in the SAME cache signatures as the strips
    // they replace, so a Klimb (level) or a turn (direction) repaints its strip
    // on the very frame that produced it, and nothing else repaints.
    uint32_t sky_signature=bands_active?0x9dU:hud.sky_visible?0x51U:0x17U;
    if(bands_active)for(const char *c=dungeon_bands->level;*c;++c)sky_signature=sky_signature*16777619U^uint32_t(uint8_t(*c));
    else for(size_t i=0;i<hud.mark_count;++i)sky_signature=sky_signature*16777619U^(uint32_t(hud.marks[i].cell)<<16|uint32_t(hud.marks[i].glyph)<<8|uint32_t(hud.marks[i].sun));
    char wind_text[34]{};
    if(bands_active)std::snprintf(wind_text,sizeof(wind_text),"%.30s",dungeon_bands->direction);
    else std::snprintf(wind_text,sizeof(wind_text)," Wind: %-16.16s",hud.wind_visible?hud.wind:"--");
    const bool viewport_changed=!viewport_cache_valid_||viewport_crc_!=viewport_crc;
    if(viewport_changed){
        constexpr int top=openu5::kHudSkyBarH;
        constexpr int height=openu5::kViewportPixels-openu5::kHudSkyBarH-openu5::kHudWindBarH;
        ESP_RETURN_ON_ERROR(draw_rgb565_strided(openu5::kHudViewportX,openu5::kHudViewportY+top,openu5::kViewportPixels,height,pixels+top*openu5::kViewportPixels,openu5::kViewportPixels),kTag,"draw strip-clipped Alpha viewport");
        debug_last_pixels_+=openu5::kViewportPixels*height;++debug_last_dirty_regions_;
    }
    if(!sky_bar_cache_valid_||sky_bar_signature_!=sky_signature){ESP_RETURN_ON_ERROR(draw_sky_bar(),kTag,"compose authentic sky strip");sky_bar_signature_=sky_signature;sky_bar_cache_valid_=true;debug_last_pixels_+=openu5::kHudSkyBarW*openu5::kHudSkyBarH;++debug_last_dirty_regions_;}
    if(!viewport_cache_valid_||std::strcmp(wind_bar_cache_,wind_text)!=0){ESP_RETURN_ON_ERROR(draw_wind_bar(),kTag,"compose lower strip");std::snprintf(wind_bar_cache_,sizeof(wind_bar_cache_),"%s",wind_text);debug_last_pixels_+=openu5::kHudWindBarW*openu5::kHudWindBarH;++debug_last_dirty_regions_;}
    viewport_crc_=viewport_crc;viewport_cache_valid_=true;
    if((!shop||!shop->active)&&(!selection||!selection->active)&&alpha_ui_size_cache_!=ui_size){
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,0,kDisplayWidth-openu5::kHudPartyFrameX,kDisplayHeight,kBlack),kTag,"reflow gameplay UI scale");
        alpha_ui_cache_valid_=false;alpha_ui_size_cache_=ui_size;
        std::memset(transcript_cache_,0,sizeof(transcript_cache_));
    }
    if(!alpha_ui_cache_valid_){
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudViewportFrameX,openu5::kHudViewportFrameY,openu5::kHudViewportFrameW,2,kCyan),kTag,"viewport frame top");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudViewportFrameX,openu5::kHudViewportFrameY+openu5::kHudViewportFrameH-2,openu5::kHudViewportFrameW,2,kCyan),kTag,"viewport frame bottom");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudViewportFrameX,openu5::kHudViewportFrameY,2,openu5::kHudViewportFrameH,kCyan),kTag,"viewport frame left");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudViewportFrameX+openu5::kHudViewportFrameW-2,openu5::kHudViewportFrameY,2,openu5::kHudViewportFrameH,kCyan),kTag,"viewport frame right");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,openu5::kHudPartyFrameY,openu5::kHudPartyFrameW,1,kCyan),kTag,"party frame top");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,openu5::kHudPartyFrameY+openu5::kHudPartyFrameH-1,openu5::kHudPartyFrameW,1,kCyan),kTag,"party frame bottom");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,openu5::kHudPartyFrameY,1,openu5::kHudPartyFrameH,kCyan),kTag,"party frame left");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX+openu5::kHudPartyFrameW-1,openu5::kHudPartyFrameY,1,openu5::kHudPartyFrameH,kCyan),kTag,"party frame right");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudWorldFrameX,openu5::kHudWorldFrameY,openu5::kHudWorldFrameW,1,kCyan),kTag,"world frame top");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudWorldFrameX,openu5::kHudWorldFrameY+openu5::kHudWorldFrameH-1,openu5::kHudWorldFrameW,1,kCyan),kTag,"world frame bottom");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudWorldFrameX,openu5::kHudWorldFrameY,1,openu5::kHudWorldFrameH,kCyan),kTag,"world frame left");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudWorldFrameX+openu5::kHudWorldFrameW-1,openu5::kHudWorldFrameY,1,openu5::kHudWorldFrameH,kCyan),kTag,"world frame right");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,openu5::kHudTranscriptSeparatorY,openu5::kHudPartyFrameW,1,kCyan),kTag,"transcript separator");
        for(const auto &p:std::array<std::array<int,4>,8>{{{{2,2,7,1}},{{2,2,1,7}},{{175,2,7,1}},{{181,2,1,7}},{{182,2,6,1}},{{182,2,1,6}},{{313,2,6,1}},{{318,2,1,6}}}})
            ESP_RETURN_ON_ERROR(fill_rect(p[0],p[1],p[2],p[3],kWhite),kTag,"draw restrained frame cap");
    }
    auto draw_context_bar=[&](const DeviceContextActionBar &bar,int x,int width,bool first)->esp_err_t{
        if(first||!context_cache_valid_){
            ESP_RETURN_ON_ERROR(fill_rect(x,kContextBarTop,width,1,kCyan),kTag,"context action separator");
        }
        if(first||!context_cache_valid_||std::strcmp(context_cache_.status,bar.status)!=0)
            ESP_RETURN_ON_ERROR(draw_text_box(x+2,kContextBarStatusY,width-4,kContextBarStatusH,bar.status,kCyan),kTag,"context active status");
        if(first||!context_cache_valid_||std::strcmp(context_cache_.actions,bar.actions)!=0)
            ESP_RETURN_ON_ERROR(draw_text_box(x+2,kContextBarActionsY,width-4,kContextBarActionsH,bar.actions,kGreen),kTag,"context valid actions");
        context_cache_=bar;context_cache_valid_=true;return ESP_OK;
    };
    if(shop&&shop->active){
        const bool first=!shop_cache_valid_;
        auto account=[&](size_t pixels){++debug_last_dirty_regions_;debug_last_pixels_+=pixels;};
        if(first){
            ESP_RETURN_ON_ERROR(fill_rect(182,0,138,240,kBlack),kTag,"initialize U5 shop panel");account(138*240);
            context_cache_valid_=false;
            for(const auto &r:std::array<std::array<int,4>,8>{{{{182,2,137,1}},{{182,33,137,1}},{{182,35,137,1}},{{182,124,137,1}},{{182,149,137,1}},{{182,239,137,1}},{{182,2,1,238}},{{318,2,1,238}}}})
                {ESP_RETURN_ON_ERROR(fill_rect(r[0],r[1],r[2],r[3],kCyan),kTag,"draw shop separator");account(size_t(r[2]*r[3]));}
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
        std::fill(std::begin(transcript_lines_),std::end(transcript_lines_),openu5::UiRenderedLine{});
        const auto count=ui.visible_lines(transcript_lines_,kShopLogRows,openu5::kHudTranscriptColumns);
        for(size_t i=0;i<kShopLogRows;++i){const char*text=i<count?transcript_lines_[i].text:"";const uint32_t seq=i<count?transcript_lines_[i].sequence:0;const uint16_t color=i<count&&transcript_lines_[i].channel==openu5::UiTextChannel::Shop?kCyan:kWhite;auto&cached=transcript_cache_[i];if(first||cached.sequence!=seq||cached.color!=color||std::strcmp(cached.text,text)!=0){ESP_RETURN_ON_ERROR(draw_text_box(184,152+int(i)*8,134,8,text,color),kTag,"shop transcript row");account(134*8);cached.sequence=seq;cached.color=color;std::snprintf(cached.text,sizeof(cached.text),"%s",text);}}
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
                ESP_RETURN_ON_ERROR(fill_rect(r[0],r[1],r[2],r[3],kCyan),kTag,"draw selector separator");account(size_t(r[2]*r[3]));}
        }
        auto changed=[&](const char*a,const char*b){return first||std::strcmp(a,b)!=0;};
        if(changed(selection->title,selection_cache_.title)){ESP_RETURN_ON_ERROR(draw_text_box(184,5,134,14,selection->title,kCyan),kTag,"selector title");account(134*14);}
        if(changed(selection->detail,selection_cache_.detail)){ESP_RETURN_ON_ERROR(draw_text_box(184,26,134,8,selection->detail,kWhite),kTag,"selector detail row one");account(134*8);}
        if(changed(selection->detail2,selection_cache_.detail2)){ESP_RETURN_ON_ERROR(draw_text_box(184,35,134,8,selection->detail2,kWhite),kTag,"selector detail row two");account(134*8);}
        const size_t rows=std::max(selection->row_count,selection_cache_.row_count);
        for(size_t i=0;i<rows&&i<kSelectionVisibleRows;++i){if(!selection_row_needs_redraw(*selection,selection_cache_,i,first))continue;char line[24]{};if(i<selection->row_count)std::snprintf(line,sizeof(line),"%c%.21s",i==selection->selected_row?'>':' ',selection->rows[i]);ESP_RETURN_ON_ERROR(draw_text_box(184,46+int(i)*14,134,12,line,i<selection->row_count&&i==selection->selected_row?kGreen:kWhite),kTag,"selector row");account(134*12);}
        std::fill(std::begin(transcript_lines_),std::end(transcript_lines_),openu5::UiRenderedLine{});const auto count=ui.visible_lines(transcript_lines_,kSelectorLogRows,openu5::kHudTranscriptColumns);
        const bool show_transcript=selection_uses_transcript(*selection);
        for(size_t i=0;i<kSelectorLogRows;++i){const char*text=show_transcript&&i<count?transcript_lines_[i].text:"";const uint32_t seq=show_transcript&&i<count?transcript_lines_[i].sequence:0;const uint16_t color=show_transcript&&i<count&&transcript_lines_[i].channel==openu5::UiTextChannel::Combat?kRed:kWhite;auto&cached=transcript_cache_[i];if(first||cached.sequence!=seq||cached.color!=color||std::strcmp(cached.text,text)!=0){ESP_RETURN_ON_ERROR(draw_text_box(184,172+int(i)*8,134,8,text,color),kTag,"selector transcript");account(134*8);cached.sequence=seq;cached.color=color;std::snprintf(cached.text,sizeof(cached.text),"%s",text);}}
        ESP_RETURN_ON_ERROR(draw_context_bar(selection->context,182,137,first),kTag,"selector context action bar");account(137*25);
        selection_cache_=*selection;selection_cache_valid_=true;alpha_ui_cache_valid_=true;return ESP_OK;
    }
    const auto members=openu5::party_members(game.party);
    // Y-04 (#213): `damage_flash` puts ONE row in reverse video -- the binary's
    // 0x2a28, an XOR of the row's rectangle, shared with the picker cursor and
    // the combat hit. It is the top of openu5::roster_invert_row's precedence.
    for(size_t row=0;row<6;++row){char line[24]{};uint16_t color=kWhite;bool invert=false;if(row<members.count){const auto index=members.indices[row];const auto&a=game.party.characters[index];const bool selected=index==party_highlight.selected,actor=index==party_highlight.actor;std::snprintf(line,sizeof(line),"%c%u %-7.7s %3u/%3u %c",selected?'>':actor?'*':' ',unsigned(row+1),a.name,unsigned(std::min<uint16_t>(a.current_hp,999)),unsigned(std::min<uint16_t>(a.max_hp,999)),a.status?a.status:'G');color=selected?kGreen:actor?kCyan:kWhite;invert=index==party_highlight.damage_flash;}ESP_RETURN_ON_ERROR(draw_text_box(openu5::kHudRightX,4+int(row)*8,openu5::kHudRightW,8,line,color,1,1,invert),kTag,"draw party row");}
    // Batch 9B.  While a dungeon session is mounted the caption is the DUNGEON's
    // name, not game.position's -- that field holds the surface RETURN context
    // for the whole descent and is stale by design (see hud_location_caption()).
    char location[24]{};const char*name=hud_location_caption(game.position.map.location,game.position.map.floor,bands_active,bands_active?dungeon_bands->dungeon_id:uint8_t(0));std::snprintf(location,sizeof(location),"%.22s",name);
    char clock[24]{};std::snprintf(clock,sizeof(clock),"Day %ld  %02ld:%02ld",long(game.time.day),long(game.time.hour),long(game.time.minute));
    const char*world[]={location,clock};for(int i=0;i<2;++i)ESP_RETURN_ON_ERROR(draw_text_box(openu5::kHudRightX,58+i*10,openu5::kHudRightW,8,world[i],i==0?kCyan:kWhite),kTag,"draw world status");
    ESP_RETURN_ON_ERROR(draw_text_box(openu5::kHudRightX,78,openu5::kHudRightW,8,
                        movement_mode?"MOVE MODE: ON":"",movement_mode?kGreen:kWhite),
                        kTag,"draw movement mode indicator");

    // Transcript history and active modal state have separate retained regions.
    // Every user size maps to distinct, aspect-correct raster metrics.
    const auto text_metrics=ui_text_metrics(ui_size);
    const bool context_active=context_bar&&context_bar->active;
    if(context_active!=context_cache_valid_){
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,openu5::kHudTranscriptSeparatorY,
                            openu5::kHudPartyFrameW,kDisplayHeight-openu5::kHudTranscriptSeparatorY,kBlack),
                            kTag,"reflow transcript context region");
        ESP_RETURN_ON_ERROR(fill_rect(openu5::kHudPartyFrameX,openu5::kHudTranscriptSeparatorY,
                            openu5::kHudPartyFrameW,1,kCyan),kTag,"restore transcript separator");
        std::memset(transcript_cache_,0,sizeof(transcript_cache_));alpha_ui_cache_valid_=false;
        if(!context_active)context_cache_valid_=false;
    }
    size_t transcript_columns=0,transcript_rows=0;
    world_transcript_geometry(ui_size,context_active,transcript_columns,transcript_rows);
    std::fill(std::begin(transcript_lines_),std::end(transcript_lines_),openu5::UiRenderedLine{});
    const auto count=ui.visible_lines(transcript_lines_,transcript_rows,transcript_columns);
    for(size_t i=0;i<transcript_rows;++i){const char*text="";uint32_t seq=0;uint16_t color=kWhite;if(i<count){text=transcript_lines_[i].text;seq=transcript_lines_[i].sequence;color=transcript_lines_[i].channel==openu5::UiTextChannel::Prompt?kCyan:transcript_lines_[i].channel==openu5::UiTextChannel::Combat?kRed:kWhite;}auto&cached=transcript_cache_[i];if(!alpha_ui_cache_valid_||cached.sequence!=seq||cached.color!=color||std::strcmp(cached.text,text)!=0){ESP_RETURN_ON_ERROR(draw_text_box_metrics(openu5::kHudRightX,openu5::kHudTranscriptY+int(i)*text_metrics.line_height,openu5::kHudRightW,text_metrics.line_height,text,color,text_metrics),kTag,"draw running log row");cached.sequence=seq;cached.color=color;std::snprintf(cached.text,sizeof(cached.text),"%s",text);}}
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
    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set scaled RGB565 window");
    auto &row_bytes=transfer_row_;gpio_set_level(pins::kTftDataCommand,1);
    for(int row=0;row<height;++row){
        const int source_y=row*source_height/height;
        for(int col=0;col<width;++col){
            const int source_x=col*source_width/width;
            const uint16_t pixel=pixels[source_y*source_stride+source_x];
            row_bytes[col*2]=uint8_t(pixel>>8);row_bytes[col*2+1]=uint8_t(pixel);
        }
        spi_transaction_t transaction{};transaction.length=width*16;transaction.tx_buffer=row_bytes.data();
        ESP_RETURN_ON_ERROR(spi_device_transmit(display_handle(display_device_),&transaction),
                            kTag,"write scaled RGB565 row");
        if(row>0&&(row&15)==0)vTaskDelay(1);
    }
    return ESP_OK;
}

esp_err_t Board::show_frontend(const openu5::FrontendView&v,const uint16_t*preview,const uint16_t*title_art,const uint16_t*panel_art,const uint16_t*creation_art,uint8_t ui_size){
    if(!display_initialized_)return ESP_ERR_INVALID_STATE;
    const bool first=!frontend_drawn_||!frontend_cache_valid_;
    const bool layout_changed=first||frontend_cache_.state!=v.state||frontend_cache_.kind!=v.kind||ui_scale_requires_full_layout(frontend_cache_.ui_size,ui_size)||
                               (frontend_cache_.title_art!=nullptr)!=(title_art!=nullptr)||
                               frontend_cache_.panel!=(panel_art!=nullptr)||
                               frontend_cache_.creation_art!=(creation_art!=nullptr);
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
        if(first||frontend_cache_.title_art){
            if(!first){ESP_RETURN_ON_ERROR(fill_rect(0,4,320,110,kBlack),kTag,"replace frontend title art");account(320*110);}
            ESP_RETURN_ON_ERROR(draw_text_box(0,10,320,22,v.title?v.title:"",kCyan,3,3),kTag,"frontend title");account(320*22);
            ESP_RETURN_ON_ERROR(draw_text_box(0,38,320,12,v.subtitle?v.subtitle:"",kWhite,2,2),kTag,"frontend subtitle");account(320*12);
        }else{
            if(std::strcmp(frontend_cache_.title,v.title?v.title:"")!=0){ESP_RETURN_ON_ERROR(draw_text_box(0,10,320,22,v.title?v.title:"",kCyan,3,3),kTag,"update frontend title");account(320*22);}
            if(std::strcmp(frontend_cache_.subtitle,v.subtitle?v.subtitle:"")!=0){ESP_RETURN_ON_ERROR(draw_text_box(0,38,320,12,v.subtitle?v.subtitle:"",kWhite,2,2),kTag,"update frontend subtitle");account(320*12);}
        }
    }

    const bool sized_menu=v.kind==openu5::FrontendViewKind::Menu||v.kind==openu5::FrontendViewKind::Settings||v.kind==openu5::FrontendViewKind::SystemMenu;
    const auto menu_metrics=ui_text_metrics(ui_size);
    auto draw_generic_body=[&]()->esp_err_t{
        int y=title_art?116:62;
        for(size_t i=0;i<v.line_count&&y<218;++i){
            const char*source=v.lines[i]?v.lines[i]:"";const bool selected=int(i)==v.selected_line;size_t at=0;
            do{
                char line[51]{};size_t n=std::min<size_t>(selected&&at==0?48:50,std::strlen(source+at));
                if(source[at+n]&&n==(selected&&at==0?48U:50U)){size_t cut=n;while(cut>20&&source[at+cut]!=' ')--cut;if(cut>20)n=cut;}
                if(v.selected_line>=0&&at==0){line[0]=selected?'>':' ';line[1]=' ';std::memcpy(line+2,source+at,n);line[n+2]=0;}else{std::memcpy(line,source+at,n);line[n]=0;}
                if(sized_menu)ESP_RETURN_ON_ERROR(draw_text_box_metrics(8,y,304,menu_metrics.line_height,line,selected?kGreen:kWhite,menu_metrics),kTag,"frontend sized line");
                else ESP_RETURN_ON_ERROR(draw_text_box(20,y,280,9,line,selected?kGreen:kWhite),kTag,"frontend line");
                account((sized_menu?304:280)*(sized_menu?menu_metrics.line_height:9));
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
    }else if(panel_art){
        if(layout_changed){ESP_RETURN_ON_ERROR(fill_rect(0,50,320,174,kBlack),kTag,"prepare acknowledgements panel");account(320*174);}
        if(layout_changed||!frontend_cache_.panel){ESP_RETURN_ON_ERROR(draw_rgb565(16,54,288,137,panel_art),kTag,"draw original acknowledgements panel");account(288*137);}
    }else if(!layout_changed&&(v.kind==openu5::FrontendViewKind::Menu||v.kind==openu5::FrontendViewKind::Settings||v.kind==openu5::FrontendViewKind::SystemMenu)){
        const size_t count=std::max(v.line_count,frontend_cache_.line_count);
        const int row_y=title_art?116:62;const int row_step=int(menu_metrics.line_height)+3;const int row_height=menu_metrics.line_height;
        for(size_t i=0;i<count&&i<12;++i){
            const char*current=i<v.line_count&&v.lines[i]?v.lines[i]:"";
            const char*cached=i<frontend_cache_.line_count?frontend_cache_.lines[i]:"";
            if(std::strcmp(current,cached)==0&&int(i)!=v.selected_line&&int(i)!=frontend_cache_.selected_line)continue;
            char line[64]{};if(i<v.line_count)std::snprintf(line,sizeof(line),"%c %.48s",int(i)==v.selected_line?'>':' ',current);
            ESP_RETURN_ON_ERROR(draw_text_box_metrics(8,row_y+int(i)*row_step,304,row_height,line,int(i)==v.selected_line?kGreen:kWhite,menu_metrics),kTag,"update retained frontend row");account(304*row_height);
        }
    }else if(!layout_changed&&v.kind==openu5::FrontendViewKind::CharacterName){
        for(size_t i=0;i<2;++i){const char*current=i<v.line_count&&v.lines[i]?v.lines[i]:"";const char*cached=i<frontend_cache_.line_count?frontend_cache_.lines[i]:"";if(std::strcmp(current,cached)==0)continue;
            ESP_RETURN_ON_ERROR(draw_text_box(20,kCharacterTextY+int(i)*11,280,kCharacterTextRowH,current,kWhite),kTag,"update character name row");account(280*kCharacterTextRowH);}
    }else{
        bool body_changed=layout_changed||v.line_count!=frontend_cache_.line_count||v.selected_line!=frontend_cache_.selected_line;
        for(size_t i=0;!body_changed&&i<v.line_count;++i)body_changed=std::strcmp(v.lines[i]?v.lines[i]:"",frontend_cache_.lines[i])!=0;
        if(body_changed){
            const int body_y=title_art?114:52;
            ESP_RETURN_ON_ERROR(fill_rect(0,body_y,320,224-body_y,kBlack),kTag,"prepare frontend body");account(320*(224-body_y));
            ESP_RETURN_ON_ERROR(draw_generic_body(),kTag,"draw frontend body");
        }
    }
    if(v.kind==openu5::FrontendViewKind::Settings&&(layout_changed||first)){
        ESP_RETURN_ON_ERROR(fill_rect(8,209,304,16,kBlack),kTag,"clear settings type preview");account(304*16);
        ESP_RETURN_ON_ERROR(draw_text_box_metrics(12,211,296,menu_metrics.line_height,
                            "Sample: The Avatar",kCyan,menu_metrics),kTag,"draw settings type preview");
        account(296*menu_metrics.line_height);
    }
    const char*footer=creation_art?"":v.footer?v.footer:"";
    if(first||layout_changed||std::strcmp(frontend_cache_.footer,footer)!=0){ESP_RETURN_ON_ERROR(draw_text_box(8,228,304,9,footer,kGreen),kTag,"frontend footer");account(304*9);}

    frontend_cache_={};frontend_cache_.state=v.state;frontend_cache_.kind=v.kind;
    std::snprintf(frontend_cache_.title,sizeof(frontend_cache_.title),"%s",v.title?v.title:"");
    std::snprintf(frontend_cache_.subtitle,sizeof(frontend_cache_.subtitle),"%s",v.subtitle?v.subtitle:"");
    frontend_cache_.line_count=std::min<size_t>(v.line_count,12);frontend_cache_.selected_line=v.selected_line;
    for(size_t i=0;i<frontend_cache_.line_count;++i)std::snprintf(frontend_cache_.lines[i],sizeof(frontend_cache_.lines[i]),"%s",v.lines[i]?v.lines[i]:"");
    std::snprintf(frontend_cache_.footer,sizeof(frontend_cache_.footer),"%s",footer);
    frontend_cache_.title_art=title_art;frontend_cache_.preview=preview!=nullptr;frontend_cache_.panel=panel_art!=nullptr;frontend_cache_.creation_art=creation_art!=nullptr;frontend_cache_.ui_size=ui_size;frontend_cache_valid_=true;
    return ESP_OK;
}

esp_err_t Board::draw_text_box(int x,int y,int width,int height,const char *text,
                               uint16_t color,int scale_x,int scale_y,bool invert)
{
    if(!display_initialized_||!text||width<=0||height<=0||width>kDisplayWidth||
       x<0||y<0||x+width>kDisplayWidth||y+height>kDisplayHeight||scale_x<=0||scale_y<=0)
        return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set coherent text window");
    auto &row_bytes=transfer_row_;gpio_set_level(pins::kTftDataCommand,1);
    const int cell_width=6*scale_x;
    const size_t text_length=std::strlen(text);
    for(int row=0;row<height;++row){
        const int glyph_row=row/scale_y;
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
            row_bytes[col*2]=uint8_t(pixel>>8);row_bytes[col*2+1]=uint8_t(pixel);
        }
        spi_transaction_t transaction{};transaction.length=width*16;transaction.tx_buffer=row_bytes.data();
        ESP_RETURN_ON_ERROR(spi_device_transmit(display_handle(display_device_),&transaction),kTag,"write coherent text row");
        if(row>0&&(row&15)==0)vTaskDelay(1);
    }
    return ESP_OK;
}

esp_err_t Board::draw_text_box_metrics(int x,int y,int width,int height,const char *text,
                                       uint16_t color,DeviceTextMetrics metrics)
{
    if(!display_initialized_||!text||width<=0||height<=0||x<0||y<0||
       x+width>kDisplayWidth||y+height>kDisplayHeight||!metrics.glyph_width||
       !metrics.glyph_height||metrics.cell_width<metrics.glyph_width||
       metrics.line_height<metrics.glyph_height)return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set metric text window");
    auto &row_bytes=transfer_row_;gpio_set_level(pins::kTftDataCommand,1);
    const size_t text_length=std::strlen(text);
    for(int row=0;row<height;++row){
        for(int col=0;col<width;++col){
            uint16_t pixel=kBlack;const size_t char_index=size_t(col/metrics.cell_width);
            const int within_x=col%metrics.cell_width;
            if(row<metrics.glyph_height&&within_x<metrics.glyph_width&&char_index<text_length){
                const int glyph_col=within_x*5/metrics.glyph_width;
                const int glyph_row=row*7/metrics.glyph_height;
                const auto bitmap=glyph(text[char_index]);
                if(bitmap[glyph_col]&(1U<<glyph_row))pixel=color;
            }
            row_bytes[col*2]=uint8_t(pixel>>8);row_bytes[col*2+1]=uint8_t(pixel);
        }
        spi_transaction_t transaction{};transaction.length=width*16;transaction.tx_buffer=row_bytes.data();
        ESP_RETURN_ON_ERROR(spi_device_transmit(display_handle(display_device_),&transaction),kTag,"write metric text row");
    }
    return ESP_OK;
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
