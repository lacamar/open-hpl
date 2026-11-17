#!/usr/bin/env python3
"""Compare OHPL_S8D_DUMP draw lists of two runs.

Fades run on the frame clock, so runs drop or repeat frames and differ in length; the random
dust, flash and ghoul particles and the random frame art (p<N>_*.tga) are masked. Almost every frame (98%) of A must appear in
the union of the reference runs bit for bit, and A must cover part of the reference frames.
Usage: s8d-compare.py A B [B2...] [--min-cover 0.3]
"""
import re
import sys


def frames(path):
    out, cur = {}, None
    for line in open(path, errors="replace"):
        if line.startswith("im "):
            cur = [line]
            out[id(cur)] = cur
        elif cur is not None:
            if line.startswith(("g dust_", "g flashes-", "g menu_bg_uw_ghoul")) or re.match(r"g p\d_", line):
                continue
            cur.append(line)
    return {"".join(f) for f in out.values()}


def main():
    args = [a for a in sys.argv[1:]]
    cover = 0.3
    if "--min-cover" in args:
        i = args.index("--min-cover")
        cover = float(args[i + 1])
        del args[i : i + 2]
    a = frames(args[0])
    b = set()
    for p in args[1:]:
        b |= frames(p)
    extra = a - b
    covered = len(a & b) / len(b) if b else 1.0
    print(f"{len(a)} frames vs {len(b)}: {len(extra)} not in the reference, reference covered {covered:.3f}")
    for f in sorted(extra)[:2]:
        print(f[:600])
    # one stray frame is allowed in short dumps (menus: a handful of frames with random frame art)
    return 0 if len(extra) <= max(1, 0.02 * len(a)) and covered >= cover and a else 1


sys.exit(main())
