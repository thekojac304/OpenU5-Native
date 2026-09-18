#pragma once

#include <cstdint>

#include "openu5/commands.h"
#include "openu5/gameplay_save.h"
#include "openu5/frontend.h"

namespace tdeck {

struct AlphaSaveScratch;

class AlphaSaveService {
  public:
    static constexpr const char *kDirectory = "/sd/ultima5/saves";
    static bool reserve_dma_headroom();
    bool save(openu5::CommandContext &, openu5::OutdoorServices &, openu5::WorldTerrain &,
              openu5::NpcActors &, openu5::save::Json &, const uint8_t *base_gam,
              size_t base_size, const uint8_t *base_ool, size_t base_ool_size,
              uint32_t &elapsed_ms, bool new_journey = false);
    bool load(openu5::CommandContext &, openu5::OutdoorServices &, openu5::WorldTerrain &,
              openu5::NpcActors &, openu5::save::Json &, uint32_t &elapsed_ms);
    bool load_slot(int slot, openu5::CommandContext &, openu5::OutdoorServices &,
                   openu5::WorldTerrain &, openu5::NpcActors &, openu5::save::Json &,
                   uint32_t &elapsed_ms);
    void inspect(openu5::FrontendSaveSlot (&slots)[2]);
    const char *last_failure() const { return last_failure_; }
  private:
    AlphaSaveScratch *scratch();
    AlphaSaveScratch *scratch_ = nullptr;
    char last_failure_[96]{};
};

class AlphaSettingsService {
  public:
    static constexpr const char *kPath = "/sd/ultima5/settings.json";
    bool load(openu5::FrontendSettings &) const;
    bool save(const openu5::FrontendSettings &) const;
};

} // namespace tdeck
