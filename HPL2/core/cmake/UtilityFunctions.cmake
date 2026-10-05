### build up utility functions

function(FindPrebuiltLibrary result_var libname)
    find_library(${result_var} NAMES ${libname})
    if(NOT ${result_var})
        message(FATAL_ERROR "Could not find library ${libname}")
    endif()
endfunction()
