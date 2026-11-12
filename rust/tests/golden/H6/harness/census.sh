#!/usr/bin/env bash
# usage: census.sh <h6_gen binary> <kind msh|dae|fbx|anm|daeanim|fbxanim> <file list> <out dir>
# Runs the h6<kind> mode with OHPL_IMPL=cpp and =rust over the list (paths relative to steamapps/common,
# read-only) and writes <out>/<kind>.cpp / <out>/<kind>.rust.
set -u
bin=$1 kind=$2 list=$3 out=$4
mkdir -p "$out"
cd "${OHPL_STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}" || exit 1
for m in cpp rust; do
	OPENHPL_HEADLESS_SOCKET=x OHPL_IMPL=$m "$bin" "h6$kind" - < "$list" > "$out/$kind.$m" 2> "$out/$kind.$m.err"
	echo "$kind $m rc=$?"
done
cmp "$out/$kind.cpp" "$out/$kind.rust" && echo "$kind: $(wc -l < "$out/$kind.cpp") files, cpp and rust match"
