#!/usr/bin/env python3
"""Runs the array/string script suites of rust/tests/golden/E-as and prints their output.
  e-as-suite.py amnesia <Soma.bin.aarch64>   suite-amnesia.as in an Amnesia-style engine (string add-on + array)
  e-as-suite.py soma                         suite-soma.txt snippets through soma-play exec (needs OHPL_GAME=soma and soma-init)
Compare with suite-amnesia.out / suite-soma.out next to the suites."""
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
GOLD = HERE.parent / "rust/tests/golden/E-as"


def amnesia(binary):
    r = subprocess.run([binary], env=dict(os.environ, OPENHPL_AS_SUITE=str(GOLD / "suite-amnesia.as")), capture_output=True, text=True, timeout=120)
    # live counts follow when the GC destroys objects, which depends on heap addresses
    sys.stdout.write(re.sub(r"live (after )?-?\d+", r"live \1N", r.stdout))
    if r.returncode:
        sys.stdout.write(f"rc={r.returncode}\n{r.stderr[-400:]}")


def blocks():
    name, body = None, []
    for line in (GOLD / "suite-soma.txt").read_text().splitlines():
        if line.startswith("### "):
            if name:
                yield name, "\n".join(body)
            name, body = line[4:], []
        else:
            body.append(line)
    if name:
        yield name, "\n".join(body)


def soma():
    play = [sys.executable, str(HERE / "soma-play.py")]
    subprocess.run(play + ["start", "--map", "00_01_apartment"], check=True, capture_output=True)
    try:
        for name, code in blocks():
            r = subprocess.run(play + ["exec", code], capture_output=True, text=True, timeout=120)
            print(f"### {name}")
            print(r.stdout.rstrip())
            if r.returncode:
                print("rc=%d %s" % (r.returncode, (r.stderr.strip().splitlines() or [""])[-1][:200]))
    finally:
        subprocess.run(play + ["stop"], capture_output=True)


if __name__ == "__main__":
    if sys.argv[1:2] == ["amnesia"] and len(sys.argv) == 3:
        amnesia(sys.argv[2])
    elif sys.argv[1:2] == ["soma"]:
        soma()
    else:
        raise SystemExit(__doc__)
