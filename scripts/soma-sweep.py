#!/usr/bin/env python3
"""Boot every SOMA map headless, collect text diagnostics, judge them
against soma/conformance/expected.json. One process per map.

  eval "$(scripts/soma-init.sh)"
  scripts/soma-sweep.py                       # all maps
  scripts/soma-sweep.py --map apartment       # substring match, repeatable
  scripts/soma-sweep.py --only-failed
  scripts/soma-sweep.py --compare old.json
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hpl_control import HplControl, HplControlError  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONF = os.path.join(REPO, "soma/conformance")

LUM_MIN, LUM_MAX = 3.0, 245.0
MAGENTA_MAX = 0.001


def cpu_ticks(pid):
    try:
        with open(f"/proc/{pid}/stat") as f:
            parts = f.read().rsplit(")", 1)[1].split()
        return int(parts[11]) + int(parts[12])
    except (OSError, IndexError, ValueError):
        return None


def backtrace_live(pid):
    # All threads: a flat-CPU hang is usually a lock/wait, not the main thread spinning.
    try:
        proc = subprocess.run(["gdb", "-p", str(pid), "-batch", "-ex", "thread apply all bt 25", "-ex", "detach"],
                              capture_output=True, text=True, timeout=600)
        lines = [l for l in proc.stdout.splitlines() if l.startswith("#") or l.startswith("Thread ")][:80]
        return lines or ["gdb produced no frames: " + proc.stderr.strip()[-300:]]
    except (OSError, subprocess.TimeoutExpired) as e:
        return [f"gdb failed: {e}"]


def backtrace_core(pid):
    time.sleep(3)
    try:
        out = subprocess.run(["coredumpctl", "debug", str(pid), "--debugger-arguments=-batch -ex 'bt 25'"],
                             capture_output=True, text=True, timeout=180).stdout
        return [l for l in out.splitlines() if l.startswith("#")][:25]
    except (OSError, subprocess.TimeoutExpired):
        return []


def log_errors(scratch, pid):
    state = os.path.join(scratch, ".xdg/state/open-hpl/soma")
    path = os.path.join(state, f"hpl-{pid}.log")
    if not os.path.exists(path):
        path = os.path.join(state, "hpl.log")
    patterns, warnings = {}, 0
    try:
        with open(path, errors="replace") as f:
            for line in f:
                if line.startswith("ERROR"):
                    key = re.sub(r"\d+", "N", re.sub(r"'[^']*'", "'*'", line.strip()))[:120]
                    entry = patterns.setdefault(key, {"count": 0, "example": line.strip()[:240]})
                    entry["count"] += 1
                elif line.startswith("WARNING"):
                    warnings += 1
    except OSError:
        pass
    errors = dict(sorted(patterns.items(), key=lambda kv: -kv[1]["count"])[:25])
    return path, errors, warnings


def run_map(name, scratch, frames, boot_timeout, sock):
    result = {"status": "ok"}
    if os.path.exists(sock):
        os.unlink(sock)

    env = dict(os.environ)
    env.update({
        "OPENHPL_HEADLESS_SOCKET": sock,
        "OPENHPL_SOMA_MAP": name,
        "OPENHPL_SOMA_SKIP_BOOT": "1",
        "OPENHPL_SOMA_FREECAM": "1",
        "XDG_CONFIG_HOME": os.path.join(scratch, ".xdg/config"),
        "XDG_CACHE_HOME": os.path.join(scratch, ".xdg/cache"),
        "XDG_DATA_HOME": os.path.join(scratch, ".xdg/data"),
        "XDG_STATE_HOME": os.path.join(scratch, ".xdg/state"),
    })
    start = time.time()
    with open(os.path.join(scratch, "sweep-run.out"), "w") as out:
        proc = subprocess.Popen(["./Soma.bin.aarch64"], cwd=scratch, env=env, stdout=out, stderr=subprocess.STDOUT,
                                stdin=subprocess.DEVNULL, start_new_session=True)

    def finish(status, **extra):
        result["status"] = status
        result.update(extra)
        result["wall_s"] = round(time.time() - start, 1)
        result["log"], result["log_errors"], result["log_warnings"] = log_errors(scratch, proc.pid)
        if proc.poll() is None:
            proc.kill()
        proc.wait()
        return result

    # The socket answers only once the main loop runs, i.e. after the map load.
    hpl = None
    last_ticks, flat_since = cpu_ticks(proc.pid), time.time()
    while True:
        if proc.poll() is not None:
            return finish("crash", exit_code=proc.returncode, backtrace=backtrace_core(proc.pid))
        if os.path.exists(sock):
            try:
                hpl = HplControl(sock, timeout=boot_timeout).connect()
                hpl.send({"cmd": "ping"})
                break
            except (OSError, HplControlError):
                hpl = None
        ticks = cpu_ticks(proc.pid)
        if ticks != last_ticks:
            last_ticks, flat_since = ticks, time.time()
        if time.time() - flat_since > 30:
            return finish("hang", backtrace=backtrace_live(proc.pid))
        if time.time() - start > boot_timeout:
            return finish("timeout", backtrace=backtrace_live(proc.pid))
        time.sleep(0.5)

    result["boot_s"] = round(time.time() - start, 1)
    try:
        hpl.send({"cmd": "wait_frames", "n": frames})
        result["load_report"] = hpl.send({"cmd": "load_report"})["load_report"]
        result["world"] = hpl.send({"cmd": "world_stats"})["world"]
        render = hpl.send({"cmd": "render_stats"})
        result["render"] = render["render"]
        result["fps"] = render.get("fps")
        result["camera"] = {k: v for k, v in hpl.send({"cmd": "camera_state"}).items() if k != "ok"}
        result["frame"] = hpl.send({"cmd": "frame_stats"})["frame"]
        shaders = hpl.send({"cmd": "shader_report"})
        result["shader_failures"] = shaders["shaders"]
        gbuffer = {}
        for target in (0, 1, 2):
            try:
                stats = hpl.send({"cmd": "read_gbuffer_stats", "target": target})
                gbuffer[str(target)] = {k: v for k, v in stats.items() if k != "ok"}
            except HplControlError as e:
                gbuffer[str(target)] = {"error": str(e)}
        result["gbuffer"] = gbuffer
        try:
            hpl.send({"cmd": "quit"})
        except (OSError, HplControlError):
            pass
    except (OSError, HplControlError) as e:
        if proc.poll() is not None:
            return finish("crash", exit_code=proc.returncode, error=str(e), backtrace=backtrace_core(proc.pid))
        return finish("hang", error=str(e), backtrace=backtrace_live(proc.pid))
    finally:
        hpl.close()

    try:
        proc.wait(timeout=60)
    except subprocess.TimeoutExpired:
        return finish("hang_on_exit", backtrace=backtrace_live(proc.pid))
    if proc.returncode not in (0, None):
        return finish("crash_on_exit", exit_code=proc.returncode, backtrace=backtrace_core(proc.pid))
    return finish("ok")


def judge(name, result, expected, allow):
    """Returns (failures, allowlisted) as lists of short strings."""
    fails, allowed = [], []
    if result["status"] != "ok":
        fails.append("status:" + result["status"])
    if "load_report" not in result:
        return fails, allowed

    exp = expected["maps"].get(name)
    tracks = result["load_report"]["tracks"]
    if exp:
        for track, e in sorted(exp["tracks"].items()):
            got = tracks.get(track, {"created": 0, "skipped": {}})
            want = e["xml"]
            if track == "ExposureArea":
                want = min(want, 1)
                if e["xml"] > 1:
                    allowed.append(f"ExposureArea:{e['xml'] - 1} unused")
            if got["created"] == want:
                continue
            msg = f"{track}:{got['created']}/{want}"
            if track in allow["tracks"]:
                allowed.append(msg)
            else:
                reasons = sorted(got["skipped"].items(), key=lambda kv: -kv[1])[:3]
                fails.append(msg + (" " + ",".join(f"{k}x{v}" for k, v in reasons) if reasons else ""))
        if exp["terrain_active"]:
            allowed.append("terrain")

    if result.get("shader_failures"):
        fails.append(f"shaders:{len(result['shader_failures'])}")
    if result.get("render", {}).get("gl_errors"):
        fails.append("gl_errors:" + ",".join(result["render"]["gl_errors"]))
    if result.get("world", {}).get("submeshes_without_material"):
        fails.append(f"no_material:{result['world']['submeshes_without_material']}")

    # Maps with no geometry never write the G-buffer; nothing to judge.
    renderables = sum(exp["tracks"][t]["xml"] for t in ("StaticObject", "Entity", "Primitive")) if exp else 1
    if renderables == 0:
        return fails, allowed

    if result.get("render", {}).get("draw_calls", 0) == 0:
        fails.append("draw_calls:0")

    for target, stats in result.get("gbuffer", {}).items():
        if "error" in stats:
            fails.append(f"gbuffer{target}:unreadable")
            continue
        nans = sum(int(v) for k, v in stats.items() if k.endswith("nan") or k.endswith("nan_count"))
        if nans:
            fails.append(f"gbuffer{target}:nan={nans}")

    frame = result.get("frame")
    if frame:
        if not LUM_MIN <= frame["lum_mean"] <= LUM_MAX:
            fails.append(f"lum_mean:{frame['lum_mean']:.1f}")
        if frame["magenta_frac"] > MAGENTA_MAX:
            fails.append(f"magenta:{frame['magenta_frac']:.3f}")
    return fails, allowed


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--map", action="append", default=[], help="substring filter, repeatable")
    ap.add_argument("--only-failed", action="store_true", help="re-run only maps failing in --out")
    ap.add_argument("--frames", type=int, default=120)
    ap.add_argument("--boot-timeout", type=float, default=900)
    ap.add_argument("--out", default=os.path.join(CONF, "results.json"))
    ap.add_argument("--compare", help="older results.json to diff verdicts against")
    ap.add_argument("--scratch", default=os.environ.get("OPENHPL_SOMA_SCRATCH"))
    args = ap.parse_args()

    if not args.scratch or not os.path.exists(os.path.join(args.scratch, "Soma.bin.aarch64")):
        sys.exit('no scratch dir - run: eval "$(scripts/soma-init.sh)"')
    if "steamapps/common" in os.path.realpath(args.scratch):
        sys.exit("refusing to run inside a Steam install")

    with open(os.path.join(CONF, "expected.json")) as f:
        expected = json.load(f)
    with open(os.path.join(CONF, "allowlist.json")) as f:
        allow = json.load(f)

    results = {"maps": {}}
    if os.path.exists(args.out):
        with open(args.out) as f:
            results = json.load(f)

    names = sorted(expected["maps"])
    if args.map:
        names = [n for n in names if any(m in n for m in args.map)]
    if args.only_failed:
        names = [n for n in names if results["maps"].get(n, {}).get("failures", ["never run"])]

    sock = os.path.join(os.environ.get("XDG_RUNTIME_DIR", "/tmp"), "ohpl-sweep.sock")
    for name in names:
        print(f"{name:34s} ", end="", flush=True)
        result = run_map(name, args.scratch, args.frames, args.boot_timeout, sock)
        errors = result["log_errors"]
        result["failures"], result["allowlisted"] = judge(name, result, expected, allow)
        results["maps"][name] = result

        verdict = "PASS" if not result["failures"] else "FAIL"
        frame = result.get("frame", {})
        print(f"{verdict} {result['status']:8s} {result.get('wall_s', 0):6.1f}s "
              f"lum={frame.get('lum_mean', -1):6.1f} draws={result.get('render', {}).get('draw_calls', -1):5d} "
              f"err={sum(e["count"] for e in errors.values()):5d}  {' '.join(result['failures'])}")
        for line in result.get("backtrace", [])[:8]:
            print("      " + line[:160])

        with open(args.out, "w") as f:
            json.dump(results, f, indent=1, sort_keys=True)
            f.write("\n")

    total = len(results["maps"])
    passing = sum(1 for r in results["maps"].values() if not r["failures"])
    print(f"\n{passing}/{total} maps passing -> {args.out}")

    if args.compare:
        with open(args.compare) as f:
            old = json.load(f)
        for name, r in sorted(results["maps"].items()):
            before = set(old["maps"].get(name, {}).get("failures", []))
            after = set(r["failures"])
            if before != after:
                print(f"  {name}: fixed={sorted(before - after)} new={sorted(after - before)}")
    sys.exit(0 if passing == total else 1)


if __name__ == "__main__":
    main()
