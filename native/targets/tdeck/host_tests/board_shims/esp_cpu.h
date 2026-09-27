#pragma once
// Alpha 3 A3-04E host-test seam: esp_cpu.h. The cycle counter is the virtual
// clock of fake_tdeck_bus.cpp at CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, so the Board's
// own cycle timing (A3-04C) measures the modelled transfer and pause times.
#include <cstdint>
typedef uint32_t esp_cpu_cycle_count_t;
esp_cpu_cycle_count_t esp_cpu_get_cycle_count();
