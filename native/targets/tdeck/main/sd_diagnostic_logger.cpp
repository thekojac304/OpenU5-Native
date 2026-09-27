#include "sd_diagnostic_logger.h"

#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_log_write.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace tdeck::sdlog {
namespace {

// Alpha 3 A3-04D (ALPHA3_AUDIO.md section 21): the switch, the log hook's body
// and the writer's per-wake logic are openu5::SdDiagLog (core, host-tested);
// this file keeps the FreeRTOS queue, task and storage mutex, stdio on FATFS
// and the A3-04C burst timing.
constexpr char kDirectory[] = "/sd/ultima5/logs";
constexpr char kLogPath[] = "/sd/ultima5/logs/alpha20-frontend-debug.log";
constexpr char kRotatedPath[] = "/sd/ultima5/logs/alpha20-frontend-debug.log.1";
constexpr size_t kStdioBufferBytes = 4096;
constexpr UBaseType_t kQueuedLines = 48;
constexpr TickType_t kWriterWakeTicks = pdMS_TO_TICKS(250);

StaticQueue_t g_queue_control{};
uint8_t *g_queue_storage = nullptr;
QueueHandle_t g_queue = nullptr;
FILE *g_file = nullptr;
char g_stdio_buffer[kStdioBufferBytes]{};
vprintf_like_t g_serial_writer = &vprintf;
std::atomic<bool> g_storage_ready{false};
std::atomic<bool> g_writer_started{false};
TaskHandle_t g_writer = nullptr;
StaticSemaphore_t g_storage_mutex_control{};
SemaphoreHandle_t g_storage_mutex=nullptr;
// A3-04C: burst statistics (written by the writer task, read by the game thread).
std::atomic<uint32_t> g_perf_bursts{0}, g_perf_busy_us{0}, g_perf_max_us{0};
std::atomic<bool> g_perf_reset{false};
// A3-04D: 1 while the writer holds the storage mutex (see burst_flag()).
volatile uint32_t g_burst_active = 0;

bool make_directory(const char *path)
{
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

bool open_active_log(const char *mode, size_t &bytes)
{
    g_file = std::fopen(kLogPath, mode);
    if (!g_file) return false;
    if (std::setvbuf(g_file, g_stdio_buffer, _IOFBF, sizeof(g_stdio_buffer)) != 0) {
        std::fclose(g_file);
        g_file = nullptr;
        return false;
    }
    if (std::fseek(g_file, 0, SEEK_END) != 0) {
        std::fclose(g_file);
        g_file = nullptr;
        return false;
    }
    const long end = std::ftell(g_file);
    if (end < 0) {
        std::fclose(g_file);
        g_file = nullptr;
        return false;
    }
    bytes = static_cast<size_t>(end);
    return true;
}

// The log file on the card: stdio (4 KiB buffer) on FATFS on sdspi, SPI2.
class CardLogFile final : public openu5::SdDiagFile {
  public:
    bool open(size_t &bytes) override
    {
        if (!make_directory("/sd/ultima5") || !make_directory(kDirectory)) return false;
        return open_active_log("ab", bytes);
    }
    bool rotate() override
    {
        close();
        unlink(kRotatedPath);
        if (rename(kLogPath, kRotatedPath) != 0 && errno != ENOENT) {
            // A bounded fresh log is more important than retaining an archive when
            // the rename itself fails.
            unlink(kLogPath);
        }
        size_t bytes = 0;
        return open_active_log("wb", bytes);
    }
    bool write(const void *data, size_t size) override
    {
        return g_file && std::fwrite(data, 1, size, g_file) == size;
    }
    bool flush() override { return g_file && std::fflush(g_file) == 0; }
    void close() override
    {
        if (!g_file) return;
        std::fflush(g_file);
        std::fclose(g_file);
        g_file = nullptr;
    }
};

class RtosLineQueue final : public openu5::SdDiagQueue {
  public:
    bool push(const openu5::SdDiagLine &line) override
    {
        return g_queue && xQueueSend(g_queue, &line, 0) == pdTRUE;
    }
    bool pop(openu5::SdDiagLine &line) override
    {
        return g_queue && xQueueReceive(g_queue, &line, 0) == pdTRUE;
    }
};

CardLogFile g_card;
RtosLineQueue g_lines;
openu5::SdDiagLog g_log(g_card, g_lines); // off at boot: openu5::kSdDiagLoggingDefault

uint64_t clock_ms()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000);
}

void writer_task(void *)
{
    openu5::SdDiagLine line{};
    for (;;) {
        if (g_log.idle()) {
            // A3-04D: off (or failed) with the log closed. No timeout, no mutex, no
            // card access -- set_enabled(true) notifies this task.
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        const bool received=xQueueReceive(g_queue,&line,kWriterWakeTicks)==pdTRUE;
        if(g_storage_mutex)xSemaphoreTake(g_storage_mutex,portMAX_DELAY);
        g_burst_active = 1;
        const int64_t burst_start_us=esp_timer_get_time();
        const openu5::SdDiagLog::Wake wake = g_log.wake(received ? &line : nullptr, clock_ms());
        if (wake.burst) {
            // A3-04C: this wake touched the card (or the stdio buffer in front of it).
            if (g_perf_reset.exchange(false)) {
                g_perf_bursts.store(0);
                g_perf_busy_us.store(0);
                g_perf_max_us.store(0);
            }
            const uint32_t us = static_cast<uint32_t>(esp_timer_get_time() - burst_start_us);
            g_perf_bursts.fetch_add(1);
            g_perf_busy_us.fetch_add(us);
            if (us > g_perf_max_us.load()) g_perf_max_us.store(us);
        }
        g_burst_active = 0;
        if(g_storage_mutex)xSemaphoreGive(g_storage_mutex);
        if (wake.failed) ESP_LOGE("Alpha20SdLog", "SD logging write failed; serial logging remains active");
    }
}

int mirror_vprintf(const char *format, va_list arguments)
{
    // Serial always; the card copy only while SD diagnostic logging is on.
    return g_log.mirror(g_serial_writer, &clock_ms, format, arguments);
}

}  // namespace

bool begin_capture()
{
    if (g_queue) return true;
    g_storage_mutex=xSemaphoreCreateMutexStatic(&g_storage_mutex_control);
    if(!g_storage_mutex)return false;
    g_queue_storage = static_cast<uint8_t *>(
        heap_caps_malloc(kQueuedLines * sizeof(openu5::SdDiagLine), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_queue_storage) return false;
    g_queue = xQueueCreateStatic(kQueuedLines, sizeof(openu5::SdDiagLine), g_queue_storage,
                                 &g_queue_control);
    if (!g_queue) {
        heap_caps_free(g_queue_storage);
        g_queue_storage = nullptr;
        return false;
    }
    const vprintf_like_t previous = esp_log_set_vprintf(&mirror_vprintf);
    if (previous) g_serial_writer = previous;
    return previous != nullptr;
}

bool initialize_storage()
{
    if (!g_queue) return false;
    g_storage_ready.store(true, std::memory_order_release);
    return true;
}

bool start_writer()
{
    if (!g_storage_ready.load(std::memory_order_acquire)) return false;
    bool expected = false;
    if (!g_writer_started.compare_exchange_strong(expected, true)) return true;
    if (xTaskCreate(writer_task, "alpha20-sd-log", 4096, nullptr, tskIDLE_PRIORITY,
                    &g_writer) != pdPASS) {
        g_writer_started.store(false, std::memory_order_release);
        g_storage_ready.store(false, std::memory_order_release);
        return false;
    }
    g_log.set_available(true);
    return true;
}

openu5::SdLogState state()
{
    return g_log.state();
}

bool set_enabled(bool on)
{
    if (!g_log.set_enabled(on)) return false;
    if (on && g_writer) xTaskNotifyGive(g_writer);
    return true;
}

const volatile uint32_t *burst_flag()
{
    return &g_burst_active;
}

bool perf_snapshot(openu5::SdLogPerf &out)
{
    out = openu5::SdLogPerf{};
    out.valid = g_writer_started.load(std::memory_order_acquire) && g_storage_ready.load(std::memory_order_acquire) &&
                !g_log.failed();
    out.off = !g_log.enabled();
    if (g_perf_reset.load()) return out.valid; // a reset not yet applied by the writer: an empty window
    out.bursts = g_perf_bursts.load();
    out.busy_us = g_perf_busy_us.load();
    out.max_us = g_perf_max_us.load();
    return out.valid;
}

void perf_reset()
{
    g_perf_reset.store(true);
}

bool begin_storage_transaction()
{
    return g_storage_mutex&&xSemaphoreTake(g_storage_mutex,portMAX_DELAY)==pdTRUE;
}

void end_storage_transaction()
{
    if(g_storage_mutex)xSemaphoreGive(g_storage_mutex);
}

}  // namespace tdeck::sdlog
