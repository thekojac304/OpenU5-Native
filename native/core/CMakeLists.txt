cmake_minimum_required(VERSION 3.20)
project(openu5_core LANGUAGES CXX)
option(OPENU5_ENABLE_DEVELOPER_TOOLS "Link the native developer/debug APIs" OFF)
include(sources.cmake)
add_library(openu5_core STATIC ${OPENU5_CORE_SOURCES})
target_include_directories(openu5_core PUBLIC include)
target_compile_features(openu5_core PUBLIC cxx_std_17)
if(OPENU5_ENABLE_DEVELOPER_TOOLS)
  target_compile_definitions(openu5_core PUBLIC OPENU5_ENABLE_DEVELOPER_TOOLS=1)
endif()
if(MSVC)
  target_compile_options(openu5_core PRIVATE /W4)
else()
  target_compile_options(openu5_core PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
include(CTest)
option(OPENU5_REAL_ARENAS "Validate extracted original combat arenas (requires user assets and Node)" OFF)
if(BUILD_TESTING)
  add_executable(frontend_tests tests/frontend_test.cpp)
  target_link_libraries(frontend_tests PRIVATE openu5_core)
  add_test(NAME frontend COMMAND frontend_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/../../game/assets/init.gam"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../game/assets/init.ool"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../original/u5/ultima5/MISCMAPS.DAT")
  add_executable(ui_session_tests tests/ui_session_test.cpp)
  target_link_libraries(ui_session_tests PRIVATE openu5_core)
  add_test(NAME ui_session COMMAND ui_session_tests)
  add_executable(display_names_tests tests/display_names_test.cpp)
  target_link_libraries(display_names_tests PRIVATE openu5_core)
  add_test(NAME display_names COMMAND display_names_tests)
  add_executable(quest_search_tests tests/quest_search_test.cpp)
  target_link_libraries(quest_search_tests PRIVATE openu5_core)
  add_test(NAME quest_search COMMAND quest_search_tests)
  add_executable(batch21b_chest_reset tests/batch21b_chest_reset_test.cpp)
  target_link_libraries(batch21b_chest_reset PRIVATE openu5_core)
  add_test(NAME batch21b_chest_reset COMMAND batch21b_chest_reset)
  set(DEBUG_LABELS_TEST_SOURCES tests/debug_labels_test.cpp)
  if(NOT OPENU5_ENABLE_DEVELOPER_TOOLS)
    list(APPEND DEBUG_LABELS_TEST_SOURCES src/debug_labels.cpp)
  endif()
  add_executable(debug_labels_tests ${DEBUG_LABELS_TEST_SOURCES})
  target_link_libraries(debug_labels_tests PRIVATE openu5_core)
  add_test(NAME debug_labels COMMAND debug_labels_tests)
  add_executable(ui_host_harness tools/ui_host_harness.cpp)
  target_link_libraries(ui_host_harness PRIVATE openu5_core)
  add_executable(measure_ui_sizes tools/measure-ui-sizes.cpp)
  target_link_libraries(measure_ui_sizes PRIVATE openu5_core)
  if(OPENU5_ENABLE_DEVELOPER_TOOLS)
    add_executable(ui_debug_menu_tests tests/ui_debug_menu_test.cpp)
    target_link_libraries(ui_debug_menu_tests PRIVATE openu5_core)
    add_test(NAME ui_debug_menu COMMAND ui_debug_menu_tests)
    set(UI_DEBUG_TEST_TARGET ui_debug_menu_tests)
  endif()
  set(DEBUG_MAP_PICKER_TEST_SOURCES tests/debug_map_picker_test.cpp)
  if(NOT OPENU5_ENABLE_DEVELOPER_TOOLS)
    list(APPEND DEBUG_MAP_PICKER_TEST_SOURCES src/debug_map_picker.cpp)
  endif()
  add_executable(debug_map_picker_tests ${DEBUG_MAP_PICKER_TEST_SOURCES})
  target_link_libraries(debug_map_picker_tests PRIVATE openu5_core)
  add_test(NAME debug_map_picker COMMAND debug_map_picker_tests)
  set(DEBUG_DEVELOPER_TEST_SOURCES tests/debug_developer_test.cpp)
  if(NOT OPENU5_ENABLE_DEVELOPER_TOOLS)
    list(APPEND DEBUG_DEVELOPER_TEST_SOURCES src/debug_developer.cpp)
  endif()
  add_executable(debug_developer_tests ${DEBUG_DEVELOPER_TEST_SOURCES})
  target_link_libraries(debug_developer_tests PRIVATE openu5_core)
  add_test(NAME debug_developer COMMAND debug_developer_tests)
  add_executable(gameplay_integration_tests tests/gameplay_integration_test.cpp)
  target_link_libraries(gameplay_integration_tests PRIVATE openu5_core)
  add_test(NAME gameplay_integration COMMAND gameplay_integration_tests)
  add_executable(look_sign_tests tests/look_sign_test.cpp)
  target_link_libraries(look_sign_tests PRIVATE openu5_core)
  add_test(NAME look_sign COMMAND look_sign_tests)
  add_executable(gameplay_driver tests/gameplay_driver.cpp)
  target_link_libraries(gameplay_driver PRIVATE openu5_core)
  add_executable(movement_flow_parity_tests tests/movement_flow_parity_test.cpp)
  target_link_libraries(movement_flow_parity_tests PRIVATE openu5_core)
  add_test(NAME movement_flow_parity COMMAND movement_flow_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/movement-flow.txt")
  add_executable(quest_driver tests/quest_driver.cpp)
  target_link_libraries(quest_driver PRIVATE openu5_core)
  add_executable(shop_parity_tests tests/shop_parity_test.cpp)
  add_executable(shop_adapter_tests tests/shop_adapter_test.cpp)
  target_link_libraries(shop_adapter_tests PRIVATE openu5_core)
  add_test(NAME shop_adapters COMMAND shop_adapter_tests)
  add_executable(shop_flow_tests tests/shop_flow_test.cpp)
  target_link_libraries(shop_flow_tests PRIVATE openu5_core)
  add_test(NAME shop_flow COMMAND shop_flow_tests "${CMAKE_CURRENT_SOURCE_DIR}/build-shops/flow.txt")
  target_link_libraries(shop_parity_tests PRIVATE openu5_core)
  add_test(NAME shop_parity COMMAND shop_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/build-shops/helpers.txt")
  add_executable(persistence_driver tests/persistence_driver.cpp)
  target_link_libraries(persistence_driver PRIVATE openu5_core)
  add_executable(dialogue_adapter_tests tests/dialogue_adapter_test.cpp)
  target_link_libraries(dialogue_adapter_tests PRIVATE openu5_core)
  add_test(NAME dialogue_adapters COMMAND dialogue_adapter_tests)
  add_executable(dialogue_parity_tests tests/dialogue_parity_test.cpp)
  target_link_libraries(dialogue_parity_tests PRIVATE openu5_core)
  add_test(NAME dialogue_parity COMMAND dialogue_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/build-dialogue/dialogue.bin")
  add_executable(transport_flow_parity_tests tests/transport_flow_parity_test.cpp)
  target_link_libraries(transport_flow_parity_tests PRIVATE openu5_core)
  add_test(NAME transport_flow_parity COMMAND transport_flow_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/transport-flow.txt")
  add_executable(world_flow_adapter_tests tests/world_flow_adapter_test.cpp)
  target_link_libraries(world_flow_adapter_tests PRIVATE openu5_core)
  add_test(NAME world_flow_adapters COMMAND world_flow_adapter_tests)
  add_executable(dungeon_flow_parity_tests tests/dungeon_flow_parity_test.cpp)
  target_link_libraries(dungeon_flow_parity_tests PRIVATE openu5_core)
  add_test(NAME dungeon_flow_parity COMMAND dungeon_flow_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
  add_executable(world_flow_parity_tests tests/world_flow_parity_test.cpp)
  target_link_libraries(world_flow_parity_tests PRIVATE openu5_core)
  add_test(NAME world_flow_parity COMMAND world_flow_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
  add_executable(dungeon_parity_tests tests/dungeon_parity_test.cpp)
  target_link_libraries(dungeon_parity_tests PRIVATE openu5_core)
  add_test(NAME dungeon_parity COMMAND dungeon_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dungeon.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dungeon-maps.txt")
  add_executable(advanced_combat_parity_tests tests/advanced_combat_parity_test.cpp)
  target_link_libraries(advanced_combat_parity_tests PRIVATE openu5_core)
  add_test(NAME advanced_combat_parity COMMAND advanced_combat_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/advanced-combat.txt")
  add_executable(magic_parity_tests tests/magic_parity_test.cpp)
  target_link_libraries(magic_parity_tests PRIVATE openu5_core)
  add_test(NAME magic_parity COMMAND magic_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/magic.txt")
  add_executable(combat_parity_tests tests/combat_parity_test.cpp)
  target_link_libraries(combat_parity_tests PRIVATE openu5_core)
  add_test(NAME combat_parity COMMAND combat_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/combat.txt")
  add_executable(item_parity_tests tests/item_parity_test.cpp)
  target_link_libraries(item_parity_tests PRIVATE openu5_core)
  add_test(NAME item_parity COMMAND item_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/items.txt")
  add_executable(command_parity_tests tests/command_parity_test.cpp)
  target_link_libraries(command_parity_tests PRIVATE openu5_core)
  add_test(NAME command_parity COMMAND command_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/commands.txt")
  add_executable(core_parity_tests tests/parity_test.cpp)
  target_link_libraries(core_parity_tests PRIVATE openu5_core)
  add_test(NAME core_parity COMMAND core_parity_tests)
  add_executable(turn_parity_tests tests/turn_parity_test.cpp)
  add_executable(travel_parity_tests tests/travel_parity_test.cpp)
  target_link_libraries(travel_parity_tests PRIVATE openu5_core)
  add_test(NAME travel_parity COMMAND travel_parity_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/travel.txt")
  target_link_libraries(turn_parity_tests PRIVATE openu5_core)
  add_test(NAME turn_parity COMMAND turn_parity_tests)
  add_executable(movement_regression ../targets/tdeck/host_tests/movement_test.cpp)
  target_link_libraries(movement_regression PRIVATE openu5_core)
  add_test(NAME movement_regression COMMAND movement_regression)
  add_executable(presentation_regression tests/presentation_test.cpp)
  target_link_libraries(presentation_regression PRIVATE openu5_core)
  add_test(NAME presentation_regression COMMAND presentation_regression)
  add_executable(dungeon_view_regression tests/dungeon_view_test.cpp)
  target_link_libraries(dungeon_view_regression PRIVATE openu5_core)
  # Batch 21A.2 -- D14 adjudicates the adjacent-door report on the SHIPPED
  # DUNGEON.DAT cells (Deceit floor 7), so it reads the same fixture the parity
  # suites do. D1-D13 are unchanged and still need no argument.
  add_test(NAME dungeon_view_regression COMMAND dungeon_view_regression
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dungeon-maps.txt")
  # Batch 9C -- the AUTHORED-ART half of R-05.  dungeon_view_regression proves
  # the semantic plan; this proves the next step only: which authored image of
  # DNG*/ITEMS/MON*.16 each plan op blits, into which box, with which flips, in
  # which order.  Still no framebuffer -- the mapping is decidable without one.
  add_executable(dungeon_art_regression tests/dungeon_art_test.cpp)
  target_link_libraries(dungeon_art_regression PRIVATE openu5_core)
  add_test(NAME dungeon_art_regression COMMAND dungeon_art_regression)
  # Batch 10 -- R-17/Y-14.  build_world_gem_view()/build_dungeon_gem_view()
  # are the semantic layer render_world_gem_view()/render_dungeon_gem_view()
  # (native_renderer.cpp) now consume; this proves category/geometry/marker
  # without any device pixel code, exactly like dungeon_view_regression above.
  add_executable(gem_view_regression tests/gem_view_test.cpp)
  target_link_libraries(gem_view_regression PRIVATE openu5_core)
  add_test(NAME gem_view_regression COMMAND gem_view_regression)
  add_executable(combat_escape_regression tests/combat_escape_regression_test.cpp)
  target_link_libraries(combat_escape_regression PRIVATE openu5_core)
  add_test(NAME combat_escape_regression COMMAND combat_escape_regression)
  add_executable(direct_troll_handoff_regression tests/direct_troll_handoff_test.cpp)
  target_link_libraries(direct_troll_handoff_regression PRIVATE openu5_core)
  add_test(NAME direct_troll_handoff_regression COMMAND direct_troll_handoff_regression)
  add_executable(combat_loot_open_regression tests/combat_loot_open_regression_test.cpp)
  target_link_libraries(combat_loot_open_regression PRIVATE openu5_core)
  add_test(NAME combat_loot_open_regression COMMAND combat_loot_open_regression)
  # Batch 21A.3 -- Set Active Player REACHABILITY during combat.  It links the
  # T-Deck ui_input_adapter because the question is a routing one end to end:
  # the handheld's only '0' is the Symbol+Mic chord, and in Combat a plain Mic
  # is Escape, so the digit route has to be proven from the raw chord all the
  # way to combat_action().
  add_executable(batch21a3_combat_active_player tests/batch21a3_combat_active_player_test.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp)
  target_link_libraries(batch21a3_combat_active_player PRIVATE openu5_core)
  target_include_directories(batch21a3_combat_active_player PRIVATE ../targets/tdeck/main)
  target_compile_features(batch21a3_combat_active_player PRIVATE cxx_std_20)
  add_test(NAME batch21a3_combat_active_player COMMAND batch21a3_combat_active_player)
  # These exercise the unchanged semantic input controller without any board driver.
  add_executable(input_regression ../targets/tdeck/host_tests/input_test.cpp
    ../targets/tdeck/main/input_controller.cpp ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp)
  target_link_libraries(input_regression PRIVATE openu5_core)
  target_compile_features(input_regression PRIVATE cxx_std_20)
  add_test(NAME input_regression COMMAND input_regression)
  # Batch 9B -- the dungeon RUNTIME regressions (R-05 part 3).  Unlike
  # dungeon_view_regression (which proves plan_dungeon_view()) and
  # dungeon_parity (which calls dungeon_action() directly), this drives the
  # physical device path: a RawInputEvent through tdeck::UiInputAdapter, the
  # real UiSession dungeon router, the real core dispatch, and AlphaRuntime's
  # per-input mode rebind via the shared ui_mode_policy.h seam.  It also owns
  # the HUD location-caption identity (tdeck::hud_location_caption).
  add_executable(dungeon_input_regression ../targets/tdeck/host_tests/dungeon_input_test.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp)
  target_link_libraries(dungeon_input_regression PRIVATE openu5_core)
  target_include_directories(dungeon_input_regression PRIVATE ../targets/tdeck/main)
  target_compile_features(dungeon_input_regression PRIVATE cxx_std_20)
  add_test(NAME dungeon_input_regression COMMAND dungeon_input_regression)
  # Batch 9D -- the non-overworld combat TRANSITION regressions.  Same device
  # seam as dungeon_input_regression, but it lets a dungeon actually start a
  # fight: corridor (A)ttack, ladder-into-room, the combat command set that
  # follows, and the return to dungeon input.  dungeon_input_regression stops
  # short of combat (it hardcodes combat=false in its resync tail) and the
  # combat parity suites never touch UiSession, so the entry/exit integration
  # between them was untested until the hardware found it.
  add_executable(dungeon_combat_regression ../targets/tdeck/host_tests/dungeon_combat_test.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp)
  target_link_libraries(dungeon_combat_regression PRIVATE openu5_core)
  target_include_directories(dungeon_combat_regression PRIVATE ../targets/tdeck/main)
  target_compile_features(dungeon_combat_regression PRIVATE cxx_std_20)
  # Batch 9E drives the AUTHORED Deceit/Destard rooms, so it reads the same two
  # shipped fixtures the parity suites do -- DUNGEON.DAT cells and DUNGEON.CBT boards.
  add_test(NAME dungeon_combat_regression COMMAND dungeon_combat_regression
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dungeon-maps.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fixed-maps.txt")
  # Batch 12B -- the two hardware discrepancies Batch 12's combat verdict did
  # not cover: the dungeon field that casts but never appears, and the enemy
  # standing in the black void.  Unlike every suite above it reads the REAL
  # shipped alpha resource pack (the exact bytes on the T-Deck's card), so the
  # authored .CBT unit slots, the authored DUNGEON.DAT field cells and the
  # dungeon-room arena catalog are the device's own data and not a fixture
  # restatement.  It links the production device renderer and the production
  # pack loader; ESP-IDF headers come from host_tests/esp_shims exactly as they
  # do for alpha_runtime_integration_regression.
  add_executable(batch12b_hardware_regression
    ../targets/tdeck/host_tests/batch12b_hardware_test.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/main/asset_pack.cpp)
  target_link_libraries(batch12b_hardware_regression PRIVATE openu5_core)
  target_include_directories(batch12b_hardware_regression PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch12b_hardware_regression PRIVATE cxx_std_20)
  add_test(NAME batch12b_hardware_regression COMMAND batch12b_hardware_regression
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  add_executable(action_feedback_regression tests/action_feedback_test.cpp)
  target_link_libraries(action_feedback_regression PRIVATE openu5_core)
  add_test(NAME action_feedback_regression COMMAND action_feedback_regression)
  # Batch 1 mode-ownership regressions (R-01, R-18, ShrineSpecial).  Drives the
  # real UiSession and core orchestration against the ESP-free mode-arbitration
  # policy AlphaRuntime shares (targets/tdeck/main/ui_mode_policy.h).
  add_executable(ui_mode_regression ../targets/tdeck/host_tests/ui_mode_test.cpp)
  target_link_libraries(ui_mode_regression PRIVATE openu5_core)
  target_include_directories(ui_mode_regression PRIVATE ../targets/tdeck/main)
  add_test(NAME ui_mode_regression COMMAND ui_mode_regression)
  # Batch 11 -- AlphaRuntime integration regressions. Unlike every suite
  # above (which drives ui_input_adapter/UiSession/core directly and, where
  # AlphaRuntime's own per-input/per-command tail matters, hand-mirrors it --
  # see the file header of dungeon_combat_test.cpp), this target links and
  # drives the REAL, UNMODIFIED alpha_runtime.cpp: the exact
  # handle()/dispatch()/command() production runs on device. ESP-IDF headers
  # it needs (esp_log/esp_timer/esp_heap_caps/esp_err/esp_check/esp_attr/
  # esp_system/FreeRTOS) are satisfied by host_tests/esp_shims -- harmless
  # host substitutes for a diagnostic/allocation surface that never decides
  # gameplay state (see esp_shims/*.h for why each one is safe). Board and
  # AlphaSaveService/AlphaSettingsService (real physical-device I/O) are
  # substituted with harmless-sink stubs under host_tests/host_stubs -- ONLY
  # for this target; main/CMakeLists.txt still builds tdeck_board.cpp and
  # alpha_save.cpp for firmware, untouched. -ffunction-sections/--gc-sections
  # let the linker drop AlphaRuntime::render()/initialize() (never called by
  # these tests) rather than requiring every symbol they touch to resolve.
  add_executable(alpha_runtime_integration_regression
    ../targets/tdeck/host_tests/alpha_runtime_integration_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(alpha_runtime_integration_regression PRIVATE openu5_core)
  target_include_directories(alpha_runtime_integration_regression PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(alpha_runtime_integration_regression PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(alpha_runtime_integration_regression PRIVATE /Gy)
    target_link_options(alpha_runtime_integration_regression PRIVATE /OPT:REF)
  else()
    target_compile_options(alpha_runtime_integration_regression PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(alpha_runtime_integration_regression PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME alpha_runtime_integration_regression COMMAND alpha_runtime_integration_regression)

  # Batch 21A -- the dungeon room-entry freeze / stuck-turn family
  # (H-149/H-150/H-151/H-152). Same seam and same stub/shim set as
  # alpha_runtime_integration_regression above: it links the REAL, unmodified
  # alpha_runtime.cpp, so a room is reached only by feeding raw input through
  # production routing.
  add_executable(batch21a_dungeon_room_regression
    ../targets/tdeck/host_tests/batch21a_dungeon_room_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch21a_dungeon_room_regression PRIVATE openu5_core)
  target_include_directories(batch21a_dungeon_room_regression PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch21a_dungeon_room_regression PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch21a_dungeon_room_regression PRIVATE /Gy)
    target_link_options(batch21a_dungeon_room_regression PRIVATE /OPT:REF)
  else()
    target_compile_options(batch21a_dungeon_room_regression PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch21a_dungeon_room_regression PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch21a_dungeon_room_regression COMMAND batch21a_dungeon_room_regression)

  # Batch 22 -- Lord British's Castle basement chests through the REAL device
  # runtime. Same seam and stub/shim set as batch21a_dungeon_room_regression,
  # but, like batch12b_hardware_regression, it reads the REAL shipped resource
  # pack, so the .NPC tables, the castle floors and the overworld location
  # table are the device's own bytes.
  add_executable(batch22_basement_objects
    ../targets/tdeck/host_tests/batch22_basement_objects_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch22_basement_objects PRIVATE openu5_core)
  target_include_directories(batch22_basement_objects PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch22_basement_objects PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch22_basement_objects PRIVATE /Gy)
    target_link_options(batch22_basement_objects PRIVATE /OPT:REF)
  else()
    target_compile_options(batch22_basement_objects PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch22_basement_objects PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch22_basement_objects COMMAND batch22_basement_objects
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 23 -- the Lord British vault against the 1988 binaries: chest loot
  # vectors from re/tools/chest_loot_oracle.py, and the chest-refill / door-
  # relock lifecycle of TOWN.OVL:0x052E -> 0x0408(1). Same seam and data as
  # batch22_basement_objects.
  add_executable(batch23_vault_parity
    ../targets/tdeck/host_tests/batch23_vault_parity_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch23_vault_parity PRIVATE openu5_core)
  target_include_directories(batch23_vault_parity PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch23_vault_parity PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch23_vault_parity PRIVATE /Gy)
    target_link_options(batch23_vault_parity PRIVATE /OPT:REF)
  else()
    target_compile_options(batch23_vault_parity PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch23_vault_parity PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch23_vault_parity COMMAND batch23_vault_parity
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 24 -- when the 1988 game rebuilds the local map (TOWN.OVL:0x0408):
  # floor change (H-161), save/load (H-162) and the end of a town fight
  # (H-163). Same seam and data as batch23_vault_parity, except the save
  # service: an in-memory generation that runs alpha_save.cpp's own core
  # serialization chain, so the real Alt+S/Alt+L and System Menu routes work.
  add_executable(batch24_reload_parity
    ../targets/tdeck/host_tests/batch24_reload_parity_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch24_reload_parity PRIVATE openu5_core)
  target_include_directories(batch24_reload_parity PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch24_reload_parity PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch24_reload_parity PRIVATE /Gy)
    target_link_options(batch24_reload_parity PRIVATE /OPT:REF)
  else()
    target_compile_options(batch24_reload_parity PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch24_reload_parity PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch24_reload_parity COMMAND batch24_reload_parity
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 25 -- H-118: the shard ritual and the world Move lock. Same seam and
  # data as batch24_reload_parity: the real AlphaRuntime, the shipped pack,
  # the real Developer Certification and raw keyboard/trackball input.
  add_executable(batch25_shard_ritual
    ../targets/tdeck/host_tests/batch25_shard_ritual_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch25_shard_ritual PRIVATE openu5_core)
  target_include_directories(batch25_shard_ritual PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch25_shard_ritual PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch25_shard_ritual PRIVATE /Gy)
    target_link_options(batch25_shard_ritual PRIVATE /OPT:REF)
  else()
    target_compile_options(batch25_shard_ritual PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch25_shard_ritual PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch25_shard_ritual COMMAND batch25_shard_ritual
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 26 -- H-115: save and load inside a dungeon. Same seam and data as
  # batch24_reload_parity (real AlphaRuntime, shipped pack, in-memory save
  # generation running alpha_save.cpp's own serialization chain), plus the
  # pack's eight DUNGEON.DAT maps so the real (E)nter reaches a dungeon.
  add_executable(batch26_dungeon_save
    ../targets/tdeck/host_tests/batch26_dungeon_save_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch26_dungeon_save PRIVATE openu5_core)
  target_include_directories(batch26_dungeon_save PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch26_dungeon_save PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch26_dungeon_save PRIVATE /Gy)
    target_link_options(batch26_dungeon_save PRIVATE /OPT:REF)
  else()
    target_compile_options(batch26_dungeon_save PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch26_dungeon_save PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch26_dungeon_save COMMAND batch26_dungeon_save
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 27 -- H-164: Alt+L against System Menu Continue Latest. Same seam and
  # data as batch26_dungeon_save.
  add_executable(batch27_alt_load
    ../targets/tdeck/host_tests/batch27_alt_load_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch27_alt_load PRIVATE openu5_core)
  target_include_directories(batch27_alt_load PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch27_alt_load PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch27_alt_load PRIVATE /Gy)
    target_link_options(batch27_alt_load PRIVATE /OPT:REF)
  else()
    target_compile_options(batch27_alt_load PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch27_alt_load PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch27_alt_load COMMAND batch27_alt_load
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 28 -- H-166: a save generation is validated whole (dungeon and
  # worldObjects sidecars included) before it may replace the live world.
  # Same seam as batch27_alt_load; the stub runs alpha_save_generation.cpp.
  add_executable(batch28_save_validation
    ../targets/tdeck/host_tests/batch28_save_validation_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch28_save_validation PRIVATE openu5_core)
  target_include_directories(batch28_save_validation PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch28_save_validation PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch28_save_validation PRIVATE /Gy)
    target_link_options(batch28_save_validation PRIVATE /OPT:REF)
  else()
    target_compile_options(batch28_save_validation PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch28_save_validation PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch28_save_validation COMMAND batch28_save_validation
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 29 -- the device's RestServices (H-154 bed NPC snap, H-155 the
  # "Thrown out of bed!" probe, H-160 reclassified). Same seam and data as
  # batch24_reload_parity; the fixture now binds production's own
  # AlphaRuntime::bind_rest_services(), so the device callbacks are tested.
  add_executable(batch29_rest_wiring
    ../targets/tdeck/host_tests/batch29_rest_wiring_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch29_rest_wiring PRIVATE openu5_core)
  target_include_directories(batch29_rest_wiring PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch29_rest_wiring PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch29_rest_wiring PRIVATE /Gy)
    target_link_options(batch29_rest_wiring PRIVATE /OPT:REF)
  else()
    target_compile_options(batch29_rest_wiring PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch29_rest_wiring PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch29_rest_wiring COMMAND batch29_rest_wiring
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 30 -- sleep housekeeping and the 05:00 / 20:00 terrain transition.
  add_executable(batch30_sleep_parity
    ../targets/tdeck/host_tests/batch30_sleep_parity_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch30_sleep_parity PRIVATE openu5_core)
  target_include_directories(batch30_sleep_parity PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch30_sleep_parity PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch30_sleep_parity PRIVATE /Gy)
    target_link_options(batch30_sleep_parity PRIVATE /OPT:REF)
  else()
    target_compile_options(batch30_sleep_parity PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch30_sleep_parity PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch30_sleep_parity COMMAND batch30_sleep_parity
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 31 -- H-165: pending town-exit prompt across a real Developer
  # dungeon teleport, using the shipped DUNGEON.DAT-derived pack payload.
  add_executable(batch31_h165_dungeon_entry
    ../targets/tdeck/host_tests/batch31_h165_dungeon_entry_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch31_h165_dungeon_entry PRIVATE openu5_core)
  target_include_directories(batch31_h165_dungeon_entry PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch31_h165_dungeon_entry PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch31_h165_dungeon_entry PRIVATE /Gy)
    target_link_options(batch31_h165_dungeon_entry PRIVATE /OPT:REF)
  else()
    target_compile_options(batch31_h165_dungeon_entry PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch31_h165_dungeon_entry PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch31_h165_dungeon_entry COMMAND batch31_h165_dungeon_entry
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 32 H-167: shipped-pack, raw-key Camp watch flow in AlphaRuntime.
  add_executable(batch32_camp_watch
    ../targets/tdeck/host_tests/batch32_camp_watch_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch32_camp_watch PRIVATE openu5_core)
  target_include_directories(batch32_camp_watch PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch32_camp_watch PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch32_camp_watch PRIVATE /Gy)
    target_link_options(batch32_camp_watch PRIVATE /OPT:REF)
  else()
    target_compile_options(batch32_camp_watch PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch32_camp_watch PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch32_camp_watch COMMAND batch32_camp_watch
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 33 H-168: shipped Ararat map/NPC, raw Fire and subsequent NPC updates.
  add_executable(batch33_cannon_npc_death
    ../targets/tdeck/host_tests/batch33_cannon_npc_death_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch33_cannon_npc_death PRIVATE openu5_core)
  target_include_directories(batch33_cannon_npc_death PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch33_cannon_npc_death PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch33_cannon_npc_death PRIVATE /Gy)
    target_link_options(batch33_cannon_npc_death PRIVATE /OPT:REF)
  else()
    target_compile_options(batch33_cannon_npc_death PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch33_cannon_npc_death PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch33_cannon_npc_death COMMAND batch33_cannon_npc_death
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  add_executable(batch34_pre_sleep_npc
    ../targets/tdeck/host_tests/batch34_pre_sleep_npc_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch34_pre_sleep_npc PRIVATE openu5_core)
  target_include_directories(batch34_pre_sleep_npc PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch34_pre_sleep_npc PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch34_pre_sleep_npc PRIVATE /Gy)
    target_link_options(batch34_pre_sleep_npc PRIVATE /OPT:REF)
  else()
    target_compile_options(batch34_pre_sleep_npc PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch34_pre_sleep_npc PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch34_pre_sleep_npc COMMAND batch34_pre_sleep_npc
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 14 / R-22 -- (Z)-stats navigation. Same seam and same stub/shim set
  # as alpha_runtime_integration_regression above: it links the REAL,
  # unmodified alpha_runtime.cpp so the modal is reached only by feeding raw
  # input through production routing, never by calling a formatter.
  add_executable(batch14_zstats_runtime_regression
    ../targets/tdeck/host_tests/batch14_zstats_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch14_zstats_runtime_regression PRIVATE openu5_core)
  target_include_directories(batch14_zstats_runtime_regression PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch14_zstats_runtime_regression PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch14_zstats_runtime_regression PRIVATE /Gy)
    target_link_options(batch14_zstats_runtime_regression PRIVATE /OPT:REF)
  else()
    target_compile_options(batch14_zstats_runtime_regression PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch14_zstats_runtime_regression PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch14_zstats_runtime COMMAND batch14_zstats_runtime_regression)

  # Batch 18 / R-26 -- the wishing-well ceremony. Same seam and same stub/shim
  # set as the two targets above: it links the REAL, unmodified
  # alpha_runtime.cpp, because R-26 is a defect in the GLUE that decides what
  # the (already-correct) core is handed, and only production routing can
  # expose it.
  add_executable(batch18_well_ceremony_regression
    ../targets/tdeck/host_tests/batch18_well_ceremony_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch18_well_ceremony_regression PRIVATE openu5_core)
  target_include_directories(batch18_well_ceremony_regression PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch18_well_ceremony_regression PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch18_well_ceremony_regression PRIVATE /Gy)
    target_link_options(batch18_well_ceremony_regression PRIVATE /OPT:REF)
  else()
    target_compile_options(batch18_well_ceremony_regression PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch18_well_ceremony_regression PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch18_well_ceremony COMMAND batch18_well_ceremony_regression)

  # Batch 19 / R-25 -- the kernel 0x4988 command-character picker and its three
  # callers (crystal ball, (S)earch, (C)ast). Same seam and same stub/shim set
  # as the target above, for the same reason: R-25 is a defect in the GLUE that
  # decides which member the (already-correct) core is handed, so only
  # production routing can expose it.
  add_executable(batch19_command_char_regression
    ../targets/tdeck/host_tests/batch19_command_char_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_host_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_runtime.cpp
    ../targets/tdeck/main/ui_input_adapter.cpp
    ../targets/tdeck/main/input_controller.cpp
    ../targets/tdeck/main/keyboard_matrix.cpp
    ../targets/tdeck/main/alpha_resources.cpp
    ../targets/tdeck/main/asset_pack.cpp
    ../targets/tdeck/main/dungeon_art_cache.cpp
    ../targets/tdeck/main/alpha_dialogue.cpp
    ../targets/tdeck/main/native_renderer.cpp
    ../targets/tdeck/host_tests/host_stubs/device_smoke_tests_host_stub.cpp)
  target_link_libraries(batch19_command_char_regression PRIVATE openu5_core)
  target_include_directories(batch19_command_char_regression PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch19_command_char_regression PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch19_command_char_regression PRIVATE /Gy)
    target_link_options(batch19_command_char_regression PRIVATE /OPT:REF)
  else()
    target_compile_options(batch19_command_char_regression PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch19_command_char_regression PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch19_command_char_runtime COMMAND batch19_command_char_regression)

  # Repair Batch 3 -- RED-only tests for R-06 (Ready), R-07/R-08 (Use
  # picker), R-19 (Yell/sails), R-20 (harpsichord), Y-20 (SetActivePlayer).
  # See native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md.
  add_executable(batch3_group_a_tests tests/batch3_group_a_test.cpp)
  target_link_libraries(batch3_group_a_tests PRIVATE openu5_core)
  add_test(NAME batch3_group_a COMMAND batch3_group_a_tests)
  add_executable(batch3_group_b_tests tests/batch3_group_b_test.cpp)
  target_link_libraries(batch3_group_b_tests PRIVATE openu5_core)
  add_test(NAME batch3_group_b COMMAND batch3_group_b_tests)
  add_executable(batch3_group_c_tests tests/batch3_group_c_test.cpp)
  target_link_libraries(batch3_group_c_tests PRIVATE openu5_core)
  add_test(NAME batch3_group_c COMMAND batch3_group_c_tests)
  # Batch 4 -- RED-only tests for R-09 (Blackthorn/guard modal handling and
  # FountainDrink presentation) and R-10 (NPC-initiated Talk/Shop). See
  # native/targets/tdeck/ALPHA20_BATCH4_ADJUDICATION.md.
  add_executable(batch4_group_a_tests tests/batch4_group_a_test.cpp)
  target_link_libraries(batch4_group_a_tests PRIVATE openu5_core)
  add_test(NAME batch4_group_a COMMAND batch4_group_a_tests)
  add_executable(batch4_group_b_tests tests/batch4_group_b_test.cpp)
  target_link_libraries(batch4_group_b_tests PRIVATE openu5_core)
  add_test(NAME batch4_group_b COMMAND batch4_group_b_tests)
  # Batch 5 -- world spell targeting (R-11) and the Rel Hur scroll getdir
  # (Y-21).  Drives the openu5::cast_target_prompt() policy seam, the real
  # UiSession TargetSelection machinery and the real world_magic() charge /
  # mana ordering.  See native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md.
  add_executable(batch5_tests tests/batch5_test.cpp)
  target_link_libraries(batch5_tests PRIVATE openu5_core)
  add_test(NAME batch5 COMMAND batch5_tests)
  add_executable(batch7b_tests tests/batch7b_test.cpp)
  target_link_libraries(batch7b_tests PRIVATE openu5_core)
  add_test(NAME batch7b COMMAND batch7b_tests)
  # Batch 12 -- combat field magic. Pins the REFERENCE behavior of the four
  # In*Grav spells against the hardware report that they "leave no field" in
  # the arena: CAST.OVL 0x004c's combat branch seeds no field by design
  # (docs/bugs-del-original.md SS2.9, ticket #91), the dungeon branch is the
  # only writer of DS:0x4596, and arena fields are authored .CBT units that
  # persist, render and apply COMBAT:0x1b1e's magnitude dispatch. Turns RED if
  # anyone "fixes" combat field creation or the two zero attack values.
  add_executable(batch12_combat_field_tests tests/batch12_combat_field_test.cpp)
  target_link_libraries(batch12_combat_field_tests PRIVATE openu5_core)
  add_test(NAME batch12_combat_field COMMAND batch12_combat_field_tests)

  add_executable(batch13_tests tests/batch13_test.cpp)
  target_link_libraries(batch13_tests PRIVATE openu5_core)
  add_test(NAME batch13 COMMAND batch13_tests)

  # Batch 14 / R-22 -- the (Z)-stats presentation model: the page axis, the
  # per-member stats and arms pages, provisions, and the four item lists.
  # Drives openu5::compose_zstats_page() itself, which is the same code
  # AlphaRuntime::compose_selection_view() paints from -- no host-only mirror.
  add_executable(batch14_zstats_model_tests tests/batch14_zstats_model_test.cpp)
  target_link_libraries(batch14_zstats_model_tests PRIVATE openu5_core)
  add_test(NAME batch14_zstats_model COMMAND batch14_zstats_model_tests)

  # Batch 15 / R-23 -- the Blackthorn sacrifice roster compaction and the
  # party-index parity downstream of it. Drives the production
  # sacrifice_first_companion() seam, the real blackthorn_action()
  # interrogation, the real execute_command() by-index party commands, the
  # real inn_leave (the sibling routine that DOES re-index the active
  # character) and the Batch 14 zstats page ring.
  add_executable(batch15_sacrifice_tests tests/batch15_sacrifice_test.cpp)
  target_link_libraries(batch15_sacrifice_tests PRIVATE openu5_core)
  add_test(NAME batch15_sacrifice_roster COMMAND batch15_sacrifice_tests)

  # Batch 17 / Y-34 -- the quest_parity host crash. Drives the same
  # openu5_test::observe_faulinei_theft() seam quest_driver.cpp uses to observe
  # Faulinei's unbounded rand(0,2) re-roll (TALK.OVL 0x11c7), so the watchdog
  # escape, the draw budget, the untouched-state contract and the exact
  # non-terminating set are all pinned outside the 5377-case parity replay.
  add_executable(batch17_theft_watchdog_tests tests/batch17_theft_watchdog_test.cpp)
  target_link_libraries(batch17_theft_watchdog_tests PRIVATE openu5_core)
  add_test(NAME batch17_theft_watchdog COMMAND batch17_theft_watchdog_tests)
  # Batch 19 / R-25 -- the shared command-character picker (kernel 0x4988) and
  # the crystal ball that is its first caller.  Drives world_look() and the
  # CommandKind::CrystalBall arm, the two production entry points, so every
  # branch of the picker is proven through a real caller rather than against a
  # local copy of the resolver.
  add_executable(batch19_command_char_tests tests/batch19_command_char_test.cpp)
  target_link_libraries(batch19_command_char_tests PRIVATE openu5_core)
  add_test(NAME batch19_command_char COMMAND batch19_command_char_tests)
  # Batch 4.5B -- Blackthorn capture production-resource integration: proves
  # ShrineServices::record/::context are actually wired to the packed
  # MISCMSG.DAT resource the way AlphaRuntime wires it (real capture and
  # shrine Codex text on hardware, not silence). See
  # native/targets/tdeck/main/alpha_runtime.cpp and misc_records.h.
  add_executable(batch45b_tests tests/batch45b_test.cpp)
  target_link_libraries(batch45b_tests PRIVATE openu5_core)
  target_include_directories(batch45b_tests PRIVATE ../targets/tdeck/main)
  add_test(NAME batch45b COMMAND batch45b_tests "${CMAKE_CURRENT_SOURCE_DIR}/../../game/assets/ds-strings.json")
  # Batch 4.5D -- the Blackthorn capture SCENE (audit R-32). Takes both the
  # real MISCMSG.DAT extraction and the real packed resource file: the
  # throne-room grid is read out of openu5-alpha1-resources.bin rather than
  # embedded, so a packer regression fails here instead of on hardware.
  add_executable(blackthorn_scene_tests tests/blackthorn_scene_test.cpp)
  target_link_libraries(blackthorn_scene_tests PRIVATE openu5_core)
  target_include_directories(blackthorn_scene_tests PRIVATE ../targets/tdeck/main)
  add_test(NAME blackthorn_scene COMMAND blackthorn_scene_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/../../game/assets/ds-strings.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Batch 4.5B correction -- the real pack's 33rd entry (misc-records.bin)
  # exceeded AlphaResourcePack's stale 32-entry bound. No openu5_core link
  # needed: this only exercises the host-portable predicate in
  # alpha_resource_limits.h.
  add_executable(alpha_resource_capacity_tests tests/alpha_resource_capacity_test.cpp)
  target_include_directories(alpha_resource_capacity_tests PRIVATE ../targets/tdeck/main)
  add_test(NAME alpha_resource_capacity COMMAND alpha_resource_capacity_tests)
  find_program(NODE_EXECUTABLE node)
  if(OPENU5_REAL_ARENAS)
    if(NOT NODE_EXECUTABLE)
      message(FATAL_ERROR "Real-arena validation requires Node and repository npm dependencies")
    endif()
    foreach(asset game/assets/maps/combatmaps.json original/u5/ultima5/BRIT.CBT original/u5/ultima5/DUNGEON.CBT)
      if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../${asset}")
        message(FATAL_ERROR "Missing ${asset}; run npm run extract -- --skip-tiles from the repository root")
      endif()
    endforeach()
    add_test(NAME real_arena_reference COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-combat-fixtures.ts" --real-arenas)
    set_tests_properties(real_arena_reference PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.." FIXTURES_SETUP real_arenas)
    add_test(NAME real_arena_parity COMMAND combat_parity_tests
      "${CMAKE_CURRENT_SOURCE_DIR}/build-real-arenas/combat.txt"
      "${CMAKE_CURRENT_SOURCE_DIR}/build-real-arenas/maps.txt")
    set_tests_properties(real_arena_parity PROPERTIES FIXTURES_REQUIRED real_arenas)
    add_test(NAME real_arena_advanced_reference COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-advanced-combat-fixtures.ts" --real-arenas)
    set_tests_properties(real_arena_advanced_reference PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.." FIXTURES_SETUP advanced_arenas)
    add_test(NAME real_arena_advanced_parity COMMAND advanced_combat_parity_tests
      "${CMAKE_CURRENT_SOURCE_DIR}/build-real-arenas/advanced-combat.txt"
      "${CMAKE_CURRENT_SOURCE_DIR}/build-real-arenas/maps.txt")
    set_tests_properties(real_arena_advanced_parity PROPERTIES FIXTURES_REQUIRED "real_arenas;advanced_arenas")
  endif()
  if(NODE_EXECUTABLE)
    add_test(NAME gameplay_parity COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/check-gameplay.ts" "$<TARGET_FILE:gameplay_driver>")
    add_test(NAME outdoor_table_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-outdoor-tables.ts" --check)
    set_tests_properties(gameplay_parity outdoor_table_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    # Batch 10 -- R-17/Y-14: guards kGemCategoryTable (gem_view.cpp) against
    # drift from the reference GEM_CATEGORY (gemmap-overworld.ts).
    add_test(NAME gem_category_table_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-gem-category-table.ts" --check)
    set_tests_properties(gem_category_table_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME movement_flow_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-movement-flow-fixtures.ts" --check)
    set_tests_properties(movement_flow_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME quest_parity COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/check-quests.ts" "$<TARGET_FILE:quest_driver>")
    add_test(NAME quest_case_table_drift COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-quest-case.ts" --check)
    set_tests_properties(quest_parity quest_case_table_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_shop_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-shop-fixtures.ts" --check)
    add_test(NAME typescript_shop_flow_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-shop-flow-fixtures.ts" --check)
    add_test(NAME typescript_shop_tables_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-shop-tables.ts" --check)
    add_test(NAME typescript_shop_greetings_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-shop-greetings.ts" --check)
    set_tests_properties(typescript_shop_fixture_drift typescript_shop_flow_drift typescript_shop_tables_drift typescript_shop_greetings_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME persistence_parity COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/check-persistence.ts" "$<TARGET_FILE:persistence_driver>")
    set_tests_properties(persistence_parity PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_dialogue_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-dialogue-fixtures.ts" --check)
    set_tests_properties(typescript_dialogue_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_transport_flow_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-transport-flow-fixtures.ts" --check)
    set_tests_properties(typescript_transport_flow_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_dungeon_flow_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-dungeon-flow-fixtures.ts" --check)
    set_tests_properties(typescript_dungeon_flow_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    # Batch 9C / R-05 -- GOLDEN EXTRACTION identity for the authored dungeon art:
    # dimensions plus indexed-pixel and mask CRC32 for one image of every semantic
    # class, agreed by three independent readings (extractor parser, packer, and
    # the generated pack read back through the device's container). Reports SKIP
    # and passes when the user's original data or the built pack is absent, the
    # same convention the extractor catalog's optional stages use.
    add_test(NAME typescript_dungeon_art_identity COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/check-dungeon-art-identity.ts")
    set_tests_properties(typescript_dungeon_art_identity PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_world_flow_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-world-flow-fixtures.ts" --check)
    set_tests_properties(typescript_world_flow_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_dungeon_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-dungeon-fixtures.ts" --check)
    set_tests_properties(typescript_dungeon_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_advanced_combat_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-advanced-combat-fixtures.ts" --check)
    set_tests_properties(typescript_advanced_combat_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_magic_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-magic-fixtures.ts" --check)
    set_tests_properties(typescript_magic_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_combat_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-combat-fixtures.ts" --check)
    set_tests_properties(typescript_combat_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_item_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-item-fixtures.ts" --check)
    set_tests_properties(typescript_item_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_command_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-command-fixtures.ts" --check)
    set_tests_properties(typescript_command_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_travel_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-travel-fixtures.ts" --check)
    set_tests_properties(typescript_travel_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-fixtures.ts" --check)
    set_tests_properties(typescript_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_test(NAME typescript_turn_fixture_drift COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-turn-fixtures.ts" --check)
    set_tests_properties(typescript_turn_fixture_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
  endif()
  if(CMAKE_CONFIGURATION_TYPES)
    set(OPENU5_CTEST_CONFIG_ARGS -C "$<CONFIG>")
  else()
    set(OPENU5_CTEST_CONFIG_ARGS)
  endif()
  add_custom_target(host-test COMMAND "${CMAKE_CTEST_COMMAND}" ${OPENU5_CTEST_CONFIG_ARGS} --output-on-failure
  DEPENDS batch17_theft_watchdog_tests batch19_command_char_tests batch19_command_char_regression frontend_tests ui_session_tests display_names_tests quest_search_tests debug_labels_tests ${UI_DEBUG_TEST_TARGET} debug_map_picker_tests debug_developer_tests gameplay_integration_tests look_sign_tests gameplay_driver movement_flow_parity_tests quest_driver shop_parity_tests shop_flow_tests shop_adapter_tests persistence_driver dialogue_adapter_tests dialogue_parity_tests transport_flow_parity_tests world_flow_adapter_tests dungeon_flow_parity_tests world_flow_parity_tests dungeon_parity_tests advanced_combat_parity_tests magic_parity_tests combat_parity_tests core_parity_tests turn_parity_tests travel_parity_tests command_parity_tests item_parity_tests movement_regression presentation_regression dungeon_view_regression dungeon_art_regression gem_view_regression dungeon_input_regression combat_escape_regression combat_loot_open_regression batch12b_hardware_regression input_regression USES_TERMINAL)
endif()
