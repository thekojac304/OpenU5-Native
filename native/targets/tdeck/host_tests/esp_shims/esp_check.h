#pragma once
// Batch 11 host-test seam. Minimal stand-in for ESP-IDF's esp_check.h.
// Same control-flow contract as the real macro (return the error immediately
// on failure); the logging side effect is dropped since host builds route
// diagnostics through esp_log.h's own shim instead.
#include "esp_err.h"

#define ESP_RETURN_ON_ERROR(x, tag, msg, ...)                                \
    do {                                                                     \
        esp_err_t _openu5_host_err = (x);                                    \
        if (_openu5_host_err != ESP_OK) return _openu5_host_err;             \
    } while (0)
