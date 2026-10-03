#!/bin/bash
# usage: ohpl-album.sh <image> <map>-<desc>
set -e
dir=${OHPL_ALBUM_DIR:-$HOME/Pictures/Screenshots/openhpl-screenshots}
mkdir -p "$dir"
out="$dir/$2-$(date +%Y%m%d).png"
magick "$1" -alpha off "$out"
echo "$out"
