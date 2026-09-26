#!/usr/bin/env python3
"""Check that every native registered in soma/src/game (SOMA_FUNC/SOMA_METHOD) matches a declaration in
soma/data/script_api.txt by signature. A mismatch means scripts still call the stub."""
import glob, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
api_globals, api_methods = set(), {}


def sig(decl):
    d = re.sub(r'\s+', ' ', decl).strip()
    d = re.sub(r'\s*=\s*[^,)]+', '', d)                      # default args
    d = re.sub(r'(\w)\s+(&|@)', r'\1\2', d)                   # "T &in" -> "T&in"
    d = re.sub(r'(&|@)\s+(in|out|inout)\b', r'\1\2', d)
    head, _, params = d.partition('(')
    params, _, tail = params.rpartition(')')
    out = []
    for p in [p.strip() for p in params.split(',') if p.strip()]:
        toks = p.split(' ')
        if len(toks) > 1 and re.fullmatch(r'[a-z_]\w*', toks[-1]) and toks[-1] not in ('in', 'out', 'inout'):
            toks = toks[:-1]                                   # parameter name
        out.append(' '.join(toks).replace(' &', '&'))
    return re.sub(r'\s+', ' ', head).strip() + '(' + ','.join(out) + ')' + (' const' if 'const' in tail else '')


cur = None
for line in open(os.path.join(ROOT, 'soma/data/script_api.txt')):
    f = line.rstrip('\n').split('\t')
    if f[0] == 'O':
        cur = f[1]
    elif f[0] == 'M':
        api_methods.setdefault(cur, set()).add(sig(f[1]))
    elif f[0] == 'G':
        api_globals.add(sig(f[1]))

bad = 0
for path in glob.glob(os.path.join(ROOT, 'soma/src/game/*.cpp')):
    src = open(path).read()
    for m in re.finditer(r'SOMA_FUNC\(\s*\w+\s*,\s*"([^"]+)"', src):
        if sig(m.group(1)) not in api_globals:
            bad += 1
            print(f'{os.path.basename(path)}: global not in API: {m.group(1)}')
    for m in re.finditer(r'SOMA_METHOD\(\s*\w+\s*,\s*("?)(\w+)\1\s*,\s*"([^"]+)"', src):
        t = m.group(2) if m.group(1) else None
        decl = m.group(3)
        if t is None:  # type passed via variable: accept any type that has it
            ok = any(sig(decl) in v for v in api_methods.values())
        else:
            ok = sig(decl) in api_methods.get(t, set())
        if not ok and decl.count('(') == decl.count(')'):
            bad += 1
            print(f'{os.path.basename(path)}: {t or "?"} method not in API: {decl}')
print(f'{bad} mismatches')
sys.exit(1 if bad else 0)
