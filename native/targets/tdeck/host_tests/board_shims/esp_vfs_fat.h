#pragma once
// Alpha 3 A3-04E host-test seam: ESP-IDF's esp_vfs_fat.h and the SD card
// types, so that tdeck_board.cpp compiles on the host. The mount always fails
// (no host test mounts a card).
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "driver/sdspi_host.h"
#include "esp_err.h"

typedef struct {
    struct {
        uint32_t capacity;
        uint32_t sector_size;
    } csd;
    bool is_mmc;
    uint32_t ocr;
    int real_freq_khz;
} sdmmc_card_t;
#define SD_OCR_SDHC_CAP (1u << 30)

typedef struct {
    bool format_if_mount_failed;
    int max_files;
    size_t allocation_unit_size;
    bool disk_status_check_enable;
    bool use_one_fat;
} esp_vfs_fat_sdmmc_mount_config_t;

esp_err_t esp_vfs_fat_sdspi_mount(const char *base_path, const sdmmc_host_t *host,
                                  const sdspi_device_config_t *slot, const esp_vfs_fat_sdmmc_mount_config_t *config,
                                  sdmmc_card_t **card);
void sdmmc_card_print_info(FILE *stream, const sdmmc_card_t *card);

#if defined(_WIN32)
// mingw has no fsync(); only initialize_and_test_sd() names it, and no host test runs that.
inline int fsync(int) { return 0; }
#endif
