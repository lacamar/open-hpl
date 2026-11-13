#!/usr/bin/env python3
"""Extracts the original C++ cEngine loop (Run, focus/device checks, frame timer) from Engine.cpp,
minus the OHPL_IMPL=rust branches, with the module/platform names redirected to the harness mocks.
usage: extract_run.py <Engine.cpp> <out.inc>"""
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
    text = text.replace("cEngineDiagnostics::", "MockDiag::").replace("cPlatform::", "MockPlat::")
    text = re.sub(r"\bLog\(", "MockLog(", text)
    text = text.replace("typedef std::chrono::steady_clock tClock;", "typedef MockClock tClock;")
    text = text.replace("iLowLevelGraphics *pllGfx", "MockLow *pllGfx")
    return text


out = []
for name, sig in (
    ("Run", "void cEngine::Run()"),
    ("UpdateFrameTimer", "void cEngine::UpdateFrameTimer()"),
    ("CheckAndBroadcastFocusChange", "void cEngine::CheckAndBroadcastFocusChange()"),
    ("CheckIfAppInFocusElseWait", "void cEngine::CheckIfAppInFocusElseWait()"),
    ("CheckAndBroadcastDeviceChange", "void cEngine::CheckAndBroadcastDeviceChange()"),
):
    out.append("void MockEngine::%s()\n{%s}\n\n" % (name, fix(body(sig))))
open(sys.argv[2], "w").write("".join(out))
