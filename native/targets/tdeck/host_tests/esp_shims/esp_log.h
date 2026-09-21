#pragma once
// Batch 11 host-test seam. Minimal stand-in for ESP-IDF's esp_log.h.
//
// Every ESP_LOGx call in alpha_runtime.cpp is a diagnostic trace: it never
// feeds back into a decision or a gameplay-state mutation (verified while
// tracing AlphaRuntime::command()/dispatch()/consume_event() for Batch 11).
// Routing them to stdout here, instead of the real UART/USB log sink, is a
// harmless-sink substitution for a device-only effect -- production still
// gets the real ESP-IDF header and real log output; only host tests see this
// file.
#include <cstdio>

#define ESP_LOG_NONE 0
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2
#define ESP_LOG_INFO 3
#define ESP_LOG_DEBUG 4
#define ESP_LOG_VERBOSE 5

#define OPENU5_HOST_LOG(level, tag, fmt, ...) \
    std::printf("[%s][%s] " fmt "\n", level, tag __VA_OPT__(,) __VA_ARGS__)

#define ESP_LOGE(tag, fmt, ...) OPENU5_HOST_LOG("E", tag, fmt __VA_OPT__(,) __VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) OPENU5_HOST_LOG("W", tag, fmt __VA_OPT__(,) __VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) OPENU5_HOST_LOG("I", tag, fmt __VA_OPT__(,) __VA_ARGS__)
// D/V are compiled out on host: production leaves these at INFO+ in release
// builds too, and the volume they'd add to test output has no test value.
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)
