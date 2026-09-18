#pragma once

#include <cstddef>
#include <cstdint>

#include "alpha_resources.h"
#include "asset_pack.h"
#include "openu5/commands.h"
#include "openu5/ui_session.h"

namespace tdeck {

constexpr char kSmokeTestLogPath[] = "/sd/ultima5/logs/smoke-tests.log";
constexpr char kSmokeTestSdPath[] = "/ultima5/logs/smoke-tests.log";
constexpr size_t kSmokeGroupCount = 15;
constexpr size_t kSmokeScenarioCount = 45;

struct SmokeEnvironment {
    openu5::CommandContext *context = nullptr;
    AlphaResourceOwners *resources = nullptr;
    const AlphaResourceReport *resource_report = nullptr;
    const openu5::AssetPackReport *tile_report = nullptr;
    openu5::UiSession *ui = nullptr;
};

struct SmokeView {
    bool running = false, complete = false;
    size_t passed = 0, failed = 0, completed = 0, total = 0;
    const char *group = "";
    const char *scenario = "";
    const char *first_failure = "";
};

class DeviceSmokeTests {
  public:
    struct Result { bool pass=false; uint32_t state_hash=0,ui_hash=0,presentation_hash=0; char reason[112]{}; };
    void bind(SmokeEnvironment environment) { environment_ = environment; }
    void start(int group = -1);
    bool pump(); // Runs at most one short scenario; true when the view changed.
    SmokeView view() const;
    static const char *group_name(size_t);

  private:
    SmokeEnvironment environment_{};
    int selected_group_ = -1;
    size_t cursor_ = 0, passed_ = 0, failed_ = 0, completed_ = 0, total_ = 0;
    bool running_ = false, complete_ = false;
    char current_[64]{}, first_failure_[112]{};

    Result run(size_t scenario) const;
    void write_header() const;
    void write_result(size_t scenario, const Result &) const;
    void write_footer() const;
};

} // namespace tdeck
