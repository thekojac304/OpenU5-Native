// Batch 11 host-test seam.
//
// Defines AlphaRuntime::attach_host_test_fixture() (declared in
// alpha_runtime.h, "Batch 11 host-test seam" section). This wires the same
// CommandContext/CombatContext/DungeonContext graph AlphaRuntime::initialize()
// wires for production -- lambda for lambda, matching initialize() exactly
// wherever a piece is included below -- except:
//   * every buffer comes from `new` instead of heap_caps PSRAM;
//   * nothing here reads an AlphaResourcePack or touches Board;
//   * resource-pack-derived tables that only matter once combat/dungeon/
//     shop/save actually run (combat enemy defs, dungeon data, shop/shrine
//     text tables, the transport/rest service singletons) are left at their
//     default (empty/null) state, which is the same quiescent state the
//     production game is in before any of those systems has anything to do.
//
// No gameplay routing is copied or duplicated here: command()/dispatch()/
// handle() are called completely unmodified by whoever holds the
// AlphaRuntime this attaches to. Only the CMake target under
// native/targets/tdeck/host_tests links this file, so it never reaches
// T-Deck firmware; production's initialize() is untouched and does not call
// this method.
#include "../main/alpha_runtime.h"
#include "openu5/npc_path.h"
#include <algorithm>
#include <cstring>
#include "openu5/rest.h"

namespace tdeck {

namespace {
// Independent of alpha_runtime.cpp's own kTranscriptBlocks (a production
// PSRAM sizing constant, private to that translation unit) -- host tests
// have no PSRAM budget to respect, just enough ring-buffer depth that a test
// run cannot wrap it out from under an assertion.
constexpr size_t kHostTestTranscriptBlocks = 256;
}

void AlphaRuntime::attach_host_test_fixture(const HostTestFixture &fixture) {
    resources_.world = fixture.world;
    if (fixture.render_pixels) {
        viewport_ = new uint16_t[openu5::kViewportPixelCount]();
        tile_cache_storage_ = new uint8_t[openu5::kCachedTileBytes]();
        tile_cache_.tiles = tile_cache_storage_;
        std::fill(std::begin(tile_cache_.palette), std::end(tile_cache_.palette), uint16_t(0xffff));
    }

    transcript_ = new openu5::UiTextBlock[kHostTestTranscriptBlocks]();
    ui_ = new openu5::UiSession({transcript_, kHostTestTranscriptBlocks}, {this, dispatch_ui}, {11, 12, 63});

    context_.actors = &actors_;
    // commands.cpp's context gate rejects EVERY command (Move included) when
    // context_.actors is non-null but context_.npc_scratch is null -- the
    // same combination production's initialize() never produces (it always
    // binds npc_scratch to astar_scratch_ in the same breath as actors).
    context_.npc_scratch = new openu5::NpcScanGrid();
    // Batch 22: a fixture may supply the pack's 32 .NPC tables, copied into
    // the same resources_.npc_locations initialize() fills from npcs.bin.
    if (fixture.npc_locations)
        std::copy(fixture.npc_locations, fixture.npc_locations + 32, resources_.npc_locations);
    context_.npc_data = resources_.npc_locations;
    context_.npc_data_count = 32;
    // Batch 21A: a fixture may supply the overworld location table so that the
    // real (E)nter command path (commands.cpp's location_at -> EnterDungeon)
    // can be driven from a host test. Supplying none keeps the previous
    // resource-pack-derived (empty) table, so existing tests are unchanged.
    if (fixture.location_x && fixture.location_y && fixture.location_count) {
        const size_t n = std::min(fixture.location_count, sizeof(resources_.location_x));
        std::memcpy(resources_.location_x, fixture.location_x, n);
        std::memcpy(resources_.location_y, fixture.location_y, n);
        resources_.location_count = n;
    }
    context_.locations = {resources_.location_x, resources_.location_y,
                           resources_.location_count, resources_.location_count};
    context_.combat_context = &combat_context_;
    context_.dungeon_context = &dungeon_context_;
    context_.dialogue_services = &dialogue_services_;
    context_.shop_services = &shop_services_;
    context_.shrine_services = &shrine_services_;
    context_.outdoor = &outdoor_;
    context_.terrain = &terrain_;
    context_.quest_world = &quest_;
    terrain_.writes = {this, terrain_write};
    context_.services = {this, command_effect, command_reload, banner};
    context_.events = {this, dispatch_event};

    blackthorn_scene_services_.capture_tiles = nullptr;
    blackthorn_scene_services_.state = &blackthorn_scene_state_;
    blackthorn_scene_services_.script = nullptr;
    context_.blackthorn = &blackthorn_;
    context_.blackthorn_scene = &blackthorn_scene_services_;

    look_services_.context = this;
    look_services_.describe = [](void *p, int32_t tile) {
        auto &r = *static_cast<AlphaRuntime *>(p);
        return tile >= 0 && size_t(tile) < r.resources_.look_count
                   ? r.resources_.look_text + r.resources_.look_offsets[tile]
                   : "something";
    };
    look_services_.sign = [](void *p, openu5::MapId map, int32_t x, int32_t y) {
        auto &r = *static_cast<AlphaRuntime *>(p);
        return openu5::resolve_look_sign(r.resources_.signs, r.resources_.sign_count, map, x, y);
    };
    context_.look = &look_services_;

    quest_.context = this;
    quest_.count = object_count; quest_.read = object_read; quest_.reserve = object_reserve;
    quest_.append = object_append; quest_.erase = object_erase; quest_.write = object_write;
    u5obj_bind(); // Batch 22: same diagnostic observer initialize() binds.
    quest_.tile_at = tile_at; quest_.volatile_tile = volatile_tile; quest_.persistent_tile = persistent_tile;
    quest_.search_objects = nullptr; quest_.search_count = 0;
    quest_.spawns = nullptr; quest_.spawn_count = 0;
    quest_.moon_phases = nullptr; quest_.moon_phase_count = 0;

    dialogue_assets_.bind(nullptr, 0);
    dialogue_services_.registry = {&dialogue_assets_, AlphaDialogueCache::lookup};

    shrine_services_.data = &resources_.shrine_data;
    shrine_services_.context = this;
    shrine_services_.record = [](void *, int32_t) -> const char * { return nullptr; };

    // Batch 21A. The SAME combat storage initialize() carves out of PSRAM
    // (alpha_runtime.cpp: 32 overflow actors, 32 overflow loot piles, 32 arena
    // fields), from `new` instead. Without it a host test runs combat on the
    // bare inline kCombatActors(22) roster while the device has 22+32=54, and
    // every capacity precondition in combat.cpp would be measured against the
    // wrong number.
    combat_actor_overflow_ = new openu5::CombatActor[32]();
    combat_pile_overflow_ = new openu5::CombatLootPile[32]();
    combat_fields_ = new openu5::CombatField[32]();
    combat_.actors.overflow = combat_actor_overflow_;
    combat_.actors.overflow_capacity = 32;
    combat_.piles.overflow = combat_pile_overflow_;
    combat_.piles.overflow_capacity = 32;
    combat_.fields = combat_fields_;

    // Batch 21A. Same four assignments initialize() makes from the SD resource
    // pack (alpha_runtime.cpp: dungeon_context_.data/count, dungeon_arenas_ ->
    // dungeon_encounters_.arenas/count, combat_context_.enemy_defs/count). A
    // fixture that supplies none of them keeps the pre-Batch-21A behaviour
    // exactly -- null arenas, zero counts -- so every existing host test is
    // untouched.
    dungeon_encounters_.combat = &combat_context_;
    dungeon_encounters_.arenas = fixture.arenas;
    dungeon_encounters_.count = fixture.arena_count;
    dungeon_context_.encounters = &dungeon_encounters_;
    dungeon_context_.data = fixture.dungeons;
    dungeon_context_.count = fixture.dungeon_count;
    combat_context_.enemy_defs = fixture.enemy_defs;
    combat_context_.enemy_def_count = fixture.enemy_def_count;

    shop_services_.context = this;
    shop_services_.record_present = [](void *, int32_t) { return false; };
    shop_services_.record = [](void *, int32_t) -> const char * { return nullptr; };
    shop_services_.tile = [](void *p, int32_t x, int32_t y) { return tile_at(p, x, y); };
    shop_services_.occupied = [](void *p, int32_t x, int32_t y) {
        auto &r = *static_cast<AlphaRuntime *>(p);
        for (size_t i = 0; i < r.actors_.count; ++i)
            if (r.actors_.actors[i].location == r.game_.position.map.location &&
                r.actors_.actors[i].z == r.game_.position.map.floor &&
                r.actors_.actors[i].x == x && r.actors_.actors[i].y == y)
                return true;
        for (const auto &o : r.objects_)
            if (o.location == r.game_.position.map.location && o.floor == r.game_.position.map.floor &&
                o.x == x && o.y == y)
                return true;
        return false;
    };
    shop_services_.plate = [](void *p, int32_t x, int32_t y, int32_t tile) { volatile_tile(p, x, y, tile); };
    shop_services_.hour_tiles = [](void *p) {
        auto &r = *static_cast<AlphaRuntime *>(p);
        r.terrain_.refresh(r.resources_.world, r.game_);
    };
    shop_services_.wake_npcs = [](void *) {};
    shop_services_.transactional_services = true;

    // Matches initialize()'s own static-owner pattern exactly (alpha_runtime.cpp,
    // "auto transport=openu5::world_transport_services(context_);"). Move/travel
    // dispatch through core needs context_.transport_services non-null even on
    // foot; production wires it unconditionally, so the host fixture does too.
    static openu5::TransportServices transport_owner;
    transport_owner = openu5::world_transport_services(context_);
    context_.transport_services = &transport_owner;
    // Batch 29: production's own binding, not a copy -- H-154/H-155 were a
    // device-only wiring defect, so a fixture-side duplicate would test itself.
    bind_rest_services();

    // Batch 24: the pack-derived members initialize() assigns for the Save
    // template and for town combat (alpha_runtime.cpp, the combat_context_/
    // combat_resources_/quest_ block), then the New Journey's own
    // load_native_state over INIT.GAM, so Save exports over a real document.
    if (const auto *pack = fixture.pack) {
        resources_.initial_gam = pack->initial_gam; resources_.initial_gam_size = pack->initial_gam_size;
        resources_.initial_ool = pack->initial_ool; resources_.initial_ool_size = pack->initial_ool_size;
        resources_.combat_map_views = pack->combat_map_views; resources_.combat_map_count = pack->combat_map_count;
        resources_.combat_enemy_views = pack->combat_enemy_views; resources_.combat_enemy_count = pack->combat_enemy_count;
        resources_.combat_tables = pack->combat_tables; resources_.combat_table_count = pack->combat_table_count;
        combat_context_.tables = {resources_.combat_tables, resources_.combat_tables + resources_.combat_table_count,
                                  resources_.combat_tables + resources_.combat_table_count * 2,
                                  resources_.combat_tables + resources_.combat_table_count * 3,
                                  resources_.combat_table_count};
        combat_context_.enemy_defs = resources_.combat_enemy_views;
        combat_context_.enemy_def_count = resources_.combat_enemy_count;
        combat_resources_.maps = resources_.combat_map_views; combat_resources_.map_count = resources_.combat_map_count;
        combat_resources_.enemies = resources_.combat_enemy_views; combat_resources_.enemy_count = resources_.combat_enemy_count;
        combat_resources_.tables = combat_context_.tables;
        outdoor_.combat = &combat_context_; outdoor_.resources = &combat_resources_; outdoor_.prize_owner = &quest_;
        quest_.encounter = &combat_context_; quest_.combat_resources = &combat_resources_;
        openu5::save::SidecarSource source;
        openu5::save::load_native_state(resources_.initial_gam, resources_.initial_gam_size, nullptr, game_, turn_,
                                        retained_, source, true);
    }

    terrain_.refresh(resources_.world, game_);

#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    // Batch 25: the Developer menu initialize() constructs over the same
    // context_ (alpha_runtime.cpp: new UiDebugMenu(context_), attach_
    // diagnostics, ui_->attach_debug_menu), so Alt+D opens the real menu and a
    // host test can walk it with raw keys. No existing host test sends Alt+D.
    debug_ = new openu5::UiDebugMenu(context_);
    debug_->attach_diagnostics({this, start_smoke});
    ui_->attach_debug_menu(debug_);
#endif

    // Production's initialize() calls frontend_.start(...), which leaves the
    // session on the title screen until a real Continue/New-Game menu flow
    // runs (frontend_.active()==true, and handle()'s very first branch routes
    // ALL input there instead of gameplay). Host-test fixtures start with a
    // game already in progress -- the same state frontend_.complete_intent()
    // reaches in production once Continue/New Game actually succeeds -- so
    // command-routing tests exercise gameplay input, not the title menu.
    frontend_.enter_game();
}

} // namespace tdeck
