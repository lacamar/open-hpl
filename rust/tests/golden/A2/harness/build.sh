#!/bin/bash
# build.sh [orig|new|both]
set -e
H=/home/lm/.cache/open-hpl/a2b-harness
W=/home/lm/.local/src/open-hpl/.claude/worktrees/agent-a46003426b7ef3f23
LIB=/home/lm/.cache/open-hpl/port-a2b/release/libohpl.a
cd $H
mkdir -p obj
FLAGS="-std=gnu++20 -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O2 -g -DNDEBUG -Wno-switch -Wno-reorder -Wno-unused-variable -Wno-unused-function -Wno-narrowing -fpermissive -DLINUX"
COMMON="-I$H -I$W/amnesia/src/game -I$W/HPL2/core/include"
which=${1:-both}
python3 -I gen.py new
{
for f in math/Math:Math math/MathTypes:MathTypes system/String:String graphics/Color:Color math/BoundingVolume:BV math/Quaternion:Quat; do
  src=${f%%:*}; o=${f##*:}
  [ -f obj/$o.o ] || echo "g++ $FLAGS $COMMON -c $W/HPL2/core/sources/$src.cpp -o obj/$o.o"
done
[ $which = new ] || echo "g++ $FLAGS $COMMON -c main.cpp -o obj/main_orig.o"
[ $which = orig ] || echo "g++ $FLAGS $COMMON -DNEWSIDE -c main.cpp -o obj/main_new.o"
} | xargs -P3 -I{} sh -c "{}"
C="obj/Math.o obj/MathTypes.o obj/String.o obj/Color.o obj/BV.o obj/Quat.o"
[ $which = new ] || g++ $FLAGS obj/main_orig.o $C $LIB -lSDL2 -Wl,--gc-sections -Wl,--unresolved-symbols=ignore-all -lpthread -ldl -lm -lz -o harness_orig
[ $which = orig ] || g++ $FLAGS obj/main_new.o $C $LIB -lSDL2 -Wl,--gc-sections -Wl,--unresolved-symbols=ignore-all -lpthread -ldl -lm -lz -o harness_new
echo built
