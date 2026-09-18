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

} // namespace tdeck
