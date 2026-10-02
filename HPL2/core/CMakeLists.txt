cmake_minimum_required (VERSION 3.10...3.28)
project(HPL2)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(BoilerPlate)

set(HPL2_VERSION_MAJOR 2)
set(HPL2_VERSION_MINOR 0)

set(DEP_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/lib/${PLATFORM_PREFIX}")
set(DEP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/include")
find_path(PREBUILT_PLATFORM_ROOT .
    PATHS "${DEP_LIB_DIR}"
    NO_DEFAULT_PATH
)
set(DEP_LIB_DIR ${DEP_LIB_DIR} PARENT_SCOPE)
set(DEP_INCLUDE_DIR ${DEP_INCLUDE_DIR} PARENT_SCOPE)
mark_as_advanced(
    PREBUILT_PLATFORM_ROOT
)

include(UtilityFunctions)

OPTION(USE_SDL2 "Use SDL2 instead of SDL1.2" ON)

add_subdirectory(../dependencies/OALWrapper OALWrapper)

SET(HPL2_DEFINES
    PUBLIC $<$<BOOL:${USE_SDL2}>:USE_SDL2>
)

add_definitions(
    -DUSE_OALWRAPPER
)

set(HPL2_INCLUDES
    PUBLIC include
    #Important that this is before dependencies so the correct OpenAL headers are used
    PRIVATE ${OALWRAPPER_INCLUDE_DIR}
)

add_library(binreloc STATIC ../dependencies/sources/binreloc/binreloc.c)
set_target_properties(binreloc PROPERTIES COMPILE_DEFINITIONS "ENABLE_BINRELOC")
list(APPEND HPL2_INCLUDES ../dependencies/sources/binreloc/)


# common shared source
file(GLOB common_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    sources/ai/*
    sources/engine/*
    sources/generate/*
    sources/graphics/*
    sources/gui/*
    sources/haptic/*
    sources/input/*
    sources/math/*
    sources/physics/*
    sources/resources/*
    sources/scene/*
    sources/sound/*
    sources/system/*

    include/ai/*
    include/engine/*
    include/generate/*
    include/graphics/*
    include/gui/*
    include/haptic/*
    include/input/*
    include/math/*
    include/physics/*
    include/resources/*
    include/scene/*
    include/sound/*
    include/system/*
    include/*.*
)
list(APPEND common_sources include/BuildID_HPL2_0.h)
list(REMOVE_DUPLICATES common_sources)

#setup various IMPL sources
file(GLOB impl_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    include/impl/tinyXML/*
    include/impl/*.*

    # tinyXML
    sources/impl/tinyXml/*
    sources/impl/XmlDocumentTiny.cpp
    # scripting
    sources/impl/SqScript.cpp
    sources/impl/scriptarray.cpp
    sources/impl/scripthelper.cpp
    sources/impl/scriptstring.cpp
    sources/impl/scriptstring_utils.cpp
    # Image loading
    sources/impl/BitmapLoader*
    # theora video
    sources/impl/VideoStreamTheora.cpp
    # Newton
    sources/impl/*Newton.cpp
    # GL
    sources/impl/FrameBufferGL.cpp
    sources/impl/GLSL*
    sources/impl/OcclusionQueryOGL.cpp
    sources/impl/VertexBufferOGL_Array.cpp
    sources/impl/VertexBufferOGL_VBO.cpp
    sources/impl/VertexBufferOpenGL.cpp
    # SDL
    sources/impl/GamepadSDL.cpp
    sources/impl/GamepadSDL2.cpp
    sources/impl/KeyboardSDL.cpp
    sources/impl/MouseSDL.cpp
    sources/impl/MutexSDL.cpp
    sources/impl/ThreadSDL.cpp
    sources/impl/TimerSDL.cpp
    sources/impl/LowLevelGraphicsSDL.cpp
    sources/impl/LowLevelInputSDL.cpp
    sources/impl/LowLevelResourcesSDL.cpp
    sources/impl/LowLevelSystemSDL.cpp
    sources/impl/SDLEngineSetup.cpp
    sources/impl/SDLFontData.cpp
    sources/impl/SDLTexture.cpp
    # OpenAL
    sources/impl/LowLevelSoundOpenAL.cpp
    sources/impl/OpenAL*
    # mesh loader
    sources/impl/MeshLoaderCollada.cpp
    sources/impl/MeshLoaderColladaHelpers.cpp
    sources/impl/MeshLoaderColladaLoader.cpp
    sources/impl/MeshLoaderMSH.cpp
    sources/impl/MeshLoaderAssimp.cpp
)

LIST(APPEND impl_sources
    sources/impl/PlatformUnix.cpp
    sources/impl/PlatformSDL.cpp
)

# Setup buildID source

SET(BUILDID_SUFFIX Linux CACHE STRING "Build ID Suffix")
find_file(BUILDID_BIN buildcounter.pl
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}
    NO_DEFAULT_PATH
)
mark_as_advanced(BUILDID_SUFFIX BUILDID_BIN)

function(GenerateBuildID outvar idname includedir sourcedir)
    add_custom_command(
        OUTPUT
            ${sourcedir}/BuildID_${idname}_${BUILDID_SUFFIX}.cpp
            ${includedir}/BuildID_${idname}.h
        COMMAND
            ${BUILDID_BIN}
        ARGS
            ${idname} ${includedir} ${sourcedir}
    )
    set(${outvar} ${sourcedir}/BuildID_${idname}_${BUILDID_SUFFIX}.cpp
            ${includedir}/BuildID_${idname}.h PARENT_SCOPE)
endfunction()

GenerateBuildID(version_source
    HPL2_0
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sources
)


# setup libs

FindPrebuiltLibrary(NEWTON_LIBRARY Newton)
FindPrebuiltLibrary(DGPHYSICS_LIBRARY dgPhysics)
FindPrebuiltLibrary(DGCORE_LIBRARY dgCore)
FindPrebuiltLibrary(ANGELSCRIPT_LIBRARY angelscript)
FindPrebuiltLibrary(GLEW_LIBRARY GLEW)
FindPrebuiltLibrary(THEORA_LIBRARY theora)
FindPrebuiltLibrary(DEVIL_LIBRARY IL)

find_package(OpenGL)
find_package(assimp REQUIRED)
mark_as_advanced(NEWTON_LIBRARY DGPHYSICS_LIBRARY DGCORE_LIBRARY ANGELSCRIPT_LIBRARY GLEW_LIBRARY THEORA_LIBRARY DEVIL_LIBRARY)

# setup HPL2 compile target

add_library(HPL2 STATIC
    ${common_sources}
    ${version_sources}
    ${impl_sources}
)

target_include_directories(HPL2
    ${HPL2_INCLUDES}
)

target_compile_options(HPL2 PUBLIC -idirafter "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/include")

target_compile_definitions(HPL2
    ${HPL2_DEFINES}
)

set(OPENHPL_VERSION "" CACHE STRING "Version shown by the dev HUD (default: git describe)")
set(hud_version "${OPENHPL_VERSION}")
if(NOT hud_version)
    execute_process(COMMAND git describe --tags --dirty WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE hud_version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(hud_version)
    set_property(SOURCE sources/engine/DevHud.cpp APPEND PROPERTY COMPILE_DEFINITIONS OPENHPL_VERSION="${hud_version}")
endif()

set(PLATFORM_LIBS
    binreloc
    pthread
    dl
)

target_link_libraries(HPL2
    OALWrapper # also pulls in ogg,vorbis,SDL
    ${NEWTON_LIBRARY}
    ${DGPHYSICS_LIBRARY}
    ${DGCORE_LIBRARY}
    ${ANGELSCRIPT_LIBRARY}
    # Modern (GLVND-aware) FindOpenGL only sets OPENGL_opengl_LIBRARY, not the
    # legacy OPENGL_gl_LIBRARY this project originally used - the imported
    # targets work correctly under both old and new CMake/FindOpenGL behavior.
    OpenGL::GL OpenGL::GLU
    ${GLEW_LIBRARY}
    ${THEORA_LIBRARY}
    ${DEVIL_LIBRARY}
    assimp::assimp
    z
    ${PLATFORM_LIBS}
)


FUNCTION(AddTestTarget target_name)
    add_executable(${target_name} EXCLUDE_FROM_ALL
        ${ARGN}
    )
    target_link_libraries(${target_name} HPL2)
    set_target_properties(${target_name} PROPERTIES
        INSTALL_RPATH "${DEP_LIB_DIR}"
    )
ENDFUNCTION()

enable_testing()
add_subdirectory(../tests tests)

