#pragma once

#include <cstddef>
#include <cstdint>

#include "inventory_picker.h"
#include "state.h"

namespace openu5 {

// ---------------------------------------------------------------------------
// R-22 -- the (Z)-stats page family, as an ESP-free presentation model.
//
// AUTHORITY.  Every structural fact below is derived from the 1988 binary:
// `ZSTATS.OVL` (overlay #23) and the DATA.OVL DGROUP tables it reads.  The
// derivation lives in re/notes/zstats.md (function map, record layout, the
// equip TYPE/WEIGHT tables) and re/notes/ztats-layout.md (the page axis, the
// per-page field order, the list renderer, the key matrix).  The TypeScript
// port (game/src/skin/fiel/ztats.ts) corroborates but is NOT the authority:
// where it abbreviates or truncates, that is the original's 14-cell panel
// column speaking, not a semantic fact -- see `kZStatsRowChars` below.
//
// This header is presentation ONLY.  Nothing here writes GameState, reads a
// clock or draws an RNG: `cmd_zstats` (ZSTATS.OVL:0x0a3a) consumes exactly
// zero RNG rolls and charges no turn, and so does this model.
// ---------------------------------------------------------------------------

// The page axis, `[bp-2]` in cmd_zstats (ztats-layout.md section 2).  17 slots:
// 12 per-member (6 members x {stats, arms}) then 5 global pages.
constexpr int kZStatsPageProvisions = 0x0c;
constexpr int kZStatsPageReagents = 0x0d;
constexpr int kZStatsPageSpells = 0x0e;
constexpr int kZStatsPageItems = 0x0f;
constexpr int kZStatsPageArmaments = 0x10;
constexpr int kZStatsPageLast = kZStatsPageArmaments;

// render_item_list (0x06e8) stops at cursor row 8 -- the frame's bottom edge --
// so a list page holds SEVEN content rows, and PgUp/PgDn move literally 7
// (`mov [bp-2],7` at 0x081c/0x086c).  ztats-layout.md sections 3 and 4.
constexpr size_t kZStatsListRows = 7;
// Rows a page may publish.  The original's panel is cols 24..39 x rows 1..9;
// the T-Deck's compact selector panel shows the same eight text rows.
constexpr size_t kZStatsPageRows = 8;
// Row width.  The original's list column is 14 cells and its name tables are
// abbreviated to fit it ("Sp. Silk", "Sht. Sword", "In Sanct G").  The T-Deck
// row is wider, so this model names every item through the SAME canonical
// display-name helper the rest of native already uses (display_names.h) and
// lets the unabbreviated name through.  The original's abbreviations are a
// column-width artifact, not a distinct authored fact; the ITEM SET, the
// ORDER, the quantity column and the page family are the authored facts, and
// those are reproduced exactly.
constexpr size_t kZStatsRowChars = 24;
constexpr size_t kZStatsBannerChars = 24;
// Armaments is the widest list: the full 48-entry equipment pack.
constexpr size_t kZStatsMaxListEntries = 48;

enum class ZStatsPageKind : uint8_t { Stats, Arms, Provisions, List };

// One row of a list page.  `quantity_hidden` is print_list_row's 0xff sentinel
// (ZSTATS.OVL:0x062e): the row skips BOTH the number and the '-' separator.
// The reference's writer census proves which quest items are stored 0xff --
// the Lord British regalia, the three Shards, the Spyglass, the HMS Cape
// plans, the Sextant, the Black Badge and the Wooden Box all are; scrolls,
// potions, magic carpets and skull keys are genuinely countable.
struct ZStatsListEntry {
    int32_t index = 0;
    const char *name = nullptr;
    int32_t quantity = 0;
    bool quantity_hidden = false;
};

struct ZStatsList {
    const char *title = "";
    ZStatsListEntry entries[kZStatsMaxListEntries]{};
    size_t count = 0;
};

struct ZStatsRow {
    char text[kZStatsRowChars]{};
};

// A fully composed page: exactly what the frontend paints, with no further
// game-state lookups needed.
struct ZStatsPage {
    ZStatsPageKind kind = ZStatsPageKind::Stats;
    int page = 0;
    int member = -1; // -1 on the five global pages
    char banner[kZStatsBannerChars]{};
    ZStatsRow rows[kZStatsPageRows]{};
    size_t row_count = 0;
    size_t list_total = 0;  // owned entries in this list (0 off a list page)
    size_t list_scroll = 0; // index of the first visible entry
    bool more_above = false, more_below = false;
};

// Everything the model reads.  `moonstones_owned` is a bit per lunar phase
// (bit p == phase p is carried, i.e. NOT buried); it has no GameState field --
// QuestWorldServices owns it -- so the caller supplies it, exactly as
// AlphaRuntime already does for the (U)se picker (inventory_picker.h).
struct ZStatsInput {
    const GameState *game = nullptr;
    uint8_t moonstones_owned = 0;
};

// --- the page axis --------------------------------------------------------
ZStatsPageKind zstats_page_kind(int page);
int zstats_member_of_page(int page);
int zstats_page_for_member(int member);
// Circular, with the four wrap gates of ztats-layout.md section 2.  The ring
// NEVER enters the unused window [party*2, 0x0b].
int zstats_axis_next(int page, int party_size);
int zstats_axis_prev(int page, int party_size);
// Clamps a page onto the ring for the given party size (a roster that shrank
// while the modal was open must not strand the cursor in the dead window).
int zstats_clamp_page(int page, int party_size);

// --- the DATA.OVL label tables (ztats-layout.md section 8.1) --------------
// Class letter from record +0x0a ("AMBFDTPRS", ZSTATS DS 0x9812 / ptr 0x1a44).
const char *zstats_class_name(char code);
// Health letter from record +0x0b (ptr table 0x1a6a).
const char *zstats_status_name(char code);

// --- pages ----------------------------------------------------------------
ZStatsList zstats_list(const ZStatsInput &, int page);
size_t zstats_max_scroll(size_t total);
ZStatsPage compose_zstats_page(const ZStatsInput &, int page, size_t scroll);

} // namespace openu5
