#include "openu5/audio_pack.h"

#include <cstring>

#include "openu5/persistence.h" // save_crc32: the same IEEE CRC-32 the packer writes

// Alpha 3 A3-01. The device-side reader of the audio pack the packer
// (native/tools/u5pack/audio.ts) writes. Format in audio_pack.h.
namespace openu5 {
namespace {

uint16_t le16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint32_t be32(const uint8_t *p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
bool id4(const uint8_t *p, const char *id) { return std::memcmp(p, id, 4) == 0; }

/** One IFF chunk header at `at`, inside [at, end): its body [body, body + length). */
bool chunk(const uint8_t *d, size_t at, size_t end, size_t &body, size_t &length) {
    if (at > end || end - at < 8) return false;
    length = be32(d + at + 4);
    body = at + 8;
    return length <= end - body;
}

unsigned popcount16(uint16_t v) {
    unsigned n = 0;
    for (; v; v = uint16_t(v & (v - 1))) ++n;
    return n;
}

} // namespace

const char *audio_pack_state_name(AudioPackState s) {
    switch (s) {
    case AudioPackState::Missing: return "missing";
    case AudioPackState::Valid: return "valid";
    case AudioPackState::NotAudioPack: return "not-an-audio-pack";
    case AudioPackState::UnsupportedVersion: return "unsupported-version";
    case AudioPackState::Corrupt: return "corrupt";
    case AudioPackState::Inconsistent: return "inconsistent";
    }
    return "invalid";
}

bool validate_xmi(const uint8_t *d, size_t n) {
    if (!d) return false;
    size_t body = 0, length = 0;
    // FORM:XDIR { INFO }
    if (!chunk(d, 0, n, body, length) || !id4(d, "FORM") || length < 4 || !id4(d + body, "XDIR")) return false;
    const size_t xdir_end = body + length;
    size_t info = 0, info_length = 0;
    if (!chunk(d, body + 4, xdir_end, info, info_length) || !id4(d + body + 4, "INFO") || info_length < 2)
        return false;
    if (le16(d + info) != 1) return false; // one sequence, the only shape the driver plays (sequence 0)
    // CAT :XMID { FORM:XMID { [TIMB] EVNT } }
    const size_t cat_at = xdir_end + (length & 1);
    size_t cat = 0, cat_length = 0;
    if (!chunk(d, cat_at, n, cat, cat_length) || !id4(d + cat_at, "CAT ") || cat_length < 4 ||
        !id4(d + cat, "XMID"))
        return false;
    const size_t cat_end = cat + cat_length;
    size_t form = 0, form_length = 0;
    if (!chunk(d, cat + 4, cat_end, form, form_length) || !id4(d + cat + 4, "FORM") || form_length < 4 ||
        !id4(d + form, "XMID"))
        return false;
    const size_t form_end = form + form_length;
    bool events = false;
    for (size_t at = form + 4; at < form_end;) {
        size_t sub = 0, sub_length = 0;
        if (!chunk(d, at, form_end, sub, sub_length)) return false;
        if (id4(d + at, "EVNT")) events = events || sub_length > 0;
        at = sub + sub_length + (sub_length & 1);
    }
    return events;
}

bool validate_timbre_bank(const uint8_t *d, size_t n) {
    constexpr size_t kIndexEntry = 6, kBlock = 14;
    if (!d) return false;
    size_t timbres = 0;
    for (size_t at = 0;; at += kIndexEntry) {
        if (n < 2 || at > n - 2) return false; // no 0xFFFF terminator
        if (le16(d + at) == 0xffff) break;
        if (at > n - kIndexEntry) return false;
        const uint32_t offset = le32(d + at + 2);
        if (offset > n || n - offset < kBlock || le16(d + offset) != kBlock) return false;
        ++timbres;
    }
    return timbres > 0;
}

AudioPackInfo inspect_audio_pack(const uint8_t *d, size_t n) {
    AudioPackInfo info{};
    if (!d || n == 0) return info; // Missing
    info.file_size = uint32_t(n > 0xffffffffu ? 0xffffffffu : n);
    auto fail = [&](AudioPackState s) {
        info.state = s;
        return info;
    };
    if (n < kAudioPackHeaderSize || n > kAudioPackMaxBytes || std::memcmp(d, kAudioPackMagic, 8) != 0)
        return fail(AudioPackState::NotAudioPack);
    info.version_major = le16(d + 8);
    info.version_minor = le16(d + 10);
    // Minor versions only ever add entry kinds, which this reader skips.
    if (info.version_major != kAudioPackVersionMajor) return fail(AudioPackState::UnsupportedVersion);
    if (le16(d + 12) != kAudioPackHeaderSize || le16(d + 14) != kAudioPackEntrySize)
        return fail(AudioPackState::NotAudioPack);
    const uint32_t count = le32(d + 16);
    info.entry_count = count;
    if (count == 0 || count > kAudioPackMaxEntries || le32(d + 20) != n) return fail(AudioPackState::Corrupt);
    const size_t table_end = kAudioPackHeaderSize + size_t(count) * kAudioPackEntrySize;
    if (table_end > n) return fail(AudioPackState::Corrupt);
    if (save::save_crc32(d + kAudioPackHeaderSize, table_end - kAudioPackHeaderSize) != le32(d + 28))
        return fail(AudioPackState::Corrupt);
    info.payload_crc32 = save::save_crc32(d + table_end, n - table_end);
    if (info.payload_crc32 != le32(d + 24)) return fail(AudioPackState::Corrupt);

    const uint8_t *record = nullptr, *bank = nullptr;
    size_t record_length = 0, bank_length = 0;
    const uint8_t *songs[kMusicSongCount]{};
    size_t song_lengths[kMusicSongCount]{};
    uint16_t song_mask = 0;
    bool duplicate = false;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *e = d + kAudioPackHeaderSize + size_t(i) * kAudioPackEntrySize;
        if (std::memchr(e, 0, 24) == nullptr) return fail(AudioPackState::Corrupt);
        const uint32_t kind = le32(e + 24), id = le32(e + 28), offset = le32(e + 32), length = le32(e + 36);
        if (offset < table_end || offset > n || length > n - offset || le32(e + 44) != 0)
            return fail(AudioPackState::Corrupt);
        if (save::save_crc32(d + offset, length) != le32(e + 40)) return fail(AudioPackState::Corrupt);
        switch (AudioPackEntryKind(kind)) {
        case AudioPackEntryKind::Capability:
            duplicate = duplicate || record != nullptr;
            record = d + offset;
            record_length = length;
            break;
        case AudioPackEntryKind::Song:
            if (id >= kMusicSongCount || (song_mask & (1u << id))) {
                duplicate = true;
                break;
            }
            song_mask = uint16_t(song_mask | (1u << id));
            songs[id] = d + offset;
            song_lengths[id] = length;
            break;
        case AudioPackEntryKind::TimbreBank:
            duplicate = duplicate || bank != nullptr;
            bank = d + offset;
            bank_length = length;
            break;
        default:
            break; // a later minor version's entry: not ours to judge
        }
    }
    if (duplicate || !record || record_length < kAudioCapabilityRecordSize)
        return fail(AudioPackState::Inconsistent);

    auto &r = info.record;
    r.record_version = record[0];
    r.capability = MusicCapability(record[1]);
    r.driver = AudioPackDriver(record[2]);
    r.bank = AudioPackBank(record[3]);
    r.songs_present = le16(record + 4);
    r.songs_valid = le16(record + 6);
    r.patch_location = record[8];
    r.data_ovl_marker = record[9];
    r.foreign_xmi = record[10];
    r.detector_version = le32(record + 12);
    info.song_entries = uint8_t(popcount16(song_mask));
    info.bank_entry = bank != nullptr;
    if (r.record_version != kAudioCapabilityRecordVersion || record[1] > 3 || record[2] > 2 || record[3] > 2)
        return fail(AudioPackState::Inconsistent);

    if (r.capability == MusicCapability::SupportedMusicPatch) {
        // Never pretend: the claim stands only on the bytes that would play.
        bool complete = song_mask == 0xffff && bank && r.driver == AudioPackDriver::ExodusUpgrade10 &&
                        r.bank == AudioPackBank::Valid && r.songs_present == 0xffff && r.songs_valid == 0xffff &&
                        validate_timbre_bank(bank, bank_length);
        for (size_t s = 0; complete && s < kMusicSongCount; ++s) complete = validate_xmi(songs[s], song_lengths[s]);
        if (!complete) return fail(AudioPackState::Inconsistent);
    } else if (song_mask != 0 || bank) {
        // Our packer never ships music data it could not vouch for.
        return fail(AudioPackState::Inconsistent);
    }
    info.state = AudioPackState::Valid;
    return info;
}

MusicAvailability music_availability(const AudioPackInfo &info) {
    switch (info.state) {
    case AudioPackState::Missing: return MusicAvailability::NoAudioPack;
    case AudioPackState::Valid:
        switch (info.record.capability) {
        case MusicCapability::SupportedMusicPatch: return MusicAvailability::Available;
        case MusicCapability::StockNoMusic: return MusicAvailability::StockNoMusic;
        case MusicCapability::IncompleteMusicPatch: return MusicAvailability::IncompletePatch;
        case MusicCapability::UnknownMusicVariant: return MusicAvailability::UnknownVariant;
        }
        return MusicAvailability::AudioPackInvalid;
    default: return MusicAvailability::AudioPackInvalid;
    }
}

} // namespace openu5
