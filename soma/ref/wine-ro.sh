#!/bin/sh
S="$HOME/.steam/steam/steamapps/common/SOMA"
# 3DC normal maps; without LATC the game decodes them sideways
export MESA_EXTENSION_OVERRIDE="+GL_EXT_texture_compression_latc $MESA_EXTENSION_OVERRIDE"
exec bwrap --dev-bind / / --ro-bind "$S" "$S" wine "$@"
