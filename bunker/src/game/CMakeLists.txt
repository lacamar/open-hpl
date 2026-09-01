cmake_minimum_required (VERSION 3.10...3.28)
project(Bunker)

# Phase 0 scaffolding target: minimal Amnesia: The Bunker game module linking
# the shared HPL2 engine library. Modeled on ../../../soma/src/game/CMakeLists.txt.

file(GLOB all_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
)

add_executable(Bunker
    ${all_sources}
)
target_link_libraries(Bunker HPL2)

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
