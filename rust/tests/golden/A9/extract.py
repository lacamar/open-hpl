#!/usr/bin/env python3
# usage: extract.py <OHPL_A9_MENUDUMP file recorded from the C++ windows> > created.txt
# keeps the "created" stage subtrees of the windows ported in A9a (Options and Key Config are not).
import sys

KEEP = {"Profiles", "Create Profile", "Load Game", "Custom Story", "", "Start New Game"}
out, keep, stage = [], False, None
for line in open(sys.argv[1], errors="replace"):
    if line.startswith("== "):
        stage = line.split()[1]
        continue
    if stage != "created":
        continue
    depth = len(line) - len(line.lstrip(" "))
    if depth == 1:
        keep = " t=1 " in line and line.split("'")[1] in KEEP
    if keep and depth >= 1:
        out.append(line.rstrip("\n"))
print("\n".join(out))
