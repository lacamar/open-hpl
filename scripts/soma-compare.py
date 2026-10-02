#!/usr/bin/env python3
"""Run the same map, snippets, camera poses and measurements on open-hpl and the official game.

"ours" is a headless Soma.bin.aarch64 (scripts/soma-run.sh, script player), "ref" is the Windows
game under wine (scripts/soma-ref.py). Snippets are AngelScript function bodies printing
`key=value` lines with __print(); values are compared numerically where both parse as numbers.

  scripts/soma-compare.py start [--map 00_01_apartment] [--pos PlayerStartArea_1] [--only ours|ref]
  scripts/soma-compare.py exec 'code' | -f file.as        # side by side, mismatches marked
  scripts/soma-compare.py ping | player | perf | entities [--pattern '*'] | lights [--pattern '*']
  scripts/soma-compare.py view [--pose X Y Z YAW PITCH] [--settle 2] [--out DIR]
  scripts/soma-compare.py fps [--secs 10]                  # frame rate sampled on both
  scripts/soma-compare.py lightattr [--n 12] [--solo] [names...]    # per-light luminance contribution (hide one / solo, diff)
  scripts/soma-compare.py report --map M [--out DIR]       # start, then all of the above -> DIR/report.json
  scripts/soma-compare.py ui start [--wait 45]          # both booted to the main menu
  scripts/soma-compare.py ui click FX FY | key K... | shot [--out F]   # same input on both (FX/FY 0-1), side by side
  scripts/soma-compare.py stop
"""
import argparse, importlib.util, json, math, os, signal, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from hpl_control import HplControl, RUNTIME, XDG_CACHE, SCRATCH, muted_env, pidfile_pid  # noqa: E402

_spec = importlib.util.spec_from_file_location("soma_ref", HERE / "soma-ref.py")
ref_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ref_mod)

SOCK = RUNTIME / "ohpl-cmp.sock"
PIDFILE = RUNTIME / "ohpl-cmp.pid"
CACHE = XDG_CACHE / "open-hpl/soma-compare"

SNIPPETS = dict(ref_mod.SNIPPETS)
SNIPPETS["entities"] = (
    'array<iLuxEntity@> v; cLux_GetCurrentMap().GetEntityArray("%s", eLuxEntityType_LastEnum, "", v); '
    '__print("count=" + v.length()); '
    'for(uint i = 0; i < v.length(); ++i) { iLuxEntity@ e = v[i]; cVector3f p = e.GetPosition(); '
    '__print(e.GetName() + "=" + e.IsActive() + " " + e.GetClassName() + " " + p.x + " " + p.y + " " + p.z); }')
SNIPPETS["lights"] = (
    'array<iLight@> v; Map_GetLightArray("%s", v); __print("count=" + v.length()); '
    'for(uint i = 0; i < v.length(); ++i) { iLight@ l = v[i]; cVector3f p = l.GetWorldPosition(); '
    'cColor c = l.GetDiffuseColor(); '
    '__print(l.GetName() + "=" + l.IsVisible() + " " + p.x + " " + p.y + " " + p.z + " r=" + l.GetRadius() '
    '+ " c=" + c.r + " " + c.g + " " + c.b + " " + c.a); }')


class Ours:
    name = "ours"

    def pid(self):
        return pidfile_pid(PIDFILE)

    def start(self, map_file, pos, size):
        self.stop()
        if not map_file.endswith(".hpm"):
            map_file += ".hpm"
        env = dict(os.environ, OPENHPL_SOMA_FREECAM="0")
        if pos:
            env["OPENHPL_SOMA_MAP_STARTPOS"] = pos
        out = subprocess.run([str(HERE / "soma-run.sh"), map_file, str(SOCK)], env=env, capture_output=True,
                             text=True, check=True).stdout.split()
        PIDFILE.write_text(out[0])
        t0 = time.time()
        while not SOCK.exists():
            if not self.pid() or time.time() - t0 > 120:
                raise SystemExit("ours did not come up")
            time.sleep(0.5)
        while True:
            try:
                if self.exec('__print(cLux_GetCurrentMap().GetName() + " " + cLux_GetPlayer().GetCurrentStateName());', 30)[0].split()[0]:
                    break
            except (RuntimeError, IndexError, OSError):
                pass
            if not self.pid():
                raise SystemExit("ours exited")
            if time.time() - t0 > 180:
                print("ours: map/player still not scriptable after 180 s, continuing")
                break
            time.sleep(1)
        self.send({"cmd": "wait_frames", "n": 30, "max_ms": 60000}, timeout=90)
        w, h = map(int, size.split("x"))
        if (w, h) != (1280, 720):
            self.send({"cmd": "resize", "width": w, "height": h})
        print(f"ours: pid {self.pid()} up in {time.time() - t0:.0f}s")

    def record_boot(self, out, secs, fps, size="1280x720", first_launch=False):
        """Fresh launch like the ref prefix: no saves, gamma already calibrated, `size` window.
        first_launch: no user_settings.cfg or gamma marker, fullscreen."""
        self.stop()
        scratch = SCRATCH
        xdg = CACHE / "boot-xdg"
        for k in ("config", "data", "state"):
            subprocess.run(["rm", "-rf", str(xdg / k)])
        (xdg / "config/open-hpl/soma").mkdir(parents=True)
        (xdg / "state/open-hpl/soma").mkdir(parents=True)
        w, h = size.split("x")
        (xdg / "config/open-hpl/soma/main_settings.cfg").write_text(
            f'<Screen Vsync="false" FullScreen="{str(first_launch).lower()}" Width="{w}" Height="{h}" />\n')
        if not first_launch:
            (xdg / "state/open-hpl/soma/gamma_screen_seen").write_text("1\n")
            (xdg / "config/open-hpl/soma/user_settings.cfg").write_text(
                (ref_mod.SOMA / "config/default_user_settings.cfg").read_text().replace("<Game />", '<Game MenuPhase="1" />')
                + f'\n<Main FirstGameStart="false" />\n<Screen Vsync="false" FullScreen="false" Width="{w}" Height="{h}" />\n')
        env = dict(muted_env(), OPENHPL_HEADLESS_SOCKET=str(SOCK), XDG_CACHE_HOME=str(scratch / ".xdg/cache"),
                   **{f"XDG_{k}_HOME": str(xdg / k.lower()) for k in ("CONFIG", "DATA", "STATE")})
        t0 = time.time()
        p = subprocess.Popen(["./Soma.bin.aarch64"], cwd=scratch, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             stdin=subprocess.DEVNULL, start_new_session=True)
        PIDFILE.write_text(str(p.pid))
        out = Path(out)
        out.mkdir(parents=True, exist_ok=True)
        frames = []
        while time.time() - t0 < secs and self.pid():
            tick = time.time()
            if SOCK.exists():
                f = out / f"{int((tick - t0) * 1000):06d}.png"
                try:
                    self.shot(f, timeout=5)
                    frames.append(f)
                except Exception:
                    pass
            time.sleep(max(0, 1 / fps - (time.time() - tick)))
        return frames

    def send(self, req, timeout=60):
        with HplControl(str(SOCK), timeout=timeout) as h:
            r = h.send(req)
        if not r.get("ok", True):
            raise RuntimeError(r.get("error", "failed"))
        return r

    def exec(self, code, timeout=60):
        with HplControl(str(SOCK), timeout=timeout) as h:
            r = h.send({"cmd": "script_exec", "code": code})
        if not r.get("ok", True):
            raise RuntimeError(r.get("error", "failed") + r.get("output", ""))
        return r.get("output", "").splitlines()

    def shot(self, path, timeout=60):
        bmp = Path(path).with_suffix(".bmp")
        self.send({"cmd": "screenshot", "path": str(bmp)}, timeout=timeout)
        for _ in range(50):
            if bmp.exists() and bmp.stat().st_size:
                break
            time.sleep(0.1)
        subprocess.run(["magick", str(bmp), "-alpha", "off", str(path)], check=True)
        bmp.unlink()
        return path

    def fps(self):
        return float(self.send({"cmd": "render_stats"}).get("fps", 0))

    def wait(self, secs):
        self.send({"cmd": "wait_frames", "n": 100000, "max_ms": int(secs * 1000)}, timeout=secs + 30)

    def stop(self):
        pid = self.pid()
        if pid:
            os.kill(pid, signal.SIGKILL)
        PIDFILE.unlink(missing_ok=True)
        SOCK.unlink(missing_ok=True)


class Ref:
    name = "ref"

    def pid(self):
        return ref_mod.game_pid()

    def start(self, map_file, pos, size):
        r = ref_mod.start(map_file, pos or "PlayerStartArea_1", size)
        t0 = time.time()
        while time.time() - t0 < 120:
            try:
                if float(r.query('__print("t=" + cLux_GetGameTime());').get("t", 0)) > 1:
                    break
            except ref_mod.ExecError:
                pass
            time.sleep(1)

    def exec(self, code, timeout=60):
        try:
            return ref_mod.Ref().exec(code, timeout)
        except ref_mod.ExecError as e:
            raise RuntimeError(str(e))

    def shot(self, path):
        return ref_mod.screenshot(path)

    def fps(self):
        return float(ref_mod.Ref().query('__print("fps=" + cEngine_GetFPS());').get("fps", 0))

    def wait(self, secs):
        time.sleep(secs)

    def stop(self):
        ref_mod.stop()


def targets(only=None):
    return [t for t in (Ours(), Ref()) if not only or t.name == only]


def running(only=None):
    ts = [t for t in targets(only) if t.pid()]
    if not ts:
        sys.exit("nothing running: soma-compare.py start")
    return ts


def parse(lines):
    kv = {}
    for l in lines:
        if "=" in l:
            k, v = l.split("=", 1)
            kv[k] = v
    return kv


def num_list(v):
    try:
        return [float(x) for x in v.split()]
    except ValueError:
        return None


def same(a, b, tol=0.02):
    if a == b:
        return True
    if a is None or b is None:
        return False
    for x, y in ((a, b), (b, a)):
        if x.lower() in ("true", "false") or y.lower() in ("true", "false"):
            return x.lower() == y.lower()
    if "=" in a and "=" in b and " " not in a + b:
        (ka, a), (kb, b) = a.split("=", 1), b.split("=", 1)
        if ka != kb:
            return False
    na, nb = num_list(a), num_list(b)
    if na is None or nb is None:
        wa, wb = a.split(), b.split()
        if len(wa) != len(wb) or len(wa) < 2:
            return False
        return all(same(x, y, tol) for x, y in zip(wa, wb))
    return len(na) == len(nb) and all(abs(x - y) <= max(tol, 0.01 * max(abs(x), abs(y))) for x, y in zip(na, nb))


def run_both(code, only=None, timeout=60):
    res = {}
    for t in running(only):
        try:
            res[t.name] = t.exec(code, timeout)
        except RuntimeError as e:
            res[t.name] = [f"error={e}"]
    return res


def diff_table(res, show_all=True):
    ours, ref = parse(res.get("ours", [])), parse(res.get("ref", []))
    rows, bad = [], 0
    for k in list(dict.fromkeys(list(ours) + list(ref))):
        a, b = ours.get(k), ref.get(k)
        ok = same(a, b)
        if not ok and k == "yaw" and num_list(a or "x") and num_list(b or "x"):
            d = (float(a) - float(b)) % 360
            ok = min(d, 360 - d) <= 0.02
        bad += not ok
        if show_all or not ok:
            rows.append((k, a, b, ok))
    return rows, bad


def print_table(rows):
    kw = min(40, max((len(r[0]) for r in rows), default=4))
    for k, a, b, ok in rows:
        print(f"{'  ' if ok else '✗ '}{k[:kw]:{kw}}  ours: {a if a is not None else '-':38}  ref: {b if b is not None else '-'}")


def image_metrics(a, b):
    import numpy as np
    from PIL import Image
    ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if ia.size != ib.size:
        ib = ib.resize(ia.size)
    x, y = np.asarray(ia, dtype=np.float64), np.asarray(ib, dtype=np.float64)
    lum = lambda im: 0.2126 * im[..., 0] + 0.7152 * im[..., 1] + 0.0722 * im[..., 2]
    la, lb = lum(x), lum(y)
    mse = ((x - y) ** 2).mean()
    c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    ma, mb = la.mean(), lb.mean()
    ssim = ((2 * ma * mb + c1) * (2 * ((la - ma) * (lb - mb)).mean() + c2)) / ((ma ** 2 + mb ** 2 + c1) * (la.var() + lb.var() + c2))
    grid = []
    h, w = la.shape
    for gy in range(3):
        grid.append([round(float(la[gy * h // 3:(gy + 1) * h // 3, gx * w // 3:(gx + 1) * w // 3].mean()
                                 - lb[gy * h // 3:(gy + 1) * h // 3, gx * w // 3:(gx + 1) * w // 3].mean()), 1) for gx in range(3)])
    return {
        "lum_ours": round(float(ma), 2), "lum_ref": round(float(mb), 2),
        "rgb_ours": [round(float(v), 1) for v in x.reshape(-1, 3).mean(0)],
        "rgb_ref": [round(float(v), 1) for v in y.reshape(-1, 3).mean(0)],
        "contrast_ours": round(float(la.std()), 2), "contrast_ref": round(float(lb.std()), 2),
        "psnr": round(10 * math.log10(255 ** 2 / mse), 2) if mse else 99.0,
        "global_ssim": round(float(ssim), 3),
        "lum_diff_3x3": grid,
    }


def side_by_side(a, b, out):
    label = lambda f, t: ["(", str(f), "-resize", "x720", "-gravity", "northwest", "-pointsize", "22", "-fill", "yellow",
                          "-annotate", "+10+8", t, ")"]
    subprocess.run(["magick", *label(a, "ours"), *label(b, "ref"), "+append", str(out)], check=True)


def pose_code(x, y, z, yaw, pitch):
    return ref_mod.teleport_code(x, y, z, yaw, pitch)


def cmd_view(a, only=None):
    out = Path(a.out or CACHE / time.strftime("view-%Y%m%d-%H%M%S"))
    out.mkdir(parents=True, exist_ok=True)
    ts = running(only)
    if a.pose:
        for t in ts:
            t.exec(pose_code(*a.pose))
    for t in ts:
        t.wait(a.settle) if t.name == "ours" else None
    time.sleep(a.settle)
    shots = {t.name: t.shot(out / f"{t.name}.png") for t in ts}
    res = {"shots": {k: str(v) for k, v in shots.items()}}
    if len(shots) == 2:
        res["metrics"] = image_metrics(shots["ours"], shots["ref"])
        side_by_side(shots["ours"], shots["ref"], out / "side_by_side.png")
        res["side_by_side"] = str(out / "side_by_side.png")
    (out / "view.json").write_text(json.dumps(res, indent=1))
    print(json.dumps(res, indent=1))
    return res


def cmd_lightattr(a, only=None):
    import numpy as np
    from PIL import Image
    out = Path(a.out or CACHE / time.strftime("lightattr-%Y%m%d-%H%M%S"))
    out.mkdir(parents=True, exist_ok=True)
    ts = running(only)
    world = "cLux_GetCurrentMap().GetWorld()"
    if a.solo:
        visible = [n for n in Ours().exec(f'cLightListIterator@ it={world}.GetLightIterator(); '
                                          'while(it.HasNext()){ iLight@ l=it.Next(); if(l.IsVisible()) __print(l.GetName()+"\\n"); }') if n]
        names = a.names or visible
    else:
        names = a.names or [l["name"] for l in Ours().send({"cmd": "lights", "n": a.n})["lights"] if l["visible"]]
    lum = lambda f: (lambda x: 0.2126 * x[..., 0] + 0.7152 * x[..., 1] + 0.0722 * x[..., 2])(
        np.asarray(Image.open(f).convert("RGB"), dtype=np.float64))

    def capture(t, f):
        t.wait(a.settle) if t.name == "ours" else time.sleep(a.settle)
        return lum(t.shot(out / f))

    def set_vis(t, ns, v):
        t.exec("".join(f'{{ iLight@ l = {world}.GetLight("{n}"); if(l !is null) l.SetVisible({v}); }}' for n in ns))

    if a.solo:
        for t in ts:
            set_vis(t, visible, "false")
    base = {t.name: capture(t, f"{t.name}_base.png") for t in ts}
    rows = []
    for n in names:
        row = {"light": n}
        for t in ts:
            set_vis(t, [n], "true" if a.solo else "false")
            d = capture(t, f"{t.name}_{n}.png") - base[t.name]
            d = d if a.solo else -d
            set_vis(t, [n], "false" if a.solo else "true")
            h, w = d.shape
            row[t.name] = round(float(d.mean()), 2)
            row[t.name + "_3x3"] = [[round(float(d[y * h // 3:(y + 1) * h // 3, x * w // 3:(x + 1) * w // 3].mean()), 1)
                                     for x in range(3)] for y in range(3)]
            Image.fromarray(np.clip(d * 4 + 128, 0, 255).astype("uint8")).save(out / f"{t.name}_{n}_diff.png")
        rows.append(row)
        print(f"{n:32s} " + "  ".join(f"{t.name} {row[t.name]:6.2f}" for t in ts), flush=True)
    if a.solo:
        for t in ts:
            set_vis(t, visible, "true")
    (out / "lightattr.json").write_text(json.dumps(rows, indent=1))
    print(out / "lightattr.json")


def cmd_fps(a, only=None):
    ts = running(only)
    samples = {t.name: [] for t in ts}
    t0 = time.time()
    while time.time() - t0 < a.secs:
        for t in ts:
            try:
                samples[t.name].append(t.fps())
            except Exception:
                pass
        time.sleep(1)
    res = {k: {"mean": round(sum(v) / len(v), 1), "min": round(min(v), 1), "max": round(max(v), 1), "n": len(v)}
           for k, v in samples.items() if v}
    print(json.dumps(res))
    return res


def small(path, size=(96, 54)):
    import numpy as np
    from PIL import Image
    return np.asarray(Image.open(path).convert("L").resize(size, Image.BILINEAR), dtype=np.float64)


def lum(path):
    import numpy as np
    from PIL import Image
    return float(np.asarray(Image.open(path).convert("L"), dtype=np.float64).mean())


def cmd_boot(a):
    """Record both boots, align them on content, score every reference frame against ours."""
    out = Path(a.out or CACHE / "boot")
    ref_dir = Path(a.ref_dir or CACHE / "boot-ref")
    if a.record_ref or not any(ref_dir.glob("*.png")):
        subprocess.run(["rm", "-rf", str(ref_dir)])
        ref_mod.start(boot=True, size=a.size, record=(ref_dir, a.secs + 15, 4))
        ref_mod.stop()
    subprocess.run(["rm", "-rf", str(out)])
    o = Ours()
    o.record_boot(out / "ours", a.secs, a.fps, a.size, a.first_launch)
    o.stop()
    t = lambda f: int(f.stem) / 1000
    ref = sorted(ref_dir.glob("*.png"))
    ours = sorted((out / "ours").glob("*.png"))
    rl = {f: lum(f) for f in ref}
    ol = {f: lum(f) for f in ours}
    first = lambda fs, l: next((t(f) for f in fs if l[f] > 0.3), None)
    # ref starts later (wine); compare on time since first visible frame
    r0, o0 = first(ref, rl), first(ours, ol)
    osm = {f: small(f) for f in ours}
    rows = []
    for f in ref:
        if t(f) < r0 or t(f) - r0 > a.secs:
            continue
        rs = small(f)
        best = min(ours, key=lambda g: ((osm[g] - rs) ** 2).mean())
        mse = ((osm[best] - rs) ** 2).mean()
        at = min(ours, key=lambda g: abs((t(g) - o0) - (t(f) - r0)))
        mse_at = ((osm[at] - rs) ** 2).mean()
        psnr = lambda m: round(10 * math.log10(255 ** 2 / m), 1) if m else 99.0
        rows.append({"t": round(t(f) - r0, 2), "ref": f.name, "lum_ref": round(rl[f], 2),
                     "best": best.name, "best_t": round(t(best) - o0, 2), "best_psnr": psnr(mse),
                     "same_time": at.name, "same_time_psnr": psnr(mse_at), "lum_ours": round(ol[at], 2)})
    for r in rows:
        print(f"{r['t']:6.2f}s  ref lum {r['lum_ref']:6.2f}  ours lum {r['lum_ours']:6.2f}  same-time psnr {r['same_time_psnr']:5.1f}"
              f"   best {r['best_t']:6.2f}s psnr {r['best_psnr']:5.1f}")
    keys = rows[::max(1, len(rows) // 16)]
    args = []
    for r in keys:
        args += ["(", str(ref_dir / r["ref"]), str(out / "ours" / r["same_time"]), "+append", "-resize", "x180",
                 "-gravity", "northwest", "-fill", "yellow", "-pointsize", "14", "-annotate", "+4+4",
                 f"{r['t']}s ref | ours  psnr {r['same_time_psnr']}", ")"]
    subprocess.run(["magick", *args, "-append", str(out / "timeline.png")], check=True)
    summary = {"frames": len(rows), "mean_same_time_psnr": round(sum(r["same_time_psnr"] for r in rows) / len(rows), 2),
               "min_same_time_psnr": min(r["same_time_psnr"] for r in rows), "first_visible": {"ref": r0, "ours": o0}}
    (out / "boot.json").write_text(json.dumps({"summary": summary, "rows": rows}, indent=1))
    print(json.dumps(summary), f"-> {out}/timeline.png")


def cmd_ui(a):
    w, h = map(int, a.size.split("x"))
    o, only = Ours(), a.only
    if a.action == "start":
        import threading
        th = None
        if only != "ours":
            th = threading.Thread(target=lambda: ref_mod.start(boot=True, size=a.size, record=(CACHE / "ui-ref", a.wait, 0.2)))
            th.start()
        if only != "ref":
            o.record_boot(CACHE / "ui-ours", a.wait, 0.2, a.size)
        if th:
            th.join()
    elif a.action == "click":
        fx, fy = float(a.args[0]), float(a.args[1])
        x, y = int(fx * w), int(fy * h)
        if only != "ref" and o.pid():
            o.send({"cmd": "input", "type": "mouse_move", "x": x, "y": y, "xrel": 1, "yrel": 1})
            o.send({"cmd": "wait_frames", "n": 2, "max_ms": 500})
            for act in ("down", "up"):
                o.send({"cmd": "input", "type": "mouse_button", "button": a.button, "action": act, "x": x, "y": y})
                o.send({"cmd": "wait_frames", "n": 2, "max_ms": 500})
        if only != "ours" and ref_mod.game_pid():
            ref_mod.gt("click", str(x), str(y), "-o", ref_mod.ref_output(a.size), *(["-b", "right"] if a.button == "right" else []))
    elif a.action == "key":
        for k in a.args:
            if only != "ref" and o.pid():
                o.send({"cmd": "input", "type": "key", "key": k, "action": "tap"})
                o.send({"cmd": "wait_frames", "n": 2, "max_ms": 500})
            if only != "ours" and ref_mod.game_pid():
                ref_mod.swaymsg('[title="^SOMA"]', "focus")
                ref_mod.gt("key", k)
    if a.action != "start":
        time.sleep(a.settle)
    out = Path(a.out or CACHE / "ui.png")
    shots = []
    for t in targets(only):
        if t.pid():
            f = out.with_name(f"{out.stem}-{t.name}.png")
            t.shot(f)
            shots.append(f)
    if shots:
        subprocess.run(["magick", *map(str, shots), "-resize", f"{w}x{h}!", "+append", str(out)], check=True)
        print(out)


def cmd_report(a):
    out = Path(a.out or CACHE / f"report-{Path(a.map).stem}-{time.strftime('%Y%m%d-%H%M%S')}")
    out.mkdir(parents=True, exist_ok=True)
    for t in targets(a.only):
        t.start(a.map, a.pos, a.size)
    rep = {"map": a.map}
    for name in ("player", "entities", "lights"):
        code = SNIPPETS[name] % "*" if "%s" in SNIPPETS[name] else SNIPPETS[name]
        rows, bad = diff_table(run_both(code, a.only, timeout=120), show_all=False)
        rep[name] = {"mismatches": bad, "rows": [{"key": k, "ours": o, "ref": r} for k, o, r, _ in rows]}
    a.pose, a.out = None, str(out)
    rep["view"] = cmd_view(a, a.only)
    a.secs = 10
    rep["fps"] = cmd_fps(a, a.only)
    (out / "report.json").write_text(json.dumps(rep, indent=1))
    print(f"{out}/report.json: " + ", ".join(f"{k} {rep[k]['mismatches']} mismatches" for k in ("player", "entities", "lights")))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--only", choices=("ours", "ref"))
    sp = p.add_subparsers(dest="cmd", required=True)
    for name in ("start", "report"):
        s = sp.add_parser(name)
        s.add_argument("--map", default="00_01_apartment.hpm")
        s.add_argument("--pos", default="PlayerStartArea_1")
        s.add_argument("--size", default="1280x720")
        s.add_argument("--settle", type=float, default=2)
        s.add_argument("--out")
    e = sp.add_parser("exec")
    e.add_argument("code", nargs="?")
    e.add_argument("-f")
    e.add_argument("--diff", action="store_true", help="only mismatching keys")
    for name in ("ping", "player", "perf", "entities", "lights"):
        s = sp.add_parser(name)
        s.add_argument("--pattern", default="*")
        s.add_argument("--diff", action="store_true")
    v = sp.add_parser("view")
    v.add_argument("--pose", type=float, nargs=5, metavar=("X", "Y", "Z", "YAW", "PITCH"))
    v.add_argument("--settle", type=float, default=2)
    v.add_argument("--out")
    la = sp.add_parser("lightattr")
    la.add_argument("names", nargs="*")
    la.add_argument("--n", type=int, default=12)
    la.add_argument("--settle", type=float, default=0.5)
    la.add_argument("--out")
    la.add_argument("--solo", action="store_true", help="all lights off, enable one at a time")
    f = sp.add_parser("fps")
    f.add_argument("--secs", type=float, default=10)
    b = sp.add_parser("boot")
    b.add_argument("--secs", type=float, default=25)
    b.add_argument("--fps", type=float, default=5)
    b.add_argument("--size", default="1280x720")
    b.add_argument("--out")
    b.add_argument("--ref-dir")
    b.add_argument("--record-ref", action="store_true")
    b.add_argument("--first-launch", action="store_true", help="ours without user_settings.cfg/gamma marker, fullscreen")
    u = sp.add_parser("ui")
    u.add_argument("action", choices=("start", "click", "key", "shot"))
    u.add_argument("args", nargs="*")
    u.add_argument("--size", default="1280x720")
    u.add_argument("--wait", type=float, default=45)
    u.add_argument("--settle", type=float, default=1)
    u.add_argument("--button", default="left")
    u.add_argument("--out")
    sp.add_parser("stop")
    a = p.parse_args()

    if a.cmd == "start":
        for t in targets(a.only):
            t.start(a.map, a.pos, a.size)
    elif a.cmd == "stop":
        for t in targets(a.only):
            t.stop()
    elif a.cmd == "view":
        cmd_view(a, a.only)
    elif a.cmd == "lightattr":
        cmd_lightattr(a, a.only)
    elif a.cmd == "fps":
        cmd_fps(a, a.only)
    elif a.cmd == "boot":
        cmd_boot(a)
    elif a.cmd == "report":
        cmd_report(a)
    elif a.cmd == "ui":
        cmd_ui(a)
    else:
        if a.cmd == "exec":
            code = Path(a.f).read_text() if a.f else a.code
        else:
            code = SNIPPETS[a.cmd] % a.pattern if "%s" in SNIPPETS[a.cmd] else SNIPPETS[a.cmd]
        res = run_both(code, a.only)
        if not any("=" in l for ls in res.values() for l in ls):
            for k, ls in res.items():
                print(f"== {k}\n" + "\n".join(ls))
            return
        rows, bad = diff_table(res, show_all=not a.diff)
        print_table(rows)
        print(f"{bad} mismatching of {len(parse(res.get('ours', [])) | parse(res.get('ref', [])))}")


if __name__ == "__main__":
    main()
