#include "openu5/inventory_picker.h"
#include "openu5/display_names.h"

namespace openu5 {

UsableItemPickerRows usable_item_picker_rows(const UsableItemPickerInput &in) {
    UsableItemPickerRows out;
    auto add = [&](int32_t id, const char *name, int32_t qty = 1) {
        if (out.count >= kUsableItemPickerMaxRows) return;
        out.rows[out.count++] = {id, name, qty};
    };
    // Transcribed verbatim from AlphaRuntime::open_selection()'s
    // InventorySelection branch (alpha_runtime.cpp), same real-id space and
    // same usable_item_display_name() offset-index bug (id 18 reads slot 2,
    // i.e. "Grapple", not the real id-18 Amulet). Do not "fix" this call --
    // it must keep reproducing current production behavior exactly.
    if (in.magic_carpets) add(16, usable_item_display_name(0), in.magic_carpets);
    if (in.skull_keys) add(17, usable_item_display_name(1), in.skull_keys);
    if (in.grapple) add(18, usable_item_display_name(2));
    if (in.spyglass) add(32, usable_item_display_name(32));
    if (in.sextant) add(34, usable_item_display_name(34));
    if (in.wooden_box) add(37, usable_item_display_name(37));
    return out;
}

} // namespace openu5
