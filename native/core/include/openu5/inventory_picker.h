#pragma once
#include <cstddef>
#include <cstdint>

namespace openu5 {

// Narrow, ESP-free view of the possession state the Use-item picker's
// usable-item rows (real ids 16-37, excluding the potion/scroll ranges
// handled separately by AlphaRuntime::open_selection()) currently read.
// Deliberately NOT the full GameState -- only the booleans/counts the
// InventorySelection branch actually consults today, plus (for test-time
// visibility only -- see GAMEPLAY_INTEGRATION_AUDIT.md R-07/R-08 and the
// Batch 3 report) the 8 per-phase moonstone possession flags mirrored from
// QuestWorldServices::moonstones. The extracted row-selection function below
// does NOT read the moonstone flags -- moonstones are not part of today's
// picker, and this is a pure extraction, not a fix -- they are carried in
// this struct only so a host test can assert their absence is a real (not a
// coverage-gap) finding.
struct UsableItemPickerInput {
    int32_t magic_carpets = 0;
    int32_t skull_keys = 0;
    bool grapple = false;
    bool spyglass = false;
    bool sextant = false;
    bool wooden_box = false;
    // moonstone_owned[phase] == true means "not buried" (possessed), for the
    // 8 moonstones (real ids 21-28, phase = id-21). See
    // QuestWorldServices::moonstones / Moonstone::buried
    // (native/core/include/openu5/quest_world.h,
    // native/core/include/openu5/transitions.h). NOT read by
    // usable_item_picker_rows() today; see the struct comment above.
    bool moonstone_owned[8] = {};
};

struct UsableItemPickerRow {
    int32_t id = 0;
    const char *name = nullptr;
    int32_t quantity = 1;
};

constexpr size_t kUsableItemPickerMaxRows = 6;

struct UsableItemPickerRows {
    UsableItemPickerRow rows[kUsableItemPickerMaxRows]{};
    size_t count = 0;
};

// Reproduces AlphaRuntime::open_selection()'s InventorySelection branch's
// usable-item row selection EXACTLY as it stands today (same real-id space,
// same offset-index bug feeding usable_item_display_name(), same omissions)
// -- this is a pure extraction of existing logic, not a fix. Both
// AlphaRuntime::open_selection() (native/targets/tdeck/main/alpha_runtime.cpp)
// and the host Batch 3 Group B test call this single definition. See
// GAMEPLAY_INTEGRATION_AUDIT.md R-07/R-08.
UsableItemPickerRows usable_item_picker_rows(const UsableItemPickerInput &);

} // namespace openu5
