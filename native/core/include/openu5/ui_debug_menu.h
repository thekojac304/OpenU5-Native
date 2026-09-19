#pragma once

#include "debug_developer.h"
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
    QuestProgression,
    Time,
    Transport,
    NpcDungeonState,
    ShortcutsPresets,
    Diagnostics,
    Count
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
    size_t equip_slot_ = 0, quest_flag_ = 0, quest_item_ = 0, npc_location_ = 0,
           npc_index_ = 0, dungeon_slot_ = 0, dungeon_room_ = 0;
    int32_t teleport_x_ = 0, teleport_y_ = 0;
    bool standard_entry_ = true;
    UiDiagnosticsServices diagnostics_{};

    size_t item_count() const;
    const char *category_name(size_t) const;
    const char *item_name() const;
    bool item_edit_range(int64_t &, int64_t &, int64_t &) const;
    void enter_or_apply();
    void apply_value(int64_t);
    void apply_action();
    bool action_requires_confirmation() const;
    DebugDestination destination() const;
    void set(DebugResult r) { last_status_ = r.status; has_result_ = true; }
};

} // namespace openu5
