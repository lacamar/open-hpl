#!/usr/bin/env bash
# Amnesia save round-trip: installed binary autosave -> new load/save -> new load/save.
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
export OHPL_GAME=amnesia; eval "$(scripts/soma-init.sh --env-only amnesia 2>/dev/null)"
S=${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/amnesia-scratch; O=${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/savert; mkdir -p "$O"
C(){ scripts/hpl_control.py --socket "$SOCK" "$@"; }
up(){ scripts/deploy-test-binary.sh "$1" $S >/dev/null; read -r PID SOCK < <(scripts/soma-run.sh 02_library.map $XDG_RUNTIME_DIR/ohpl-savert.sock); for i in $(seq 60); do C state >/dev/null 2>&1 && break; sleep 2; done; C start_map map=02_library.map >/dev/null; C wait_frames n=120 max_ms=60000 >/dev/null; }
down(){ C quit >/dev/null 2>&1; for i in $(seq 30); do kill -0 $PID 2>/dev/null || return; sleep 1; done; kill -9 $PID; }
up /usr/libexec/open-hpl/Amnesia.bin.aarch64
before=$(date +%s); C run_script 'line=AutoSave();' >/dev/null; C wait_frames n=60 max_ms=30000 >/dev/null; sleep 2
A=$(find $S/.xdg -name '*.sav' -newermt "@$before" | head -1); down
echo "A=$A"; [ -n "$A" ] || { echo "no autosave"; exit 1; }; cp "$A" $O/A.sav
up $R/amnesia/src/build/Amnesia.bin.aarch64
C load_game file=$O/A.sav; C wait_frames n=120 max_ms=60000 >/dev/null
C save_game file=$O/B.sav; C wait_frames n=10 >/dev/null
C load_game file=$O/B.sav; C wait_frames n=120 max_ms=60000 >/dev/null
C save_game file=$O/C.sav; C wait_frames n=10 >/dev/null; down
ls -la $O/*.sav
strip(){ sed -E 's/val="[^"]*"/val=""/g' "$1"; }
echo "structure diffs A-B: $(diff <(strip $O/A.sav) <(strip $O/B.sav) | grep -c '^[<>]')  B-C: $(diff <(strip $O/B.sav) <(strip $O/C.sav) | grep -c '^[<>]')"
