#pragma once

#include <cstdint>

#include "openu5/commands.h"
#include "openu5/gameplay_save.h"
#include "openu5/frontend.h"
#include "openu5/pc_save.h"

#include <vector>

namespace tdeck {

struct AlphaSaveScratch;

class AlphaSaveService {
  public:
    static constexpr const char *kDirectory = "/sd/ultima5/saves";
    // Alpha 4 A4-SAVE2 (ALPHA4_UI.md section 4): three manual slots, each a
    // two-generation pair with the A4-SAVE1 recovery rules. Slot 0 ("Slot 1")
    // is the pre-SAVE2 pair alpha1-g{0,1}, so an older card is Slot 1 as it
    // is; slots 1 and 2 are alpha1-s2-g{0,1} and alpha1-s3-g{0,1}.
    static constexpr int kSlots = openu5::kSaveSlotCount;
    static bool reserve_dma_headroom();
    // `slot`: 0..2, or -1 for the slot holding the card's newest commit
    // record (a blank card: slot 0) -- what every pre-SAVE2 save wrote.
    bool save(openu5::CommandContext &, openu5::OutdoorServices &, openu5::WorldTerrain &,
              openu5::NpcActors &, openu5::save::Json &, const uint8_t *base_gam,
              size_t base_size, const uint8_t *base_ool, size_t base_ool_size,
              uint32_t &elapsed_ms, bool new_journey = false, int slot = -1);
    // Continue: the slot whose newest commit is the card's newest, falling
    // back within that slot, then to the next most recent slot.
    bool load(openu5::CommandContext &, openu5::OutdoorServices &, openu5::WorldTerrain &,
              openu5::NpcActors &, openu5::save::Json &, uint32_t &elapsed_ms);
    // One logical slot (0..2), with the same fallback within its pair.
    bool load_slot(int slot, openu5::CommandContext &, openu5::OutdoorServices &,
                   openu5::WorldTerrain &, openu5::NpcActors &, openu5::save::Json &,
                   uint32_t &elapsed_ms);
    // The save list the menus show: per slot, the newest generation (and the
    // one before it only when the newest is refused), from the save list's
    // memory when a commit record is unchanged.
    void inspect_catalog(openu5::FrontendSaveCatalog &);
    // Both generations of one slot, each verified (storage tests and
    // diagnostics; the menus use inspect_catalog). inspect() is Slot 1's.
    void inspect_generations(int slot, openu5::FrontendSaveSlot (&generations)[2]);
    void inspect(openu5::FrontendSaveSlot (&generations)[2]) { inspect_generations(0, generations); }
    // The slot of the last successful save or load (-1: none yet): the live
    // journey's slot, which Alt+S / Alt+L and the menus' cursors use.
    int last_slot() const { return last_slot_; }
    const char *last_failure() const { return last_failure_; }
    // Alpha 4 A4-UI3: the last successful load restored something older than
    // the slot's newest commit (the one before it), or Continue had to pass
    // over the newest slot. From the commits the load read anyway.
    bool last_load_recovered() const { return last_load_recovered_; }

    // Alpha 4 A4-SAVE3 (ALPHA4_UI.md section 5): original PC/DOS saves. The
    // import folder is the player's; nothing here ever writes to it. Import
    // itself is the runtime's (it needs the game's data for the fresh map
    // entry) and ends in save(), the ordinary slot transaction.
    static constexpr const char *kImportDirectory = "/sd/ultima5/import";
    static constexpr const char *kExportDirectory = "/sd/ultima5/export";
    static constexpr const char *kImportMarker = "/sd/ultima5/pc-import.txt";
    enum class PcRead : uint8_t { Ok, NoFiles, NoGam, NoOol, ReadFailed };
    struct PcSource {
        std::vector<uint8_t> gam, ool;   // read only when the size is the original one
        size_t gam_size = 0, ool_size = 0;
        uint32_t gam_crc = 0, ool_crc = 0;
    };
    // import/SAVED.GAM and import/SAVED.OOL. A file of the wrong size is not
    // read (its size is reported for the check).
    PcRead read_pc_import(PcSource &);
    // The slot the last successful import of these same files went to, or -1.
    int pc_import_marker(uint32_t gam_crc, uint32_t ool_crc);
    bool record_pc_import(uint32_t gam_crc, uint32_t ool_crc, int slot);
    enum class PcExport : uint8_t { Ok, NoSlot, NoLoadable, Dungeon, WriteFailed, VerifyFailed, NoWorkspace };
    struct PcExportResult {
        int generation = -1;            // the physical generation exported
        uint64_t sequence = 0;
        openu5::save::pc::ExportReport report{};
    };
    // Slot `slot` (0..2) -> export/slot<N>/SAVED.GAM, SAVED.OOL, EXPORT.TXT:
    // the generation load_slot() restores, completed by the bridge. The slot's
    // files are only read.
    PcExport export_pc_save(int slot, PcExportResult &);
  private:
    AlphaSaveScratch *scratch();
    AlphaSaveScratch *scratch_ = nullptr;
    int last_slot_ = -1;
    bool last_load_recovered_ = false;
    char last_failure_[96]{};
};

class AlphaSettingsService {
  public:
    static constexpr const char *kPath = "/sd/ultima5/settings.json";
    bool load(openu5::FrontendSettings &) const;
    bool save(const openu5::FrontendSettings &) const;
};

} // namespace tdeck
