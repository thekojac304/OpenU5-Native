#pragma once

#include "debug_developer.h"
#include "debug_labels.h"
#include "debug_map_picker.h"
#include "ui_session.h"

namespace openu5 {

// This header is usable only by developer-tool builds.  The implementation is
// not present in the release library.
enum class UiDebugCategory : uint8_t {
    Teleport,
    Party,
    Stats,
    Inventory,
    Equipment,
    Reagents,
    QuestItems,
    SpecialItems,
    QuestWorld,
    Time,
    Transport,
    NpcDungeonState,
    ShortcutsPresets,
    Diagnostics,
    Count
};
static_assert(debug_root_category_count() == size_t(UiDebugCategory::Count),
              "debug_labels root category table must match UiDebugCategory::Count 1:1");

// Describes one visible debug-menu row's current state independently of
// cursor position (Batch 4.5A-2 Part 3): every row can report its own label
// and value without the caller having to move the cursor there first.
struct DebugRowValue {
    enum class Kind : uint8_t {
        None,        // pure action row (e.g. "Teleport now"); nothing to show
        Integer,     // plain numeric state
        Boolean,     // value != 0 renders as Yes/No
        Text,        // text is the whole story (e.g. a name)
        TextWithId,  // text plus its canonical_id (e.g. "Shard of Falsehood [29]")
        Unsupported  // presentation parity with DebugStatus::Unsupported
    };

    Kind kind = Kind::None;
    int64_t value = 0;
    const char *text = nullptr;
    int32_t canonical_id = -1;
};

struct UiDiagnosticsServices {
    void *context = nullptr;
    void (*start)(void *, int group) = nullptr; // -1 runs every group.
};

struct UiDebugMenuView {
    bool open = false, editing = false, editable = false, confirming = false, has_result = false;
    int16_t category = -1;
    size_t cursor = 0, count = 0;
    const char *title = nullptr, *item = nullptr;
    const char *confirmation = nullptr;
    int64_t value = 0, minimum = 0, maximum = 0;
    DebugStatus last_status = DebugStatus::Applied;
    DebugTeleportStatus last_teleport_status = DebugTeleportStatus::Applied;
    DebugTeleportRequest teleport_request{};
    // Carries the authoritative DebugTeleportResult passability metadata for
    // the last teleport, so device presentation can distinguish "Applied
    // (walkable)" from "Applied (impassable)" -- last_teleport_status alone
    // cannot, since an explicit manual impassable coordinate still reports
    // Applied (see Part 3/T4/T5).
    bool teleport_passability_known = false;
    bool teleport_passable = false;
};

class UiDebugMenu {
  public:
    explicit UiDebugMenu(CommandContext &context) : context_(context) {}
    void attach_diagnostics(UiDiagnosticsServices services) { diagnostics_ = services; }
    void open();
    void close();
    bool is_open() const { return open_; }
    bool handle_input(const UiAction &);
    UiDebugMenuView view() const;

    // Batch 4.5A-2 Part 3: row_label()/row_value() describe row `index`
    // independently of cursor_, so every visible row -- not just the
    // currently selected one -- can be presented with its real label and
    // current state. At the root menu (no category entered) every row is a
    // category name and row_value() is always Kind::None.
    const char *row_label(size_t index) const;
    DebugRowValue row_value(size_t index) const;

  private:
    CommandContext &context_;
    bool open_ = false, editing_ = false, edit_typed_ = false, confirming_ = false;
    int16_t category_ = -1;
    size_t cursor_ = 0;
    int64_t edit_value_ = 0, edit_min_ = 0, edit_max_ = 0;
    DebugStatus last_status_ = DebugStatus::Applied;
    DebugTeleportStatus last_teleport_status_ = DebugTeleportStatus::Applied;
    DebugTeleportRequest last_teleport_request_{};
    bool last_teleport_passability_known_ = false, last_teleport_passable_ = false;
    bool has_result_ = false;
    size_t destination_ = 0, floor_ = 0, member_ = 0, inventory_index_ = 0;
    size_t equip_slot_ = 0, quest_flag_ = 0, npc_location_ = 0,
           npc_index_ = 0, dungeon_slot_ = 0, dungeon_room_ = 0;
    int32_t teleport_x_ = 0, teleport_y_ = 0;
    bool standard_entry_ = true;
    UiDiagnosticsServices diagnostics_{};
    // Scratch formatting storage for row_value() text that must be composed
    // rather than borrowed verbatim (floor labels, character-name fallback).
    // Safe because each is consumed by the caller immediately after the
    // row_value() call that filled it, exactly like DeviceDebugScreen's own
    // per-call formatting buffers.
    mutable char floor_label_buf_[20]{};
    mutable char character_label_buf_[20]{};
    mutable char quest_item_label_buf_[32]{};
    mutable char special_item_label_buf_[32]{};

    size_t item_count() const;
    const char *category_name(size_t) const;
    bool item_edit_range(size_t row, int64_t &, int64_t &, int64_t &) const;
    DebugRowValue character_row_value(size_t member) const;
    // Direct possession reads shared by row_value() (presentation) and
    // apply_action() (the Confirm-toggles-it mutation) so the two can never
    // drift (Batch 4.5A-3 Part 3/4): these categories fire on a single Confirm
    // rather than the numeric edit-dialog two-step, so item_edit_range() is
    // deliberately never true for their rows.
    bool quest_item_value(size_t index) const;
    bool special_item_value(size_t index) const;
    void enter_or_apply();
    void apply_value(int64_t);
    void apply_action();
    bool action_requires_confirmation() const;
    DebugDestination destination() const;
    void set(DebugResult r) { last_status_ = r.status; has_result_ = true; }
};

} // namespace openu5
