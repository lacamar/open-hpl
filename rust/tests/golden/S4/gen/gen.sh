#!/usr/bin/env bash
# Usage: gen.sh <build-dir> <outbin>   (build-dir: configured and built `Soma`; links libHPL2.a for cString)
# Then: <outbin> 12 > ../s4.txt
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$here" rev-parse --show-toplevel)
b=$(realpath "$1")
inc=(-I"$b/core2" -I"$root/HPL2/core" -I"$root/HPL2/core/include" -I"$root/HPL2/dependencies/OALWrapper/include"
	-I/usr/include/AL -I"$root/HPL2/dependencies/newton/." -isystem /usr/include/SDL2
	-idirafter "$root/HPL2/dependencies/include")
c++ -Wl,--allow-multiple-definition -std=c++17 -O1 -g -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -DHPL_VULKAN -DUSE_OALWRAPPER -DUSE_SDL2 \
	-Wno-switch -Wno-reorder "${inc[@]}" "$here/gen.cpp" "$here/../../L5/gen/oal_fake.cpp" -o "$2" \
	"$b/core2/libHPL2.a" "$b/cargo/release/libohpl.a" "$b/core2/libohpl_cdeps.a" "$b/core2/newton/libNewton.a" "$b/core2/libbinreloc.a" \
	-lvorbisfile -lvorbis -logg -lSDL2 -langelscript -lGLU -lGLX -lOpenGL -lGLEW -ltheora -lIL -lassimp -lz -lpugixml -lpoly2tri -lrt -lpthread -ldl \
	-lvulkan /usr/lib64/libSPIRV.a /usr/lib64/libglslang.a -lSPIRV-Tools-opt -lSPIRV-Tools -lSPIRV-Tools-link \
	/usr/lib64/libglslang-default-resource-limits.a
