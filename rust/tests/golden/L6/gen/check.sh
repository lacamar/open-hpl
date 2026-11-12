#!/usr/bin/env bash
# Usage: check.sh <build-dir> [stride]   (build-dir: configured and built `HPL2`, see gen.sh)
# Lists the .msh/.dae/.fbx/.anm of the installed games (read-only), loads every file (or every
# stride-th) with the C++ and the Rust path, diffs the full outputs and compares them with the
# golden files. Skinned .dae (with <controller>) crash the C++ loader without an MSH cache and are
# left out of the dae lists.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
g=$(dirname "$here")
stride=${2:-1}
root=${OHPL_STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}
tmp=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/l6-tmp/check.XXXXXX")
export OPENHPL_HEADLESS_SOCKET=x
"$here/gen.sh" "$1" "$tmp/gen"
games=("Amnesia The Dark Descent" "Machine for Pigs" SOMA "Amnesia Rebirth" "Amnesia The Bunker")
list() { (cd "$root" && for d in "${games[@]}"; do find "$d" -iname "*.$1"; done | sort | awk -v s="$stride" 'NR%s==1||s==1'); }
list dae > "$tmp/dae.all"
(cd "$root" && tr '\n' '\0' < "$tmp/dae.all" | xargs -0 grep -l '<controller' || true) > "$tmp/dae.skinned"
grep -vxFf "$tmp/dae.skinned" "$tmp/dae.all" > "$tmp/dae.lst"
list msh > "$tmp/msh.lst"
list fbx > "$tmp/fbx.lst"
list anm > "$tmp/anm.lst"
cp "$tmp/fbx.lst" "$tmp/fbxanim.lst"
awk 'NR%12<3' "$tmp/dae.lst" > "$tmp/daeanim.lst"
for k in msh fbx anm fbxanim dae daeanim; do
	for m in cpp rust; do
		(cd "$root" && OHPL_IMPL=$m "$tmp/gen" "$k" - < "$tmp/$k.lst" > "$tmp/$k.$m" 2> /dev/null)
	done
	if ! diff "$tmp/$k.cpp" "$tmp/$k.rust" > "$tmp/$k.diff"; then
		echo "$k: $(grep -c '^<' "$tmp/$k.diff") differences, see $tmp/$k.diff"
		continue
	fi
	python3 "$here/compact.py" < "$tmp/$k.cpp" | sort > "$tmp/$k.compact"
	if [ "$stride" = 1 ]; then cmp "$tmp/$k.compact" "$g/$k.txt"; else { grep -vxFf "$g/$k.txt" "$tmp/$k.compact" || true; } | head -3; fi
	echo "$k: $(wc -l < "$tmp/$k.cpp") files, cpp and rust match"
done
