// Batch 11 host-test seam.
//
// Harmless-sink stand-in for AlphaSaveService/AlphaSettingsService's PUBLIC
// interface (alpha_save.h, unmodified) so alpha_runtime.cpp links on host
// without pulling in the real SD-card/FATFS implementation (alpha_save.cpp),
// which needs ESP-IDF's VFS and actual storage hardware. Save/Load are
// physical-device operations by nature (Batch 11 spec: "substitute harmless
// sinks for ... physical-device operations, while still observing that the
// production routing attempted to emit them") -- reporting failure here is
// the same honest, harmless outcome a device with no SD card inserted would
// already produce through the real code path.
//
// Never linked into the T-Deck firmware: only this CMake host test target
// compiles it. alpha_save.cpp (the real device implementation) is untouched.
#include "alpha_save.h"

#include <cstdio>

namespace tdeck {

bool AlphaSaveService::reserve_dma_headroom() { return true; }

bool AlphaSaveService::save(openu5::CommandContext &, openu5::OutdoorServices &, openu5::WorldTerrain &,
                             openu5::NpcActors &, openu5::save::Json &, const uint8_t *, size_t,
                             const uint8_t *, size_t, uint32_t &elapsed_ms, bool, int) {
    elapsed_ms = 0;
    std::snprintf(last_failure_, sizeof(last_failure_), "%s", "host test build: no storage backend");
    return false;
}

bool AlphaSaveService::load(openu5::CommandContext &, openu5::OutdoorServices &, openu5::WorldTerrain &,
                             openu5::NpcActors &, openu5::save::Json &, uint32_t &elapsed_ms) {
    elapsed_ms = 0;
    std::snprintf(last_failure_, sizeof(last_failure_), "%s", "host test build: no storage backend");
    return false;
}

bool AlphaSaveService::load_slot(int, openu5::CommandContext &, openu5::OutdoorServices &,
                                  openu5::WorldTerrain &, openu5::NpcActors &, openu5::save::Json &,
                                  uint32_t &elapsed_ms) {
    elapsed_ms = 0;
    std::snprintf(last_failure_, sizeof(last_failure_), "%s", "host test build: no storage backend");
    return false;
}

void AlphaSaveService::inspect_generations(int, openu5::FrontendSaveSlot (&slots)[2]) {
    slots[0] = openu5::FrontendSaveSlot{};
    slots[1] = openu5::FrontendSaveSlot{};
}

void AlphaSaveService::inspect_catalog(openu5::FrontendSaveCatalog &catalog) { catalog = openu5::FrontendSaveCatalog{}; }

// Alpha 4 A4-SAVE3: no card here, so no PC save to import and nothing to export.
AlphaSaveService::PcRead AlphaSaveService::read_pc_import(PcSource &source) {
    source = PcSource{};
    return PcRead::NoFiles;
}
int AlphaSaveService::pc_import_marker(uint32_t, uint32_t) { return -1; }
bool AlphaSaveService::record_pc_import(uint32_t, uint32_t, int) { return false; }
AlphaSaveService::PcExport AlphaSaveService::export_pc_save(int, PcExportResult &result) {
    result = PcExportResult{};
    return PcExport::NoLoadable;
}

bool AlphaSettingsService::load(openu5::FrontendSettings &) const { return false; }
bool AlphaSettingsService::save(const openu5::FrontendSettings &) const { return false; }

} // namespace tdeck
