#include <cinttypes>
#include <cstddef>

#include "esp_chip_info.h"
#include "esp_clk_tree.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "asset_pack.h"
#include "tdeck_board.h"

namespace {
constexpr char kTag[] = "OpenU5-TDeck";
constexpr size_t kMiB = 1024 * 1024;
constexpr uint32_t kInternalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
constexpr uint32_t kPsramCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
}  // namespace

extern "C" void app_main(void)
{
    // Allow the USB serial port time to enumerate; never wait for a host forever.
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "OpenU5-TDeck | Milestone 3: native asset validation");
    ESP_LOGI(kTag, "LilyGO T-Deck Plus native ESP-IDF target");
    ESP_LOGI(kTag, "========================================");

    esp_chip_info_t chip{};
    esp_chip_info(&chip);
    ESP_LOGI(kTag, "Chip model: %s (model ID %d)",
             chip.model == CHIP_ESP32S3 ? "ESP32-S3" : "unexpected chip",
             static_cast<int>(chip.model));
    ESP_LOGI(kTag, "Chip revision: v%u.%u (raw %u)",
             static_cast<unsigned>(chip.revision / 100),
             static_cast<unsigned>(chip.revision % 100),
             static_cast<unsigned>(chip.revision));
    ESP_LOGI(kTag, "CPU cores: %u", static_cast<unsigned>(chip.cores));
    uint32_t cpu_frequency_hz = 0;
    if (esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT,
                                     &cpu_frequency_hz) == ESP_OK) {
        ESP_LOGI(kTag, "CPU frequency: %" PRIu32 " MHz", cpu_frequency_hz / 1000000);
    }

    uint32_t flash_size = 0;
    const esp_err_t flash_result = esp_flash_get_physical_size(nullptr, &flash_size);
    if (flash_result == ESP_OK) {
        ESP_LOGI(kTag, "Flash size (physical): %" PRIu32 " bytes (%u MiB)",
                 flash_size, static_cast<unsigned>(flash_size / kMiB));
    } else {
        ESP_LOGW(kTag, "Flash size query failed: %s", esp_err_to_name(flash_result));
    }

    // ESP-IDF initializes PSRAM before app_main; do not initialize it twice.
    const bool psram_initialized = esp_psram_is_initialized();
    const size_t psram_size = psram_initialized ? esp_psram_get_size() : 0;
    const size_t free_psram = heap_caps_get_free_size(kPsramCaps);
    ESP_LOGI(kTag, "Detected PSRAM: %zu bytes (%zu MiB)", psram_size, psram_size / kMiB);
    ESP_LOGI(kTag, "Free internal heap (8-bit): %zu bytes",
             heap_caps_get_free_size(kInternalCaps));
    ESP_LOGI(kTag, "Free PSRAM (8-bit): %zu bytes", free_psram);
    ESP_LOGI(kTag, "ESP-IDF version: %s", esp_get_idf_version());

    if (!psram_initialized || psram_size == 0 || free_psram == 0) {
        ESP_LOGW(kTag, "WARNING: PSRAM unavailable or no usable PSRAM heap; continuing proof of life.");
    } else {
        ESP_LOGI(kTag, "PSRAM available and registered with the heap allocator.");
        if (psram_size != 8 * kMiB) {
            ESP_LOGW(kTag, "Expected 8 MiB PSRAM on the T-Deck Plus; detected %zu bytes.",
                     psram_size);
        }
    }

    tdeck::Board board;
    const esp_err_t display_result = board.initialize_display();
    if (display_result != ESP_OK) {
        ESP_LOGE(kTag, "Display initialization failed: %s", esp_err_to_name(display_result));
    }

    const tdeck::SdStatus sd = board.initialize_and_test_sd();
    board.show_diagnostics(sd.ok);
    if (sd.ok) {
        ESP_LOGI(kTag, "SD: OK | type=%s | size=%llu MiB | test source=%s",
                 sd.type, sd.capacity_bytes / (1024ULL * 1024ULL),
                 sd.used_existing_test_file ? "existing file" : "temporary file");
        ESP_LOGI(kTag, "Display/SD shared SPI runtime test: %s",
                 sd.shared_bus_verified ? "PASS" : "FAIL");
        openu5::AssetPackReport assets{};
        const esp_err_t asset_result = openu5::validate_asset_pack(openu5::kAssetPackPath, assets);
        if (asset_result == ESP_ERR_NOT_FOUND) {
            ESP_LOGW(kTag, "Native assets absent; copy the generated pack to %s",
                     openu5::kAssetPackPath);
        } else if (asset_result != ESP_OK) {
            ESP_LOGE(kTag, "Native asset validation failed; no assets will be used");
        }
    } else {
        ESP_LOGE(kTag, "SD: FAIL (%s); firmware will remain alive",
                 esp_err_to_name(sd.error));
    }

    ESP_LOGI(kTag, "Firmware size: run idf.py size and idf.py size-components on the build host.");
    ESP_LOGI(kTag, "Initialization complete; heartbeat every 5 seconds.");
    for (;;) {
        // Yield to idle tasks so watchdogs remain serviced; no busy loop or reboot.
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(kTag, "Alive | internal heap: %zu bytes | PSRAM heap: %zu bytes",
                 heap_caps_get_free_size(kInternalCaps),
                 heap_caps_get_free_size(kPsramCaps));
    }
}
