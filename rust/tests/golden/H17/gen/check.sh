#!/usr/bin/env bash
# Usage: check.sh <libohpl.a>   Runs the C++ and Rust paths of the harness and compares both with trace.txt.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
g=$(dirname "$here")
tmp=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h17-tmp/check.XXXXXX")
"$here/gen.sh" "$1" "$tmp/gen"
for m in cpp rust; do
	OHPL_IMPL=$m "$tmp/gen" "$g/script.txt" >"$tmp/$m.txt"
	diff "$tmp/$m.txt" "$g/trace.txt"
done
rm -rf "$tmp"
echo "H17 harness: cpp and rust match the golden trace ($(grep -vc '~' "$g/trace.txt") lines)"
