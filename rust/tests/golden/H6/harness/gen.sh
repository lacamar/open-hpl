#!/usr/bin/env bash
# Usage: gen.sh <build-dir> <source.cpp> <outbin>   (build-dir: configured and built `HPL2`)
# source: h6_gen.cpp (census; CreateMaterial replaced so no material is loaded), h6_script.cpp,
# h6_creator.cpp (needs `make HPL2TraceRecorder` in the build dir).
# Census run:  OHPL_IMPL=cpp <outbin> h6msh a.msh > cpp.txt; OHPL_IMPL=rust <outbin> h6msh a.msh > rust.txt; diff cpp.txt rust.txt
# Script run:  H6_SCENARIO=skel|anim|mesh H6_SEED=<n> OHPL_IMPL=cpp|rust <outbin>
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$here" rev-parse --show-toplevel)
b=$(realpath "$1")
inc=(-I"$root/HPL2/tests" -I"$b/core2" -I"$root/HPL2/core" -I"$root/HPL2/core/include" -I"$root/HPL2/dependencies/OALWrapper/include"
	-I/usr/include/AL -I"$root/HPL2/dependencies/newton/." -isystem /usr/include/SDL2
	-idirafter "$root/HPL2/dependencies/include")
c++ -Wl,--allow-multiple-definition -std=c++17 -O1 -g -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -DHPL_VULKAN -DUSE_OALWRAPPER -DUSE_SDL2 \
	-Wno-switch -Wno-reorder "${inc[@]}" "$here/$2" -o "$3" \
	"$b/core2/tests/libHPL2TraceRecorder.a" "$b/core2/libHPL2.a" "$b/cargo/release/libohpl.a" "$b/core2/libohpl_cdeps.a" "$b/core2/newton/libNewton.a" "$b/core2/libbinreloc.a" \
	"$b/core2/OALWrapper/libOALWrapper.a" \
	-lvorbisfile -lvorbis -logg -lSDL2 -langelscript -lGLU -lGLX -lOpenGL -lGLEW -ltheora -lIL -lassimp -lz -lpugixml -lpoly2tri -lrt -lpthread -ldl -lopenal \
	-lvulkan /usr/lib64/libSPIRV.a /usr/lib64/libglslang.a -lSPIRV-Tools-opt -lSPIRV-Tools -lSPIRV-Tools-link \
	/usr/lib64/libglslang-default-resource-limits.a
