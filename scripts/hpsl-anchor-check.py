#!/usr/bin/env python3
# Occurrences of each GpuShaderManager HPSL patch anchor in every HPL3 game's shaders.
import codecs, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ohpl_game import GAMES, REPO

games = {g: os.path.expanduser(GAMES[g][1]) for g in ("soma", "rebirth", "bunker")}
src = open(os.path.join(REPO, "HPL2/core/sources/resources/GpuShaderManager.cpp"), encoding="latin-1").read()
for tab in ("gvHpslSourcePatches", "gvHpslSourceReplacements", "gvHpslGammaAlbedoReplacements"):
    a = src.index(tab + "[][3] = {")
    body = "\n".join(l for l in src[a:src.index("\n\t};", a)].split("= {", 1)[1].split("\n") if not l.strip().startswith("//"))
    depth, cur, i = 0, [[]], 0
    while i < len(body):
        c = body[i]
        if c == '"':
            j = i + 1
            while body[j] != '"':
                j += 2 if body[j] == "\\" else 1
            cur[-1].append(codecs.decode(body[i + 1:j], "unicode_escape"))
            i = j
        elif c == "{":
            depth, cur = depth + 1, [[]]
        elif c == "," and depth == 1:
            cur.append([])
        elif c == "}":
            depth -= 1
            f, old = "".join(cur[0]), "".join(cur[1])
            counts = []
            for g, root in games.items():
                p = os.path.join(root, "core/shaders/hpsl", f)
                n = open(p, encoding="latin-1", newline="").read().replace("\r", "").count(old) if os.path.exists(p) else "-"
                counts.append("%s:%s" % (g, n))
            print("%-12s %-40s %-28s %r" % (tab[10:], f, " ".join(counts), old[:60]))
        i += 1
