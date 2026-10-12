#!/usr/bin/env python3
# Derive a script_api.txt for a game without a Linux binary (Bunker) from a sibling game's recovered API
# plus the difference between the hps_api.hps dumps both games ship.
# Usage: soma-hps-api.py <base script_api.txt> <base hps_api.hps> <target hps_api.hps> <out script_api.txt>
# The dumps drop const qualifiers and the parentheses of default arguments, so only lines that differ
# between the two dumps are converted; everything else keeps the recovered declaration.
import re, sys
from collections import OrderedDict

# new type -> base type whose T/B/C lines it copies
LIKE = (("cScript.*Set", "cScriptStringSet"), ("cScript.*Map", "cScriptStringIntMap"), ("cLux.*Area", "cLuxSoundscapeArea"),
        ("cLux.*Handler", "cLuxVoiceHandler"))


def parse_dump(path):
    scopes, kinds, cur, comment = OrderedDict({"": []}), {"": "global"}, "", False
    for l in open(path, encoding="latin-1").read().split("\n"):
        s = re.sub(r"\s+", " ", l.strip())
        comment = comment or s.startswith("/**")
        if comment:
            comment = not s.endswith("*/")
            continue
        m = re.match(r"(class|enum|interface) (\w+)( \{)?$", s)
        if m:
            cur = m.group(2)
            kinds[cur] = m.group(1)
            scopes.setdefault(cur, [])
        elif s in ("}", "};"):
            cur = ""
        elif s and s != "{" and not s.startswith("template"):
            scopes[cur].append(s)
    del scopes["array"], kinds["array"]
    return scopes, kinds


def key(decl):
    decl = re.sub(r",?\s*const\)$", ")", decl.rstrip(";").strip())
    decl = re.sub(r"\)\s*const$", ")", decl)
    decl = re.sub(r",\s*\)", ")", re.sub(r"=\s*(c\w+|uint)\s*,(?=\s*-?[\d.])", r"=\1", decl))
    return re.sub(r"[\s()]|\buint32\b", lambda m: "uint" if m.group(0) == "uint32" else "", decl)


def fname(decl):
    m = re.search(r"(\w+)\(", decl)
    return m.group(1) if m else decl


def decl(line):
    s = line.rstrip(";")
    const = re.search(r",?\s*const\)$", s)
    s = re.sub(r",?\s*const\)$", ")", s)
    # "= cColor,1,1" lost the parentheses of "cColor(1,1)"
    s = re.sub(r"= ?(c\w+|uint)((?:, ?-?[\d.]+f?)+)", lambda m: f"={m.group(1)}({m.group(2).lstrip(', ')})", s)
    s = re.sub(r"\buint32\b", "uint", s)
    return s + (" const" if const else "")


def enum_values(lines):
    out, n = [], 0
    for l in lines:
        m = re.match(r"(\w+)\s*(?:=\s*(-?\w+))?", l)
        n = int(m.group(2), 0) if m.group(2) else n
        out.append((m.group(1), n))
        n += 1
    return out


def main(base_api, base_dump, target_dump, out):
    (bd, _), (td, tk) = parse_dump(base_dump), parse_dump(target_dump)
    enums, types, objs, globs = OrderedDict(), OrderedDict(), OrderedDict(), []
    cur = None
    for l in open(base_api).read().splitlines():
        v = l.split("\t")
        if l.startswith("#"):
            continue
        if v[0] == "E":
            cur = enums.setdefault(v[1], [])
        elif v[0] == "V":
            cur.append((v[1], int(v[2])))
        elif v[0] == "T":
            types[v[1]] = v[2:]
        elif v[0] == "O":
            cur = objs.setdefault(v[1], [])
        elif v[0] in ("G", "GP"):
            globs.append(v)
        else:
            cur.append(v)

    def patch(rows, scope, tags):
        old, new = bd.get(scope, []), td[scope]
        have, keep = {key(x) for x in old}, {key(x) for x in new}
        gone, names = have - keep, {fname(x) for x in new}
        # a recovered overload the target dump replaced would make calls ambiguous
        rows[:] = [r for r in rows if r[0] not in tags.values() or key(r[1]) in keep or (key(r[1]) not in gone and fname(r[1]) not in names)]
        have |= {key(r[1]) for r in rows if r[0] in tags.values()}
        for x in new:
            if key(x) not in have:
                rows.append([tags["(" in x], decl(x)] + ([""] if "(" in x else ["0"] if scope else []))

    for name in [n for n in enums if n not in td]:
        del enums[name]
    for name in [n for n in objs if n not in td]:
        del objs[name], types[name]
    for name, kind in tk.items():
        if kind == "enum":
            if name not in bd or [key(x) for x in bd[name]] != [key(x) for x in td[name]]:
                enums[name] = enum_values(td[name])
        elif kind != "global":
            if name not in objs:
                like = next((b for pat, b in LIKE if re.fullmatch(pat, name)), None)
                types[name] = list(types[like]) if like else ["interface" if kind == "interface" else "ref", "0", "0"]
                objs[name] = [r for r in objs.get(like, []) if r[0] in "BC"]
            patch(objs[name], name, {True: "M", False: "P"})
    patch(globs, "", {True: "G", False: "GP"})

    with open(out, "w") as f:
        print(f"# derived by scripts/soma-hps-api.py from {base_api} and the hps_api.hps dumps", file=f)
        for name, vals in enums.items():
            print(f"E\t{name}", file=f)
            for v in vals:
                print(f"V\t{v[0]}\t{v[1]}", file=f)
        for name, t in types.items():
            print("\t".join(["T", name] + t), file=f)
        for name, rows in objs.items():
            print(f"O\t{name}", file=f)
            for r in rows:
                print("\t".join(r), file=f)
        for g in sorted(globs, key=lambda g: g[0] == "GP"):
            print("\t".join(g), file=f)


if __name__ == "__main__":
    main(*sys.argv[1:5])
