#!/usr/bin/env python3
"""Per-game paths for the test tools. OHPL_GAME picks the game (default soma).

Shell: eval "$(scripts/ohpl_game.py [game])" sets GAME BIN ROOT SCRATCH CONF HPL3.
"""
import os
import shlex
import sys
from types import SimpleNamespace

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Rebirth and Bunker run on the SOMA game module, Machine for Pigs on the Amnesia one
# binary, Steam root, dir under $XDG_*_HOME/open-hpl
GAMES = {
    "soma": ("Soma.bin.aarch64", "~/.local/share/Steam/steamapps/common/SOMA", "soma"),
    "rebirth": ("Soma.bin.aarch64", "~/.steam/steam/steamapps/common/Amnesia Rebirth", "rebirth"),
    "bunker": ("Soma.bin.aarch64", "~/.steam/steam/steamapps/common/Amnesia The Bunker", "bunker"),
    "amnesia": ("Amnesia.bin.aarch64", "~/.local/share/Steam/steamapps/common/Amnesia The Dark Descent", "Amnesia/Main"),
    "amfp": ("Amnesia.bin.aarch64", "~/.local/share/Steam/steamapps/common/Machine for Pigs", "Amnesia/Pig"),
}


def game(name=None):
    name = name or os.environ.get("OHPL_GAME") or "soma"
    if name not in GAMES:
        sys.exit(f"unknown game '{name}' (one of: {' '.join(GAMES)})")
    binary, root, user = GAMES[name]
    env = lambda key: os.environ.get(f"OPENHPL_{name.upper()}_{key}")
    # soma-init.sh points XDG_CACHE_HOME into a scratch dir, hence the separate root
    scratch_root = os.environ.get("OPENHPL_SCRATCH_ROOT") or os.path.join(
        os.environ.get("XDG_CACHE_HOME") or os.path.expanduser("~/.cache"), "open-hpl")
    return SimpleNamespace(
        name=name, bin=binary, hpl3=binary == "Soma.bin.aarch64", user="open-hpl/" + user,
        map_ext=".hpm" if binary == "Soma.bin.aarch64" else ".map",
        root=env("ROOT") or os.path.expanduser(root),
        scratch=env("SCRATCH") or os.path.join(scratch_root, name + "-scratch"),
        conf=os.path.join(REPO, name, "conformance"))


if __name__ == "__main__":
    g = game(sys.argv[1] if len(sys.argv) > 1 else None)
    for key, value in (("GAME", g.name), ("BIN", g.bin), ("ROOT", g.root), ("SCRATCH", g.scratch), ("CONF", g.conf),
                       ("HPL3", "1" if g.hpl3 else "")):
        print(f"{key}={shlex.quote(value)}")
