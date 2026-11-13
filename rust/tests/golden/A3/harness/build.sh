#!/bin/bash
# build.sh [orig|new|both]: differential harness for the A3 player states. Needs the release libohpl.a
# (CARGO_TARGET_DIR=$A3/cargo cargo build --release in rust/). Scratch output goes to $A3/harness.
set -e
R=$(cd "$(dirname "$0")" && pwd)
W=$(cd "$R/../../../../.." && pwd)
A3=${A3:-$HOME/.cache/open-hpl/port-a3}
H=$A3/harness
LIB=${LIB:-$A3/cargo/release/libohpl.a}
mkdir -p "$H/obj"
cd "$H"
FLAGS="-std=gnu++20 -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O2 -g -DNDEBUG -Wno-switch -Wno-reorder -Wno-unused-variable -Wno-unused-function -Wno-narrowing -fpermissive -DLINUX -w"
COMMON="-I$H -I$R -I$W/amnesia/src/game -I$W/HPL2/core/include"
which=${1:-both}
python3 -I "$R/genwrap.py" "$H"
[ $which = new ] || python3 -I "$R/gen.py" orig "$H" "$W"
[ $which = orig ] || python3 -I "$R/gen.py" new "$H" "$W"
{
for f in math/Math:Math math/MathTypes:MathTypes system/String:String graphics/Color:Color math/BoundingVolume:BV math/Quaternion:Quat; do
  src=${f%%:*}; o=${f##*:}
  [ -f obj/$o.o ] || echo "g++ $FLAGS $COMMON -c $W/HPL2/core/sources/$src.cpp -o obj/$o.o"
done
[ $which = new ] || echo "g++ $FLAGS $COMMON -c $R/main.cpp -o obj/main_orig.o"
[ $which = orig ] || echo "g++ $FLAGS $COMMON -DNEWSIDE -c $R/main.cpp -o obj/main_new.o"
} | xargs -P2 -I{} sh -c "{}"
C="obj/Math.o obj/MathTypes.o obj/String.o obj/Color.o obj/BV.o obj/Quat.o"
LINK="-lSDL2 -Wl,--gc-sections -Wl,--unresolved-symbols=ignore-all -lpthread -ldl -lm -lz"
[ $which = new ] || g++ $FLAGS obj/main_orig.o $C $LIB $LINK -o harness_orig
[ $which = orig ] || g++ $FLAGS obj/main_new.o $C $LIB $LINK -o harness_new
echo built
