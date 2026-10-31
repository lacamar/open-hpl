#!/usr/bin/env python3
"""Render one map with the GL and the Vulkan backend and compare the frames.

  scripts/vk-compare.py <map> [--out DIR] [--frames N] [--env K=V ...]

Uses the scratch dir of $OHPL_GAME (run soma-init.sh first). Writes gl.png, vulkan.png,
diff.png (abs diff x4) and side_by_side.png; prints mean/max abs diff and luminance.
"""
import argparse
import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hpl_control import HplControl, HplControlError  # noqa: E402

import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def game_env():
    out = subprocess.run([os.path.join(HERE, "ohpl_game.py")], capture_output=True, text=True, check=True).stdout
    env = {}
    for line in out.splitlines():
        line = line.removeprefix("export ").strip()
        if "=" in line:
            k, v = line.split("=", 1)
            env[k] = v.strip("'\"")
    return env


def render(map_name, renderer, path, frames, extra, targets=(), dumps=()):
    sock = os.path.join(os.environ.get("XDG_RUNTIME_DIR", "/tmp"), f"ohpl-vkc-{renderer}.sock")
    env = dict(os.environ, **extra)
    env.pop("OPENHPL_RENDERER", None)
    if renderer == "vulkan":
        env["OPENHPL_RENDERER"] = "vulkan"
    pid, sock = subprocess.run([os.path.join(HERE, "soma-run.sh"), map_name, sock], env=env,
                               capture_output=True, text=True, check=True).stdout.split()
    pid = int(pid)
    try:
        deadline = time.time() + 180
        hpl = None
        while time.time() < deadline:
            try:
                hpl = HplControl(sock, timeout=120)
                hpl.connect()
                hpl.send({"cmd": "ping"})
                break
            except (OSError, HplControlError):
                hpl = None
                time.sleep(0.5)
        if hpl is None:
            raise RuntimeError(f"{renderer}: no control socket")
        if not game_env().get("HPL3"):
            hpl.send({"cmd": "start_map", "map": map_name})
        hpl.send({"cmd": "wait_frames", "n": frames, "max_ms": 60000})
        if os.path.exists(path):
            os.remove(path)
        hpl.send({"cmd": "screenshot", "path": path})
        for _ in range(50):
            if os.path.exists(path):
                break
            hpl.send({"cmd": "wait_frames", "n": 1, "max_ms": 1000})
        stats = hpl.send({"cmd": "frame_stats"}).get("frame", {})
        hpl.send({"cmd": "render_stats"})
        hpl.send({"cmd": "wait_frames", "n": 30, "max_ms": 5000})
        stats["render"] = {k: v for k, v in hpl.send({"cmd": "render_stats"}).items() if k != "ok"}
        for t in dumps:
            hpl.send({"cmd": "dump_target", "target": t, "path": f"{path[:-4]}_t{t}.pfm"})
        if targets:
            stats["targets"] = {}
            for t in targets:
                try:
                    stats["targets"][t] = {k: v for k, v in hpl.send({"cmd": "read_gbuffer_stats", "target": t}).items() if k != "ok"}
                except HplControlError as e:
                    stats["targets"][t] = str(e)
        try:
            hpl.send({"cmd": "quit"})
        except (OSError, HplControlError):
            pass
        return stats
    finally:
        for _ in range(100):
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                break
            time.sleep(0.1)
        else:
            os.kill(pid, 9)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("map")
    ap.add_argument("--out", default=os.path.join(os.environ.get("XDG_CACHE_HOME", os.path.expanduser("~/.cache")), "open-hpl", "vk-compare"))
    ap.add_argument("--frames", type=int, default=300)
    ap.add_argument("--env", action="append", default=[], help="extra K=V for both runs")
    ap.add_argument("--only", choices=["gl", "vulkan"])
    ap.add_argument("--dump", type=int, nargs="*", default=[], help="dump_target targets as <renderer>_t<N>.pfm (HPL3)")
    ap.add_argument("--stats", type=int, nargs="*", default=[], help="read_gbuffer_stats targets to compare (HPL3)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    extra = dict(kv.split("=", 1) for kv in a.env)

    stats = {}
    for r in ("gl", "vulkan"):
        if a.only and a.only != r:
            continue
        stats[r] = render(a.map, r, os.path.join(a.out, f"{r}.png"), a.frames, extra, a.stats, a.dump)
    if a.only:
        print(json.dumps(stats, indent=1))
        return

    gl = np.asarray(Image.open(os.path.join(a.out, "gl.png")).convert("RGB")).astype(np.int16)
    vk = np.asarray(Image.open(os.path.join(a.out, "vulkan.png")).convert("RGB")).astype(np.int16)
    h, w = min(gl.shape[0], vk.shape[0]), min(gl.shape[1], vk.shape[1])
    d = np.abs(gl[:h, :w] - vk[:h, :w])
    Image.fromarray(np.clip(d * 4, 0, 255).astype(np.uint8)).save(os.path.join(a.out, "diff.png"))
    side = np.concatenate([gl[:h, :w], vk[:h, :w]], axis=1).astype(np.uint8)
    Image.fromarray(side).save(os.path.join(a.out, "side_by_side.png"))
    print(json.dumps({
        "mean_abs_diff": round(float(d.mean()), 3),
        "max_abs_diff": int(d.max()),
        "pixels_over_16": round(float((d.max(axis=2) > 16).mean()), 4),
        "lum": {r: s.get("lum_mean") for r, s in stats.items()},
        "targets": {r: s.get("targets") for r, s in stats.items() if s.get("targets")},
        "out": a.out,
    }, indent=1))


if __name__ == "__main__":
    main()
