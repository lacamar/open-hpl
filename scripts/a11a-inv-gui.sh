#!/usr/bin/env bash
# Amnesia inventory GUI drive: give items, open the inventory with Tab, hover/pick/drop/combine/double-click slots,
# screenshot + save file after each step. Two binaries driven the same way give identical files.
# Usage: scripts/a11a-inv-gui.sh <Amnesia.bin.aarch64> <out-dir>
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
BIN=${1:?binary}; O=${2:?out dir}; mkdir -p "$O"
export OHPL_GAME=amnesia; eval "$(scripts/soma-init.sh --env-only amnesia 2>/dev/null)"
S=$OPENHPL_AMNESIA_SCRATCH
C(){ scripts/hpl_control.py --socket "$SOCK" "$@"; }
scripts/deploy-test-binary.sh "$BIN" "$S" >/dev/null
read -r PID SOCK < <(scripts/soma-run.sh 00_rainy_hall.map "$XDG_RUNTIME_DIR/${OHPL_PLAY_NAME:-ohpl-a11a}.sock")
for i in $(seq 60); do C state >/dev/null 2>&1 && break; sleep 2; done
C start_map map=00_rainy_hall.map >/dev/null; C wait_frames n=120 max_ms=60000 >/dev/null
step(){ C wait_frames n=${2:-20} max_ms=30000 >/dev/null; C screenshot path=$O/$1.bmp >/dev/null; C save_game file=$O/$1.sav >/dev/null; C wait_frames n=3 >/dev/null; }
mv(){ C input type=mouse_move x=$1 y=$2 xrel=1 yrel=1 >/dev/null; }
btn(){ C input type=mouse_button button=left action=$1 >/dev/null; }
slot(){ echo $((200 + 77 * $1)) $((100 + 100 * $2)); }
C run_script line='SetInventoryDisabled(false);SetPlayerHealth(40);AddPlayerSanity(-50);SetPlayerLampOil(10);GiveItem("puz_a","Puzzle","agrippa_head","agrippa_head.tga",0);GiveItem("puz_b","Puzzle","bone_saw","bone_saw.tga",0);GiveItem("hp_a","Health","health","coins_large.tga",25);GiveItem("oil_a","LampOil","oil","bucket_of_tar.tga",20);GiveItem("lantern","Lantern","lantern","lantern.tga",0);GiveItem("hand_a","HandObject","hand","bone_saw.tga",0);GiveItem("hp_b","Health","health","coins_large.tga",25);' >/dev/null
C wait_frames n=30 max_ms=30000 >/dev/null
C input type=key key=tab action=tap >/dev/null; C wait_frames n=40 max_ms=30000 >/dev/null
C input type=key key=tab action=tap >/dev/null
step open 90
for k in "0 0" "1 0" "2 0" "3 0" "0 1" "5 2"; do mv $(slot $k); step hover_${k// /_} 30; done
mv 80 170; step health 30
mv 80 440; step sanity 30
mv 700 300; step oil 30
mv 700 120; step tinder 30
mv 700 460; step journal 30
mv $(slot 0 0); C wait_frames n=10 >/dev/null; btn down; C wait_frames n=5 >/dev/null
mv $(slot 1 0); step drag 10
btn up; step combine_fail 60
mv $(slot 3 0); C wait_frames n=10 >/dev/null; btn down; C wait_frames n=5 >/dev/null
mv $(slot 4 0); step drag_oil 10
btn up; step combine_oil 60
mv $(slot 2 0); C wait_frames n=10 >/dev/null; btn down; btn up; btn down; btn up; step dbl_health 40
mv $(slot 5 0); C wait_frames n=10 >/dev/null; btn down; btn up; btn down; btn up; step dbl_hand 40
mv $(slot 4 0); C wait_frames n=10 >/dev/null; btn down; btn up; btn down; btn up; step dbl_lantern 40
C input type=key key=escape action=tap >/dev/null; step esc 90
C quit >/dev/null 2>&1; for i in $(seq 30); do kill -0 "$PID" 2>/dev/null || exit 0; sleep 1; done; kill -9 "$PID"
