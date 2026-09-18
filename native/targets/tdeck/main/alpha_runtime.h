#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "alpha_resources.h"
#include "alpha_dialogue.h"
#include "alpha_save.h"
#include "asset_pack.h"
#include "openu5/combat.h"
#include "openu5/dialogue_orchestration.h"
#include "openu5/dungeon_encounters.h"
#include "openu5/look.h"
#include "openu5/blackthorn.h"
#include "openu5/outdoor.h"
#include "openu5/shop_orchestration.h"
#include "openu5/shrine.h"
#include "openu5/ui_debug_menu.h"
#include "openu5/ui_session.h"
#include "openu5/frontend.h"
#include "openu5/intro_view.h"
#include "openu5/system_menu.h"
#include "openu5/world_terrain.h"
#include "native_renderer.h"
#include "device_smoke_tests.h"
#include "device_ui_views.h"
#include "ui_input_adapter.h"

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

    static void dispatch_ui(void *, const openu5::UiIntent &);
    static void dispatch_event(void *, const openu5::GameEvent &);
    void consume_event(const openu5::GameEvent &);
    void dispatch(const openu5::UiIntent &);
    void command(openu5::Command);
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
    void synchronize_loaded_world();
    void service_frontend_intent();
    void service_system_menu_intent();
    DeviceDebugScreen debug_screen() const;
    const DeviceShopView *compose_shop_view();
    const DeviceSelectionView *compose_selection_view();
    DevicePartyHighlight compose_party_highlight() const;
    const DeviceContextActionBar *compose_context_bar();
    void modal(const openu5::UiIntent &);
    void open_selection(openu5::UiMode, openu5::UiRequestId);
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
