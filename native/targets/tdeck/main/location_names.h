#pragma once

#include <cstdint>

namespace tdeck {

// Presentation names for the 32 local maps and eight dungeons.  These mirror
// the established display names in game/src/core/location-display.ts; they do
// not participate in map selection or gameplay rules.
inline const char *location_display_name(uint8_t location) {
    static constexpr const char *names[] = {
        nullptr,
        "Moonglow", "Britain", "Jhelom", "Yew", "Minoc", "Trinsic",
        "Skara Brae", "New Magincia", "Fogsbane", "Stormcrow", "Greyhaven",
        "Waveguide", "Iolo's Hut", "Suteks Hut", "SinVraals Hut",
        "Grendels Hut", "Lord British's Castle", "Palace of Blackthorn",
        "West Britanny", "North Britanny", "East Britanny", "Paws", "Cove",
        "Buccaneer's Den", "Ararat", "Bordermarch", "Farthing", "Windemere",
        "Stonegate", "The Lycaeum", "Empath Abbey", "Serpent's Hold",
        "Deceit", "Despise", "Destard", "Wrong", "Covetous", "Shame",
        "Hythloth", "Doom",
    };
    return location < sizeof(names) / sizeof(names[0]) ? names[location] : nullptr;
}

// Batch 9B.  The HUD's location caption.
//
// A dungeon session deliberately does NOT rewrite GameState::position: the
// surface map id and coordinate are the RETURN context and must survive the
// whole descent (openu5::exit_dungeon() is what consumes them).  The caption
// therefore cannot be derived from that id while a session is live, or it names
// wherever the party happened to be last -- the physical report after Batch 9
// was "Serpent's Hold" (32) while standing in Deceit (33), because 32 was the
// surface map the tester teleported from.
//
// `dungeon_active` / `dungeon_id` come straight from
// openu5::hud_dungeon_bands(), which reads the authoritative DungeonState, so
// the caption and the two dungeon bands can never disagree about which dungeon
// is mounted.  Dungeons occupy 33..40 in the same table as the 32 local maps.
inline const char *hud_location_caption(uint8_t surface_location, int16_t surface_floor,
                                        bool dungeon_active, uint8_t dungeon_id) {
    const char *name = dungeon_active ? location_display_name(dungeon_id)
                       : surface_location == 0
                           ? (surface_floor < 0 ? "Underworld" : "Britannia")
                           : location_display_name(surface_location);
    return name ? name : "Unknown place";
}

} // namespace tdeck
