cmake_minimum_required (VERSION 3.10...3.28)
project(Lux)

# Must be called at the top-level project scope for `ctest` (run from this
# project's build dir) to discover tests registered further down in
# HPL2/core/CMakeLists.txt / HPL2/tests/CMakeLists.txt.
enable_testing()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

OPTION(USERDIR_RESOURCES "Allow using local userdirectory resources" ON)

set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2010-2013 Frictional Games")

OPTION(IS_APPLE_64 "Apple build in 64 bits" OFF)

if(APPLE)
    if(NOT IS_APPLE_64)
        set(CMAKE_OSX_ARCHITECTURES "i386")
    endif()
endif()

# silence some annoying warnings
add_definitions(
    -Wno-switch
    -Wno-reorder
    -Wno-unused-variable
    -Wno-unused-function
)

add_subdirectory(../../HPL2/core core2)

if(USERDIR_RESOURCES)
    add_definitions(
        -DUSERDIR_RESOURCES
    )
endif()

set(VERSION "1.3.1")

add_subdirectory(game game)

# Phase 0 of the SOMA game module (see PORTING_NOTES.md / the SOMA plan doc):
# a sibling target that links the same HPL2 library. Additive only - does not
# touch anything above.
add_subdirectory(../../soma/src/game soma_game)

# Phase 0 of the AMFP (Amnesia: A Machine for Pigs) game module - another
# sibling target linking the same HPL2 library, same as soma_game above.
add_subdirectory(../../amfp/src/game amfp_game)

# Phase 0 of the Amnesia: Rebirth and Amnesia: The Bunker game modules -
# further sibling targets, same additive pattern as soma_game/amfp_game
# above. Both are Windows-only titles with no Linux binary of their own in
# this project's Steam library, so unlike SOMA there is no existing native
# build to compare against - these boot straight off each game's own real
# data files.
add_subdirectory(../../rebirth/src/game rebirth_game)
add_subdirectory(../../bunker/src/game bunker_game)

if(APPLE)
    add_subdirectory(launcher-macosx launcher)
else()
    add_subdirectory(launcher launcher)
endif()

add_custom_target(GameRelease
    DEPENDS Amnesia Launcher
)
