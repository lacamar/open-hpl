#!/usr/bin/env bash
# Usage: run.sh <h11_script binary> <out dir> [cpp|rust|both]
# Runs every scenario of scenarios.txt (meshes and animations read from steamapps/common, read-only), plus the light and fog
# scripts, writing <out>/<kind>_<name>.txt per OHPL_IMPL (<out>/cpp, <out>/rust). "both" compares the dumps and prints the counts.
# Record the golden vectors: run.sh <baseline binary> <dir> cpp; copy <dir>/cpp/*.txt next to this directory's parent.
set -u
here=$(cd "$(dirname "$0")" && pwd)
bin=$(realpath "$1")
out=$2
modes=${3:-both}
[ "$modes" = both ] && modes="cpp rust"
common=${OHPL_STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}
export OPENHPL_HEADLESS_SOCKET=x
for m in $modes; do
	mkdir -p "$out/$m"
	while IFS='|' read -r name seed steps mesh anims; do
		case "$name" in '#'*|'') continue ;; esac
		args=()
		IFS='|' read -r -a extra <<<"$anims"
		(cd "$common" && OHPL_IMPL=$m "$bin" mesh "$seed" "$steps" "$mesh" "${extra[@]}") > "$out/$m/mesh_$name.txt" 2> "$out/$m/mesh_$name.err"
	done < "$here/scenarios.txt"
	for seed in 1 2 3 4 5 6 7 8 9 10 11 12; do
		OHPL_IMPL=$m "$bin" light "$seed" 300 > "$out/$m/light_$seed.txt" 2> /dev/null
	done
	for seed in 1 2 3; do
		OHPL_IMPL=$m "$bin" fog "$seed" 120 > "$out/$m/fog_$seed.txt" 2> /dev/null
	done
done
if [ "$modes" = "cpp rust" ]; then
	n=0
	bad=0
	for f in "$out"/cpp/*.txt; do
		n=$((n + 1))
		if ! cmp -s "$f" "$out/rust/$(basename "$f")"; then
			bad=$((bad + 1))
			echo "DIFF $(basename "$f"): $(diff "$f" "$out/rust/$(basename "$f")" | head -3 | tr '\n' ' ')"
		fi
	done
	ops=$(cat "$out"/cpp/*.txt | grep -c '^o ')
	echo "scenarios: $n files, $ops ops, $bad differ"
fi
