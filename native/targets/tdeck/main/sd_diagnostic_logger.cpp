#include "sd_diagnostic_logger.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
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

constexpr char kDirectory[] = "/sd/ultima5/logs";
constexpr char kLogPath[] = "/sd/ultima5/logs/alpha20-frontend-debug.log";
constexpr char kRotatedPath[] = "/sd/ultima5/logs/alpha20-frontend-debug.log.1";
constexpr size_t kMaximumBytes = 512 * 1024;
constexpr size_t kStdioBufferBytes = 4096;
constexpr size_t kMaximumLineBytes = 768;
constexpr UBaseType_t kQueuedLines = 48;
constexpr TickType_t kWriterWakeTicks = pdMS_TO_TICKS(250);
constexpr uint64_t kFlushPeriodMs = 2000;

struct LogRecord {
    uint64_t timestamp_ms;
    uint16_t length;
    char text[kMaximumLineBytes];
};

StaticQueue_t g_queue_control{};
uint8_t *g_queue_storage = nullptr;
QueueHandle_t g_queue = nullptr;
FILE *g_file = nullptr;
char g_stdio_buffer[kStdioBufferBytes]{};
size_t g_file_bytes = 0;
vprintf_like_t g_serial_writer = &vprintf;
std::atomic<uint32_t> g_dropped_lines{0};
std::atomic<bool> g_capture_enabled{false};
std::atomic<bool> g_storage_ready{false};
std::atomic<bool> g_writer_started{false};
StaticSemaphore_t g_storage_mutex_control{};
SemaphoreHandle_t g_storage_mutex=nullptr;

bool make_directory(const char *path)
{
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

bool open_active_log(const char *mode)
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
    g_file_bytes = static_cast<size_t>(end);
    return true;
}

bool rotate_log()
{
    if (g_file) {
        std::fflush(g_file);
        std::fclose(g_file);
        g_file = nullptr;
    }
    unlink(kRotatedPath);
    if (rename(kLogPath, kRotatedPath) != 0 && errno != ENOENT) {
        // A bounded fresh log is more important than retaining an archive when
        // the rename itself fails.
        unlink(kLogPath);
    }
    return open_active_log("wb");
}

bool important_event(const char *text)
{
    constexpr const char *needles[] = {
        "IDENTITY ", "RESOURCE_MISMATCH", "Required hardware", "INPUT_EDGE",
        "INPUT_HOLD", "INPUT_RESYNC", "DEBUG_OPEN", "DEBUG_ACTION", "TELEPORT",
        "ENEMY_ID", "save generation=", "load generation=", "failed", "FAILED",
        "FRONTEND_STATE", "FRONTEND_RENDER", "CHAR_NAME", "CHAR_GENDER",
        "CHAR_CREATE intent=", "KEYBOARD_ERROR", "KEYBOARD_RECOVER", "KEYBOARD_METRICS",
        "INPUT_SERVICE_GAP", "FRONTEND_INTENT", "DEVELOPER_ENTRY", "NEWGAME_SAVE",
        " E (", " W (",
    };
    for (const char *needle : needles) {
        if (std::strstr(text, needle)) return true;
    }
    return false;
}

bool write_bytes(const void *data, size_t size)
{
    if (!size) return true;
    if (std::fwrite(data, 1, size, g_file) != size) return false;
    g_file_bytes += size;
    return true;
}

bool ensure_capacity(size_t upcoming)
{
    return g_file_bytes + upcoming <= kMaximumBytes || rotate_log();
}

bool write_record(const LogRecord &record)
{
    // Prefix every physical output line, including multi-line ESP log calls.
    size_t offset = 0;
    do {
        size_t end = offset;
        while (end < record.length && record.text[end] != '\n') ++end;
        const bool had_newline = end < record.length;
        char prefix[32]{};
        const int prefix_length = std::snprintf(prefix, sizeof(prefix), "[%010llu] ",
                                                static_cast<unsigned long long>(record.timestamp_ms));
        const size_t body_length = end - offset;
        const size_t total = static_cast<size_t>(prefix_length) + body_length + 1;
        if (!ensure_capacity(total) ||
            !write_bytes(prefix, static_cast<size_t>(prefix_length)) ||
            !write_bytes(record.text + offset, body_length) || !write_bytes("\n", 1)) {
            return false;
        }
        offset = had_newline ? end + 1 : record.length;
    } while (offset < record.length);
    return true;
}

bool write_drop_notice(uint32_t dropped, uint64_t timestamp_ms)
{
    char message[96]{};
    const int length = std::snprintf(message, sizeof(message),
                                     "SD_LOG_QUEUE_DROPPED count=%lu",
                                     static_cast<unsigned long>(dropped));
    LogRecord record{timestamp_ms, static_cast<uint16_t>(std::max(0, length)), {}};
    std::memcpy(record.text, message,
                std::min<size_t>(record.length, sizeof(record.text) - 1));
    record.length = static_cast<uint16_t>(std::min<size_t>(record.length, sizeof(record.text) - 1));
    return write_record(record);
}

void disable_storage()
{
    g_capture_enabled.store(false, std::memory_order_release);
    g_storage_ready.store(false, std::memory_order_release);
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

void writer_task(void *)
{
    uint64_t last_flush_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    LogRecord record{};
    while (g_storage_ready.load(std::memory_order_acquire)) {
        bool flush_now = false;
        const bool received=xQueueReceive(g_queue,&record,kWriterWakeTicks)==pdTRUE;
        if(g_storage_mutex)xSemaphoreTake(g_storage_mutex,portMAX_DELAY);
        if (received) {
            do {
                if (!write_record(record)) {
                    disable_storage();
                    if(g_storage_mutex)xSemaphoreGive(g_storage_mutex);
                    ESP_LOGE("Alpha20SdLog", "SD logging write failed; serial logging remains active");
                    vTaskDelete(nullptr);
                    return;
                }
                flush_now = flush_now || important_event(record.text);
            } while (xQueueReceive(g_queue, &record, 0) == pdTRUE);
        }
        const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000);
        const uint32_t dropped = g_dropped_lines.exchange(0, std::memory_order_acq_rel);
        if (dropped && !write_drop_notice(dropped, now_ms)) {
            disable_storage();
            if(g_storage_mutex)xSemaphoreGive(g_storage_mutex);
            ESP_LOGE("Alpha20SdLog", "SD logging write failed; serial logging remains active");
            vTaskDelete(nullptr);
            return;
        }
        flush_now = flush_now || dropped || now_ms - last_flush_ms >= kFlushPeriodMs;
        if (flush_now) {
            if (std::fflush(g_file) != 0) {
                disable_storage();
                if(g_storage_mutex)xSemaphoreGive(g_storage_mutex);
                ESP_LOGE("Alpha20SdLog", "SD logging flush failed; serial logging remains active");
                vTaskDelete(nullptr);
                return;
            }
            last_flush_ms = now_ms;
        }
        if(g_storage_mutex)xSemaphoreGive(g_storage_mutex);
    }
    vTaskDelete(nullptr);
}

int mirror_vprintf(const char *format, va_list arguments)
{
    va_list serial_arguments;
    va_copy(serial_arguments, arguments);
    const int serial_result = g_serial_writer(format, serial_arguments);
    va_end(serial_arguments);

    if (!g_queue || !g_capture_enabled.load(std::memory_order_acquire)) return serial_result;
    LogRecord record{};
    record.timestamp_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    va_list file_arguments;
    va_copy(file_arguments, arguments);
    const int formatted = std::vsnprintf(record.text, sizeof(record.text), format, file_arguments);
    va_end(file_arguments);
    if (formatted <= 0) return serial_result;
    record.length = static_cast<uint16_t>(
        std::min<size_t>(static_cast<size_t>(formatted), sizeof(record.text) - 1));
    if (xQueueSend(g_queue, &record, 0) != pdTRUE) {
        g_dropped_lines.fetch_add(1, std::memory_order_relaxed);
    }
    return serial_result;
}

}  // namespace

bool begin_capture()
{
    if (g_queue) return true;
    g_storage_mutex=xSemaphoreCreateMutexStatic(&g_storage_mutex_control);
    if(!g_storage_mutex)return false;
    g_queue_storage = static_cast<uint8_t *>(
        heap_caps_malloc(kQueuedLines * sizeof(LogRecord), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_queue_storage) return false;
    g_queue = xQueueCreateStatic(kQueuedLines, sizeof(LogRecord), g_queue_storage,
                                 &g_queue_control);
    if (!g_queue) {
        heap_caps_free(g_queue_storage);
        g_queue_storage = nullptr;
        return false;
    }
    g_capture_enabled.store(true, std::memory_order_release);
    const vprintf_like_t previous = esp_log_set_vprintf(&mirror_vprintf);
    if (previous) g_serial_writer = previous;
    else g_capture_enabled.store(false, std::memory_order_release);
    return previous != nullptr;
}

bool initialize_storage()
{
    if (!g_queue || !make_directory("/sd/ultima5") || !make_directory(kDirectory)) {
        g_capture_enabled.store(false, std::memory_order_release);
        return false;
    }
    struct stat status {};
    if (stat(kLogPath, &status) == 0 && static_cast<size_t>(status.st_size) >= kMaximumBytes) {
        if (!rotate_log()) {
            g_capture_enabled.store(false, std::memory_order_release);
            return false;
        }
    } else if (!open_active_log("ab")) {
        g_capture_enabled.store(false, std::memory_order_release);
        return false;
    }
    g_storage_ready.store(true, std::memory_order_release);
    return true;
}

bool start_writer()
{
    if (!g_storage_ready.load(std::memory_order_acquire)) return false;
    bool expected = false;
    if (!g_writer_started.compare_exchange_strong(expected, true)) return true;
    if (xTaskCreate(writer_task, "alpha20-sd-log", 4096, nullptr, tskIDLE_PRIORITY,
                    nullptr) != pdPASS) {
        g_writer_started.store(false, std::memory_order_release);
        disable_storage();
        return false;
    }
    return true;
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
