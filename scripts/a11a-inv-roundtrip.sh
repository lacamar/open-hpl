#!/usr/bin/env bash
# Inventory save round trip: items, counts, equipped hand item and combine callbacks saved by <save-binary>, loaded and
# re-saved by <load-binary>, twice; the inventory section must be identical in all three saves.
# Usage: scripts/a11a-inv-roundtrip.sh <save-binary> <load-binary> <out-dir>
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
SAVEBIN=${1:?save binary}; LOADBIN=${2:?load binary}; O=${3:?out dir}; mkdir -p "$O"
export OHPL_GAME=amnesia; eval "$(scripts/soma-init.sh --env-only amnesia 2>/dev/null)"
S=$OPENHPL_AMNESIA_SCRATCH
C(){ scripts/hpl_control.py --socket "$SOCK" "$@"; }
up(){
	scripts/deploy-test-binary.sh "$1" "$S" >/dev/null
	read -r PID SOCK < <(scripts/soma-run.sh 00_rainy_hall.map "$XDG_RUNTIME_DIR/${OHPL_PLAY_NAME:-ohpl-a11a-rt}.sock")
	for i in $(seq 60); do C state >/dev/null 2>&1 && break; sleep 2; done
}
down(){ C quit >/dev/null 2>&1; for i in $(seq 30); do kill -0 "$PID" 2>/dev/null && sleep 1 || return; done; kill -9 "$PID"; }
up "$SAVEBIN"
C start_map map=00_rainy_hall.map >/dev/null; C wait_frames n=120 max_ms=60000 >/dev/null
C run_script line='SetInventoryDisabled(true);GiveItem("hand_a","HandObject","hand","bone_saw.tga",0);GiveItem("puz_a","Puzzle","agrippa_head","agrippa_head.tga",0);GiveItem("puz_b","Puzzle","bone_saw","bone_saw.tga",3);GiveItem("hp_a","Health","health","coins_large.tga",25);GiveItem("hp_b","Health","health","coins_large.tga",25);GiveItem("oil_a","LampOil","oil","bucket_of_tar.tga",20);GiveItem("lantern","Lantern","lantern","lantern.tga",0);GiveItem("hand_b","HandObject","hand2","bone_saw.tga",0);AddCombineCallback("cb1","puz_a","puz_b","CombineFn",true);AddCombineCallback("cb2","hp_a","oil_a","OtherFn",false);' >/dev/null
C wait_frames n=30 max_ms=30000 >/dev/null
C save_game file=$O/A.sav >/dev/null; C wait_frames n=10 >/dev/null; down
up "$LOADBIN"
C load_game file=$O/A.sav >/dev/null; C wait_frames n=120 max_ms=60000 >/dev/null
C save_game file=$O/B.sav >/dev/null; C wait_frames n=10 >/dev/null
C load_game file=$O/B.sav >/dev/null; C wait_frames n=120 max_ms=60000 >/dev/null
C save_game file=$O/C.sav >/dev/null; C wait_frames n=10 >/dev/null; down
python3 - "$O" <<'EOF'
import re, sys
o = sys.argv[1]
def inv(n):
    t = open(f"{o}/{n}.sav", errors="replace").read()
    m = re.search(r'\n([ \t]*)<class type="cLuxInventory_SaveData".*?\n\1</class>', t, re.S)
    return re.sub(r"(graphics/item/)+", "graphics/item/", m.group(0)) if m else None
a, b, c = inv("A"), inv("B"), inv("C")
items = len(re.findall(r'<class type="cLuxInventory_Item_SaveData"', a or ""))
print(f"items in A: {items}  A==B: {a == b}  B==C: {b == c}")
sys.exit(0 if a and a == b == c and items else 1)
EOF
