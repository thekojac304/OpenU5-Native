#pragma once
// Alpha 3 A3-04E (ALPHA3_AUDIO.md section 22) host-test seam: the T-Deck's
// display bus under the REAL tdeck_board.cpp.
//
//   * an ST7789 that decodes CASET / RASET / RAMWR from the byte stream the
//     Board sends (D/C from gpio_set_level) into a 320x240 RGB565 GRAM, and
//     hashes every transaction -- so two pacing policies can be proved to put
//     exactly the same bytes, in the same order, on the panel;
//   * a timing model on the host's virtual clock (esp_timer.h): each
//     spi_device_transmit costs ESP-IDF's documented ESP32-S3 interrupt-
//     transaction time (docs/en/api-reference/peripherals/spi_master.rst,
//     "Transaction Duration": 26 us via DMA, 24 us via CPU, with
//     CONFIG_SPI_MASTER_ISR_IN_IRAM as this firmware has it) plus the bits at
//     40 MHz; vTaskDelay(n) sleeps to the n-th next 10 ms tick
//     (CONFIG_FREERTOS_HZ=100); taskYIELD costs 1 us (nothing else of the
//     game thread's priority is ready on core 0).
//
// Row building (the Board's "fill") is not modelled: the device measures it.
// So a modelled TFT time is transfers + pauses, a lower bound of the device's.
#include <cstdint>
#include <vector>

namespace openu5_host_bus {

struct Model {
    uint32_t spi_hz = 40000000;
    uint32_t dma_transaction_ns = 26000; // ESP32-S3, interrupt transaction via DMA
    uint32_t cpu_transaction_ns = 24000; // via CPU (SPI_TRANS_USE_TXDATA, <= 32 bits)
    uint32_t tick_us = 10000;            // CONFIG_FREERTOS_HZ=100
    uint32_t yield_ns = 1000;
    uint32_t cpu_mhz = 240;
    // false: count everything, move the clock for nothing -- the virtual time
    // is then the test's script alone, so two pacing policies draw the same
    // content and their byte streams must match exactly.
    bool timed = true;
};

struct Stats {
    uint64_t transactions = 0, bytes = 0;
    uint64_t xfer_ns = 0;
    uint64_t tick_sleeps = 0, sleep_us = 0; // vTaskDelay(n>0)
    uint64_t zero_delays = 0;               // vTaskDelay(0): a reschedule
    uint64_t yields = 0;                    // taskYIELD
    uint64_t malformed = 0;                 // pixel data that was not whole pixels, or outside a window
};

/** One scheduler call of the draw code, with the panel window it interrupted. */
struct Pause {
    int x0, y0, x1, y1;
    bool sleep; // vTaskDelay(n>0); false = taskYIELD
};

Model &model();
Stats &stats();
/** Installs the vTaskDelay / taskYIELD hooks and clears the panel and the stats. */
void install();
void reset_stats();
/** Every pause since install() / reset_stats(). */
const std::vector<Pause> &pauses();
/** The panel's memory, 320 x 240 RGB565, row-major. */
const uint16_t *gram();
/** FNV-1a over every transaction since install() / restart_stream(): D/C level, length, bytes. */
uint64_t stream_hash();
/** Restarts the stream hash and the stats; the panel keeps what it shows. */
void restart_stream();
/** Advances the virtual clock by `ns` (sub-microsecond remainders carry). */
void advance_ns(uint64_t ns);

} // namespace openu5_host_bus
