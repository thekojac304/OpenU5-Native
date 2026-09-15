#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <cinttypes>
#include <cstddef>
#include <cstdio>

#include "esp_chip_info.h"
#include "esp_check.h"
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
#include "boot_trace.h"
#include "input_controller.h"
#include "native_movement.h"
#include "native_renderer.h"
#include "tdeck_board.h"
#include "tdeck_input.h"

namespace {
constexpr char kTag[] = "OpenU5-TDeck";
constexpr size_t kMiB = 1024 * 1024;
constexpr uint32_t kInternalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
constexpr uint32_t kPsramCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

openu5::MapGeometry geometry_for(const openu5::AssetPackReport &pack)
{
    return pack.initial_location == 0 && pack.initial_floor == 0
               ? openu5::MapGeometry{pack.world_width, pack.world_height, true}
               : openu5::MapGeometry{pack.initial_map_width, pack.initial_map_height, false};
}

esp_err_t read_map_tile(openu5::AssetPackReader &reader,
                        const openu5::AssetPackReport &pack,
                        openu5::Position position, uint8_t &tile)
{
    if (pack.initial_location == 0 && pack.initial_floor == 0) {
        return reader.read_world_span(position.y, position.x, &tile, 1);
    }
    if (pack.initial_location == 13 && pack.initial_floor == 0) {
        return reader.read_initial_map_span(position.y, position.x, &tile, 1);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t render_and_show(tdeck::Board &board, openu5::AssetPackReader &reader,
                          const openu5::AssetPackReport &pack,
                          openu5::Position position, uint16_t *viewport,
                          bool first_draw, openu5::RenderReport &render)
{
    ESP_RETURN_ON_ERROR(openu5::render_view(reader, pack, position.x, position.y,
                                             viewport, openu5::kViewportPixelCount, render),
                        kTag, "compose viewport");
    char coordinates[20]{};
    char location[20]{};
    std::snprintf(coordinates, sizeof(coordinates), "X:%03u Y:%03u", position.x, position.y);
    std::snprintf(location, sizeof(location), "LOC:%02u F:%02X",
                  pack.initial_location, pack.initial_floor);
    return board.show_view(viewport, openu5::kViewportPixels, openu5::kViewportPixels,
                           coordinates, location, first_draw);
}
}  // namespace

extern "C" void app_main(void)
{
    debug51::begin();
    // Allow the USB serial port time to enumerate; never wait for a host forever.
    vTaskDelay(pdMS_TO_TICKS(1500));
    debug51::stage(2, "serial-ready-M51-DIAGNOSTIC");
    INPUT_TRACE("SCHED tick_hz=%d delay_5ms_ticks=%u main_stack=%d",
                CONFIG_FREERTOS_HZ, (unsigned)pdMS_TO_TICKS(5), CONFIG_ESP_MAIN_TASK_STACK_SIZE);
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "OpenU5-TDeck | Milestone 5: native directional movement");
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

    debug51::stage(3, "board-construction");
    // App-lifetime state belongs in internal static storage, not in every
    // nested startup/render/I/O call's main-task stack budget.
    static tdeck::Board board;
    const esp_err_t display_result = board.initialize_display();
    debug51::stack_checkpoint("after-board-display-init");
    if (display_result != ESP_OK) {
        ESP_LOGE(kTag, "Display initialization failed: %s", esp_err_to_name(display_result));
    }

    const tdeck::SdStatus sd = board.initialize_and_test_sd();
    debug51::stack_checkpoint("after-sd-init");
    board.show_diagnostics(sd.ok);
    static tdeck::InputHardware input;
    debug51::stage(7, "keyboard-initialize");
    const esp_err_t input_result = input.initialize();
    debug51::stage(8, "keyboard-initialize-returned");
    debug51::stack_checkpoint("after-keyboard-init");
    if (input_result != ESP_OK) {
        ESP_LOGE(kTag, "Input initialization failed: %s", esp_err_to_name(input_result));
    }
    static openu5::AssetPackReport assets{};
    static openu5::AssetPackReader asset_reader;
    // Static bounded foundation state avoids enlarging the unverified app-task stack.
    static openu5::GameState core_state{};
    openu5::Position &position = core_state.position.xy;
    uint16_t *viewport = nullptr;
    bool movement_ready = false;
    if (sd.ok) {
        ESP_LOGI(kTag, "SD: OK | type=%s | size=%llu MiB | test source=%s",
                 sd.type, sd.capacity_bytes / (1024ULL * 1024ULL),
                 sd.used_existing_test_file ? "existing file" : "temporary file");
        ESP_LOGI(kTag, "Display/SD shared SPI runtime test: %s",
                 sd.shared_bus_verified ? "PASS" : "FAIL");
        debug51::stage(9, "asset-open-validation");
        debug51::stack_checkpoint("before-asset-validation");
        const esp_err_t asset_result = asset_reader.open(openu5::kAssetPackPath, assets);
        INPUT_TRACE("ASSET validation-return result=%s", esp_err_to_name(asset_result));
        debug51::stack_checkpoint("after-asset-validation");
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
            position = {assets.initial_x, assets.initial_y};
            core_state.position.map = {assets.initial_location, assets.initial_floor};
            viewport = static_cast<uint16_t *>(
                heap_caps_malloc(openu5::kViewportPixelCount * sizeof(uint16_t),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (viewport == nullptr) {
                ESP_LOGE(kTag, "Could not allocate %zu-byte RGB565 viewport in PSRAM",
                         openu5::kViewportPixelCount * sizeof(uint16_t));
            } else {
                const int64_t render_start = esp_timer_get_time();
                debug51::stage(10, "initial-render-entry");
                openu5::RenderReport render{};
                const esp_err_t render_result = render_and_show(
                    board, asset_reader, assets, position, viewport, true, render);
                const uint32_t render_ms = static_cast<uint32_t>(
                    (esp_timer_get_time() - render_start + 999) / 1000);
                INPUT_TRACE("RENDER initial-return result=%s", esp_err_to_name(render_result));
                debug51::stack_checkpoint("after-renderer-init");
                const size_t internal_after = heap_caps_get_free_size(kInternalCaps);
                const size_t psram_after = heap_caps_get_free_size(kPsramCaps);
                if (render_result != ESP_OK) {
                    ESP_LOGE(kTag, "Initial viewport render failed: %s",
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
                    movement_ready = input_result == ESP_OK;
                }
            }
        }
    } else {
        ESP_LOGE(kTag, "SD: FAIL (%s); firmware will remain alive",
                 esp_err_to_name(sd.error));
    }

    ESP_LOGI(kTag, "Milestone 5 input loop active; hardware behavior awaits device validation");
    static openu5::InputController controller;
    debug51::stack_checkpoint("after-game-init");
    debug51::stage(12, "input-loop");
    int64_t next_heartbeat_us = esp_timer_get_time() + 5000000;
    for (;;) {
        tdeck::RawInputEvent raw{};
        if (input_result == ESP_OK && input.poll(raw)) {
            if (raw.kind == tdeck::RawInputKind::Keyboard) {
                ESP_LOGD(kTag,
                         "Raw key: matrix=%u,%u code=0x%02x state=%s modifiers=S%d A%d H%d",
                         raw.column, raw.row, raw.code,
                         tdeck::transition_name(raw.transition), raw.modifiers.symbol,
                         raw.modifiers.alt, raw.modifiers.shift);
            } else {
                ESP_LOGD(kTag, "Raw trackball direction: %s",
                         tdeck::raw_input_name(raw.kind));
            }
            openu5::Direction direction{};
            const bool accepted = controller.normalize(raw, direction);
            INPUT_TRACE("ACTION t=%lld event_t=%lld src=%s matrix=%u,%u code=%02x state=%s S%d A%d H%d accepted=%d reason=%s action=%s ready=%d",
                        (long long)esp_timer_get_time(), (long long)raw.timestamp_us,
                        tdeck::raw_input_name(raw.kind), raw.column, raw.row, raw.code,
                        tdeck::transition_name(raw.transition), raw.modifiers.symbol,
                        raw.modifiers.alt, raw.modifiers.shift, accepted, controller.decision(),
                        accepted ? openu5::direction_name(direction) : "none", movement_ready);
            if (accepted) {
                ESP_LOGI(kTag, "Input action: direction=%s source=%s",
                         openu5::direction_name(direction), tdeck::raw_input_name(raw.kind));
                if (movement_ready) {
                    const openu5::MapGeometry map = geometry_for(assets);
                    openu5::Position target{};
                    uint8_t target_tile = 0xff;
                    esp_err_t tile_result = ESP_OK;
                    if (openu5::target_for_step(position, map, direction, target)) {
                        tile_result = read_map_tile(asset_reader, assets, target, target_tile);
                    }
                    if (tile_result != ESP_OK) {
                        ESP_LOGE(kTag, "Target tile read failed: %s", esp_err_to_name(tile_result));
                    } else {
                        const openu5::MoveReport move =
                            openu5::apply_movement_slice(core_state, map, {direction}, target_tile).movement;
                        ESP_LOGI(kTag,
                                 "Move %u,%u -> %d,%d tile=0x%02x %s; result=%u,%u",
                                 move.old_position.x, move.old_position.y,
                                 move.attempted_x, move.attempted_y,
                                 move.target_tile, move.passable ? "passable" : "blocked",
                                 move.resulting_position.x, move.resulting_position.y);
                        if (move.moved) {
                            const int64_t redraw_start = esp_timer_get_time();
                            INPUT_TRACE("REDRAW begin t=%lld", (long long)redraw_start);
                            openu5::RenderReport render{};
                            const esp_err_t redraw_result = render_and_show(
                                board, asset_reader, assets, position, viewport, false, render);
                            const uint32_t redraw_ms = static_cast<uint32_t>(
                                (esp_timer_get_time() - redraw_start + 999) / 1000);
                            INPUT_TRACE("REDRAW end t=%lld us=%lld result=%s",
                                        (long long)esp_timer_get_time(),
                                        (long long)(esp_timer_get_time()-redraw_start), esp_err_to_name(redraw_result));
                            if (redraw_result == ESP_OK) {
                                ESP_LOGI(kTag, "Redraw duration: %" PRIu32
                                         " ms; tile records=%u; CRC32=%08" PRIx32,
                                         redraw_ms, render.tile_records_read,
                                         render.viewport_crc32);
                            } else {
                                ESP_LOGE(kTag, "Viewport redraw failed: %s",
                                         esp_err_to_name(redraw_result));
                            }
                        }
                    }
                }
            }
        }
        const int64_t now = esp_timer_get_time();
        if (now >= next_heartbeat_us) {
            debug51::stack_checkpoint("running-input-loop");
            next_heartbeat_us = now + 5000000;
            ESP_LOGI(kTag, "Alive | x=%u y=%u | internal heap=%zu | PSRAM heap=%zu",
                     position.x, position.y, heap_caps_get_free_size(kInternalCaps),
                     heap_caps_get_free_size(kPsramCaps));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
