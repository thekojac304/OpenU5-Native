#pragma once
#include "esp_log.h"
#include "esp_timer.h"
// Temporary diagnostic build: all reproduction cycles are logged.
#define INPUT_TRACE(...) ESP_LOGI("M51", __VA_ARGS__)
