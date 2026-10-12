#!/usr/bin/env python3
# Emit SOMA's property offsets for the script structs native code reads (SomaImGuiLayout.inc).
# Rebirth and Bunker lay these out differently; their properties are registered at SOMA's offsets.
import os, re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(REPO, "soma/src/game")
types = set(re.findall(r'^\t\{"(\w+)"', open(os.path.join(GAME, "SomaImGuiDefaults.cpp")).read(), re.M))
types.add("cLuxScreenTextFormatParameters")

out, cur = [], None
for l in open(os.path.join(REPO, "soma/data/script_api.txt")):
    v = l.rstrip("\n").split("\t")
    if v[0] == "O":
        cur = v[1]
    elif v[0] == "P" and cur in types:
        out.append('{"%s", "%s", %s},' % (cur, v[1].split()[-1], v[2]))
open(os.path.join(GAME, "SomaImGuiLayout.inc"), "w").write("\n".join(out) + "\n")
print(len(out), "properties")
