#!/usr/bin/env bash
# Copy game files out of steamapps/common before modifying them; prints the copy's path.
# Usage: scripts/re-copy.sh <file-or-dir under steamapps/common>
set -euo pipefail
SRC=$(realpath "${1:?usage: re-copy.sh <path under steamapps/common>}")
case "$SRC" in */steamapps/common/*) ;; *) echo "'$SRC' is not under steamapps/common" >&2; exit 1 ;; esac
DST="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/re/${SRC#*/steamapps/common/}"
if [ ! -e "$DST" ]; then
	mkdir -p "$(dirname "$DST")"
	cp -a --reflink=auto "$SRC" "$DST"
	chmod -R u+w "$DST"
fi
echo "$DST"
