#pragma once
#include <cstddef>
#include <cstdint>

namespace openu5 {

// Narrow, ESP-free view of the possession state the Use-item picker's
// usable-item rows read (the real canonical ids 16-37 of the ZSTATS extended
// item table 0xB9EE; the scroll/potion ranges 0-15 are named and gated
// separately by AlphaRuntime::open_selection()).  Deliberately NOT the full
// GameState/QuestWorldServices -- only the possession gates the reference's
// buildUseRows() consults (game/src/core/usePicker.ts).  Each field names its
// authoritative owner so the seam cannot drift from it.
struct UsableItemPickerInput {
    int32_t magic_carpets = 0;  // GameState::magic_carpets      -> id 16
    int32_t skull_keys = 0;     // GameState::skull_keys         -> id 17
    // GameState::grapple.  Carried so callers can pass the whole possession
    // picture without a second struct, but deliberately NOT read: the Grapple
    // is not an entry of the (U)se table at all.  It is a Klimb-only item
    // (commands.cpp's Klimb handler / dungeon.cpp's pit logic consume it), and
    // the id-18 slot belongs to the Amulet of Lord British.  See R-07.
    bool grapple = false;
    bool spyglass = false;      // GameState::spyglass           -> id 32
    bool sextant = false;       // GameState::sextant            -> id 34
    bool wooden_box = false;    // GameState::wooden_box         -> id 37
    // QuestState::artifacts[0..2] (amulet/crown/sceptre) -> ids 18/19/20.
    bool artifacts[3] = {};
    // QuestState::shards[0..2] (falsehood/hatred/cowardice) -> ids 29/30/31.
    bool shards[3] = {};
    // GameState::hms_cape -> id 33.  This IS the possession flag: quest_world.cpp's
    // apply_search_grant(id==4, quality==255) sets it on (G)et, and the reference's
    // useHmsCape() confirms the Use-time write is a no-op because the pickup already
    // wrote 0xFF.  There is no separate "rigged" ownership bit.
    bool hms_cape = false;
    // GameState::black_badge -> id 36.  Possession only; GameState/TurnState's
    // time_spell is the separate Use-time effect, not an ownership gate.
    bool black_badge = false;
    // moonstone_owned[phase] == true means "not buried" (carried), for the 8
    // moonstones (real ids 21-28, phase = id-21).  Mirrored from
    // QuestWorldServices::moonstones / Moonstone::buried
    // (native/core/include/openu5/quest_world.h, .../transitions.h), which is
    // the authoritative owner -- there is no scalar GameState field for them.
    // The reference gates the same way: buildUseRows() pushes a row per
    // `!m.buried` moonstone.  Reading current ownership here is independent of
    // R-14 (moonstone persistence across save/load), which stays open.
    bool moonstone_owned[8] = {};
};

struct UsableItemPickerRow {
    int32_t id = 0;
    const char *name = nullptr;
    int32_t quantity = 1;
};

// 16, 17, 18, 19, 20, 21-28, 29, 30, 31, 32, 33, 34, 36, 37 = 21 rows.  Id 35
// (Pocket Watch) is excluded: no field anywhere backs it, so no row can be
// gated on possessing it.  Grapple is excluded: Klimb-only, never a Use row.
constexpr size_t kUsableItemPickerMaxRows = 21;

struct UsableItemPickerRows {
    UsableItemPickerRow rows[kUsableItemPickerMaxRows]{};
    size_t count = 0;
};

// Builds AlphaRuntime::open_selection()'s InventorySelection usable-item rows.
// Row order follows the reference's extended-item table
// (game/src/core/usePicker.ts::buildUseRows): carpet, skull key, amulet, crown,
// sceptre, moonstones, shards, spyglass, plans, sextant, badge, box.  Every row
// carries the REAL canonical id, which is exactly the id later dispatched as
// CommandKind::UseItem, and names it through usable_item_display_name() at that
// same real id -- no offset interpretation.  Both AlphaRuntime::open_selection()
// (native/targets/tdeck/main/alpha_runtime.cpp) and the host Batch 3 Group B
// test call this single definition.  See GAMEPLAY_INTEGRATION_AUDIT.md R-07/R-08.
UsableItemPickerRows usable_item_picker_rows(const UsableItemPickerInput &);

} // namespace openu5
