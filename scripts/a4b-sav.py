#!/usr/bin/env python3
"""Print the enemy entity blocks of a saved game (class types containing "Enemy" at the entity level), one block after another.
Fields the constructors never set are masked. Other entities carry uninitialised matrices that differ between runs, so saves are compared through these blocks.
Usage: a4b-sav.py <save>   (diff the output of two saves)"""
import re
import sys


# never initialised by the constructors: the save carries whatever the heap held
MASK = re.compile(r'(name="(?:mlStuckDoorID|mlTempVal|mfTempVal|mvTempPos)" )val="[^"]*"')


def main():
    lines = open(sys.argv[1], errors="replace").read().splitlines()
    i, out = 0, []
    while i < len(lines):
        m = re.match(r"(\s*)<(class_ptr|class) type=\"(cLux\w*Enemy\w*_SaveData)\"", lines[i])
        if not m:
            i += 1
            continue
        ind = m.group(1)
        j = i
        while j < len(lines) and lines[j] != f"{ind}</{m.group(2)}>":
            j += 1
        out.extend(MASK.sub(r'\1val=""', l) for l in lines[i:j + 1])
        i = j + 1
    print("\n".join(out))
    return 0 if out else 1


sys.exit(main())
