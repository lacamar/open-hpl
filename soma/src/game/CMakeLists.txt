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

add_executable(Soma
    ${all_sources}
)
target_link_libraries(Soma HPL2)

IF(APPLE)
    add_definitions(
        -DMAC_OS
    )
ELSEIF(LINUX)
    add_definitions(
        -DLINUX
    )
ENDIF()

# vim: noet ts=4:
