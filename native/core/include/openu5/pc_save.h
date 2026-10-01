#pragma once
// Alpha 4 A4-SAVE3 (targets/tdeck/ALPHA4_UI.md section 5): the bridge between
// an original PC/DOS Ultima V save and a Native save document.
//
// The 1988 game saves in one place, CAST2.OVL:0x10FE: SAVED.GAM is a verbatim
// dump of the 4192-byte live-state window DS:0x55A6, and SAVED.OOL (512 B) the
// two parked object tables, BRIT then UNDER. Native's `.gam` and `.ool` use the
// same layouts; its codec (persistence.cpp) reads and writes every field it
// models exactly, and is not changed here. What the codec leaves out, and
// Native keeps in its sidecar instead, is carried by this bridge (B1-B10 of
// section 5.5); what it cannot carry is refused or named (L1-L5).
//
// No storage, clock or runtime service is used: the caller reads the files,
// and the runtime owns the parts that need the game's data (the fresh map
// entry of an imported town, section 5.6).
#include "persistence.h"
#include "state.h"
#include "turn.h"
#include <cstddef>
#include <cstdint>

namespace openu5::save::pc {

// The accepted source files (section 5.1), as the DOS game names them.
constexpr const char *kGamFile = "SAVED.GAM";
constexpr const char *kOolFile = "SAVED.OOL";

enum class Check : uint8_t {
    Ok,
    GamSize,   // SAVED.GAM is not exactly 4192 bytes
    OolSize,   // SAVED.OOL is not exactly 512 bytes
    Party,     // party size outside 1..6
    Roster,    // a party member's record is not a character
    Clock,     // month/day/hour/minute outside the game's calendar
    Location,  // location above 40, or a floor/position the map cannot hold
    Dungeon,   // inside a dungeon (33..40): not bridged (L2)
    Weather,   // wind or sail direction outside 0..4
    Codec,     // Native's own codec refused the document
};
// One line (at most 47 characters) for the title screen.
const char *check_text(Check);

// Whether a SAVED.GAM/SAVED.OOL pair is an original save the bridge accepts:
// the exact sizes, the domains of what import reads, and not in a dungeon.
Check check_original(const uint8_t *gam, size_t gam_size, const uint8_t *ool, size_t ool_size);

// What the title lists for a checked SAVED.GAM: the Avatar's name and where
// the party stands. `floor` is -1 for the file's 0xFF (the underworld
// outdoors, a basement in a town), the convention hud_location_caption() reads.
struct OriginalSummary {
    char name[10]{};
    uint8_t location = 0, party = 0;
    int16_t floor = 0;
};
OriginalSummary summarize_original(const uint8_t *gam);

struct ImportReport {
    uint8_t frigates = 0, horses = 0, skiffs = 0; // B10, from the live table and the parked blocks
    uint8_t search_found = 0;                      // B1
};
// SAVED.GAM/SAVED.OOL (sizes already checked) -> game, turn and document,
// assigned only on success. The codec reads the file exactly as every New
// Journey reads INIT.GAM (no sidecar); the bridge completes the document before
// restore_core() projects it. `game.rng` is kept, as restore_core does.
Error import_original(const uint8_t *gam, const uint8_t *ool, GameState &, TurnState &, Json &document,
                      ImportReport * = nullptr);

struct ExportReport {
    uint8_t frigates = 0, horses = 0, skiffs = 0; // B10 records written
    uint8_t unplaced = 0;                         // B10 vehicles with no free object slot
    uint8_t search_found = 0;                     // B1 bits set
    uint8_t carpets = 0, loot = 0, shadowlords = 0; // L4: present, not bridged
    bool town_npcs_left_out = false;              // L1: the party is in a town
};
// Check::Dungeon for a document inside a dungeon (location 33..40 or a
// dungeon session), else Check::Ok.
Check exportable(const Json &document);
// A generation's own .gam and .ool, completed from its (staged) document:
// B1-B8 into the .gam, B10 into the live table or the parked .OOL block.
void complete_export(const Json &document, Gam &, Ool &, ExportReport * = nullptr);

} // namespace openu5::save::pc
