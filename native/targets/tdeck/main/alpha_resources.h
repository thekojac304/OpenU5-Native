#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "alpha_resource_limits.h"
#include "esp_err.h"
#include "misc_records.h"
#include "openu5/commands.h"
#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include "openu5/shops.h"
#include "openu5/world.h"
#include "openu5/quest_world.h"
#include "openu5/look.h"
#include "openu5/intro_view.h"
#include "openu5/blackthorn_scene.h"

namespace tdeck {

constexpr char kAlphaResourcePath[] = "/sd/ultima5/openu5-alpha1-resources.bin";
constexpr uint16_t kAlphaResourceVersionMajor = 2;
constexpr uint16_t kAlphaResourceVersionMinor = 0;
// Batch 53: + endmsg-records.bin, karma-records.bin, words-of-power.bin
// (Batch 9C..52: 2,039,545 B, CRC 0x2065ad91, SHA-256 434cd664..b4ea).
constexpr uint32_t kExpectedAlphaResourceSize = 2041466;
constexpr uint32_t kExpectedAlphaResourceCrc32 = 0x26f75ae6U;
constexpr char kExpectedAlphaResourceSha256[] =
    "a48abdbfc88eb5ab43453880a8ea029a1ea0684dfec2045f9dae941b31aa379b";

struct CreationSprite {
    uint16_t width = 0, height = 0;
    const uint8_t *pixels = nullptr; // RGB565 little-endian + alpha byte.
};

struct AlphaResourceReport {
    uint16_t version_major = 0;
    uint16_t version_minor = 0;
    uint32_t file_size = 0;
    uint32_t payload_crc32 = 0;
    uint32_t entry_count = 0;
    uint32_t small_map_count = 0;
    uint32_t dungeon_count = 0;
    uint32_t npc_count = 0;
    bool firmware_match = false;
};

// All large buffers are capability-allocated in PSRAM. The pointer tables stay
// in internal RAM because the command engine touches them frequently.
struct AlphaResourceOwners {
    openu5::WorldData world{};
    openu5::MapData *small_maps = nullptr;
    uint8_t *overworld = nullptr;
    uint8_t *underworld = nullptr;
    uint8_t *small_tiles = nullptr;
    openu5::DungeonData *dungeons = nullptr;
    openu5::NpcSlot *npc_slots = nullptr;
    openu5::NpcLocationData npc_locations[32]{};
    uint8_t location_x[40]{}, location_y[40]{};
    size_t location_count = 0;
    int32_t *moon_phases = nullptr;
    size_t moon_phase_count = 0;
    openu5::SearchObject *search_objects = nullptr;
    size_t search_count = 0;
    openu5::ShardSpawn *shard_spawns = nullptr;
    size_t shard_spawn_count = 0;
    openu5::CombatMap *combat_maps = nullptr;
    const openu5::CombatMap **combat_map_views = nullptr;
    uint8_t *combat_sprites = nullptr;
    openu5::CombatEnemy *combat_enemies = nullptr;
    const openu5::CombatEnemy **combat_enemy_views = nullptr;
    int32_t *combat_tables = nullptr;
    size_t combat_map_count = 0, combat_enemy_count = 0, combat_table_count = 0;
    openu5::ShopRecord *shop_records = nullptr;
    int32_t *shop_numbers = nullptr;
    char *shop_names = nullptr;
    size_t shop_record_count = 0, shop_number_count = 0;
    openu5::ShopData shop_data{};
    uint32_t *shop_text_offsets = nullptr;
    char *shop_text_records = nullptr;
    size_t shop_text_record_count = 0;
    // MISCMSG.DAT, borrowed English text: Blackthorn capture/interrogation
    // (blackthorn.cpp records 0-11) and shrine Codex/mantra text (shrine.cpp
    // records 12-44) both resolve through ShrineServices::record here.
    uint32_t *misc_text_offsets = nullptr;
    char *misc_text_records = nullptr;
    size_t misc_text_record_count = 0;
    // Batch 53 (RB-1 / RB-2 / H-190). ENDMSG.DAT (11 records: the ending's
    // dialogue, record 9 the wooden-box line) and KARMA.DAT (6 records, raw
    // and unquoted) in the same layout as misc-records.bin, and the eight
    // DATA.OVL Words of Power widened to UTF-16 for QuestWorldServices::words.
    uint32_t *end_text_offsets = nullptr;
    char *end_text_records = nullptr;
    size_t end_text_record_count = 0;
    uint32_t *karma_text_offsets = nullptr;
    char *karma_text_records = nullptr;
    size_t karma_text_record_count = 0;
    char16_t *word_text = nullptr;
    openu5::TalkText words[8]{};
    size_t word_count = 0;
    uint8_t *dialogue_data = nullptr;
    size_t dialogue_data_size = 0;
    char16_t *shrine_text = nullptr;
    openu5::ShrineData shrine_data{};
    uint32_t *look_offsets = nullptr;
    char *look_text = nullptr;
    size_t look_count = 0;
    openu5::LookSignRecord *signs = nullptr;
    char *sign_text = nullptr;
    uint8_t *sign_raw = nullptr;
    size_t sign_count = 0;
    uint8_t *initial_gam = nullptr;
    size_t initial_gam_size = 0;
    uint8_t *initial_ool = nullptr;
    size_t initial_ool_size = 0;
    char *question_text = nullptr;
    const char *questions[28]{};
    size_t question_count = 0;
    char *intro_text = nullptr;
    const char *intro_scenes[21]{};
    size_t intro_scene_count = 0;
    uint16_t *intro_title = nullptr;
    uint16_t *credits_panel = nullptr;
    uint8_t *creation_sprite_blob = nullptr;
    CreationSprite creation_sprites[11]{};
    uint8_t *demo_scene = nullptr;
    uint8_t *runes_font = nullptr;
    // Blackthorn's private throne room (#324 / R-32): MISCMAPS.DAT record 0 as
    // an 11x11 int16 grid, the stage the capture scene mounts. Borrowed by
    // BlackthornSceneServices::capture_tiles; never mutated (the scene pacer
    // patches its own working copy).
    int16_t *blackthorn_scene_tiles = nullptr;
    openu5::IntroViewData intro_view{};
    size_t psram_bytes = 0;

    void release();
};

class AlphaResourcePack {
  public:
    ~AlphaResourcePack();
    AlphaResourcePack(const AlphaResourcePack &) = delete;
    AlphaResourcePack &operator=(const AlphaResourcePack &) = delete;
    AlphaResourcePack() = default;

    esp_err_t open(const char *path, AlphaResourceReport &);
    esp_err_t load(AlphaResourceOwners &, AlphaResourceReport &);
    void close();
    bool is_open() const { return file_ != nullptr; }

    /**
     * Byte length of a named TOC entry, or 0 when the pack does not carry it.
     * Batch 9C: the dungeon art cache sizes its own PSRAM allocations from this
     * rather than from a constant, so a pack whose art grows cannot overrun a
     * buffer sized by an older firmware.
     */
    uint32_t entry_length(const char *name) const;

    /**
     * Copy a whole named entry into `out`. `capacity` must be at least
     * `entry_length(name)`; the entry's own CRC32 was already verified by
     * open(), so a successful read here is authored data, not a guess.
     */
    esp_err_t read_entry(const char *name, void *out, size_t capacity) const;

  private:
    struct Entry {
        char name[32]{};
        uint32_t offset = 0, length = 0, crc32 = 0, records = 0, stride = 0;
    };
    static constexpr size_t kMaxEntries = kAlphaResourceMaxEntries;
    FILE *file_ = nullptr;
    Entry entries_[kMaxEntries]{};
    size_t entry_count_ = 0;

    const Entry *find(const char *) const;
    esp_err_t read(const Entry &, size_t, void *, size_t) const;
    // Batch 53: one reader for the misc-records.bin string-record layout.
    // `expected` != 0 pins the record count the original file has.
    esp_err_t read_text_records(const char *name, uint32_t expected, uint32_t *&offsets, char *&text,
                                size_t &count, size_t &text_bytes) const;
};

} // namespace tdeck
