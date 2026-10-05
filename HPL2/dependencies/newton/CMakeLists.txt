file(GLOB newton_sources *.cpp core/*.cpp physics/*.cpp)
add_library(Newton STATIC ${newton_sources})
target_include_directories(Newton PUBLIC .)
target_compile_options(Newton PRIVATE -w -fno-strict-aliasing)
