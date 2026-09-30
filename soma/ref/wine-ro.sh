#!/bin/sh
S="$HOME/.steam/steam/steamapps/common/SOMA"
exec bwrap --dev-bind / / --ro-bind "$S" "$S" wine "$@"
