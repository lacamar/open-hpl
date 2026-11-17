#!/usr/bin/env python3
"""Writes OHPL_S4B_SCRIPT files from a discovery dump (OHPL_S4B_DUMP with a bare "300 END" script):
one script per map, waking every agent, walking the player to each, driving states and sound messages.
Usage: s4b-scenario.py <discovery-dump> <outdir> <game>"""
import os
import re
import sys

dump, outdir, game = sys.argv[1:4]
os.makedirs(outdir, exist_ok=True)
maps, cur = {}, None
for line in open(dump):
    if line.startswith("M "):
        cur = line[2:].strip()
        maps[cur] = {}
    elif cur and not line.startswith(("T ", "END", "call ", "ev ", "msg ")):
        m = re.match(r"(\S+) a=\d+ pos=([^,]+),([^,]+),([^ ]+)", line)
        if m and m.group(1) not in maps[cur]:
            maps[cur][m.group(1)] = tuple(float(m.group(i)) for i in (2, 3, 4))
for name, agents in maps.items():
    names = sorted(agents)[:8]
    s = [f"3 wake {n}" for n in names]
    t = 20
    for n in names:
        x, y, z = agents[n]
        s += [f"{t} player {x + 2.5:.3f} {y - 0.9:.3f} {z:.3f}"]
        s += [f"{t + 10} msg {n} 4", f"{t + 20} state {n} 1", f"{t + 35} state {n} 2", f"{t + 50} state {n} 3"]
        s += [f"{t + 65} player {x - 6:.3f} {y - 0.9:.3f} {z + 6:.3f}", f"{t + 80} msg {n} 8", f"{t + 90} state {n} 0"]
        t += 100
    s += [f"{t + 5} dumpsave", f"{t + 6} save s4brt.sav", f"{t + 40} load s4brt.sav", f"{t + 120} dumpsave", f"{t + 160} END"]
    open(os.path.join(outdir, f"{game}-{name}.script"), "w").write("\n".join(s) + "\n")
print(len(maps), "scenarios")
