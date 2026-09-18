#pragma once

#include <cstddef>
#include <cstdint>

namespace openu5 {

// Presentation names derived from the authoritative DATA.OVL resources used by
// the TypeScript reference.  These helpers deliberately return nullptr for an
// out-of-domain id: callers may then log an unresolved-name defect instead of
// silently displaying an internal numeric identifier.
const char *equipment_display_name(int32_t id);
const char *reagent_display_name(int32_t id);
const char *spell_display_name(int32_t id);
const char *potion_display_name(int32_t id);
const char *scroll_display_name(int32_t id);
const char *usable_item_display_name(int32_t id);
const char *guild_item_display_name(int32_t id);
const char *ship_service_display_name(int32_t id);
const char *wine_display_name(int32_t id);

bool is_generic_identifier_label(const char *text);

} // namespace openu5
