#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "alpha_resources.h"
#include "alpha_dialogue.h"
#include "alpha_save.h"
#include "asset_pack.h"
#include "dungeon_art_cache.h"
#include "openu5/audio.h"
#include "openu5/ambient_sfx.h"
#include "openu5/audio_pack.h"
#include "openu5/audio_stream.h"
#include "openu5/perf_report.h"
#include "openu5/command_char.h"
#include "openu5/combat.h"
#include "openu5/dialogue_orchestration.h"
#include "openu5/dungeon_encounters.h"
#include "openu5/look.h"
#include "openu5/blackthorn.h"
#include "openu5/blackthorn_scene.h"
#include "openu5/narrative_scene.h"
#include "openu5/poison_tick.h"
#include "openu5/world_fx.h"
#include "openu5/outdoor.h"
#include "openu5/shop_orchestration.h"
#include "openu5/shrine.h"
#include "openu5/ui_debug_menu.h"
#include "openu5/ui_session.h"
#include "openu5/frontend.h"
#include "openu5/intro_view.h"
#include "openu5/system_menu.h"
#include "openu5/world_terrain.h"
#include "openu5/zstats.h"
#include "native_renderer.h"
#include "device_smoke_tests.h"
#include "device_ui_views.h"
#include "ui_input_adapter.h"
#include "ui_mode_policy.h"

namespace tdeck {
class Board;
class IdleService;
struct DeviceDebugScreen;

class AlphaRuntime {
  public:
    esp_err_t initialize(AlphaResourcePack &, AlphaResourceReport &, openu5::AssetPackReader &,
                         const openu5::AssetPackReport &);
    bool handle(const RawInputEvent &);
    esp_err_t render(Board &, bool force = false);
    void log_metrics(const char *checkpoint) const;
    // A3-01. The audio seam: the pack's capability (read at boot by
    // tdeck::load_audio_pack_info) and the output. main.cpp calls it after
    // initialize(); a host test calls it after attach_host_test_fixture().
    // Until it is called the runtime is silent and Settings reports "no
    // audio pack". A null backend is silent.
    void configure_audio(const openu5::AudioPackInfo &, openu5::AudioBackend *);
    const openu5::AudioService &audio() const { return audio_; }
    // A3-04A. The audio task's performance windows (the device backend; null
    // on the host and with no audio output): the Developer "Audio
    // performance" / "Audio stats (live)" rows and the heartbeat's
    // AUDIO_PERF line read it. main.cpp attaches it after configure_audio().
    void attach_audio_perf(openu5::AudioPerfSource *source) { audio_perf_ = source; }
    bool audio_benchmark_running() const { return audio_bench_.running(); }
    // A3-04B (ALPHA3_AUDIO.md section 19). The machine-wide window -- per-core
    // and per-task CPU from the FreeRTOS run-time statistics, heap, stacks --
    // for the same report; main.cpp attaches the device's, the host has none.
    void attach_system_perf(openu5::SystemPerfSource *source) { system_perf_ = source; }
    // The game thread's own frames and inputs (always counted; diagnostics only).
    const openu5::RenderPerfCounters &render_perf() const { return render_perf_; }
    // Alpha 3 A3-04C (ALPHA3_AUDIO.md section 20): the contention map. The SD
    // log writer's burst statistics (device only; main.cpp attaches them),
    // main.cpp's loop passes, the window's one-line summary (the heartbeat's
    // A3C_PERF line) and the Developer "synth bypass" probe's state.
    struct SdLogPerfHooks {
        bool (*snapshot)(openu5::SdLogPerf &) = nullptr;
        void (*reset)() = nullptr;
        // A3-04D (section 21): the SD diagnostic log's switch -- the Developer
        // row "Probe: SD diag logging" and the scenario's "sdlog ON/OFF".
        openu5::SdLogState (*state)() = nullptr;
        bool (*set_enabled)(bool) = nullptr;
    };
    void attach_sd_log_perf(SdLogPerfHooks hooks) { sd_log_perf_ = hooks; }
    void note_loop_pass(uint32_t us) { contention_.on_loop(us); }
    const openu5::ContentionCounters &contention() const { return contention_; }
    size_t contention_line(char *out, size_t cap) const;
    bool music_bypass() const { return music_bypass_; }
    openu5::SdLogState sd_log_state() const;
    // Alpha 3 A3-04E (ALPHA3_AUDIO.md section 22): the game thread's pacing.
    // The draw loops' pause (pushed to the Board before every draw) and
    // main.cpp's end-of-pass wait. A3-04E's by default; the two Developer
    // probes switch either back to the legacy behaviour. Never saved.
    const openu5::PacingPolicy &pacing() const { return pacing_; }
    // May main.cpp's loop block for a tick after this pass? No while anything
    // is scheduled relative to the moment it is serviced (see the .cpp).
    bool loop_may_sleep() const;
    uint32_t loop_wait_ticks() const { return openu5::loop_wait_ticks(pacing_.loop, loop_may_sleep()); }
    void note_loop_wait(uint32_t us, bool input) { contention_.on_loop_wait(us, input); }
    size_t pacing_line(char *out, size_t cap, uint32_t heartbeat = 0) const; // the heartbeat's A3E_PACE line
    // A3-04E.1 (section 23): the idle-service guard's window, for the report and A3E_PACE.
    void attach_idle_service(IdleService *service) { idle_service_ = service; }
    // The combined AUDIO / RENDER PERF report: it replaces the Developer
    // screen's rows until dismissed (Enter / Back), and scrolls with Up/Down.
    // "Pending" = it finished while the Developer menu was closed: Alt+D shows it.
    bool perf_report_open() const { return perf_report_open_; }
    bool perf_report_pending() const { return perf_report_pending_; }
    size_t perf_report_line_count() const { return perf_report_count_; }
    const char *perf_report_line(size_t i) const {
        return perf_report_lines_ && i < perf_report_count_ ? perf_report_lines_[i] : "";
    }
    // A3-03. The ambient ticker (ambient_sfx.h) and how often it ran.
    const openu5::AmbientTicker &ambient() const { return ambient_; }
    uint32_t ambient_ticks() const { return ambient_ticks_; }
    const openu5::AudioPackInfo &audio_pack() const { return audio_pack_; }
    const openu5::FrontendSettings &device_settings() const { return settings_; }
    openu5::GameState &game() { return game_; }
    const openu5::UiSession *ui() const { return ui_; }

    // Batch 11 host-test seam --------------------------------------------
    // Wires this exact instance's CommandContext/CombatContext/
    // DungeonContext and constructs its UiSession the same way initialize()
    // does for production, except every buffer comes from ordinary `new`
    // instead of heap_caps PSRAM, and Board/AlphaResourcePack are never
    // touched. Callers set world/party/turn state afterward through
    // game()/turn()/travel()/commands() and then drive the exact same
    // handle()/command()/dispatch() production uses -- no gameplay routing
    // is duplicated or mirrored. Production's initialize() is completely
    // unchanged and never calls this; only the CMake target under
    // native/targets/tdeck/host_tests links the .cpp that defines it, so it
    // never ships in T-Deck firmware.
    //
    // Batch 21A adds the dungeon/room-combat resources. Production reads the
    // identical four things out of the SD resource pack in initialize()
    // (dungeon_context_.data/count, dungeon_arenas_, combat_context_.
    // enemy_defs/enemy_def_count); leaving them null is exactly the quiescent
    // pre-dungeon state, and supplying them lets a host test reach the real
    // room-entry path instead of bouncing off CombatResult::MissingMap. No
    // behaviour is added or branched on: the fields are copied into the same
    // members initialize() assigns, and nothing else reads HostTestFixture.
    struct HostTestFixture {
        openu5::WorldData world{};
        const openu5::DungeonData *dungeons = nullptr;
        size_t dungeon_count = 0;
        const openu5::DungeonArena *arenas = nullptr;
        size_t arena_count = 0;
        const openu5::CombatEnemy *const *enemy_defs = nullptr;
        size_t enemy_def_count = 0;
        const uint8_t *location_x = nullptr, *location_y = nullptr;
        size_t location_count = 0;
        // Batch 22: the 32 per-location .NPC tables initialize() takes from
        // the pack (resources_.npc_locations). Null keeps the empty tables.
        const openu5::NpcLocationData *npc_locations = nullptr;
        // Batch 24: the loaded pack itself, for the INIT.GAM/.OOL templates
        // the Save path exports over and the combat maps/enemies/tables a
        // town fight (TOWN.OVL:0x09BC) needs. attach copies exactly the
        // members initialize() assigns from them, then runs the one
        // load_native_state(INIT.GAM) a New Journey runs. Null keeps both out.
        const AlphaResourceOwners *pack = nullptr;
        // Host framebuffer tests may compose deterministic all-white map tiles.
        bool render_pixels = false;
        bool indexed_test_tiles = false;
        // Batch 51: the scene pacers get the same storage and binder as
        // initialize(). `false` is the pacers' own harness contract (a zero
        // unit drains every beat synchronously -- set_paced(false) /
        // set_unit_ms(0)); `true` is the device's real cadence, driven by the
        // host esp_timer shim's virtual clock.
        bool paced_scenes = false;
    };
    void attach_host_test_fixture(const HostTestFixture &);
    // Batch 51 read-only windows on the scene pacers, for timing assertions.
    const openu5::NarrativeScenePacer &narrative_pacer() const { return narrative_pacer_; }
    const openu5::BlackthornScenePacer &blackthorn_pacer() const { return blackthorn_pacer_; }
    bool camp_scene_inverted() const { return camp_scene_inverted_; }
    // A3-04E: the last composed 176x176 viewport -- what the Board was handed.
    const uint16_t *composed_viewport() const { return viewport_; }
    bool system_menu_open() const { return system_menu_.active(); }
    // A3-01: the System Menu page as it would be drawn (read-only).
    openu5::FrontendView system_menu_view() const { return system_menu_.view(); }
    // Batch 53 (D-53): the status/prompt line render() hands to the Board.
    const char *status_overlay() const { return overlay(); }
    uint32_t routed_command_count() const { return routed_command_sequence_; }
    // Scene pacer storage (#324 / Y-04). Class constants so initialize() and
    // the host fixture size the same queues.
    // The longest real Blackthorn turn is the capture entry: 15 events and 51
    // scene beats (Batch 51 split the materialization into sweep + circle +
    // arrival); the escorted finale is 44 beats. The arena holds the copied
    // narrative for one turn.
    static constexpr size_t kBlackthornSceneSteps = 96, kBlackthornSceneTextBytes = 4096;
    // The troll crossing is 1 + 6*5 + 1 = 32 beats, the refuge script 16, and
    // the Camp apparition's longest paced segment (six members, none levelling)
    // 2 + 1 + 6*6 + 3 = 42 steps; each can be followed by the rest of its turn.
    static constexpr size_t kNarrativeSceneSteps = 64, kNarrativeSceneTextBytes = 2048;
    openu5::TurnState &turn() { return turn_; }
    openu5::TravelState &travel() { return travel_; }
    openu5::CommandState &commands() { return commands_; }
    openu5::NpcActors &actors() { return actors_; }
    // Read-only windows on the two session states a dungeon/room regression has
    // to assert against. Nothing here mutates; they exist so a test can read the
    // SAME objects production commands wrote, never a copy.
    const openu5::DungeonState &dungeon_state() const { return dungeon_; }
    const openu5::CombatState &combat_state() const { return combat_; }
    const openu5::CommandContext &command_context() const { return context_; }
    openu5::DungeonState &dungeon_state_for_test() { return dungeon_; }
    // A3-02: the arena roster present_audio() resolves a hit's target side in.
    openu5::CombatState &combat_state_for_test() { return combat_; }
    // Batch 22: the SAME context the device's UiDebugMenu is constructed
    // over (initialize(): new UiDebugMenu(context_)), so a host test can make
    // the identical apply_debug_teleport(context_, ...) call, and the SAME
    // pool QuestWorldServices writes.
    openu5::CommandContext &command_context_for_test() { return context_; }
    const std::vector<openu5::QuestObject> &objects_for_test() const { return objects_; }

    // Batch 14 / R-22 host-test seam --------------------------------------
    // The (Z)-stats modal, observable without a Board. `zstats_view()` is the
    // SAME openu5::ZStatsPage compose_selection_view() paints, so a host test
    // and the device panel cannot disagree about a row.
    bool zstats_active() const { return zstats_open_; }
    openu5::ZStatsPage zstats_view() const;

  private:
    struct Selection { char label[40]{}; int16_t value = -1; bool enabled = true; };
    openu5::GameState game_{};
    openu5::TurnState turn_{};
    openu5::TravelState travel_{};
    openu5::CommandState commands_{};
    AlphaResourceOwners resources_{};
    openu5::NpcActors actors_{};
    openu5::WorldTerrain terrain_{};
    openu5::OutdoorServices outdoor_{};
    std::vector<openu5::QuestObject> objects_{};
    openu5::QuestWorldServices quest_{};
    // Batch 53 (RB-3 / H-188). The one runtime owner of the eight moonstones
    // (GAM 0x28a..0x2a9). quest_.moonstones points here; every load route
    // re-hydrates it from the loaded document (synchronize_loaded_world) and
    // every save captures it back (capture_save_document).
    openu5::Moonstone moonstones_[8]{};
    // Batch 53 (H-190). KARMA.DAT's six records quoted at use, as game.ts
    // 0x0b03 composes them; built once by bind_quest_services() from the pack.
    std::string karma_speech_[6]{};
    openu5::CombatState combat_{};
    openu5::CombatContext combat_context_{game_,turn_,combat_};
    openu5::CombatResources combat_resources_{};
    openu5::CombatActor *combat_actor_overflow_ = nullptr;
    openu5::CombatLootPile *combat_pile_overflow_ = nullptr;
    openu5::CombatField *combat_fields_ = nullptr;
    openu5::DungeonState dungeon_{};
    openu5::DungeonScratch dungeon_scratch_{};
    openu5::DungeonContext dungeon_context_{dungeon_,dungeon_scratch_};
    openu5::DungeonArena *dungeon_arenas_ = nullptr;
    openu5::DungeonEncounters dungeon_encounters_{};
    // Batch 9C / R-05 -- the authored dungeon art (DNG1/2/3.16, ITEMS.16,
    // MON0-7.16), read once from the resource pack during initialize() while it
    // is still open. Presentation only: it never reads or writes GameState, the
    // clock or any RNG, and after the one load it never touches SD again.
    DungeonArtCache dungeon_art_{};
    openu5::DialogueSession dialogue_{};
    AlphaDialogueCache dialogue_assets_{};
    openu5::DialogueServices dialogue_services_{dialogue_};
    openu5::ShopSession shop_{};
    openu5::ShopData shop_data_{};
    openu5::ShopServices shop_services_{shop_,shop_data_};
    openu5::ShrineSession shrine_{};
    openu5::ShrineServices shrine_services_{shrine_};
    openu5::LookServices look_services_{};
    openu5::BlackthornSession blackthorn_{};
    // #324 / R-32 -- the Blackthorn capture scene. The semantic session above
    // is untouched; these are presentation only. The pacer defers the whole
    // capture turn and releases it beat by beat, so the staged sequence plays
    // instead of the narrative arriving as one burst over the Palace lobby.
    // Its queue, text arena, working room grid and per-segment script scratch
    // are all PSRAM, like every other large runtime buffer here.
    openu5::BlackthornSceneState blackthorn_scene_state_{};
    openu5::BlackthornSceneServices blackthorn_scene_services_{};
    openu5::BlackthornScenePacer blackthorn_pacer_{};
    openu5::BlackthornSceneScript *blackthorn_script_ = nullptr;
    openu5::BlackthornSceneStep *blackthorn_steps_ = nullptr;
    char *blackthorn_scene_text_ = nullptr;
    int16_t *blackthorn_scene_grid_ = nullptr;
    uint32_t blackthorn_released_ = 0;
    // Y-04 -- the five remaining feedback/VFX channels (Batch 7B). All three
    // owners are PRESENTATION ONLY: none of them reads or writes GameState,
    // the clock or any RNG. `narrative_pacer_` is the single sequencer the
    // Refuge and TrollSneak scenes share; its queue and text arena are PSRAM,
    // like every other large runtime buffer here.
    openu5::WorldFxLayer world_fx_{};
    openu5::PoisonFlashPacer poison_{};
    openu5::NarrativeScenePacer narrative_pacer_{};
    openu5::NarrativeSceneStep *narrative_steps_ = nullptr;
    char *narrative_text_ = nullptr;
    uint32_t narrative_released_ = 0;
    openu5::CommandContext context_{game_,turn_,travel_,commands_,resources_.world};
    openu5::save::Json retained_{};
    AlphaSaveService save_{};
    AlphaSettingsService settings_store_{};
    openu5::FrontendSession frontend_{};
    openu5::FrontendSettings settings_{};
    // A3-01. Presentation only: the service reads the cue, the settings'
    // volumes and the pack's capability, and never GameState or any RNG.
    openu5::AudioService audio_{};
    openu5::AudioPackInfo audio_pack_{};
    // A3-04A. Diagnostics only; nothing in play reads them.
    openu5::AudioPerfSource *audio_perf_ = nullptr;
    openu5::AudioBenchmark audio_bench_{};
    uint32_t bench_guard_ns_ = 0, bench_idle_channels_x100_ = 0;
    size_t bench_internal_free_ = 0, bench_psram_free_ = 0;
    // A3-04B. Diagnostics only; nothing in play reads them.
    openu5::SystemPerfSource *system_perf_ = nullptr;
    openu5::RenderPerfCounters render_perf_{};
    // A3-04C. Diagnostics only; nothing in play reads them.
    openu5::ContentionCounters contention_{};
    SdLogPerfHooks sd_log_perf_{};
    bool music_bypass_ = false;
    openu5::PacingPolicy pacing_ = openu5::kPacingDefault; // A3-04E
    IdleService *idle_service_ = nullptr; // A3-04E.1
    mutable uint32_t heartbeat_seq_ = 0;  // A3-04E.1: A3E_PACE's hb=
    openu5::PerfScenario perf_scenario() const;
    openu5::AudioPerfSnapshot bench_idle_{};
    bool bench_idle_valid_ = false;
    uint32_t bench_status_second_ = UINT32_MAX;
    int64_t input_pending_us_ = -1; // capture time of the oldest input no gameplay frame has shown yet
    char (*perf_report_lines_)[openu5::kPerfReportLineBytes] = nullptr; // kPerfReportMaxLines rows, PSRAM
    size_t perf_report_count_ = 0, perf_report_top_ = 0;
    bool perf_report_open_ = false, perf_report_pending_ = false;
    // A3-03. ambient_sfx_tick 0x4102's counters, and the 55 ms tick and the
    // clock reading it last ran on.
    openu5::AmbientTicker ambient_{};
    uint32_t ambient_tick_ = UINT32_MAX, ambient_ticks_ = 0;
    int64_t ambient_clock_key_ = -1;
    openu5::IntroViewPlayer intro_view_{};
    openu5::IntroViewFrame intro_frame_{};
    openu5::SystemMenuSession system_menu_{};
    UiInputAdapter input_{};
    DeviceSmokeTests smoke_{};
    openu5::UiSession *ui_ = nullptr;
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    openu5::UiDebugMenu *debug_ = nullptr;
#endif
    openu5::UiTextBlock *transcript_ = nullptr;
    uint16_t *viewport_ = nullptr;
    Board *board_ = nullptr; // Bound by render(); bed fill occurs inside handle().
    uint16_t *creation_canvas_ = nullptr;
    uint8_t *tile_cache_storage_ = nullptr;
    openu5::PresentationTileCache tile_cache_{};
    openu5::PresentationSnapshot presentation_{};
    openu5::FrontendView frontend_view_{};
    DeviceDebugScreen *debug_view_ = nullptr;
    DeviceShopView shop_view_{};
    DeviceSelectionView selection_view_{};
    DeviceContextActionBar context_bar_{};
    openu5::ActorAnimationClock actor_animation_{};
    void *astar_scratch_ = nullptr;
    Selection selections_[64]{};
    size_t selection_count_ = 0;
    openu5::UiRequestId selection_request_ = openu5::UiRequestId::None;
    int16_t pending_order_from_ = -1;
    // R-10: NpcInitiatesTalk/NpcInitiatesShop must not re-enter command()
    // from inside consume_event() (it runs while the emitting world command
    // is still on the stack). consume_event() copies only the stable
    // identity here -- never the event's borrowed NpcActor* -- and
    // drain_pending_npc_initiation() dispatches BeginConversation once the
    // outer input has fully unwound (see AlphaRuntime::handle()).
    enum class PendingNpcInitiation : uint8_t { None, Talk, Shop };
    PendingNpcInitiation pending_npc_initiation_ = PendingNpcInitiation::None;
    int16_t pending_npc_slot_ = -1;
    uint8_t pending_npc_location_ = 0;
    char16_t shrine_virtue_[64]{};
    size_t shrine_virtue_length_ = 0;
    openu5::AssetPackReader *tiles_ = nullptr;
    openu5::AssetPackReport tile_report_{};
    AlphaResourceReport alpha_report_{};
    uint32_t transcript_high_water_ = 0;
    uint32_t render_high_us_ = 0;
    uint32_t dungeon_render_high_us_ = 0;
    uint32_t frontend_render_high_us_ = 0;
    uint32_t command_high_us_ = 0;
    uint32_t rendered_animation_tick_ = UINT32_MAX;
    // A3-HF2.1: the last PRESENTATION_DISPATCH logged (UI mode, source);
    // render() logs the line only when one of them changes.
    int32_t dispatch_logged_mode_ = -1;
    const char *dispatch_logged_source_ = "";
    uint32_t rendered_frontend_title_frame_ = UINT32_MAX;
    uint32_t rendered_attract_frame_ = UINT32_MAX;
    uint8_t applied_brightness_ = 0;
    bool dungeon_presentation_pending_ = false;
    // Batch 53A: the one System line that says why input stops answering.
    bool ending_announced_ = false;
    bool gem_view_active_ = false;
    bool gem_view_charges_turn_ = false;
    // R-13: Use Spyglass. Closes on any key like gem view, but charges no
    // deferred turn -- (U)se already spent it. e.zodiac is borrowed for the
    // synchronous emit only, so the star/sign field is copied out here.
    bool zodiac_view_active_ = false;
    openu5::ZodiacView zodiac_view_{};
    uint32_t attract_origin_frame_ = 0;
    uint32_t frontend_reconstruction_count_ = 0;
    uint32_t frontend_state_change_count_ = 0;
    openu5::FrontendState observed_frontend_state_ = openu5::FrontendState::EnterGame;
    int64_t next_enemy_step_us_ = 0;
    openu5::UiAction combat_input_queue_[8]{};
    size_t combat_input_count_ = 0;
    int16_t pending_combat_spell_ = -1;
    int16_t pending_ready_member_ = -1;
    int16_t pending_use_item_ = -1;
    int16_t status_member_ = -1;
    // R-25 (Batch 19). The two kernel 0x4988 callers whose picker has to span a
    // modal: the (S)earch command is held whole (it already carries its
    // direction from SJOG 0x097e, which the binary asks for BEFORE the picker
    // at 0x09a0), and the (C)ast is held as "a spell menu is owed once a
    // caster exists", because CAST.OVL 0x0dd5 resolves the caster BEFORE the
    // "Spell name:" prompt.
    openu5::Command pending_search_{};
    bool pending_search_active_ = false;
    int16_t pending_caster_ = -1;
    // R-22. The (Z)-stats page modal: the axis slot of ztats-layout.md
    // section 2 plus the current list's scroll offset. Presentation only --
    // while it is open nothing is dispatched, no turn is charged and no RNG
    // is drawn, exactly as cmd_zstats (ZSTATS.OVL:0x0a3a) behaves.
    bool zstats_open_ = false;
    int zstats_page_ = 0;
    size_t zstats_scroll_ = 0;
    int32_t combat_world_tile_before_ = openu5::kOffMap;
    bool combat_world_tile_saved_ = false;
    struct DirectTrollTrace {
        bool active = false;
        openu5::MapId map{};
        int32_t trigger_x = openu5::kOffMap, trigger_y = openu5::kOffMap;
        int32_t bridge_tile = openu5::kOffMap;
        int32_t inserted_x = openu5::kOffMap, inserted_y = openu5::kOffMap;
    } direct_troll_{};
    bool animation_visible_ = false;
    bool dirty_ = true;
    const char *dirty_reason_ = "mode-entry";
    uint32_t debug_open_count_ = 0;
    uint32_t debug_render_count_ = 0;
    uint32_t routed_command_sequence_ = 0;
    openu5::CommandKind last_routed_command_{};
    bool teleport_snapshot_pending_ = false;
    int64_t magic_invert_start_us_ = 0, magic_invert_end_us_ = 0;
    bool magic_was_inverted_ = false;
    bool camp_scene_active_ = false;
    bool camp_scene_apparition_ = false;
    bool camp_viewport_only_ = false;
    bool camp_scene_inverted_ = false;
    uint8_t camp_awake_mask_ = 0;
    int8_t camp_guard_ = -1;
    int8_t camp_guard_col_ = -1;
    int8_t camp_guard_row_ = -1;
    // R-12: Wis An Ylem / In Quas Wis / Death Vision. Wall-clock, not
    // turn-gated -- matches the reference's revealViewport(ms) timer, not a
    // per-turn counter. Input is swallowed while active (see handle()) and
    // compose_world_presentation's reveal_all bypasses the light/wall
    // censorship for the same window (see consume_event/render()).
    int64_t map_reveal_end_us_ = 0;
    bool map_reveal_was_active_ = false;
    // Y-04 (Quake): the reference collapses every {kind:"quake"} event of a
    // turn into ONE trigger(now, count*QUAKE_PULSES) call (skin.ts's
    // planTurnPhase/applyTurnFx) -- a sustained shake, not N overlapping
    // ones. consume_event() sees events one at a time, so a retrigger while
    // one is still running EXTENDS the pulse count instead of resetting the
    // clock, reconstructing the same total duration.
    int64_t quake_start_us_ = 0;
    int quake_pulses_ = 0;
    bool quake_was_active_ = false;
    bool world_fx_was_active_ = false;
    bool poison_was_active_ = false;
    bool narrative_was_active_ = false;

    static void dispatch_ui(void *, const openu5::UiIntent &);
    static void dispatch_event(void *, const openu5::GameEvent &);
    void consume_event(const openu5::GameEvent &);
    void dispatch(const openu5::UiIntent &);
    void command(openu5::Command);
    // R-25 (Batch 19). The two halves of kernel 0x4988 that have to be spoken
    // by whoever owns the roster and the modals. Both delegate every RULE to
    // the shared openu5/command_char.h seam the host suite drives; what lives
    // here is only the plumbing the seam must not know about.
    //   resolve: branches 2/3/4, with the side effect each one owes already
    //     applied. Resolved means `member` is settled and the caller proceeds
    //     now; None means "None!" was printed and the command is over; Prompt
    //     means the "Player: " roster modal is open under `request` and the
    //     caller must park whatever it still needs to finish.
    //   accept: the branch-4 post-pick gate at @0x4a2e -- false means
    //     "Disabled!" was printed and the SAME prompt was reopened (@0x4a57),
    //     which is a re-ask, not a rejection.
    openu5::CommandCharOutcome resolve_command_char_or_prompt(openu5::UiRequestId request, int16_t &member);
    bool accept_command_char_pick(openu5::UiRequestId request, int16_t member);
    void service_combat();
    void schedule_combat();
    bool combat_ai_turn();
    bool finish_combat_if_needed();
    void log_combat_counts(const char *reason) const;
    void save_combat_world_tile();
    void log_combat_world_restore();
    void trace_direct_troll_precombat();
    void trace_direct_troll_begin();
    void trace_direct_troll_tile(const char *stage) const;
    void trace_direct_troll_save(const char *stage) const;
    static void terrain_write(void *, const openu5::TerrainWrite &);
    void cast_selected_spell(int16_t spell);
    void start_magic_ceremony(int index);
    void synchronize_after_debug(openu5::WorldPosition before, bool dungeon_before);
    void drain_pending_npc_initiation();
    void synchronize_loaded_world();
    // Batch 53A: keep UiMode::Ending in step with the live game's game-won.
    void synchronize_ending(const char *site);
    void service_frontend_intent();
    void service_system_menu_intent();
    DeviceDebugScreen debug_screen() const;
    const DeviceShopView *compose_shop_view();
    const DeviceSelectionView *compose_selection_view();
    DevicePartyHighlight compose_party_highlight() const;
    const DeviceContextActionBar *compose_context_bar();
    void modal(const openu5::UiIntent &);
    // Pushes the two narrow world facts UiSession's exploration key routing
    // needs (sails context, harpsichord context) from authoritative runtime
    // state into the session, immediately before an input is routed.  See
    // UiSession::set_sail_context/set_harpsichord_active (R-19/R-20).
    void refresh_session_context();
    /** Release whatever of the deferred Blackthorn scene is due; true = redraw. */
    bool service_blackthorn_scene();
    /** ms of this device's own shake window still owed at `now_us`. */
    int64_t quake_remaining_ms(int64_t now_us) const;
    /** Release whatever of the deferred Refuge/TrollSneak/Camp scene is due. */
    bool service_narrative_scene();
    /**
     * Batch 51. The one place the scene pacers are attached to their storage
     * and given their cadence -- initialize() and the host fixture both call
     * it, so a host test can never run a differently wired pacer.
     */
    void bind_scene_pacers(bool paced);
    /** Apply a Camp scene visual event to the CampFire stage; false = not one. */
    bool apply_camp_scene_event(const openu5::GameEvent &);
    /** Forward sink of the narrative pacer: Camp stage, status panel, or session. */
    static void release_scene_event(void *, const openu5::GameEvent &);
    /** Advance the poison roster flash; true = redraw. */
    bool service_poison_flash();
    /** Beat sink for the narrative pacer (append / continue / cue / phase). */
    static void narrative_beat(void *, const openu5::NarrativeSceneBeat &);
    void open_selection(openu5::UiMode, openu5::UiRequestId);
    /** Open the (Z)-stats page axis on `member`'s stats page (R-22). */
    void open_zstats(int member);
    /** Route one input into the open (Z)-stats modal; true = it was consumed. */
    bool handle_zstats_input(const openu5::UiAction &);
    /** The possession bits the Items page needs that GameState does not own. */
    openu5::ZStatsInput zstats_input() const;
    static size_t selection_count(void *);
    static openu5::UiSelectionItem selection_item(void *, size_t);
    const char *overlay() const;
    void compose_creation_art();

    static size_t object_count(void *);
    static openu5::QuestObject object_read(void *, size_t);
    static bool object_reserve(void *, size_t);
    static void object_append(void *, const openu5::QuestObject &);
    static void object_erase(void *, size_t);
    static void object_write(void *, size_t, const openu5::QuestObject &);
    static int32_t tile_at(void *, int32_t, int32_t);
    static void volatile_tile(void *, int32_t, int32_t, int32_t);
    static void persistent_tile(void *, int32_t, int32_t, int32_t);
    static bool command_effect(void *, openu5::CommandEffect, openu5::EventSink);
    static void command_reload(void *, openu5::ReloadEffect, uint8_t, openu5::EventSink);
    static const char *banner(void *, uint8_t);
    // Batch 29: the device's RestServices, bound in ONE place so initialize()
    // and the host-test fixture share the exact callbacks (H-154/H-155).
    void bind_rest_services();
    // Batch 53 (RB-1 .. RB-4). The same rule for every QuestWorldServices and
    // ShopServices hook: the pack-backed end_record / karma_record / words,
    // the moonstone owner and the shop's ship / horse / reserve are bound here
    // and nowhere else, and initialize() and the host fixture both call these.
    // Batch 52 found four release blockers hidden by fixture/driver copies.
    void bind_quest_services();
    void bind_shop_services();
    static const char *karma_speech(void *, int32_t);
    /** ULTIMA.EXE 0x368E find_object_at_xy != 0: an NPC or an object of the
     *  current location at (x, y, floor) -- the 1988 table holds both. */
    bool object_or_npc_at(int32_t x, int32_t y, int32_t floor) const;

    // Batch 22 -- temporary U5OBJ trace of Lord British's Castle basement
    // objects (location 17, floor -1). Silent for every other location. It
    // observes the pool, the hydration decisions and the first snapshots; it
    // never changes what any of them do.
    openu5::InteriorHydrationTrace hydration_trace_{};
    bool u5obj_hydrating_ = false;
    bool u5obj_in_basement_ = false;
    uint8_t u5obj_present_seen_ = 0, u5obj_render_pending_ = 0;
    uint32_t u5obj_hydrate_calls_ = 0, u5obj_accepted_ = 0, u5obj_dropped_ = 0;
    static void u5obj_begin(void *, int32_t, const openu5::NpcLocationData *, size_t);
    static void u5obj_slot(void *, size_t, const openu5::NpcSlot &, uint8_t, const char *,
                           const openu5::QuestObject *);
    static void u5obj_end(void *, int32_t, bool, const char *);
    void u5obj_dump_pool(const char *stage) const;
    void u5obj_trace_present(const openu5::PresentationSnapshot &);
    void u5obj_trace_render(const openu5::PresentationSnapshot &);
    void u5obj_bind();
    static void start_smoke(void *, int group);
    // A3-01. Developer > Diagnostics > "Audio test tone (SFX)".
    static void audio_test_tone(void *);
    // A3-04A. Developer > Diagnostics > "Audio performance" (the ~47 s
    // benchmark, ticked from render()) and "Audio stats (live)".
    static void audio_perf_start(void *);
    static void audio_stats_now(void *);
    static bool music_bypass_probe(void *, bool toggle);
    static openu5::SdLogState sd_log_probe(void *, bool toggle);
    // A3-04E. Developer > Diagnostics > "Probe: legacy TFT pacing" / "... loop spin".
    static bool legacy_tft_probe(void *, bool toggle);
    static bool legacy_loop_probe(void *, bool toggle);
    void service_audio_benchmark(int64_t now_us);
    // A3-04B. The report view (section 19.3) and the three windows it reads.
    void reset_perf_windows();
    void publish_perf_report(const char *title, const openu5::AudioPerfSnapshot *first, const char *first_heading,
                             const openu5::AudioPerfSnapshot *second, const char *second_heading, bool with_guard);
    bool handle_perf_report_input(const openu5::UiAction &);
    /** handle()'s body; handle() wraps it to time each input (section 19.5). */
    bool handle_input_event(const RawInputEvent &);
    // A3-01. The one binder of the Developer diagnostics services, shared by
    // initialize() and the host fixture (the fixture used to copy the call).
    void bind_developer_diagnostics();
    // A3-01. settings.json -> settings_ -> the input adapter and the audio
    // service; initialize() and the host fixture both run it.
    void load_device_settings();
    /** Push settings_ to every device consumer (input adapter, audio volumes). */
    void apply_device_settings();
    /**
     * A presented event's sound, forwarded to the service: an Sfx cue as the
     * core named it, and (A3-02) the two sounds the original makes for events
     * that are not Sfx cues -- the magic ceremony CAST2.OVL:0x0000(index) and
     * the arena hit/death bursts (the reference's sfxForCombatEvent).
     */
    void present_audio(const openu5::GameEvent &);
    // A3-04. Re-derives the music context from EXISTING runtime state (no
    // new gameplay events) and hands it to AudioService::play_music(), which
    // itself de-dupes by song -- so calling this liberally costs nothing
    // when nothing changed. Mirrors the patch driver's own behaviour: it
    // re-derives on every key poll (handle()) rather than being told when to
    // change. See ALPHA3_AUDIO.md section 17.6 for the priority order.
    void sync_music();
    /** True when combat actor `id` is a party member (the 0x35ac side test). */
    bool combat_actor_is_player(int32_t id) const;
    /** A3-02: the Blackthorn pacer's beat cues, released with their beat. */
    static void blackthorn_cue(void *, openu5::BlackthornSfx);
    /**
     * A3-03: one ambient_sfx_tick 0x4102 per 55 ms tick while the original
     * would be waiting in getkey_with_redraw 0x266c (world, town or arena; no
     * dungeon, scene, menu, Ending or An Tym). Presentation only.
     */
    void service_ambient(int64_t now_us);
    /** A3-03: a new world (load, title): the ambient counters start over. */
    void reset_ambient();
};

} // namespace tdeck
