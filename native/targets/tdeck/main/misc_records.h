#pragma once
#include <cstddef>
#include <cstdint>

namespace tdeck {

// Layout produced by native/tools/u5pack/alpha1.ts's encodeStringRecords():
// a little-endian u32 record count, then (count+1) little-endian u32 byte
// offsets into a blob of NUL-terminated UTF-8 records concatenated in
// order. shop-records.bin and misc-records.bin (MISCMSG.DAT) share this
// exact layout. No ESP-IDF dependency: AlphaResourcePack (ESP-only, reads
// the packed file into PSRAM) and native/core/tests/batch45b_test.cpp
// (host-only, reads the same bytes from a test buffer) both call this.
struct MiscTextRecords {
    const uint32_t *offsets = nullptr;
    const char *text = nullptr;
    size_t count = 0;
};

// Bounds-checked record lookup. Matches ShrineServices::record's contract:
// nullptr for an out-of-range index, otherwise a NUL-terminated C string.
inline const char *misc_text_record(const MiscTextRecords &records, int32_t index) {
    return index >= 0 && size_t(index) < records.count ? records.text + records.offsets[index] : nullptr;
}

// Validates a decoded [count, offsets[0..count], text] triple: offsets
// strictly increasing, all in range, and every record NUL-terminated.
// Mirrors the checks AlphaResourcePack::load() already applies inline to
// shop-records.bin. Returns false (never dereferences out of range) if the
// buffer is malformed.
inline bool validate_misc_text_records(const uint32_t *offsets, const char *text, size_t text_length,
                                        uint32_t count) {
    if (!offsets || (!text && text_length) || offsets[count] != text_length) return false;
    for (uint32_t i = 0; i < count; ++i) {
        if (offsets[i] >= offsets[i + 1] || offsets[i + 1] > text_length || text[offsets[i + 1] - 1] != '\0')
            return false;
    }
    return true;
}

} // namespace tdeck
