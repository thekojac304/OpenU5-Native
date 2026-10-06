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
    // A3-04E.1: core 0's idle loop (the task watchdog's feed), modelled. A
    // block of the game thread lets the idle task finish a pass -- one count
    // of the idle hook -- when it lasts at least idle_pass_us. rows_feed_idle
    // is A3-04E's assumption that every TFT transaction's wait does; false is
    // what the hardware showed (only tick-long sleeps reliably do).
    bool rows_feed_idle = true;
    uint32_t idle_pass_us = 20;
};

// A3-04F (ALPHA3_AUDIO.md section 26): a pixel transaction whose bits take
// less time on the wire than the transaction's fixed cost (26 us = 130 bytes
// at 40 MHz) is overhead-dominated -- "thin". A "tiny" one is one 16 px row or less.
constexpr uint32_t kThinPixelBytes = 130;
constexpr uint32_t kTinyPixelBytes = 32;

struct Stats {
    uint64_t transactions = 0, bytes = 0;
    uint64_t xfer_ns = 0;
    uint64_t tick_sleeps = 0, sleep_us = 0; // vTaskDelay(n>0)
    uint64_t zero_delays = 0;               // vTaskDelay(0): a reschedule
    uint64_t yields = 0;                    // taskYIELD
    uint64_t malformed = 0;                 // pixel data that was not whole pixels, or outside a window
    // A3-04F: the pixel side of the stream (RAMWR payload transactions).
    uint64_t pixel_transactions = 0, pixel_bytes = 0;
    uint64_t thin_transactions = 0, tiny_transactions = 0;
    uint64_t windows = 0;           // RAMWR commands: one per draw primitive call
    uint32_t max_transaction_bytes = 0;
    uint64_t oversize = 0;          // refused as the driver refuses them: length > max_transfer_sz
};

/** A3-04F: one RAMWR -- the window its pixels went to, and how they travelled. */
struct WindowWrite {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    uint32_t pixels = 0, transactions = 0, thin = 0;
    bool solid = true;   // every pixel the same colour (a fill, or a blank text row)
    uint16_t colour = 0; // the first pixel's
    int width() const { return x1 - x0 + 1; }
    int height() const { return y1 - y0 + 1; }
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
/** A3-04F: every RAMWR window since install() / reset_stats(), in order. */
const std::vector<WindowWrite> &windows();
/** A3-04F: the bus's max_transfer_sz as the Board configured it (spi_bus_initialize). */
uint32_t max_transfer_bytes();
/** The panel's memory, 320 x 240 RGB565, row-major. */
const uint16_t *gram();
/** FNV-1a over every transaction since install() / restart_stream(): D/C level, length, bytes. */
uint64_t stream_hash();
/** Restarts the stream hash and the stats; the panel keeps what it shows. */
void restart_stream();
/** Advances the virtual clock by `ns` (sub-microsecond remainders carry). */
void advance_ns(uint64_t ns);
/** The modelled core-0 idle-hook counter (what tdeck::IdleService watches on the device). */
const volatile uint32_t *idle_passes();
/** main.cpp's idle wait timing out: blocked until the next tick. */
void idle_wait_one_tick();

/** BOOT2: the SD mount seam. With ok=false (the default) every mount fails; with ok=true the
 *  mount hands back a card (the read-back test then fails on the host: there is no /sd). */
void sd_reset(bool mount_ok = false);
/** max_freq_khz of every esp_vfs_fat_sdspi_mount call since sd_reset(). */
const std::vector<int> &sd_mount_khz();
/** esp_vfs_fat_sdcard_unmount calls since sd_reset(). */
int sd_unmounts();

} // namespace openu5_host_bus
