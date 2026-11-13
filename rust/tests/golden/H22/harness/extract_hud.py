#!/usr/bin/env python3
"""Extracts the original C++ cDevHud::Render and RenderBugReport from DevHud.cpp (minus the
OHPL_IMPL=rust branch) with platform/font/low-level names redirected to the harness mocks.
usage: extract_hud.py <DevHud.cpp> <out.inc>"""
import re
import sys

src = open(sys.argv[1]).read()


def body(sig):
    i = src.index(sig)
    j = src.index("{", i)
    depth, k = 0, j
    while True:
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[j + 1 : k]
        k += 1


def fix(text):
    text = re.sub(r"\s*if\(OhplBackendRustRequested\(\)\)\s*\{[^}]*\}\n", "\n", text)
    text = text.replace("cPlatform::", "MockPlat::").replace("iFontData *", "MockFont *").replace("iLowLevelGraphics *", "MockLow *")
    return text


out = []
for name, sig, args in (
    ("Render", "void cDevHud::Render(bool abHud)", "bool abHud"),
    ("RenderBugReport", "void cDevHud::RenderBugReport(const cVector2l &avScreen)", "const cVector2l &avScreen"),
):
    out.append("void MockHud::%s(%s)\n{%s}\n\n" % (name, args, fix(body(sig))))
open(sys.argv[2], "w").write("".join(out))
