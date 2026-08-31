cmake_minimum_required (VERSION 3.10...3.28)
project(Amfp)

# Phase 0 scaffolding target: minimal AMFP game module linking the shared
# HPL2 engine library. Modeled on ../../../soma/src/game/CMakeLists.txt,
# which is itself modeled on ../../../amnesia/src/game/CMakeLists.txt but
# trimmed to just the essentials (no Apple bundle, no demo target, no serial
# key checking) - this is Linux-only for now.

file(GLOB all_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
)

add_executable(Amfp
    ${all_sources}
)
target_link_libraries(Amfp HPL2)

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
