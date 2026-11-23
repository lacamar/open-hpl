cmake_minimum_required (VERSION 3.10...3.28)
project(Soma)

# Phase 0 scaffolding target: minimal SOMA game module linking the shared HPL2
# engine library. Modeled on ../../../amnesia/src/game/CMakeLists.txt but
# trimmed to just the essentials (no Apple bundle, no demo target, no serial
# key checking) - this is Linux-only for now.

file(GLOB all_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
)

add_library(SomaGame STATIC
    ${all_sources}
)
target_link_libraries(SomaGame PUBLIC HPL2)
set_target_properties(SomaGame PROPERTIES CXX_STANDARD 20)
target_compile_definitions(SomaGame PRIVATE OPENHPL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../..")
ohpl_game(Soma soma)

add_definitions(-DLINUX)

# vim: noet ts=4:
