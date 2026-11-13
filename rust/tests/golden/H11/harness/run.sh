#!/usr/bin/env bash
# Usage: run.sh <h11_script binary> <out dir>
# Runs every scenario of scenarios.txt (meshes and animations read from steamapps/common, read-only), plus the light and fog
# scripts, writing <out>/<kind>_<name>.txt, then compares each file with the golden vector next to this directory's parent.
# Record the golden vectors: run, then copy <out>/*.txt over the golden files.
set -u
here=$(cd "$(dirname "$0")" && pwd)
bin=$(realpath "$1")
out=$2
common=${OHPL_STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}
export OPENHPL_HEADLESS_SOCKET=x
mkdir -p "$out"
while IFS='|' read -r name seed steps mesh anims; do
	case "$name" in '#'*|'') continue ;; esac
	IFS='|' read -r -a extra <<<"$anims"
	(cd "$common" && "$bin" mesh "$seed" "$steps" "$mesh" "${extra[@]}") > "$out/mesh_$name.txt" 2> "$out/mesh_$name.err"
done < "$here/scenarios.txt"
for seed in 1 2 3 4 5 6 7 8 9 10 11 12; do
	"$bin" light "$seed" 300 > "$out/light_$seed.txt" 2> /dev/null
done
for seed in 1 2 3; do
	"$bin" fog "$seed" 120 > "$out/fog_$seed.txt" 2> /dev/null
done
n=0
bad=0
for f in "$out"/*.txt; do
	n=$((n + 1))
	if ! cmp -s "$f" "$here/../$(basename "$f")"; then
		bad=$((bad + 1))
		echo "DIFF $(basename "$f"): $(diff "$f" "$here/../$(basename "$f")" | head -3 | tr '\n' ' ')"
	fi
done
ops=$(cat "$out"/*.txt | grep -c '^o ')
echo "scenarios: $n files, $ops ops, $bad differ from golden"
[ "$bad" = 0 ]
