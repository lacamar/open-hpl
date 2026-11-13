#!/bin/bash
# usage: make-fpdir.sh <dir>   fixed file tree (fixed mtimes) the file picker harness browses
set -e
D=$1
[ -d "$D" ] && chmod -R u+w "$D"
rm -rf "$D"
mkdir -p "$D/sub/deeper" "$D/personal" "$D/other"
for f in a.txt b.txt c.lua notes.md saved.txt sub/x.txt sub/y.lua personal/p.txt .hidden.txt; do
  head -c $(( ${#f} * 137 )) /dev/zero | tr '\0' 'x' > "$D/$f"
  touch -d "2020-01-02 03:04:05" "$D/$f"
done
head -c 5000 /dev/zero > "$D/big.txt"
touch "$D/sub/hpl.log"  # the engine writes it in the working directory
touch -d "2021-05-06 07:08:09" "$D/big.txt"
touch -d "2020-03-04 05:06:07" "$D/sub/deeper" "$D/sub" "$D/personal" "$D/other" "$D"
# read-only so the engine cannot create hpl.log in the browsed working directory
chmod a-w "$D/sub" "$D"
