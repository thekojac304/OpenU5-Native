#pragma once
// Batch 11 host-test seam. Minimal stand-in for ESP-IDF's esp_attr.h.
// These are link-placement attributes (which RAM segment survives a
// reset/sleep); host tests have no such segments and no gameplay logic
// depends on retention across a "reset" that never happens in a test
// process, so they compile to nothing.
#define IRAM_ATTR
#define DRAM_ATTR
#define RTC_NOINIT_ATTR
#define RTC_DATA_ATTR
#define EXT_RAM_ATTR
#define IRAM_ATTR_STARTUP
