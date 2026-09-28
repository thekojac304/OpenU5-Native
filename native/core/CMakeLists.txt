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
  # Batch 45: authentic castle sleeper/guard visibility, Look and bed snap.
  add_test(NAME batch45_npc_beds COMMAND batch29_rest_wiring
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin" --batch45-only)

  add_test(NAME batch48_bed_pose COMMAND batch29_rest_wiring
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin" --batch48-only)
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

  add_executable(batch35_q_sleep_target
    ../targets/tdeck/host_tests/batch35_q_sleep_target_test.cpp
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
  target_link_libraries(batch35_q_sleep_target PRIVATE openu5_core)
  target_include_directories(batch35_q_sleep_target PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch35_q_sleep_target PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch35_q_sleep_target PRIVATE /Gy)
    target_link_options(batch35_q_sleep_target PRIVATE /OPT:REF)
  else()
    target_compile_options(batch35_q_sleep_target PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch35_q_sleep_target PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch35_q_sleep_target COMMAND batch35_q_sleep_target
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 36 H-171: shipped-pack raw-key Camp cadence and survival non-effects.
  add_executable(batch36_camp_housekeeping
    ../targets/tdeck/host_tests/batch36_camp_housekeeping_test.cpp
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
  target_link_libraries(batch36_camp_housekeeping PRIVATE openu5_core)
  target_include_directories(batch36_camp_housekeeping PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch36_camp_housekeeping PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch36_camp_housekeeping PRIVATE /Gy)
    target_link_options(batch36_camp_housekeeping PRIVATE /OPT:REF)
  else()
    target_compile_options(batch36_camp_housekeeping PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch36_camp_housekeeping PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch36_camp_housekeeping COMMAND batch36_camp_housekeeping
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 37 H-172: actual raw-key sleep and captured viewport pixels.
  add_executable(batch37_bed_viewport
    ../targets/tdeck/host_tests/batch37_bed_viewport_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
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
  target_link_libraries(batch37_bed_viewport PRIVATE openu5_core)
  target_include_directories(batch37_bed_viewport PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch37_bed_viewport PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch37_bed_viewport PRIVATE /Gy)
    target_link_options(batch37_bed_viewport PRIVATE /OPT:REF)
  else()
    target_compile_options(batch37_bed_viewport PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch37_bed_viewport PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch37_bed_viewport COMMAND batch37_bed_viewport
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 39 H-174: original intermediate status-panel frames.
  add_executable(batch39_party_panel
    ../targets/tdeck/host_tests/batch39_party_panel_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
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
  target_link_libraries(batch39_party_panel PRIVATE openu5_core)
  target_include_directories(batch39_party_panel PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch39_party_panel PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch39_party_panel PRIVATE /Gy)
    target_link_options(batch39_party_panel PRIVATE /OPT:REF)
  else()
    target_compile_options(batch39_party_panel PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch39_party_panel PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch39_party_panel COMMAND batch39_party_panel
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 41 H-175: raw-key member holds and synchronous panel frames.
  add_executable(batch41_raw_key
    ../targets/tdeck/host_tests/batch41_raw_key_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
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
  target_link_libraries(batch41_raw_key PRIVATE openu5_core)
  target_include_directories(batch41_raw_key PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch41_raw_key PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch41_raw_key PRIVATE /Gy)
    target_link_options(batch41_raw_key PRIVATE /OPT:REF)
  else()
    target_compile_options(batch41_raw_key PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch41_raw_key PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch41_raw_key COMMAND batch41_raw_key
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 42 H-176: actor tile and XOR framebuffer frames.
  add_executable(batch42_advancement_flash
    ../targets/tdeck/host_tests/batch42_advancement_flash_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
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
  target_link_libraries(batch42_advancement_flash PRIVATE openu5_core)
  target_include_directories(batch42_advancement_flash PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch42_advancement_flash PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch42_advancement_flash PRIVATE /Gy)
    target_link_options(batch42_advancement_flash PRIVATE /OPT:REF)
  else()
    target_compile_options(batch42_advancement_flash PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch42_advancement_flash PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch42_advancement_flash COMMAND batch42_advancement_flash
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 43 H-177: pre-apparition Camp sleep scene.
  add_executable(batch43_sleep_scene
    ../targets/tdeck/host_tests/batch43_sleep_scene_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
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
  target_link_libraries(batch43_sleep_scene PRIVATE openu5_core)
  target_include_directories(batch43_sleep_scene PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch43_sleep_scene PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch43_sleep_scene PRIVATE /Gy)
    target_link_options(batch43_sleep_scene PRIVATE /OPT:REF)
  else()
    target_compile_options(batch43_sleep_scene PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch43_sleep_scene PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch43_sleep_scene COMMAND batch43_sleep_scene
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Batch 46 H-179: Ring of Invisibility in the CampFire framebuffer.
  add_executable(batch46_camp_invisibility
    ../targets/tdeck/host_tests/batch46_camp_invisibility_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
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
  target_link_libraries(batch46_camp_invisibility PRIVATE openu5_core)
  target_include_directories(batch46_camp_invisibility PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch46_camp_invisibility PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch46_camp_invisibility PRIVATE /Gy)
    target_link_options(batch46_camp_invisibility PRIVATE /OPT:REF)
  else()
    target_compile_options(batch46_camp_invisibility PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch46_camp_invisibility PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch46_camp_invisibility COMMAND batch46_camp_invisibility
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Batch 44 H-178: equipped-ring Camp scene-entry expiry.
  add_executable(batch44_camp_ring_entry
    ../targets/tdeck/host_tests/batch44_camp_ring_entry_test.cpp
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
  target_link_libraries(batch44_camp_ring_entry PRIVATE openu5_core)
  target_include_directories(batch44_camp_ring_entry PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch44_camp_ring_entry PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch44_camp_ring_entry PRIVATE /Gy)
    target_link_options(batch44_camp_ring_entry PRIVATE /OPT:REF)
  else()
    target_compile_options(batch44_camp_ring_entry PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch44_camp_ring_entry PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch44_camp_ring_entry COMMAND batch44_camp_ring_entry
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 38 H-173: original target-hour Camp stop through shipped-pack raw keys.
  add_executable(batch38_camp_duration
    ../targets/tdeck/host_tests/batch38_camp_duration_test.cpp
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
  target_link_libraries(batch38_camp_duration PRIVATE openu5_core)
  target_include_directories(batch38_camp_duration PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch38_camp_duration PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch38_camp_duration PRIVATE /Gy)
    target_link_options(batch38_camp_duration PRIVATE /OPT:REF)
  else()
    target_compile_options(batch38_camp_duration PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch38_camp_duration PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch38_camp_duration COMMAND batch38_camp_duration
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 40 -- original OUTSUBS advancement through the live Rest command.
  add_executable(batch40_advancement tests/batch40_advancement_test.cpp)
  target_link_libraries(batch40_advancement PRIVATE openu5_core)
  add_test(NAME batch40_advancement COMMAND batch40_advancement)

  # Batch 41 -- original per-member Camp getkey and status staging.
  add_executable(batch41_staged_advancement tests/batch41_staged_advancement_test.cpp)
  target_link_libraries(batch41_staged_advancement PRIVATE openu5_core)
  add_test(NAME batch41_staged_advancement COMMAND batch41_staged_advancement)
  # Batch 51: scripted-scene pacing on a virtual clock -- the Camp apparition
  # through the real Rest command into NarrativeScenePacer, and the Blackthorn
  # materialization/siren holds through BlackthornScenePacer.
  add_executable(batch51_scene_pacing tests/batch51_scene_pacing_test.cpp)
  target_link_libraries(batch51_scene_pacing PRIVATE openu5_core)
  add_test(NAME batch51_scene_pacing COMMAND batch51_scene_pacing)
  # Batch 51: the Camp apparition's dwell through the REAL AlphaRuntime, raw
  # keys in and captured frames out, on the esp_timer shim's virtual clock.
  add_executable(batch51_camp_pacing
    ../targets/tdeck/host_tests/batch51_camp_pacing_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
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
  target_link_libraries(batch51_camp_pacing PRIVATE openu5_core)
  target_include_directories(batch51_camp_pacing PRIVATE
    ../targets/tdeck/host_tests/esp_shims ../targets/tdeck/main)
  target_compile_features(batch51_camp_pacing PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch51_camp_pacing PRIVATE /Gy)
    target_link_options(batch51_camp_pacing PRIVATE /OPT:REF)
  else()
    target_compile_options(batch51_camp_pacing PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch51_camp_pacing PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch51_camp_pacing COMMAND batch51_camp_pacing
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 53 -- the four Alpha 2 release blockers (RB-1 .. RB-4) on the device's
  # own binding shape: the real AlphaRuntime through the fixture, which calls
  # the production binders; the shipped pack; raw keys; the in-memory save stub
  # running the production save-generation gate. The two source paths let the
  # test check that initialize() and the fixture share the binders.
  add_executable(batch53_release_blockers
    ../targets/tdeck/host_tests/batch53_release_blockers_test.cpp
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
  target_link_libraries(batch53_release_blockers PRIVATE openu5_core)
  target_include_directories(batch53_release_blockers PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_definitions(batch53_release_blockers PRIVATE
    OPENU5_RUNTIME_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/../targets/tdeck/main/alpha_runtime.cpp"
    OPENU5_FIXTURE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp")
  target_compile_features(batch53_release_blockers PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch53_release_blockers PRIVATE /Gy)
    target_link_options(batch53_release_blockers PRIVATE /OPT:REF)
  else()
    target_compile_options(batch53_release_blockers PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch53_release_blockers PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch53_release_blockers COMMAND batch53_release_blockers
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 53A -- the state after the final Doom absorption. ENDGAME.OVL never
  # returns to the dungeon; the device must enter the terminal UiMode::Ending
  # instead of falling back into Dungeon mode in the enclosed final cell. Same
  # seam and sources as batch53_release_blockers.
  add_executable(batch53a_ending_terminal
    ../targets/tdeck/host_tests/batch53a_ending_terminal_test.cpp
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
  target_link_libraries(batch53a_ending_terminal PRIVATE openu5_core)
  target_include_directories(batch53a_ending_terminal PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch53a_ending_terminal PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch53a_ending_terminal PRIVATE /Gy)
    target_link_options(batch53a_ending_terminal PRIVATE /OPT:REF)
  else()
    target_compile_options(batch53a_ending_terminal PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch53a_ending_terminal PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch53a_ending_terminal COMMAND batch53a_ending_terminal
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")

  # Batch 53B -- Phase 7E-C's return trip through a moongate: the exact
  # physical sequence (teleport, hour 21, step onto the gate, step off and
  # back on the destination gate) on the device runtime, adjudicated against
  # MAINOUT 0x0b00 / ULTIMA.EXE 0x48a8-0x4977. Same seam as Batch 53.
  add_executable(batch53b_moongate_return
    ../targets/tdeck/host_tests/batch53b_moongate_return_test.cpp
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
  target_link_libraries(batch53b_moongate_return PRIVATE openu5_core)
  target_include_directories(batch53b_moongate_return PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(batch53b_moongate_return PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(batch53b_moongate_return PRIVATE /Gy)
    target_link_options(batch53b_moongate_return PRIVATE /OPT:REF)
  else()
    target_compile_options(batch53b_moongate_return PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(batch53b_moongate_return PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME batch53b_moongate_return COMMAND batch53b_moongate_return
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Alpha 3 A3-01 -- the audio contract (vocabulary, music selector, service
  # policy, audio-pack reader, Settings rows) in the portable core. The real
  # audio pack is built locally by `npm run pack:audio` and never committed.
  add_executable(a3_01_audio_contract tests/a3_01_audio_contract_test.cpp)
  target_link_libraries(a3_01_audio_contract PRIVATE openu5_core)
  target_compile_features(a3_01_audio_contract PRIVATE cxx_std_17)
  add_test(NAME a3_01_audio_contract COMMAND a3_01_audio_contract
    "${CMAKE_CURRENT_SOURCE_DIR}/../../game/src/core/sfx.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../game/src/ui/music.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/a3-01-audio-stock.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin")
  # Alpha 3 A3-02 -- the PC-speaker synthesizer (primitives measured from the
  # PCM they render, the cue table against the binary's constants and the
  # TypeScript reference, the harpsichord, the playback policy).
  add_executable(a3_02_sfx_synth tests/a3_02_sfx_synth_test.cpp)
  target_link_libraries(a3_02_sfx_synth PRIVATE openu5_core)
  target_compile_features(a3_02_sfx_synth PRIVATE cxx_std_17)
  add_test(NAME a3_02_sfx_synth COMMAND a3_02_sfx_synth
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/a3-02-sfx-reference.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}")
  # Alpha 3 A3-03 -- the SFX inventory (every cue and every original call site
  # classified), the A3-03 programs against the binary, the rumble / ladder /
  # ambient measured, and the ambient priority policy.
  add_executable(a3_03_sfx_inventory tests/a3_03_sfx_inventory_test.cpp)
  target_link_libraries(a3_03_sfx_inventory PRIVATE openu5_core)
  target_compile_features(a3_03_sfx_inventory PRIVATE cxx_std_17)
  add_test(NAME a3_03_sfx_inventory COMMAND a3_03_sfx_inventory "${CMAKE_CURRENT_SOURCE_DIR}")
  # Alpha 3 A3-04 -- the music decoder/synth (FAT.OPL bank, OPL2 emulator,
  # voice allocator, direct XMI event parse, song player determinism and
  # looping, the intro/endgame context tables) against synthetic fixtures
  # and the real local patch corpus.
  add_executable(a3_04_music_synth tests/a3_04_music_synth_test.cpp)
  target_link_libraries(a3_04_music_synth PRIVATE openu5_core)
  target_compile_features(a3_04_music_synth PRIVATE cxx_std_17)
  add_test(NAME a3_04_music_synth COMMAND a3_04_music_synth
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin")
  # A3-04A -- the real-time audio path: the device audio task's pump driven
  # against a model of the ESP-IDF i2s_std TX descriptor ring (prime, drain,
  # stall tolerance, underrun detection, exact playback order, switch/volume
  # latency, zero allocation), the hot-path guard scan, the benchmark
  # timeline, and the device backend's wiring.
  add_executable(a3_04a_audio_stream tests/a3_04a_audio_stream_test.cpp)
  target_link_libraries(a3_04a_audio_stream PRIVATE openu5_core)
  target_compile_features(a3_04a_audio_stream PRIVATE cxx_std_17)
  add_test(NAME a3_04a_audio_stream COMMAND a3_04a_audio_stream
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin")
  # A3-04B -- music CPU contention: the synth's fast paths bit-exact against
  # fingerprints of the A3-04A synth, Music Volume 0 % (no synth work, restart
  # on unmute), the mix-saturation counter against an oracle, the render
  # counters, the combined Developer report, the two-consumer scheduling
  # model, and the firmware's IRAM placement.
  add_executable(a3_04b_perf tests/a3_04b_perf_test.cpp)
  target_link_libraries(a3_04b_perf PRIVATE openu5_core)
  target_compile_features(a3_04b_perf PRIVATE cxx_std_17)
  add_test(NAME a3_04b_perf COMMAND a3_04b_perf
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin")
  # A3-01 -- the same seam through the REAL AlphaRuntime: the settings rows by
  # raw keys, reboot/load persistence, the Developer test tone, presentation-
  # time cue routing, and scene timing / game state identical with audio on,
  # off, refused or muted.
  add_executable(a3_01_audio_runtime
    ../targets/tdeck/host_tests/a3_01_audio_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_01_audio_runtime PRIVATE openu5_core)
  target_include_directories(a3_01_audio_runtime PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_01_audio_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_01_audio_runtime PRIVATE /Gy)
    target_link_options(a3_01_audio_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_01_audio_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_01_audio_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_01_audio_runtime COMMAND a3_01_audio_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/a3-01-audio-stock.bin")
  # Alpha 3 A3-02 -- real effects through the REAL AlphaRuntime: the
  # synthesizer behind the device's own sfx_supported gate, the harpsichord by
  # raw keys, combat/ceremony cues from emitted events, Camp and Blackthorn
  # timelines under six audio setups, and load/title/Developer/Ending safety.
  add_executable(a3_02_sfx_runtime
    ../targets/tdeck/host_tests/a3_02_sfx_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_02_sfx_runtime PRIVATE openu5_core)
  target_include_directories(a3_02_sfx_runtime PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_02_sfx_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_02_sfx_runtime PRIVATE /Gy)
    target_link_options(a3_02_sfx_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_02_sfx_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_02_sfx_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_02_sfx_runtime COMMAND a3_02_sfx_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin")

  # Alpha 3 A3-03 -- the remaining effects through the REAL AlphaRuntime: a
  # real fountain and clock, a real troll victory, the quake, shrine and Refuge
  # cues, world/shop/intro derivations, timing under six audio setups, and
  # load / title / menu / dungeon / Ending safety.
  add_executable(a3_03_sfx_runtime
    ../targets/tdeck/host_tests/a3_03_sfx_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_03_sfx_runtime PRIVATE openu5_core)
  target_include_directories(a3_03_sfx_runtime PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_03_sfx_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_03_sfx_runtime PRIVATE /Gy)
    target_link_options(a3_03_sfx_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_03_sfx_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_03_sfx_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_03_sfx_runtime COMMAND a3_03_sfx_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin")
  # Alpha 3 A3-04 -- music context routing through the REAL AlphaRuntime:
  # boot, location switches, Camp freeze, Ending, Settings Music Volume, and
  # a stock pack's silence, all by raw keys against a recording backend.
  add_executable(a3_04_music_runtime
    ../targets/tdeck/host_tests/a3_04_music_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_04_music_runtime PRIVATE openu5_core)
  target_include_directories(a3_04_music_runtime PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_04_music_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_04_music_runtime PRIVATE /Gy)
    target_link_options(a3_04_music_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_04_music_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_04_music_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_04_music_runtime COMMAND a3_04_music_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/a3-01-audio-stock.bin")
  # A3-04B -- the Developer perf report through the REAL AlphaRuntime: the
  # benchmark's and the live window's results on the Developer screen until
  # dismissed (A3-04A's went to the hidden transcript), the render counters,
  # and the game untouched.
  add_executable(a3_04b_perf_runtime
    ../targets/tdeck/host_tests/a3_04b_perf_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_04b_perf_runtime PRIVATE openu5_core)
  target_include_directories(a3_04b_perf_runtime PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_04b_perf_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_04b_perf_runtime PRIVATE /Gy)
    target_link_options(a3_04b_perf_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_04b_perf_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_04b_perf_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_04b_perf_runtime COMMAND a3_04b_perf_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # A3-04C -- the contention map: the Developer "synth bypass" probe on the
  # production pump (the song keeps playing, the synth does no work, it
  # resumes where it stopped), the audio task's own work per block, the TFT /
  # loop / row-level contention counters, the report section and the one-line
  # A3C_PERF heartbeat, and the device wiring (source scans).
  add_executable(a3_04c_contention tests/a3_04c_contention_test.cpp)
  target_link_libraries(a3_04c_contention PRIVATE openu5_core)
  target_compile_features(a3_04c_contention PRIVATE cxx_std_17)
  add_test(NAME a3_04c_contention COMMAND a3_04c_contention
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin")
  # A3-04C -- the same through the REAL AlphaRuntime: every gameplay frame's
  # TFT timing reaches the live report and the A3C_PERF line, the probe row
  # toggles the audio task's probe and labels the report, the game untouched.
  add_executable(a3_04c_contention_runtime
    ../targets/tdeck/host_tests/a3_04c_contention_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_04c_contention_runtime PRIVATE openu5_core)
  target_include_directories(a3_04c_contention_runtime PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_04c_contention_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_04c_contention_runtime PRIVATE /Gy)
    target_link_options(a3_04c_contention_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_04c_contention_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_04c_contention_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_04c_contention_runtime COMMAND a3_04c_contention_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Alpha 3 A3-04D (targets/tdeck/ALPHA3_AUDIO.md section 21) -- the SD
  # diagnostic log's switch: off at boot, serial always, the card untouched
  # while off (the device writer's own logic against a fake card), the
  # report / A3C_PERF labels, the SD attribution of slow TFT transactions,
  # and the device wiring.
  add_executable(a3_04d_sd_log tests/a3_04d_sd_log_test.cpp)
  target_link_libraries(a3_04d_sd_log PRIVATE openu5_core)
  target_compile_features(a3_04d_sd_log PRIVATE cxx_std_17)
  add_test(NAME a3_04d_sd_log COMMAND a3_04d_sd_log "${CMAKE_CURRENT_SOURCE_DIR}")
  # A3-04D -- the same switch through the REAL AlphaRuntime: the Developer
  # row, the labels, n/a without a logger, the game untouched.
  add_executable(a3_04d_sd_log_runtime
    ../targets/tdeck/host_tests/a3_04d_sd_log_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_04d_sd_log_runtime PRIVATE openu5_core)
  target_include_directories(a3_04d_sd_log_runtime PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_04d_sd_log_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_04d_sd_log_runtime PRIVATE /Gy)
    target_link_options(a3_04d_sd_log_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_04d_sd_log_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_04d_sd_log_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_04d_sd_log_runtime COMMAND a3_04d_sd_log_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Alpha 3 A3-04E (targets/tdeck/ALPHA3_AUDIO.md section 22) -- the game
  # thread's pacing: the draw loops' pause and main.cpp's end-of-pass wait
  # (policy, counters, report, A3E_PACE, the Developer rows, device wiring).
  add_executable(a3_04e_pacing tests/a3_04e_pacing_test.cpp)
  target_link_libraries(a3_04e_pacing PRIVATE openu5_core)
  target_compile_features(a3_04e_pacing PRIVATE cxx_std_17)
  add_test(NAME a3_04e_pacing COMMAND a3_04e_pacing "${CMAKE_CURRENT_SOURCE_DIR}")
  # A3-04E -- the REAL AlphaRuntime driving the REAL tdeck_board.cpp, compiled
  # for the host over host_tests/board_shims (a fake ST7789 decoding the byte
  # stream, the SPI transaction cost and the 100 Hz tick on the virtual clock).
  # board_shims comes first: its driver headers and the Board's sdkconfig.h.
  add_executable(a3_04e_pacing_runtime
    ../targets/tdeck/host_tests/a3_04e_pacing_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/board_shims/fake_tdeck_bus.cpp
    ../targets/tdeck/main/tdeck_board.cpp
    ../targets/tdeck/main/idle_service.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_04e_pacing_runtime PRIVATE openu5_core)
  target_include_directories(a3_04e_pacing_runtime PRIVATE
    ../targets/tdeck/host_tests/board_shims
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_04e_pacing_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_04e_pacing_runtime PRIVATE /Gy)
    target_link_options(a3_04e_pacing_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_04e_pacing_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_04e_pacing_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_04e_pacing_runtime COMMAND a3_04e_pacing_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Alpha 3 gameplay hotfix A3-HF1 -- the troll-encounter reward chest whose
  # Get answered "Nothing to get!": a roaming troll by the bridge near
  # Britain, the fight, (O)pen and (G)et, all by raw keys through the REAL
  # AlphaRuntime, with ordinary and Developer-maxed inventories.
  add_executable(a3_hf1_arena_loot
    ../targets/tdeck/host_tests/a3_hf1_arena_loot_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_hf1_arena_loot PRIVATE openu5_core)
  target_include_directories(a3_hf1_arena_loot PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_hf1_arena_loot PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_hf1_arena_loot PRIVATE /Gy)
    target_link_options(a3_hf1_arena_loot PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_hf1_arena_loot PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_hf1_arena_loot PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_hf1_arena_loot COMMAND a3_hf1_arena_loot
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Alpha 3 A3-HF2 -- ambient SFX parity: the grandfather clock strikes only
  # when advance_clock moves the hour (0x514a), and the fountain sustains while
  # the party stands still, through the REAL AlphaRuntime on A3-04E's
  # device-loop model (the REAL tdeck_board.cpp over board_shims).
  add_executable(a3_hf2_ambient_parity
    ../targets/tdeck/host_tests/a3_hf2_ambient_parity_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/board_shims/fake_tdeck_bus.cpp
    ../targets/tdeck/main/tdeck_board.cpp
    ../targets/tdeck/main/idle_service.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_hf2_ambient_parity PRIVATE openu5_core)
  target_include_directories(a3_hf2_ambient_parity PRIVATE
    ../targets/tdeck/host_tests/board_shims
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_hf2_ambient_parity PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_hf2_ambient_parity PRIVATE /Gy)
    target_link_options(a3_hf2_ambient_parity PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_hf2_ambient_parity PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_hf2_ambient_parity PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_hf2_ambient_parity COMMAND a3_hf2_ambient_parity
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin")
  # Alpha 3 A3-HF2.1 -- PRESENTATION_DISPATCH logs a change of presentation
  # source / UI mode, not every gameplay frame, through the REAL AlphaRuntime
  # with stdout captured per phase (A3-HF1's harness and stub set).
  add_executable(a3_hf2_1_dispatch_log
    ../targets/tdeck/host_tests/a3_hf2_1_dispatch_log_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_hf2_1_dispatch_log PRIVATE openu5_core)
  target_include_directories(a3_hf2_1_dispatch_log PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_hf2_1_dispatch_log PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_hf2_1_dispatch_log PRIVATE /Gy)
    target_link_options(a3_hf2_1_dispatch_log PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_hf2_1_dispatch_log PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_hf2_1_dispatch_log PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_hf2_1_dispatch_log COMMAND a3_hf2_1_dispatch_log
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Alpha 3 A3-04F -- render / TFT efficiency (ALPHA3_AUDIO.md section 26): the
  # REAL AlphaRuntime on the REAL tdeck_board.cpp over A3-04E's fake ST7789.
  # The census (transactions, thin transfers, windows by region per scenario)
  # and the panel golden (a3_04f_panel_goldens.h, recorded from the baseline).
  add_executable(a3_04f_render_runtime
    ../targets/tdeck/host_tests/a3_04f_render_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/board_shims/fake_tdeck_bus.cpp
    ../targets/tdeck/main/tdeck_board.cpp
    ../targets/tdeck/main/idle_service.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_04f_render_runtime PRIVATE openu5_core)
  target_include_directories(a3_04f_render_runtime PRIVATE
    ../targets/tdeck/host_tests/board_shims
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/host_tests
    ../targets/tdeck/main)
  target_compile_features(a3_04f_render_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_04f_render_runtime PRIVATE /Gy)
    target_link_options(a3_04f_render_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_04f_render_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_04f_render_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_04f_render_runtime COMMAND a3_04f_render_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Alpha 3 A3-HF3 -- D-63, combat hit feedback (ALPHA3_AUDIO.md section 27).
  # The core half: the 0x3564 cue's length against the rendered burst, the
  # queue, and which combat events are a cue.
  add_executable(a3_hf3_combat_hit_cue tests/a3_hf3_combat_hit_cue_test.cpp)
  target_link_libraries(a3_hf3_combat_hit_cue PRIVATE openu5_core)
  target_compile_features(a3_hf3_combat_hit_cue PRIVATE cxx_std_17)
  add_test(NAME a3_hf3_combat_hit_cue COMMAND a3_hf3_combat_hit_cue)
  # The device half: the REAL AlphaRuntime on the REAL tdeck_board.cpp over the
  # fake ST7789 -- the roster row's reverse video read off the panel, the tile-0
  # marker off the composed viewport, the burst off a recording audio backend.
  add_executable(a3_hf3_combat_hit_runtime
    ../targets/tdeck/host_tests/a3_hf3_combat_hit_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/board_shims/fake_tdeck_bus.cpp
    ../targets/tdeck/main/tdeck_board.cpp
    ../targets/tdeck/main/idle_service.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_hf3_combat_hit_runtime PRIVATE openu5_core)
  target_include_directories(a3_hf3_combat_hit_runtime PRIVATE
    ../targets/tdeck/host_tests/board_shims
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/host_tests
    ../targets/tdeck/main)
  target_compile_features(a3_hf3_combat_hit_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_hf3_combat_hit_runtime PRIVATE /Gy)
    target_link_options(a3_hf3_combat_hit_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_hf3_combat_hit_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_hf3_combat_hit_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_hf3_combat_hit_runtime COMMAND a3_hf3_combat_hit_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin")
  # Alpha 3 A3-04G (ALPHA3_AUDIO.md section 28) -- the System Menu's save
  # inspection and the storage heap. The first target to compile the REAL
  # alpha_save.cpp: host_tests/sd_shims gives it a fake SD card ("/sd/..."
  # redirected to a host directory, every call counted, faults by path) through
  # host_sd_vfs.h, force-included into that one file only, and a counting
  # esp_heap_caps.h. The runtime, the fixture and the core are the usual ones.
  add_executable(a3_04g_storage_runtime
    ../targets/tdeck/host_tests/a3_04g_storage_runtime_test.cpp
    ../targets/tdeck/host_tests/sd_shims/host_sd_vfs.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_board_host_stub.cpp
    ../targets/tdeck/main/alpha_save.cpp
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
  set_source_files_properties(../targets/tdeck/main/alpha_save.cpp PROPERTIES COMPILE_OPTIONS
    "-include;${CMAKE_CURRENT_SOURCE_DIR}/../targets/tdeck/host_tests/sd_shims/host_sd_vfs.h")
  target_link_libraries(a3_04g_storage_runtime PRIVATE openu5_core)
  target_include_directories(a3_04g_storage_runtime PRIVATE
    ../targets/tdeck/host_tests/sd_shims
    ../targets/tdeck/host_tests/board_shims
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/host_tests
    ../targets/tdeck/main)
  target_compile_features(a3_04g_storage_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_04g_storage_runtime PRIVATE /Gy)
    target_link_options(a3_04g_storage_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_04g_storage_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_04g_storage_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_04g_storage_runtime COMMAND a3_04g_storage_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Alpha 3 A3-05 -- the audio finalization controls through the REAL
  # AlphaRuntime: Alt+Shift+M / Alt+Shift+S mutes by raw keys, the Settings
  # rows while muted, stock vs patched assets, and a kill's single hit burst.
  # A3-01's source set, so tools/a3_05_red_first.py can swap HEAD's files in.
  add_executable(a3_05_audio_mute tests/a3_05_audio_mute_test.cpp)
  target_link_libraries(a3_05_audio_mute PRIVATE openu5_core)
  target_compile_features(a3_05_audio_mute PRIVATE cxx_std_17)
  add_test(NAME a3_05_audio_mute COMMAND a3_05_audio_mute)
  add_executable(a3_05_audio_controls
    ../targets/tdeck/host_tests/a3_05_audio_controls_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_05_audio_controls PRIVATE openu5_core)
  target_include_directories(a3_05_audio_controls PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_05_audio_controls PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_05_audio_controls PRIVATE /Gy)
    target_link_options(a3_05_audio_controls PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_05_audio_controls PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_05_audio_controls PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_05_audio_controls COMMAND a3_05_audio_controls
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-audio.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/a3-01-audio-stock.bin")
  # Alpha 3 A3-HF4 -- a successful load discards the replaced game's prompts,
  # pickers, views and presentation timers (ALPHA3_AUDIO.md section 30). The
  # A3-05 source set: the REAL AlphaRuntime over the two-slot memory card, and
  # the capture Board that records the last overlay line and picker panel.
  add_executable(a3_hf4_load_transient_runtime
    ../targets/tdeck/host_tests/a3_hf4_load_transient_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/host_stubs/batch37_board_capture_stub.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_hf4_load_transient_runtime PRIVATE openu5_core)
  target_include_directories(a3_hf4_load_transient_runtime PRIVATE
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/main)
  target_compile_features(a3_hf4_load_transient_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_hf4_load_transient_runtime PRIVATE /Gy)
    target_link_options(a3_hf4_load_transient_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_hf4_load_transient_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_hf4_load_transient_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_hf4_load_transient_runtime COMMAND a3_hf4_load_transient_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # Alpha 3 A3-HF5 -- TLK Pause (0x83) / KeyWait (0x8F) reach the screen: the
  # REAL AlphaRuntime over the pack's TLK corpus, on the virtual clock.
  add_executable(a3_hf5_dialogue_pacing_runtime
    ../targets/tdeck/host_tests/a3_hf5_dialogue_pacing_runtime_test.cpp
    ../targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp
    ../targets/tdeck/host_tests/board_shims/fake_tdeck_bus.cpp
    ../targets/tdeck/main/tdeck_board.cpp
    ../targets/tdeck/main/idle_service.cpp
    ../targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp
    ../targets/tdeck/main/alpha_save_generation.cpp
    ../targets/tdeck/host_tests/host_stubs/tdeck_input_host_stub.cpp
    ../targets/tdeck/main/alpha_audio.cpp
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
  target_link_libraries(a3_hf5_dialogue_pacing_runtime PRIVATE openu5_core)
  target_include_directories(a3_hf5_dialogue_pacing_runtime PRIVATE
    ../targets/tdeck/host_tests/board_shims
    ../targets/tdeck/host_tests/esp_shims
    ../targets/tdeck/host_tests
    ../targets/tdeck/main)
  target_compile_features(a3_hf5_dialogue_pacing_runtime PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(a3_hf5_dialogue_pacing_runtime PRIVATE /Gy)
    target_link_options(a3_hf5_dialogue_pacing_runtime PRIVATE /OPT:REF)
  else()
    target_compile_options(a3_hf5_dialogue_pacing_runtime PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(a3_hf5_dialogue_pacing_runtime PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME a3_hf5_dialogue_pacing_runtime COMMAND a3_hf5_dialogue_pacing_runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/openu5-alpha1-resources.bin")
  # A3-HF5 -- the DialoguePacer queue on its own (no pack, no runtime).
  add_executable(a3_hf5_dialogue_pacer tests/a3_hf5_dialogue_pacer_test.cpp)
  target_link_libraries(a3_hf5_dialogue_pacer PRIVATE openu5_core)
  add_test(NAME a3_hf5_dialogue_pacer COMMAND a3_hf5_dialogue_pacer)
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
    # A3-01 -- the packer's music-capability detector and audio-pack writer:
    # synthetic installs (no game bytes) plus read-only views of the real one.
    add_test(NAME a3_01_audio_capability COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/u5pack/check-audio-pack.ts"
      --real "${CMAKE_CURRENT_SOURCE_DIR}/../../original/u5/ultima5"
      --fixture "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/a3-01-audio-stock.bin")
    set_tests_properties(a3_01_audio_capability PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    # A3-02 -- the reference speaker catalogue the synthesizer is compared with.
    add_test(NAME a3_02_sfx_reference_drift COMMAND "${NODE_EXECUTABLE}" --import tsx
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-sfx-fixtures.ts" --check)
    set_tests_properties(a3_02_sfx_reference_drift PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
  endif()
  if(CMAKE_CONFIGURATION_TYPES)
    set(OPENU5_CTEST_CONFIG_ARGS -C "$<CONFIG>")
  else()
    set(OPENU5_CTEST_CONFIG_ARGS)
  endif()
  add_custom_target(host-test COMMAND "${CMAKE_CTEST_COMMAND}" ${OPENU5_CTEST_CONFIG_ARGS} --output-on-failure
  DEPENDS batch17_theft_watchdog_tests batch19_command_char_tests batch19_command_char_regression frontend_tests ui_session_tests display_names_tests quest_search_tests debug_labels_tests ${UI_DEBUG_TEST_TARGET} debug_map_picker_tests debug_developer_tests gameplay_integration_tests look_sign_tests gameplay_driver movement_flow_parity_tests quest_driver shop_parity_tests shop_flow_tests shop_adapter_tests persistence_driver dialogue_adapter_tests dialogue_parity_tests transport_flow_parity_tests world_flow_adapter_tests dungeon_flow_parity_tests world_flow_parity_tests dungeon_parity_tests advanced_combat_parity_tests magic_parity_tests combat_parity_tests core_parity_tests turn_parity_tests travel_parity_tests command_parity_tests item_parity_tests movement_regression presentation_regression dungeon_view_regression dungeon_art_regression gem_view_regression dungeon_input_regression combat_escape_regression combat_loot_open_regression batch12b_hardware_regression input_regression USES_TERMINAL)
endif()
