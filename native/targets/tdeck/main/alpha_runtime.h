#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "alpha_resources.h"
#include "alpha_dialogue.h"
#include "alpha_save.h"
#include "asset_pack.h"
#include "dungeon_art_cache.h"
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
struct DeviceDebugScreen;

class AlphaRuntime {
  public:
    esp_err_t initialize(AlphaResourcePack &, AlphaResourceReport &, openu5::AssetPackReader &,
                         const openu5::AssetPackReport &);
    bool handle(const RawInputEvent &);
    esp_err_t render(Board &, bool force = false);
    void log_metrics(const char *checkpoint) const;
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
    };
    void attach_host_test_fixture(const HostTestFixture &);
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
    uint32_t rendered_frontend_title_frame_ = UINT32_MAX;
    uint32_t rendered_attract_frame_ = UINT32_MAX;
    uint8_t applied_brightness_ = 0;
    bool dungeon_presentation_pending_ = false;
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
    /** Release whatever of the deferred Refuge/TrollSneak scene is due. */
    bool service_narrative_scene();
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
    static void start_smoke(void *, int group);
};

} // namespace tdeck
