#pragma once

#include <cstddef>
#include <cstdint>

#include "audio.h"

// Alpha 3 A3-01 -- the SD audio pack, /ultima5/openu5-audio.bin (OU5AUDIO).
//
// It is SEPARATE from openu5-alpha1-resources.bin on purpose: the game pack is
// locked to one exact size + CRC, and a stock DOS install and a music-patched
// one build a byte-identical game pack (native/core/a3-01-stock-vs-patched-
// extract.log). Recording the music capability inside it would make those two
// valid installs build different game packs, and the exact lock would refuse
// one of them. The audio pack is optional: without it the game runs and
// Settings says why there is no music.
//
// It is built on the user's machine by `npm run pack:audio`
// (native/tools/u5pack/audio.ts) from the user's own files. The repository
// never contains music data; the pack carries the patch's XMI and timbre-bank
// bytes verbatim only when the packer found the supported patch complete.
//
// Layout (little endian), version 1.0:
//   header 32 bytes
//     0  "OU5AUDIO"
//     8  u16 major (1)       10 u16 minor (0)
//    12  u16 header size (32) 14 u16 entry size (48)
//    16  u32 entry count      20 u32 exact file size
//    24  u32 CRC-32 of every byte after the table
//    28  u32 CRC-32 of the table
//   table: entry count x 48 bytes
//     0  name, 24 bytes, NUL padded
//    24  u32 kind (0 capability, 1 song, 2 timbre bank)
//    28  u32 id   (song id 0..15 for songs, else 0)
//    32  u32 offset   36 u32 length   40 u32 CRC-32   44 u32 reserved (0)
//   payloads, back to back in table order.
//   capability record (kind 0, "capability", 32 bytes):
//     0  u8  record version (1)
//     1  u8  MusicCapability
//     2  u8  driver: 0 absent, 1 Exodus U5 Upgrade 1.0 mid.drv, 2 unrecognized
//     3  u8  bank:   0 absent, 1 valid FAT.OPL, 2 invalid
//     4  u16 songs present (bit i = song i's file was found)
//     6  u16 songs valid   (bit i = it is a valid single-sequence XMI)
//     8  u8  where the patch files were: bit0 game dir, bit1 upgrade/ subdir
//     9  u8  DATA.OVL driver slot (fileoff 0x5350): 0 unread, 1 "T1K.DRV", 2 "MID.DRV", 3 other
//    10  u8  foreign *.XMI files (not a song of the known driver; SETM.XMI excluded)
//    11  u8  reserved
//    12  u32 detector version (1)
//    16  16 reserved bytes (0)
namespace openu5 {

constexpr char kAudioPackMagic[8] = {'O', 'U', '5', 'A', 'U', 'D', 'I', 'O'};
constexpr uint16_t kAudioPackVersionMajor = 1, kAudioPackVersionMinor = 0;
constexpr uint16_t kAudioPackHeaderSize = 32, kAudioPackEntrySize = 48;
constexpr uint32_t kAudioPackMaxEntries = 64;
/** The largest pack the device will read (the supported patch is ~56 KB). */
constexpr uint32_t kAudioPackMaxBytes = 512u * 1024u;
constexpr uint32_t kAudioCapabilityRecordSize = 32;
constexpr uint8_t kAudioCapabilityRecordVersion = 1;
constexpr uint32_t kAudioDetectorVersion = 1;

enum class AudioPackEntryKind : uint32_t { Capability = 0, Song = 1, TimbreBank = 2 };
enum class AudioPackDriver : uint8_t { Absent = 0, ExodusUpgrade10 = 1, Unrecognized = 2 };
enum class AudioPackBank : uint8_t { Absent = 0, Valid = 1, Invalid = 2 };

struct AudioCapabilityRecord {
    uint8_t record_version = 0;
    MusicCapability capability = MusicCapability::StockNoMusic;
    AudioPackDriver driver = AudioPackDriver::Absent;
    AudioPackBank bank = AudioPackBank::Absent;
    uint16_t songs_present = 0, songs_valid = 0;
    uint8_t patch_location = 0, data_ovl_marker = 0, foreign_xmi = 0;
    uint32_t detector_version = 0;
};

enum class AudioPackState : uint8_t {
    Missing,            // no file (or an empty one)
    Valid,
    NotAudioPack,       // wrong magic / header geometry / too large
    UnsupportedVersion, // an OU5AUDIO this firmware does not read (stale or newer)
    Corrupt,            // bounds or CRC failure
    Inconsistent        // well formed, but its content does not support its claim
};
const char *audio_pack_state_name(AudioPackState);

struct AudioPackInfo {
    AudioPackState state = AudioPackState::Missing;
    AudioCapabilityRecord record{};
    uint16_t version_major = 0, version_minor = 0;
    uint32_t file_size = 0, payload_crc32 = 0, entry_count = 0;
    uint8_t song_entries = 0;
    bool bank_entry = false;
};

/**
 * Alpha 3 A3-04. Pointers INTO the caller's `data` for the payload a music
 * player needs: filled by inspect_audio_pack only when the pack is Valid and
 * its capability is SupportedMusicPatch (otherwise every field stays null/0
 * -- there is nothing to play, by construction, not by omission). The
 * pointers alias `data` and are only valid as long as the caller keeps it.
 */
struct AudioPackPayload {
    const uint8_t *song[kMusicSongCount]{};
    size_t song_length[kMusicSongCount]{};
    const uint8_t *bank = nullptr;
    size_t bank_length = 0;
};

/**
 * Validate a whole audio pack held in memory. PURE: no I/O, no allocation.
 * A pack that claims the supported patch is Valid only if it really carries
 * all 16 songs (each a structurally valid XMI) and a valid timbre bank.
 * `payload`, if given, is filled with pointers to that song/bank data (see
 * AudioPackPayload) -- the validation walk already finds them, so a caller
 * that wants to actually play the pack does not need a second scan.
 */
AudioPackInfo inspect_audio_pack(const uint8_t *data, size_t size, AudioPackPayload *payload = nullptr);
/** The run-time availability a pack gives; anything not Valid gives no music. */
MusicAvailability music_availability(const AudioPackInfo &);

/**
 * XMI (Miles "eXtended MIDI", IFF) structure as the Exodus patch ships it:
 * FORM:XDIR { INFO(u16 sequences == 1) } CAT :XMID { FORM:XMID { [TIMB] EVNT } },
 * every chunk inside its parent and the file. Structure only; no playback.
 */
bool validate_xmi(const uint8_t *data, size_t size);
/**
 * Miles AIL Global Timbre Library (FAT.OPL): 6-byte index records
 * (u8 patch, u8 bank, u32 offset) ended by u16 0xFFFF, each pointing at a
 * 14-byte two-operator block whose u16 size field is 14. The rules of
 * game/src/ui/opl/bank.ts parseMilesOplBank.
 */
bool validate_timbre_bank(const uint8_t *data, size_t size);

} // namespace openu5
