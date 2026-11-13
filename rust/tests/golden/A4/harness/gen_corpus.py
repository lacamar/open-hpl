#!/usr/bin/env python3
"""gen_corpus.py N out: enemy variable sets from the real .ent files, nodes from the real maps (read only)."""
import glob
import random
import re
import sys

S = '/home/lm/.local/share/Steam/steamapps/common/'
ents = []
for g in ['Amnesia The Dark Descent', 'Machine for Pigs']:
    for f in glob.glob(S + g + '/entities/**/*.ent', recursive=True):
        if not any(k in f for k in ('/enemy/manpig/', '/enemy/tesla/', '/enemy/waterlurker/')):
            continue
        t = open(f, encoding='utf-8', errors='replace').read()
        if 'SightRange' not in t:
            continue
        v = dict(re.findall(r'<Var Name="([A-Za-z_0-9]+)" Value="([^"]*)"', t))
        ents.append((f.split('/entities/')[1], v))
maps = []
for g in ['Amnesia The Dark Descent', 'Machine for Pigs']:
    for f in glob.glob(S + g + '/maps/**/*.map', recursive=True):
        t = open(f, encoding='utf-8', errors='replace').read()
        pts = re.findall(r'AreaType="PathNode"[^>]*WorldPos="([-0-9.e]+) ([-0-9.e]+) ([-0-9.e]+)"', t)
        if len(pts) >= 8:
            maps.append((f.split('/maps/')[1], [tuple(float(x) for x in p) for p in pts]))
if not ents or not maps:
    sys.exit('no data')
n = int(sys.argv[1])
r = random.Random(7)
out = []
for i in range(n):
    ename, vars_ = ents[i % len(ents)]
    mname, pts = maps[r.randrange(len(maps))]
    c = pts[r.randrange(len(pts))]
    pts2 = sorted(pts, key=lambda p: (p[0] - c[0]) ** 2 + (p[2] - c[2]) ** 2)[:r.choice([8, 16, 30, 60])]
    out.append('CASE %d %d %s %s' % (1000 + i, i, ename, mname))
    for k, v in vars_.items():
        if re.fullmatch(r'-?[0-9.]+( -?[0-9.]+)*', v.strip()):
            out.append('V %s %s' % (k, v.strip()))
    for p in pts2:
        out.append('N %r %r %r' % p)
    out.append('END')
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
print(len(ents), 'enemies', len(maps), 'maps', n, 'cases')
