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

    transcript_ = new openu5::UiTextBlock[kHostTestTranscriptBlocks]();
    ui_ = new openu5::UiSession({transcript_, kHostTestTranscriptBlocks}, {this, dispatch_ui}, {11, 12, 63});

    context_.actors = &actors_;
    // commands.cpp's context gate rejects EVERY command (Move included) when
    // context_.actors is non-null but context_.npc_scratch is null -- the
    // same combination production's initialize() never produces (it always
    // binds npc_scratch to astar_scratch_ in the same breath as actors).
    context_.npc_scratch = new openu5::NpcScanGrid();
    context_.npc_data = resources_.npc_locations;
    context_.npc_data_count = 32;
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
    quest_.tile_at = tile_at; quest_.volatile_tile = volatile_tile; quest_.persistent_tile = persistent_tile;
    quest_.search_objects = nullptr; quest_.search_count = 0;
    quest_.spawns = nullptr; quest_.spawn_count = 0;
    quest_.moon_phases = nullptr; quest_.moon_phase_count = 0;

    dialogue_assets_.bind(nullptr, 0);
    dialogue_services_.registry = {&dialogue_assets_, AlphaDialogueCache::lookup};

    shrine_services_.data = &resources_.shrine_data;
    shrine_services_.context = this;
    shrine_services_.record = [](void *, int32_t) -> const char * { return nullptr; };

    dungeon_encounters_.combat = &combat_context_;
    dungeon_encounters_.arenas = nullptr;
    dungeon_encounters_.count = 0;
    dungeon_context_.encounters = &dungeon_encounters_;

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
    static openu5::RestServices rest_owner;
    rest_owner.context = this;
    rest_owner.snap_npcs = [](void *) {};
    rest_owner.occupied = [](void *, int32_t, int32_t, int32_t) { return false; };
    rest_owner.cell_free = [](void *, int32_t, int32_t) { return true; };
    rest_owner.karma_record = [](void *, int32_t) {
        return "\"Rest well, Avatar. Continue upon the path of virtue.\"";
    };
    context_.rest_services = &rest_owner;

    terrain_.refresh(resources_.world, game_);

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
