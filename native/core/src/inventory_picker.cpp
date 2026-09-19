#include "openu5/inventory_picker.h"
#include "openu5/display_names.h"

namespace openu5 {

UsableItemPickerRows usable_item_picker_rows(const UsableItemPickerInput &in) {
    UsableItemPickerRows out;
    auto add = [&](int32_t id, int32_t qty = 1) {
        if (out.count >= kUsableItemPickerMaxRows) return;
        out.rows[out.count++] = {id, usable_item_display_name(id), qty};
    };
    // Order and gates transcribed from the reference's buildUseRows()
    // (game/src/core/usePicker.ts): carpet -> skull key -> LB artifacts ->
    // moonstones -> shards -> spyglass -> plans -> sextant -> badge -> box.
    // The reference's pocket-watch row (id 35) has no native backing field and
    // is intentionally absent; the Grapple was never an entry of this table.
    if (in.magic_carpets > 0) add(16, in.magic_carpets);
    if (in.skull_keys > 0) add(17, in.skull_keys);
    if (in.artifacts[0]) add(18);
    if (in.artifacts[1]) add(19);
    if (in.artifacts[2]) add(20);
    for (int phase = 0; phase < 8; ++phase)
        if (in.moonstone_owned[phase]) add(21 + phase);
    if (in.shards[0]) add(29);
    if (in.shards[1]) add(30);
    if (in.shards[2]) add(31);
    if (in.spyglass) add(32);
    if (in.hms_cape) add(33);
    if (in.sextant) add(34);
    if (in.black_badge) add(36);
    if (in.wooden_box) add(37);
    return out;
}

} // namespace openu5
