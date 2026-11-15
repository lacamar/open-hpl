#!/usr/bin/env bash
# Journal save round trip: notes, diaries and quest notes saved by <save-binary>, loaded and re-saved by <load-binary>,
# twice; the journal section must be identical in all three saves.
# Usage: scripts/a11c-journal-roundtrip.sh <save-binary> <load-binary> <out-dir>
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
SAVEBIN=${1:?save binary}; LOADBIN=${2:?load binary}; O=${3:?out dir}; mkdir -p "$O"
export OHPL_GAME=amnesia; eval "$(scripts/soma-init.sh --env-only amnesia 2>/dev/null)"
S=$OPENHPL_AMNESIA_SCRATCH
C(){ scripts/hpl_control.py --socket "$SOCK" "$@"; }
up(){
	scripts/deploy-test-binary.sh "$1" "$S" >/dev/null
	read -r PID SOCK < <(scripts/soma-run.sh 00_rainy_hall.map "$XDG_RUNTIME_DIR/${OHPL_PLAY_NAME:-ohpl-a11c-rt}.sock")
	for i in $(seq 60); do C state >/dev/null 2>&1 && break; sleep 2; done
}
down(){ C quit >/dev/null 2>&1; for i in $(seq 30); do kill -0 "$PID" 2>/dev/null && sleep 1 || return; done; kill -9 "$PID"; }
up "$SAVEBIN"
C start_map map=00_rainy_hall.map >/dev/null; C wait_frames n=120 max_ms=60000 >/dev/null
C run_script line='AddNote("CH01L03_VillageTales","papernote.tga");AddNote("CH01L11_AccountOfHowling","letter.tga");AddDiary("CH01L03_Daniel","diary_page.tga");AddDiary("CH01L03_Daniel","diary_page.tga");AddDiary("CH01L06_Daniel02","diary_page.tga");AddQuest("00Trail","00Trail");AddQuest("02Web","02Web");CompleteQuest("02Web","02Web");' >/dev/null
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
def jrn(n):
    t = open(f"{o}/{n}.sav", errors="replace").read()
    m = re.search(r'\n([ \t]*)<class type="cLuxJournal_SaveData".*?\n\1</class>', t, re.S)
    return m.group(0) if m else None
a, b, c = jrn("A"), jrn("B"), jrn("C")
notes = len(re.findall(r'<class type="cLuxNote"', a or ""))
diaries = len(re.findall(r'<class type="cLuxDiary"', a or ""))
quests = len(re.findall(r'<class type="cLuxQuestNote"', a or ""))
print(f"notes {notes} diaries {diaries} quests {quests}  A==B: {a == b}  B==C: {b == c}")
sys.exit(0 if a and a == b == c and notes == 2 and diaries == 3 and quests == 2 else 1)
EOF
