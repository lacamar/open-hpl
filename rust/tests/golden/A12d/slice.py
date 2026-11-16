#!/usr/bin/env python3
"""Slice of an a12d dump recorded from the C++ debug handler: the widgets the handler creates
(type 'text' pos uv checked; size where the handler sets it).
usage: slice.py <a12d dump> > tree.txt"""
import re
import sys

SIZED = {"window", "group", "frame", "button"}
out, stack = [], []
for line in open(sys.argv[1]):
    if line.startswith("s init"):
        break
    m = re.match(r"w( *)(\w+) '(.*)' pos=(\S+) size=(\S+) font=\S+ en=\d vis=\d uv=(-?\d+)(?: checked=(\d))?", line)
    if not m:
        continue
    depth = len(m.group(1)) // 2
    typ, text, pos, size, uv, checked = m.groups()[1:]
    del stack[depth:]
    stack.append(typ)
    if typ == "label" or depth > 2 or (depth == 2 and stack[1] == "frame"):
        continue
    if typ in ("combobox", "slider"):
        text = "*"
    out.append(f"{typ} '{text}' pos={pos} uv={uv} checked={checked or 0}" + (f" size={size}" if typ in SIZED else ""))
sys.stdout.write("\n".join(out) + "\n")
