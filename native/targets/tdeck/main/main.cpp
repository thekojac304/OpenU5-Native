#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <cinttypes>
#include <cstddef>
#include <cstdio>

#include "esp_chip_info.h"
#include "esp_app_desc.h"
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

#include "alpha_resources.h"
#include "alpha_runtime.h"
#include "asset_pack.h"
#include "boot_trace.h"
#include "sd_diagnostic_logger.h"
#include "tdeck_board.h"
#include "tdeck_input.h"

namespace {
constexpr char kTag[]="OpenU5-TDeck";
constexpr size_t kMiB=1024*1024;
constexpr uint32_t kInternal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,kPsram=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
#if !defined(OPENU5_GIT_COMMIT)
#define OPENU5_GIT_COMMIT "unknown"
#endif
}

extern "C" void app_main(void) {
    const bool sd_log_capture=tdeck::sdlog::begin_capture();
    debug51::begin();vTaskDelay(pdMS_TO_TICKS(1500));debug51::stage(2,"serial-ready-ALPHA1");
    ESP_LOGI(kTag,"========================================");
    ESP_LOGI(kTag,"OpenU5-TDeck | Alpha 2.0 alpha2 Hardware-Truth Frontend Debug");
    ESP_LOGI(kTag,"HOST-BUILT: physical hardware validation pending");
    ESP_LOGI(kTag,"========================================");
    esp_chip_info_t chip{};esp_chip_info(&chip);uint32_t cpu=0;esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU,ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT,&cpu);
    uint32_t flash=0;esp_flash_get_physical_size(nullptr,&flash);
    ESP_LOGI(kTag,"Chip=%s rev=%u cores=%u CPU=%" PRIu32 "MHz flash=%" PRIu32 "MiB IDF=%s",chip.model==CHIP_ESP32S3?"ESP32-S3":"unexpected",unsigned(chip.revision),unsigned(chip.cores),cpu/1000000,flash/kMiB,esp_get_idf_version());
    const bool psram_ok=esp_psram_is_initialized();const size_t psram_size=psram_ok?esp_psram_get_size():0;
    ESP_LOGI(kTag,"Boot memory: internal=%zu PSRAM=%zu/%zu",heap_caps_get_free_size(kInternal),heap_caps_get_free_size(kPsram),psram_size);
    if(!psram_ok||psram_size<7*kMiB)ESP_LOGE(kTag,"Alpha 2.0 requires working 8 MiB PSRAM");

    static tdeck::Board board;debug51::stage(3,"board-construction");
    const esp_err_t display=board.initialize_display();debug51::stack_checkpoint("after-board-display-init");
    const tdeck::SdStatus sd=board.initialize_and_test_sd();debug51::stack_checkpoint("after-sd-init");board.show_diagnostics(sd.ok);
    const bool sd_log_ready=sd.ok&&sd_log_capture&&tdeck::sdlog::initialize_storage();
    if(sd_log_ready)ESP_LOGI(kTag,"SD logging initialized successfully: %s (buffered, 512 KiB cap)",tdeck::sdlog::kCardLogPath);
    else ESP_LOGW(kTag,"SD logging initialization failed; serial logging remains active");
    static tdeck::InputHardware input;
    esp_err_t input_result=ESP_FAIL;
    {
        debug51::Step trace("keyboard-initialize");
        input_result=input.initialize();
    }
    debug51::stack_checkpoint("after-keyboard-init");
    if(display!=ESP_OK||!sd.ok||input_result!=ESP_OK||!psram_ok)ESP_LOGE(kTag,"Required hardware initialization failed: display=%s SD=%s input=%s PSRAM=%d",esp_err_to_name(display),esp_err_to_name(sd.error),esp_err_to_name(input_result),psram_ok);

    static openu5::AssetPackReader tile_pack;static openu5::AssetPackReport tile_report{};
    static tdeck::AlphaResourcePack alpha_pack;static tdeck::AlphaResourceReport alpha_report{};
    static tdeck::AlphaRuntime runtime;bool ready=false;
    if(display==ESP_OK&&sd.ok&&input_result==ESP_OK&&psram_ok){
        debug51::stage(9,"alpha-assets-open");
        esp_err_t tiles=ESP_FAIL;
        {
            debug51::Step trace("asset-pack-open-validate");
            tiles=tile_pack.open(openu5::kAssetPackPath,tile_report);
        }
        esp_err_t alpha=ESP_FAIL;
        {
            debug51::Step trace("alpha-resource-open-validate");
            alpha=alpha_pack.open(tdeck::kAlphaResourcePath,alpha_report);
        }
        if(tiles!=ESP_OK)ESP_LOGE(kTag,"Tile pack unavailable at %s: %s",openu5::kAssetPackPath,esp_err_to_name(tiles));
        if(alpha!=ESP_OK)ESP_LOGE(kTag,"Alpha resource pack unavailable at %s: %s",tdeck::kAlphaResourcePath,esp_err_to_name(alpha));
        const auto *app=esp_app_get_description();
        char firmware[64]{},git[64]{},build[64]{},alpha_id[96]{},asset_id[96]{};
        std::snprintf(firmware,sizeof(firmware),"FW %s",app&&app->version[0]?app->version:"2.0.0-alpha2-debug");
        std::snprintf(git,sizeof(git),"Git %s",OPENU5_GIT_COMMIT);
        std::snprintf(build,sizeof(build),"Build %s %s",app?app->date:__DATE__,app?app->time:__TIME__);
        std::snprintf(alpha_id,sizeof(alpha_id),"RES v%u.%u %luB CRC %08lx",unsigned(alpha_report.version_major),unsigned(alpha_report.version_minor),(unsigned long)alpha_report.file_size,(unsigned long)alpha_report.payload_crc32);
        std::snprintf(asset_id,sizeof(asset_id),"ASSET v%u.%u %luB CRC %08lx",unsigned(tile_report.version_major),unsigned(tile_report.version_minor),(unsigned long)tile_report.file_size,(unsigned long)tile_report.payload_crc32);
        const bool packs_match=tiles==ESP_OK&&alpha==ESP_OK&&tile_report.firmware_match&&alpha_report.firmware_match;
        {
            debug51::Step trace("identity-log");
            ESP_LOGI(kTag,"IDENTITY firmware=%s git=%s build=%s_%s alpha=v%u.%u,size=%lu,crc=%08lx,expected_sha=%.12s asset=v%u.%u,size=%lu,crc=%08lx,expected_sha=%.12s match=%d",
                     app&&app->version[0]?app->version:"2.0.0-alpha2-debug",OPENU5_GIT_COMMIT,
                     app?app->date:__DATE__,app?app->time:__TIME__,
                     unsigned(alpha_report.version_major),unsigned(alpha_report.version_minor),
                     (unsigned long)alpha_report.file_size,(unsigned long)alpha_report.payload_crc32,
                     tdeck::kExpectedAlphaResourceSha256,
                     unsigned(tile_report.version_major),unsigned(tile_report.version_minor),
                     (unsigned long)tile_report.file_size,(unsigned long)tile_report.payload_crc32,
                     openu5::kExpectedAssetPackSha256,packs_match);
        }
        {
            debug51::Step trace("identity-screen-draw");
            board.show_runtime_identity(firmware,git,build,alpha_id,asset_id,packs_match);
        }
        if(packs_match){
            {
                debug51::Step trace("identity-screen-hold");
                vTaskDelay(pdMS_TO_TICKS(1800));
            }
            esp_err_t initialized=ESP_FAIL;
            {
                debug51::Step trace("alpha-runtime-initialize");
                initialized=runtime.initialize(alpha_pack,alpha_report,tile_pack,tile_report);
            }
            ready=initialized==ESP_OK;
            {
                debug51::Step trace("alpha-resource-close");
                alpha_pack.close();
            }
            {
                debug51::Step trace("asset-pack-close");
                tile_pack.close();
            }
            ESP_LOGI(kTag,"Resource packs closed after PSRAM/cache load; save descriptor reserve restored");
            if(!ready)ESP_LOGE(kTag,"Alpha runtime initialization failed: %s",esp_err_to_name(initialized));
        }
        else ESP_LOGE(kTag,"RESOURCE_MISMATCH startup blocked expected_res_size=%lu expected_res_crc=%08lx expected_asset_size=%lu expected_asset_crc=%08lx",
                      (unsigned long)tdeck::kExpectedAlphaResourceSize,(unsigned long)tdeck::kExpectedAlphaResourceCrc32,
                      (unsigned long)openu5::kExpectedAssetPackSize,(unsigned long)openu5::kExpectedAssetPackCrc32);
    }
    bool sd_log_writer=false;
    {
        debug51::Step trace("sd-log-writer-start");
        sd_log_writer=sd_log_ready&&tdeck::sdlog::start_writer();
    }
    if(sd_log_ready&&!sd_log_writer)ESP_LOGW(kTag,"SD logging writer failed to start; serial logging remains active");
    if(ready){
        debug51::Step trace("frontend-first-render");
        runtime.render(board,true);
        debug51::stack_checkpoint("after-alpha-render");
    }
    ESP_LOGI(kTag,"Alpha input loop active; no physical-device success is asserted");debug51::stage(12,"alpha-input-loop");
    int64_t heartbeat=esp_timer_get_time()+5000000;
    for(;;){tdeck::RawInputEvent raw{};bool input_dirty=false;size_t drained=0;
        while(input_result==ESP_OK&&drained<64&&input.poll(raw)){
            ++drained;
            ESP_LOGD(kTag,"input src=%s code=%02x state=%s S%d A%d H%d",tdeck::raw_input_name(raw.kind),raw.code,tdeck::transition_name(raw.transition),raw.modifiers.symbol,raw.modifiers.alt,raw.modifiers.shift);
            if(ready&&runtime.handle(raw))input_dirty=true;
        }
        if(input_dirty&&ready){const auto draw=runtime.render(board);if(draw!=ESP_OK)ESP_LOGE(kTag,"Alpha redraw failed: %s",esp_err_to_name(draw));}
        if(ready){const auto draw=runtime.render(board);if(draw!=ESP_OK)ESP_LOGE(kTag,"Alpha animation redraw failed: %s",esp_err_to_name(draw));}
        const int64_t now=esp_timer_get_time();if(now>=heartbeat){debug51::stack_checkpoint("running-alpha-loop");input.log_metrics();if(ready)runtime.log_metrics("heartbeat");else ESP_LOGW(kTag,"Alpha runtime not ready; internal=%zu PSRAM=%zu",heap_caps_get_free_size(kInternal),heap_caps_get_free_size(kPsram));heartbeat=now+5000000;}
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
