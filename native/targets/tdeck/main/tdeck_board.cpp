#include "tdeck_board.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include "driver/gpio.h"
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
constexpr size_t kReadLimit = 160;

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
    case '-': return {0x08, 0x08, 0x08, 0x08, 0x08};
    case ':': return {0x00, 0x36, 0x36, 0x00, 0x00};
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
    case 'H': return {0x7F, 0x08, 0x08, 0x08, 0x7F};
    case 'I': return {0x00, 0x41, 0x7F, 0x41, 0x00};
    case 'K': return {0x7F, 0x08, 0x14, 0x22, 0x41};
    case 'L': return {0x7F, 0x40, 0x40, 0x40, 0x40};
    case 'M': return {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    case 'O': return {0x3E, 0x41, 0x41, 0x41, 0x3E};
    case 'P': return {0x7F, 0x09, 0x09, 0x09, 0x06};
    case 'R': return {0x7F, 0x09, 0x19, 0x29, 0x46};
    case 'S': return {0x46, 0x49, 0x49, 0x49, 0x31};
    case 'T': return {0x01, 0x01, 0x7F, 0x01, 0x01};
    case 'U': return {0x3F, 0x40, 0x40, 0x40, 0x3F};
    case 'a': return {0x20, 0x54, 0x54, 0x54, 0x78};
    case 'c': return {0x38, 0x44, 0x44, 0x44, 0x20};
    case 'e': return {0x38, 0x54, 0x54, 0x54, 0x18};
    case 'h': return {0x7F, 0x08, 0x04, 0x04, 0x78};
    case 'i': return {0x00, 0x44, 0x7D, 0x40, 0x00};
    case 'k': return {0x7F, 0x10, 0x28, 0x44, 0x00};
    case 'l': return {0x00, 0x41, 0x7F, 0x40, 0x00};
    case 'n': return {0x7C, 0x08, 0x04, 0x04, 0x78};
    case 'o': return {0x38, 0x44, 0x44, 0x44, 0x38};
    case 'p': return {0x7C, 0x14, 0x14, 0x14, 0x08};
    case 's': return {0x48, 0x54, 0x54, 0x54, 0x20};
    case 't': return {0x04, 0x3F, 0x44, 0x40, 0x20};
    default: return {0x02, 0x01, 0x51, 0x09, 0x06};
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
        {0x36, 1, {0x08}, 0},
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
        // Landscape rotation 1: MX + MV + BGR, matching LilyGO's setRotation(1).
        {0x36, 1, {0x68}, 0},
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
    gpio_set_level(pins::kTftBacklight, 1);
    ESP_LOGI(kTag, "ST7789 initialized at 320x240 landscape, SPI clock %d Hz", kTftClockHz);
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
    std::array<uint8_t, kDisplayWidth * 2> pixels{};
    const size_t pixels_per_chunk = std::min(width, kDisplayWidth);
    for (size_t index = 0; index < pixels_per_chunk; ++index) {
        pixels[index * 2] = static_cast<uint8_t>(color >> 8);
        pixels[index * 2 + 1] = static_cast<uint8_t>(color);
    }
    gpio_set_level(pins::kTftDataCommand, 1);
    int remaining = width * height;
    while (remaining > 0) {
        const int count = std::min(remaining, static_cast<int>(pixels_per_chunk));
        spi_transaction_t transaction{};
        transaction.length = count * 16;
        transaction.tx_buffer = pixels.data();
        ESP_RETURN_ON_ERROR(spi_device_transmit(display_handle(display_device_), &transaction),
                            kTag, "write TFT pixels");
        remaining -= count;
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
    if (!display_initialized_) {
        return;
    }
    if (fill_rect(0, 0, kDisplayWidth, kDisplayHeight, kBlack) != ESP_OK ||
        draw_text(18, 14, "OpenU5-TDeck", kCyan, 3) != ESP_OK ||
        draw_text(18, 50, "Milestone 2", kWhite, 2) != ESP_OK ||
        draw_text(18, 82, "ESP32-S3", kWhite, 2) != ESP_OK ||
        draw_text(18, 108, "16 MB Flash", kWhite, 2) != ESP_OK ||
        draw_text(18, 134, "8 MB PSRAM", kWhite, 2) != ESP_OK ||
        draw_text(18, 174, sd_ok ? "SD: OK" : "SD: FAIL", sd_ok ? kGreen : kRed, 3) != ESP_OK) {
        ESP_LOGE(kTag, "Failed to draw diagnostic screen");
    }
}

esp_err_t Board::draw_shared_bus_marker(int pass)
{
    const uint16_t color = (pass % 2 == 0) ? kCyan : kGreen;
    return fill_rect(286 + pass * 8, 220, 6, 6, color);
}

SdStatus Board::initialize_and_test_sd()
{
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
        .max_files = 3,
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
