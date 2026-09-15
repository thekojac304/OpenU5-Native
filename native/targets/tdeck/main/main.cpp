#include <cinttypes>
#include <cstddef>
#include <cstdio>

#include "esp_chip_info.h"
#include "esp_clk_tree.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "asset_pack.h"
#include "native_renderer.h"
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
    ESP_LOGI(kTag, "OpenU5-TDeck | Milestone 4: corrected initial viewport");
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
        openu5::AssetPackReader asset_reader;
        const esp_err_t asset_result = asset_reader.open(openu5::kAssetPackPath, assets);
        if (asset_result == ESP_ERR_NOT_FOUND) {
            ESP_LOGW(kTag, "Native assets absent; copy the generated pack to %s",
                     openu5::kAssetPackPath);
        } else if (asset_result != ESP_OK) {
            ESP_LOGE(kTag, "Native asset validation failed; no assets will be used");
        } else {
            ESP_LOGI(kTag, "Initial coordinates: x=%u y=%u", assets.initial_x, assets.initial_y);
            ESP_LOGI(kTag, "Initial location/floor: location=%u floor=0x%02x",
                     assets.initial_location, assets.initial_floor);
            const size_t internal_before = heap_caps_get_free_size(kInternalCaps);
            const size_t psram_before = heap_caps_get_free_size(kPsramCaps);
            auto *viewport = static_cast<uint16_t *>(
                heap_caps_malloc(openu5::kViewportPixelCount * sizeof(uint16_t),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (viewport == nullptr) {
                ESP_LOGE(kTag, "Could not allocate %zu-byte RGB565 viewport in PSRAM",
                         openu5::kViewportPixelCount * sizeof(uint16_t));
            } else {
                const int64_t render_start = esp_timer_get_time();
                openu5::RenderReport render{};
                const esp_err_t render_result = openu5::render_initial_view(
                    asset_reader, assets, viewport, openu5::kViewportPixelCount, render);
                const uint32_t render_ms = static_cast<uint32_t>(
                    (esp_timer_get_time() - render_start + 999) / 1000);
                const size_t internal_after = heap_caps_get_free_size(kInternalCaps);
                const size_t psram_after = heap_caps_get_free_size(kPsramCaps);
                if (render_result != ESP_OK) {
                    ESP_LOGE(kTag, "Static Britannia render failed: %s",
                             esp_err_to_name(render_result));
                } else {
                    ESP_LOGI(kTag, "Selected initial map/context: %s", render.map_context);
                    ESP_LOGI(kTag, "Viewport bounds: left=%d top=%d right=%d bottom=%d",
                             render.left, render.top, render.right, render.bottom);
                    ESP_LOGI(kTag, "Center map tile=0x%02x Avatar tile=0x%03x",
                             render.center_map_tile, render.avatar_tile);
                    ESP_LOGI(kTag, "Tiles read from SD: %u whole 128-byte records",
                             render.tile_records_read);
                    ESP_LOGI(kTag, "Render duration: %" PRIu32 " ms", render_ms);
                    ESP_LOGI(kTag, "Free internal RAM before/after: %zu / %zu bytes",
                             internal_before, internal_after);
                    ESP_LOGI(kTag, "Free PSRAM before/after: %zu / %zu bytes",
                             psram_before, psram_after);
                    ESP_LOGI(kTag, "Viewport RGB565-LE CRC32: %08" PRIx32,
                             render.viewport_crc32);
                    char coordinates[20]{};
                    char location[20]{};
                    std::snprintf(coordinates, sizeof(coordinates), "X:%03u Y:%03u",
                                  assets.initial_x, assets.initial_y);
                    std::snprintf(location, sizeof(location), "LOC:%02u F:%02X",
                                  assets.initial_location, assets.initial_floor);
                    const esp_err_t draw_result = board.show_initial_view(
                        viewport, openu5::kViewportPixels, openu5::kViewportPixels,
                        coordinates, location);
                    if (draw_result != ESP_OK) {
                        ESP_LOGE(kTag, "Viewport display transfer failed: %s",
                                 esp_err_to_name(draw_result));
                    }
                }
                heap_caps_free(viewport);
            }
        }
    } else {
        ESP_LOGE(kTag, "SD: FAIL (%s); firmware will remain alive",
                 esp_err_to_name(sd.error));
    }

    ESP_LOGI(kTag, "Firmware size: run idf.py size and idf.py size-components on the build host.");
    ESP_LOGI(kTag, "Milestone 4 initialization complete; heartbeat every 5 seconds.");
    for (;;) {
        // Yield to idle tasks so watchdogs remain serviced; no busy loop or reboot.
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(kTag, "Alive | internal heap: %zu bytes | PSRAM heap: %zu bytes",
                 heap_caps_get_free_size(kInternalCaps),
                 heap_caps_get_free_size(kPsramCaps));
    }
}
