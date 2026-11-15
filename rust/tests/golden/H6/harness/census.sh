#!/usr/bin/env bash
# usage: census.sh <h6_gen binary> <kind msh|dae|fbx|anm|daeanim|fbxanim> <file list> <out dir>
# Runs the h6<kind> mode over the list (paths relative to steamapps/common,
# read-only) and writes <out>/<kind>.rust; compare it with the golden census.
set -u
bin=$1 kind=$2 list=$3 out=$4
mkdir -p "$out"
cd "${OHPL_STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}" || exit 1
OPENHPL_HEADLESS_SOCKET=x "$bin" "h6$kind" - < "$list" > "$out/$kind.rust" 2> "$out/$kind.rust.err"
echo "$kind rc=$? $(wc -l < "$out/$kind.rust") files"
