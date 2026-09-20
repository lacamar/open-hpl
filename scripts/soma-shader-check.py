#!/usr/bin/env python3
"""Validate transpiled HPSL->GLSL dumps (OPENHPL_DUMP_HPSL_SHADERS_DIR) offline
with glslangValidator, and report which .hpsl files were never requested.

  scripts/soma-shader-check.py <dump-dir> [--soma-root DIR] [--json out.json]
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys


def stage_of(name):
    if re.search(r"_(vtx|vs)\b|_vtx", name):
        return "vert"
    return "frag"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dump_dir")
    ap.add_argument("--soma-root", default=os.environ.get("OPENHPL_SOMA_ROOT", os.path.expanduser("~/.local/share/Steam/steamapps/common/SOMA")))
    ap.add_argument("--json")
    args = ap.parse_args()

    seen, requested, failures = {}, set(), []
    for dirpath, _, files in os.walk(args.dump_dir):
        for f in sorted(files):
            if not f.endswith(".glsl"):
                continue
            base = re.sub(r"(\.glsl)+$", "", re.sub(r"^\d+_", "", f))
            requested.add(base)
            path = os.path.join(dirpath, f)
            with open(path, "rb") as fh:
                digest = hashlib.sha1(fh.read()).hexdigest()
            seen.setdefault((base, digest), path)

    for (base, _), path in sorted(seen.items()):
        proc = subprocess.run(["glslangValidator", "-S", stage_of(base), path], capture_output=True, text=True)
        if proc.returncode != 0:
            lines = [l for l in proc.stdout.splitlines() if l.startswith("ERROR")][:6]
            failures.append({"shader": base, "file": path, "errors": lines})

    corpus = set()
    hpsl_dir = os.path.join(args.soma_root, "core/shaders/hpsl")
    if os.path.isdir(hpsl_dir):
        corpus = {f[:-5] for f in os.listdir(hpsl_dir) if f.endswith(".hpsl") and not f.startswith("helper_")}
    never = sorted(corpus - requested)

    print(f"{len(seen)} unique variants of {len(requested)} shaders; {len(failures)} fail glslang; "
          f"{len(never)}/{len(corpus)} corpus shaders never requested")
    for fail in failures:
        print(f"  FAIL {fail['shader']}  ({fail['file']})")
        for line in fail["errors"]:
            print("       " + line[:200])
    if never:
        print("  never requested: " + " ".join(never))

    if args.json:
        with open(args.json, "w") as f:
            json.dump({"variants": len(seen), "failures": failures, "never_requested": never}, f, indent=1)
            f.write("\n")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
