#pragma once
// Alpha 3 A3-04E host-test seam: ESP-IDF's driver/spi_master.h, as far as the
// REAL tdeck_board.cpp uses it. spi_device_transmit() is the fake ST7789 and
// the timing model in fake_tdeck_bus.cpp. Designated-initialiser order below
// follows ESP-IDF 6.1's structs (C++20 requires declaration order).
#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "esp_err.h"

typedef enum { SPI1_HOST = 0, SPI2_HOST = 1, SPI3_HOST = 2 } spi_host_device_t;
typedef enum { SPI_DMA_DISABLED = 0, SPI_DMA_CH_AUTO = 3 } spi_dma_chan_t;
typedef int spi_clock_source_t;
#define SPI_CLK_SRC_DEFAULT 0
typedef enum { SPI_SAMPLING_POINT_PHASE_0 = 0, SPI_SAMPLING_POINT_PHASE_1 } spi_sampling_point_t;
typedef int esp_intr_cpu_affinity_t;
#define ESP_INTR_CPU_AFFINITY_AUTO 0
#define SPICOMMON_BUSFLAG_MASTER (1u << 0)
#define SPI_DEVICE_NO_DUMMY (1u << 6)
#define SPI_TRANS_USE_TXDATA (1u << 3)

typedef struct {
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int data4_io_num;
    int data5_io_num;
    int data6_io_num;
    int data7_io_num;
    bool data_io_default_level;
    int max_transfer_sz;
    int dma_burst_size;
    uint32_t flags;
    esp_intr_cpu_affinity_t isr_cpu_id;
    int intr_flags;
} spi_bus_config_t;

struct spi_transaction_t;
typedef void (*transaction_cb_t)(struct spi_transaction_t *);

typedef struct {
    uint8_t command_bits;
    uint8_t address_bits;
    uint8_t dummy_bits;
    uint8_t mode;
    spi_clock_source_t clock_source;
    uint16_t duty_cycle_pos;
    uint16_t cs_ena_pretrans;
    uint8_t cs_ena_posttrans;
    int clock_speed_hz;
    int input_delay_ns;
    spi_sampling_point_t sample_point;
    int spics_io_num;
    uint32_t flags;
    int queue_size;
    transaction_cb_t pre_cb;
    transaction_cb_t post_cb;
} spi_device_interface_config_t;

struct spi_transaction_t {
    uint32_t flags;
    uint16_t cmd;
    uint64_t addr;
    size_t length;   // bits
    size_t rxlength; // bits
    void *user;
    union {
        const void *tx_buffer;
        uint8_t tx_data[4];
    };
    union {
        void *rx_buffer;
        uint8_t rx_data[4];
    };
};
typedef struct spi_transaction_t spi_transaction_t;

typedef struct openu5_host_spi_device *spi_device_handle_t;

esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *config, spi_dma_chan_t dma);
esp_err_t spi_bus_add_device(spi_host_device_t host, const spi_device_interface_config_t *config,
                             spi_device_handle_t *handle);
esp_err_t spi_device_transmit(spi_device_handle_t handle, spi_transaction_t *transaction);
