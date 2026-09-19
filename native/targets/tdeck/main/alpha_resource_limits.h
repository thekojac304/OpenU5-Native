#pragma once
#include <cstddef>
#include <cstdint>

namespace tdeck {

// Maximum TOC entries native/tools/u5pack/alpha1.ts may pack into
// openu5-alpha1-resources.bin. AlphaResourcePack::entries_ (alpha_resources.h)
// is a fixed-size array sized to this bound -- growing it costs
// (new-old)*sizeof(Entry) bytes of static storage in the single
// `static tdeck::AlphaResourcePack alpha_pack` instance (main.cpp),
// permanently resident whether or not a pack is ever opened. sizeof(Entry)
// is 52 bytes (char[32] + 5 x uint32_t, no padding), so each unit of
// headroom here costs 52 bytes.
//
// Batch 4.5B added misc-records.bin, bringing the real pack to 33 entries
// against a stale bound of 32 -- AlphaResourcePack::open() rejected the
// otherwise-valid pack with ESP_ERR_INVALID_SIZE before any entry/CRC
// validation ran. This constant is kept host-portable (no ESP-IDF headers)
// so native/core/tests/alpha_resource_capacity_test.cpp exercises the exact
// predicate production uses, not a reimplemented copy.
constexpr size_t kAlphaResourceMaxEntries = 64;

constexpr bool alpha_resource_entry_count_ok(uint32_t entry_count) {
    return entry_count != 0 && entry_count <= kAlphaResourceMaxEntries;
}

} // namespace tdeck
