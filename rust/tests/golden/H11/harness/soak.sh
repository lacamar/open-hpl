#!/usr/bin/env bash
# Usage: soak.sh <h11_script binary> [seeds]   extra seeds over every scenario plus long light/fog scripts, cpp vs rust, nothing recorded.
set -u
here=$(cd "$(dirname "$0")" && pwd)
bin=$(realpath "$1")
seeds=${2:-"101 102 103"}
common=${OHPL_STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}
tmp=${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h11-soak
mkdir -p "$tmp"
export OPENHPL_HEADLESS_SOCKET=x
n=0 bad=0 ops=0
while IFS='|' read -r name seed steps mesh anims; do
	case "$name" in '#'*|'') continue ;; esac
	IFS='|' read -r -a extra <<<"$anims"
	for s in $seeds; do
		for m in cpp rust; do
			(cd "$common" && OHPL_IMPL=$m "$bin" mesh "$s" 600 "$mesh" "${extra[@]}") > "$tmp/$m.txt" 2> /dev/null
		done
		n=$((n + 1))
		ops=$((ops + $(grep -c '^o ' "$tmp/cpp.txt")))
		cmp -s "$tmp/cpp.txt" "$tmp/rust.txt" || { bad=$((bad + 1)); echo "DIFF $name $s"; }
	done
done < "$here/scenarios.txt"
for s in 21 22 23 24 25 26; do
	for k in light fog; do
		for m in cpp rust; do OHPL_IMPL=$m "$bin" $k "$s" 2000 > "$tmp/$m.txt" 2> /dev/null; done
		n=$((n + 1))
		ops=$((ops + $(grep -c '^o ' "$tmp/cpp.txt")))
		cmp -s "$tmp/cpp.txt" "$tmp/rust.txt" || { bad=$((bad + 1)); echo "DIFF $k $s"; }
	done
done
echo "soak: $n runs, $ops ops, $bad differ"
