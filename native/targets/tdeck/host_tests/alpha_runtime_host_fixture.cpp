// Batch 11 host-test seam.
//
// Defines AlphaRuntime::attach_host_test_fixture() (declared in
// alpha_runtime.h, "Batch 11 host-test seam" section). This wires the same
// CommandContext/CombatContext/DungeonContext graph AlphaRuntime::initialize()
// wires for production -- lambda for lambda, matching initialize() exactly
// wherever a piece is included below -- except:
//   * every buffer comes from `new` instead of heap_caps PSRAM;
//   * nothing here reads an AlphaResourcePack or touches Board;
//   * resource-pack-derived tables are copied in only when the test supplies
//     the loaded pack (HostTestFixture::pack); otherwise they stay empty.
//
// Since Batch 53 no gameplay SERVICE is declared here: the quest, shop, rest
// and scene-pacer hooks come from the same production binders initialize()
// calls (bind_quest_services, bind_shop_services, bind_rest_services,
// bind_scene_pacers). batch53_release_blockers checks that from the source.
//
// No gameplay routing is copied or duplicated here: command()/dispatch()/
// handle() are called completely unmodified by whoever holds the
// AlphaRuntime this attaches to. Only the CMake target under
// native/targets/tdeck/host_tests links this file, so it never reaches
// T-Deck firmware; production's initialize() is untouched and does not call
// this method.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "openu5/npc_path.h"
#include <algorithm>
#include <cstring>
#include <iterator>
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
        if (fixture.indexed_test_tiles) {
            for (int i=0;i<16;++i) tile_cache_.palette[i]=uint16_t(i*0x1111);
            for (int tile=0;tile<512;++tile)
                std::fill(tile_cache_storage_+tile*128,tile_cache_storage_+(tile+1)*128,
                          uint8_t((tile&15)*0x11));
        }
        if (fixture.patterned_test_tiles) {
            for (int i=0;i<16;++i) tile_cache_.palette[i]=uint16_t(i*0x1111);
            for (int tile=0;tile<512;++tile)
                for (int b=0;b<128;++b)
                    tile_cache_storage_[tile*128+b]=uint8_t(tile*37+b*11+(b>>3)*7);
        }
    }

    transcript_ = new openu5::UiTextBlock[kHostTestTranscriptBlocks]();
    // A3-04B: the two PSRAM buffers initialize() gives the Developer screen
    // (its view and the perf report's rows), so a host test can render with
    // the menu open. Before this, rendering in DebugMenu mode dereferenced a
    // null debug_view_ on the host.
    debug_view_ = new DeviceDebugScreen();
    perf_report_lines_ = new char[openu5::kPerfReportMaxLines][openu5::kPerfReportLineBytes]();
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

    // Batch 24: the pack-derived members initialize() assigns for the Save
    // template and for town combat (alpha_runtime.cpp, the combat_context_/
    // combat_resources_ block), then the New Journey's own load_native_state
    // over INIT.GAM, so Save exports over a real document.
    // Batch 53: also the pack tables the production binders read -- moon
    // phases, search objects, shard spawns, the shop tables and text, ENDMSG,
    // KARMA and the Words of Power -- and all of it BEFORE the binders run,
    // because initialize() also binds after its INIT.GAM load: the moonstone
    // owner hydrates from that document. Data only; no hook is set here.
    if (const auto *pack = fixture.pack) {
        resources_.initial_gam = pack->initial_gam; resources_.initial_gam_size = pack->initial_gam_size;
        resources_.initial_ool = pack->initial_ool; resources_.initial_ool_size = pack->initial_ool_size;
        resources_.combat_map_views = pack->combat_map_views; resources_.combat_map_count = pack->combat_map_count;
        resources_.combat_enemy_views = pack->combat_enemy_views; resources_.combat_enemy_count = pack->combat_enemy_count;
        resources_.combat_tables = pack->combat_tables; resources_.combat_table_count = pack->combat_table_count;
        resources_.moon_phases = pack->moon_phases; resources_.moon_phase_count = pack->moon_phase_count;
        // A3-04E: the sky strip's glyphs; the real Board (a3_04e_pacing_runtime)
        // refuses a world frame without them, the capture stub never read them.
        resources_.runes_font = pack->runes_font;
        resources_.search_objects = pack->search_objects; resources_.search_count = pack->search_count;
        resources_.shard_spawns = pack->shard_spawns; resources_.shard_spawn_count = pack->shard_spawn_count;
        resources_.shop_data = pack->shop_data;
        resources_.shop_text_offsets = pack->shop_text_offsets; resources_.shop_text_records = pack->shop_text_records;
        resources_.shop_text_record_count = pack->shop_text_record_count;
        resources_.end_text_offsets = pack->end_text_offsets; resources_.end_text_records = pack->end_text_records;
        resources_.end_text_record_count = pack->end_text_record_count;
        resources_.karma_text_offsets = pack->karma_text_offsets; resources_.karma_text_records = pack->karma_text_records;
        resources_.karma_text_record_count = pack->karma_text_record_count;
        std::copy(std::begin(pack->words), std::end(pack->words), std::begin(resources_.words));
        resources_.word_count = pack->word_count;
        // A3-03: the attract demo's View data, bound as initialize() binds it
        // ("intro-view-bind"), so a host test can run the demo and hear its cues.
        resources_.intro_view = pack->intro_view;
        intro_view_.bind(resources_.intro_view);
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
        openu5::save::SidecarSource source;
        openu5::save::load_native_state(resources_.initial_gam, resources_.initial_gam_size, nullptr, game_, turn_,
                                        retained_, source, true);
    }
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
    // Batch 51: the scene pacers' storage, sized by the same class constants
    // initialize() uses and attached by the same production binder. Before
    // this the fixture attached none, so a Refuge/TrollSneak event reaching
    // the host runtime was taken by the pacer and silently dropped.
    blackthorn_steps_ = new openu5::BlackthornSceneStep[kBlackthornSceneSteps]();
    blackthorn_scene_text_ = new char[kBlackthornSceneTextBytes]();
    blackthorn_scene_grid_ = new int16_t[openu5::kBlackthornSceneCells]();
    narrative_steps_ = new openu5::NarrativeSceneStep[kNarrativeSceneSteps]();
    narrative_text_ = new char[kNarrativeSceneTextBytes]();
    bind_scene_pacers(fixture.paced_scenes);

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

    // Batch 53: production's own binder, not a copy. Until this batch the
    // fixture re-declared these hooks -- and, like initialize(), left
    // end_record / karma / words / moonstones unbound (RB-1 .. RB-3).
    bind_quest_services();

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
    // Batch 53: the pack block above now runs first; the pack's enemy table
    // keeps precedence, as it did when that block ran last.
    if (!fixture.pack) {
        combat_context_.enemy_defs = fixture.enemy_defs;
        combat_context_.enemy_def_count = fixture.enemy_def_count;
    }

    // Batch 53: production's own binder (RB-4 / H-146 hid behind a copy here).
    bind_shop_services();

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

    terrain_.refresh(resources_.world, game_);

#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    // Batch 25: the Developer menu initialize() constructs over the same
    // context_ (alpha_runtime.cpp: new UiDebugMenu(context_), attach_
    // diagnostics, ui_->attach_debug_menu), so Alt+D opens the real menu and a
    // host test can walk it with raw keys. No existing host test sends Alt+D.
    debug_ = new openu5::UiDebugMenu(context_);
    bind_developer_diagnostics(); // A3-01: the production binder, not a copy
    ui_->attach_debug_menu(debug_);
#endif

    // Production's initialize() calls frontend_.start(...), which leaves the
    // session on the title screen until a real Continue/New-Game menu flow
    // runs (frontend_.active()==true, and handle()'s very first branch routes
    // ALL input there instead of gameplay). Host-test fixtures start with a
    // game already in progress -- the same state frontend_.complete_intent()
    // reaches in production once Continue/New Game actually succeeds -- so
    // command-routing tests exercise gameplay input, not the title menu.
    // A3-01: settings.json through the same binder initialize() runs (the
    // host stubs' store is empty unless a test opts in), so a test sees the
    // device's load-and-apply path, not a copy of it.
    load_device_settings();
    frontend_.enter_game();
}

} // namespace tdeck
