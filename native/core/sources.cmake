# Shared source list: host and ESP-IDF compile the same portable implementation.
set(OPENU5_CORE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/rng.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/time.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/state.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/world.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/movement.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/turn.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/actors.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/npc_path.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/pathfind.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/transitions.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/commands.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/inventory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/rest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/combat.cpp")
