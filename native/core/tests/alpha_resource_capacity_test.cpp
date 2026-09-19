// Batch 4.5B correction -- alpha resource pack entry-capacity regression.
//
// Root cause: native/tools/u5pack/alpha1.ts's Batch 4.5B misc-records.bin
// addition brought the packed openu5-alpha1-resources.bin to 33 TOC
// entries, but AlphaResourcePack::entries_ (native/targets/tdeck/main/
// alpha_resources.h) was still sized to a stale kMaxEntries=32 bound, so
// AlphaResourcePack::open() rejected the otherwise-valid pack with
// ESP_ERR_INVALID_SIZE before any entry/CRC validation ran -- a real
// hardware startup failure, "Alpha resource pack unavailable ...
// ESP_ERR_INVALID_SIZE".
//
// AlphaResourcePack::open() itself cannot be host-compiled (it depends on
// esp_log.h/esp_heap_caps.h -- the same ESP-IDF constraint documented in
// native/targets/tdeck/host_tests/ui_mode_test.cpp for AlphaRuntime, and
// already worked around the same way for record lookups in misc_records.h
// / native/core/tests/batch45b_test.cpp). The entry-count bound check was
// pulled out into the host-portable alpha_resource_limits.h and
// open() now calls it directly, so this test exercises the exact predicate
// production uses -- not a reimplemented copy -- and is governed by the
// same kAlphaResourceMaxEntries constant open() is.
#include "alpha_resource_limits.h"

#include <cstdio>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *what) {
    ++checks;
    std::fprintf(stderr, "[%s] alpha_resource_capacity check %d: %s\n", ok ? "PASS" : "FAIL", checks, what);
    if (!ok) ++failures;
}
} // namespace

int main() {
    check(tdeck::alpha_resource_entry_count_ok(1), "a 1-entry pack is accepted");
    check(tdeck::alpha_resource_entry_count_ok(32), "a 32-entry pack (the old bound) is still accepted");
    check(tdeck::alpha_resource_entry_count_ok(33),
          "the real current 4.5B pack has 33 entries (misc-records.bin is the 33rd) and must be "
          "accepted by the same predicate AlphaResourcePack::open() calls -- fails against a stale "
          "32-entry bound, exactly reproducing the real hardware ESP_ERR_INVALID_SIZE startup failure");
    check(!tdeck::alpha_resource_entry_count_ok(0),
          "a zero-entry header is still rejected -- capacity was raised, not removed");
    check(!tdeck::alpha_resource_entry_count_ok(1000),
          "an absurdly large entry count is still rejected -- bounds were widened, not eliminated");

    std::fprintf(stderr, "alpha_resource_capacity: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
