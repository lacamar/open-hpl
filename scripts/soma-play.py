#!/usr/bin/env python3
"""Drive the script player of a headless open-hpl SOMA instance through the story.

  scripts/soma-play.py start [--map 00_03_laboratory] [--pos PlayerStartArea_1]
  scripts/soma-play.py state                          # map, player state/pose, dialog, fade
  scripts/soma-play.py goto ENTITY [--dist 1.0]       # feet next to an entity, facing it
  scripts/soma-play.py look ENTITY                    # aim the camera at an entity
  scripts/soma-play.py interact ENTITY [--hold 0.1]   # look at it and click
  scripts/soma-play.py drag ENTITY DX DY [--steps 60]  # hold click, move the mouse by DX,DY over STEPS frames
  scripts/soma-play.py mouse DX DY [--steps 30]       # relative mouse look
  scripts/soma-play.py key KEY [--hold 0.1] | click [--hold 0.1] | wait SECS
  scripts/soma-play.py walk SECS [--key w]            # hold a movement key
  scripts/soma-play.py entities [PATTERN] [--near 5]  # entities: active, class, interactable, distance
  scripts/soma-play.py exec 'code' [--module M]       # AngelScript, __print() output; M: inside the first script file matching M
  scripts/soma-play.py log [REGEX] [--all]            # new log lines since the last call
  scripts/soma-play.py gui [TEXT] [--entity E] [--at X Y]  # list GUI texts of the focused screen, or click one
scripts/soma-play.py shot OUT.png | stop
"""
import argparse, math, os, re, signal, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from hpl_control import HplControl, HplControlError, RUNTIME, SCRATCH, pidfile_pid  # noqa: E402

SOCK = RUNTIME / "ohpl-play.sock"
PIDFILE = RUNTIME / "ohpl-play.pid"
LOGPOS = RUNTIME / "ohpl-play.logpos"


def send(req, timeout=60):
    try:
        with HplControl(str(SOCK), timeout=timeout) as h:
            r = h.send(req)
    except (HplControlError, OSError) as e:
        raise SystemExit(f"error: {e}")
    if not r.get("ok", True):
        raise SystemExit(r.get("error", "failed") + r.get("output", ""))
    return r


def ex(code, module=""):
    return send({"cmd": "script_exec", "code": code, "module": module}).get("output", "")


def kv(code):
    return dict(l.split("=", 1) for l in ex(code).splitlines() if "=" in l)


def pid():
    return pidfile_pid(PIDFILE)


def frames(secs):
    send({"cmd": "wait_frames", "n": max(1, int(secs * 60)), "max_ms": int(secs * 1000) + 5000}, timeout=secs + 30)


def log_path():
    p = pid()
    return SCRATCH / f".xdg/state/open-hpl/soma/hpl-{p}.log" if p else None


def cmd_start(a):
    cmd_stop(a)
    m = a.map if a.map.endswith(".hpm") else a.map + ".hpm"
    env = dict(os.environ, OPENHPL_SOMA_FREECAM="0")
    out = subprocess.run([str(HERE / "soma-run.sh"), m, str(SOCK)], env=env, capture_output=True, text=True, check=True).stdout.split()
    PIDFILE.write_text(out[0])
    LOGPOS.write_text("0")
    t = time.time()
    while not SOCK.exists():
        if pid() is None:
            raise SystemExit("instance exited during load")
        time.sleep(0.5)
    send({"cmd": "wait_frames", "n": 60, "max_ms": 120000}, timeout=150)
    if a.pos:
        ex(f'Entity_PlaceAtEntity("Player", "{a.pos}");')
    print(f"pid {out[0]} {m} in {time.time() - t:.0f}s")


def cmd_stop(a):
    p = pid()
    if p:
        os.kill(p, signal.SIGKILL)
        print(f"killed {p}")
    PIDFILE.unlink(missing_ok=True)


STATE = (
    '__print("map=" + cLux_GetCurrentMap().GetName());'
    'cLuxPlayer@ p = cLux_GetPlayer(); iCharacterBody@ b = p.GetCharacterBody(); cVector3f f = b.GetFeetPosition();'
    '__print("feet=" + f.x + " " + f.y + " " + f.z);'
    '__print("yaw=" + cMath_ToDeg(p.GetCamera().GetYaw()) + " pitch=" + cMath_ToDeg(p.GetCamera().GetPitch()));'
    '__print("state=" + p.GetCurrentStateName() + " active=" + p.IsActive());'
    '__print("voice=" + cLux_GetVoiceHandler().AnySceneIsActive());'
)


def cmd_state(a):
    out = ex(STATE)
    print(out.rstrip())
    r = send({"cmd": "player_state"})
    print("move_state=" + str(r.get("move_state")))


def ent_pos(name):
    d = kv(f'iLuxEntity@ e = cLux_GetCurrentMap().GetEntityByName("{name}"); if(e is null) {{ __print("err=missing"); return; }}'
           'cVector3f p = e.GetPosition(); __print("p=" + p.x + " " + p.y + " " + p.z);'
           'cBoundingVolume@ bv = null; if(e.GetMainBody() !is null) @bv = e.GetMainBody().GetBoundingVolume();'
           'if(bv !is null) { cVector3f c = bv.GetWorldCenter(); __print("c=" + c.x + " " + c.y + " " + c.z); }')
    if "err" in d:
        raise SystemExit(f"no entity {name}")
    return [float(v) for v in d.get("c", d["p"]).split()]


def camera_pos():
    d = kv('cVector3f c = cLux_GetPlayer().GetCamera().GetPosition(); __print("c=" + c.x + " " + c.y + " " + c.z);'
           'cVector3f f = cLux_GetPlayer().GetCharacterBody().GetFeetPosition(); __print("f=" + f.x + " " + f.y + " " + f.z);')
    return [float(v) for v in d["c"].split()], [float(v) for v in d["f"].split()]


def aim(target):
    cam, _ = camera_pos()
    dx, dy, dz = (target[i] - cam[i] for i in range(3))
    yaw = -math.atan2(dx, -dz)
    pitch = math.atan2(dy, math.hypot(dx, dz))
    ex(f"cLuxPlayer@ p = cLux_GetPlayer(); p.GetCharacterBody().SetYaw({yaw}); p.GetCamera().SetYaw({yaw});"
       f"p.GetCharacterBody().SetPitch({pitch}); p.GetCamera().SetPitch({pitch});")


def aim_entity(name):
    target = ent_pos(name)
    cam, _ = camera_pos()
    hits = raycast(cam, target)
    if not hits or hits[0][2] == name:
        return aim(target)
    d = kv(f'iLuxEntity@ e = cLux_GetCurrentMap().GetEntityByName("{name}"); if(e.GetMainBody() is null) return;'
           'cBoundingVolume@ bv = e.GetMainBody().GetBoundingVolume(); cVector3f a = bv.GetMin(), b = bv.GetMax();'
           '__print("a=" + a.x + " " + a.y + " " + a.z); __print("b=" + b.x + " " + b.y + " " + b.z);')
    if "a" not in d:
        return aim(target)
    lo, hi = [float(v) for v in d["a"].split()], [float(v) for v in d["b"].split()]
    n = 4
    pts = [[lo[k] + (hi[k] - lo[k]) * (0.1 + 0.8 * (i, j, l)[k] / n) for k in range(3)]
           for i in range(n + 1) for j in range(n + 1) for l in range(n + 1)]
    pts.sort(key=lambda q: math.dist(q, target))
    for q in pts:
        h = raycast(cam, q)
        if h and h[0][2] == name:
            return aim(q)
    aim(target)


def cmd_look(a):
    aim_entity(a.entity)
    frames(0.2)


def raycast(a, b):
    r = send(dict(cmd="raycast", **{k: str(v) for k, v in zip(("x", "y", "z", "x2", "y2", "z2"), (*a, *b))}))
    out = []
    for l in r.get("hits", "").splitlines():
        if "char=1" not in l:
            continue
        f = l.split()
        out.append((float(f[0]), f[1], f[2].split("=", 1)[1]))
    return out


def stand_spot(target, feet, dist, name):
    base = math.atan2(feet[0] - target[0], feet[2] - target[2])
    best = None
    for i in range(24):
        ang = base + (i + 1) // 2 * (1 if i % 2 else -1) * math.pi / 12
        x, z = target[0] + math.sin(ang) * dist, target[2] + math.cos(ang) * dist
        top = target[1] + 0.3
        hits = raycast((x, top, z), (x, top - 4.0, z))
        if not hits or hits[0][0] < 0.05:
            continue
        floor = top - hits[0][0]
        if raycast((x, floor + 0.1, z), (x, floor + 1.9, z)):
            continue
        eye = (x, floor + 1.6, z)
        los = raycast(eye, target)
        seg = math.dist(eye, target)
        if los and los[0][0] < seg - 0.25 and name not in (los[0][1], los[0][2]):
            continue
        score = abs(floor - feet[1]) + i * 0.02
        if best is None or score < best[0]:
            best = (score, x, floor, z)
    return best


def cmd_goto(a):
    target = ent_pos(a.entity)
    _, feet = camera_pos()
    spot = None if a.keep_height else stand_spot(target, feet, a.dist, a.entity)
    if spot:
        _, x, y, z = spot
        y += 0.05
    else:
        dx, dz = feet[0] - target[0], feet[2] - target[2]
        n = math.hypot(dx, dz) or 1.0
        x, y, z = target[0] + dx / n * a.dist, feet[1], target[2] + dz / n * a.dist
    ex(f'cLux_GetPlayer().GetCharacterBody().SetFeetPosition(cVector3f({x}, {y}, {z}), true);')
    frames(0.3)
    aim(target)
    frames(0.2)
    cmd_state(a)


def press(kind, name, hold):
    if kind == "key":
        send({"cmd": "input", "type": "key", "key": name, "action": "down"})
        frames(hold)
        send({"cmd": "input", "type": "key", "key": name, "action": "up"})
    else:
        send({"cmd": "input", "type": "mouse_button", "button": name, "action": "down"})
        frames(hold)
        send({"cmd": "input", "type": "mouse_button", "button": name, "action": "up"})
    frames(0.2)


def cmd_interact(a):
    aim_entity(a.entity)
    frames(0.3)
    press("mouse", "left", a.hold)
    cmd_log(argparse.Namespace(regex=None, all=False))


def cmd_drag(a):
    aim_entity(a.entity)
    frames(0.3)
    send({"cmd": "input", "type": "mouse_button", "button": "left", "action": "down"})
    frames(0.2)
    for _ in range(a.steps):
        send({"cmd": "input", "type": "mouse_move", "xrel": str(int(a.dx / a.steps)), "yrel": str(int(a.dy / a.steps))})
        send({"cmd": "wait_frames", "n": 1, "max_ms": 1000})
    frames(0.3)
    send({"cmd": "input", "type": "mouse_button", "button": "left", "action": "up"})
    frames(0.3)
    cmd_log(argparse.Namespace(regex=None, all=False))


def cmd_mouse(a):
    for _ in range(a.steps):
        send({"cmd": "input", "type": "mouse_move", "xrel": str(int(a.dx / a.steps)), "yrel": str(int(a.dy / a.steps))})
        send({"cmd": "wait_frames", "n": 1, "max_ms": 1000})
    frames(0.2)
    cmd_state(a)


def cmd_key(a):
    press("key", a.key, a.hold)


def cmd_click(a):
    press("mouse", a.button, a.hold)


def cmd_walk(a):
    press("key", a.key, a.secs)
    cmd_state(a)


def cmd_wait(a):
    frames(a.secs)
    cmd_log(argparse.Namespace(regex=None, all=False))


def cmd_entities(a):
    code = ('array<iLuxEntity@> v; cLux_GetCurrentMap().GetEntityArray("%s", eLuxEntityType_LastEnum, "", v);'
            'cVector3f c = cLux_GetPlayer().GetCamera().GetPosition();'
            'for(uint i = 0; i < v.length(); ++i) { iLuxEntity@ e = v[i]; cVector3f p = e.GetPosition();'
            'float d = cMath_Vector3Dist(p, c); if(d > %f) continue;'
            '__print(e.GetName() + "=" + (e.IsActive() ? "on " : "off ") + e.GetClassName() + " d=" + d'
            ' + (e.GetInteractionDisabled() ? " nointeract" : "")); }') % (a.pattern, a.near)
    for l in sorted(ex(code).splitlines(), key=lambda l: float(re.search(r"d=([\d.e+-]+)", l).group(1)) if "d=" in l else 0):
        print(l)


def cmd_exec(a):
    print(ex(a.code, a.module).rstrip())


NOISE = re.compile(r"script warning|Sampler \w+ does not exist|Signed/Unsigned")


def cmd_log(a):
    p = log_path()
    if p is None or not p.exists():
        return
    data = p.read_bytes()
    start = 0 if a.all else int(LOGPOS.read_text() or 0) if LOGPOS.exists() else 0
    LOGPOS.write_text(str(len(data)))
    for line in data[start:].decode(errors="replace").splitlines():
        if NOISE.search(line):
            continue
        if a.regex is None or re.search(a.regex, line):
            print(line)


OP = re.compile(r"(gfx|text '(.*?)') ([-\d.]+),([-\d.]+),[-\d.]+ ([-\d.]+)x([-\d.]+)")


def gui_texts(entity):
    req = {"cmd": "imgui_ops"}
    if entity:
        req["name"] = entity
    ops = send(req)["ops"].split(": ", 1)[-1].split("; ")
    return [(m.group(2), float(m.group(3)), float(m.group(4)), float(m.group(6))) for m in map(OP.match, ops) if m and m.group(2)]


def gui_click(entity, x, y):
    req = {"cmd": "imgui_cursor", "x": x, "y": y}
    if entity:
        req["name"] = entity
    send(req)
    frames(0.1)
    press("mouse", "left", 0.1)


def cmd_gui(a):
    if a.at:
        gui_click(a.entity, *a.at)
        print(f"clicked {a.at[0]:.0f},{a.at[1]:.0f}")
        return
    texts = gui_texts(a.entity)
    if a.text is None:
        for t, x, y, h in texts:
            print(f"{x:7.1f} {y:7.1f}  {t}")
        return
    hits = [t for t in texts if a.text.lower() in t[0].lower()]
    if not hits:
        raise SystemExit(f"no text matching {a.text!r}")
    t, x, y, h = hits[0]
    gui_click(a.entity, x + 4, y + h * 0.5)
    print(f"clicked {t!r} at {x + 4:.0f},{y + h * 0.5:.0f}")


def cmd_shot(a):
    bmp = Path(a.out).resolve().with_suffix(".bmp")
    send({"cmd": "screenshot", "path": str(bmp)})
    for _ in range(50):
        if bmp.exists() and bmp.stat().st_size:
            break
        time.sleep(0.1)
    subprocess.run(["magick", str(bmp), "-alpha", "off", str(Path(a.out).resolve())], check=True)
    bmp.unlink()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("start"); s.add_argument("--map", default="00_03_laboratory"); s.add_argument("--pos")
    sub.add_parser("stop"); sub.add_parser("state")
    s = sub.add_parser("goto"); s.add_argument("entity"); s.add_argument("--dist", type=float, default=1.0)
    s.add_argument("--keep-height", action="store_true")
    s = sub.add_parser("look"); s.add_argument("entity")
    s = sub.add_parser("interact"); s.add_argument("entity"); s.add_argument("--hold", type=float, default=0.1)
    s = sub.add_parser("drag"); s.add_argument("entity"); s.add_argument("dx", type=int); s.add_argument("dy", type=int)
    s.add_argument("--steps", type=int, default=60)
    s = sub.add_parser("mouse"); s.add_argument("dx", type=int); s.add_argument("dy", type=int)
    s.add_argument("--steps", type=int, default=30)
    s = sub.add_parser("key"); s.add_argument("key"); s.add_argument("--hold", type=float, default=0.1)
    s = sub.add_parser("click"); s.add_argument("--button", default="left"); s.add_argument("--hold", type=float, default=0.1)
    s = sub.add_parser("walk"); s.add_argument("secs", type=float); s.add_argument("--key", default="w")
    s = sub.add_parser("wait"); s.add_argument("secs", type=float)
    s = sub.add_parser("entities"); s.add_argument("pattern", nargs="?", default="*"); s.add_argument("--near", type=float, default=1e9)
    s = sub.add_parser("exec"); s.add_argument("code"); s.add_argument("--module", default="")
    s = sub.add_parser("log"); s.add_argument("regex", nargs="?"); s.add_argument("--all", action="store_true")
    s = sub.add_parser("shot"); s.add_argument("out")
    s = sub.add_parser("gui"); s.add_argument("text", nargs="?"); s.add_argument("--entity"); s.add_argument("--at", type=float, nargs=2)
    a = ap.parse_args()
    globals()["cmd_" + a.cmd](a)


if __name__ == "__main__":
    main()
