#!/usr/bin/env python3
"""Drive the script player of a headless open-hpl SOMA instance through the story.

  scripts/soma-play.py start [--map 00_03_laboratory] [--pos PlayerStartArea_1]
  scripts/soma-play.py state                          # map, player state/pose, dialog, fade
  scripts/soma-play.py goto ENTITY [--dist 1.0]       # feet next to an entity, facing it
  scripts/soma-play.py look ENTITY                    # aim the camera at an entity
  scripts/soma-play.py interact ENTITY [--hold 0.1]   # look at it and click
  scripts/soma-play.py drag ENTITY DX DY [--steps 60]  # hold click, move the mouse by DX,DY over STEPS frames
  scripts/soma-play.py throw ENTITY TARGET [--place S] # grab ENTITY, aim at TARGET, throw (or hold S s and release)
  scripts/soma-play.py mouse DX DY [--steps 30]       # relative mouse look
  scripts/soma-play.py key KEY [--hold 0.1] | click [--hold 0.1] | wait SECS
  scripts/soma-play.py walk SECS [--key w] [--jump T] # hold a movement key, jump T s in
  scripts/soma-play.py walkto ENTITY|X Y Z [--tol 0.5] [--nav|--grid] [--run] # steer with w until within TOL m (stops when stuck)
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

NAME = os.environ.get("OHPL_PLAY_NAME", "ohpl-play")
SOCK = RUNTIME / f"{NAME}.sock"
PIDFILE = RUNTIME / f"{NAME}.pid"
LOGPOS = RUNTIME / f"{NAME}.logpos"


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
    if a.save:
        ex(f'cLux_GetSaveHandler().LoadGameFromFile("{a.save}");')
        send({"cmd": "wait_frames", "n": 120, "max_ms": 120000}, timeout=150)
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
       f"p.GetCamera().SetPitch({pitch});")


def focused(name):
    # the game's own pick: range, CanInteract, offset rays
    frames(0.15)
    f = kv('cScript_RunGlobalFunc("State_Normal", "", "_Global_GetFocusEntityName");'
           '__print("f=" + cScript_GetGlobalReturnString());').get("f")
    if f == name:
        return True
    # InteractAux areas forward interaction to their parent
    out = send({"cmd": "script_vars", "name": f}).get("output", "") if f else ""
    return f'msInteractParent="{name}"' in out


def aim_entity(name):
    target = ent_pos(name)
    cam, _ = camera_pos()
    aim(target)
    if focused(name):
        return
    d = kv(f'iLuxEntity@ e = cLux_GetCurrentMap().GetEntityByName("{name}"); if(e.GetMainBody() is null) return;'
           'cBoundingVolume@ bv = e.GetMainBody().GetBoundingVolume(); cVector3f a = bv.GetMin(), b = bv.GetMax();'
           '__print("a=" + a.x + " " + a.y + " " + a.z); __print("b=" + b.x + " " + b.y + " " + b.z);')
    if "a" not in d:
        return aim(target)
    lo, hi = [float(v) for v in d["a"].split()], [float(v) for v in d["b"].split()]
    n = 4
    pts = [[lo[k] + (hi[k] - lo[k]) * (0.03 + 0.94 * (i, j, l)[k] / n) for k in range(3)]
           for i in range(n + 1) for j in range(n + 1) for l in range(n + 1)]
    # interact range is short: nearest points first
    pts.sort(key=lambda q: math.dist(q, cam))
    for q in pts:
        h = raycast(cam, [c + (v - c) * 1.5 for c, v in zip(cam, q)], name)
        if h and h[0][2] == name:
            aim(q)
            # camera moves with pitch
            if focused(name):
                return
            cam, _ = camera_pos()
    aim(target)


def cmd_look(a):
    aim([float(v) for v in a.entity]) if len(a.entity) == 3 else aim_entity(a.entity[0])
    frames(0.2)


def raycast(a, b, keep=None):
    r = send(dict(cmd="raycast", **{k: str(v) for k, v in zip(("x", "y", "z", "x2", "y2", "z2"), (*a, *b))}))
    out = []
    for l in r.get("hits", "").splitlines():
        if "char=1" not in l and f"entity={keep} " not in l:
            continue
        f = l.split()
        out.append((float(f[0]), f[1], f[2].split("=", 1)[1]))
    return out


def screen_normal(name):
    m = send({"cmd": "lux_entity", "name": name}).get("gui_mtx")
    if not m:
        return None
    rows = [[float(v) for v in r.split(":")] for r in m.strip("[]").split("] [")]
    return rows[0][2], rows[2][2]


def stand_spot(target, feet, dist, name):
    front = screen_normal(name)
    base = math.atan2(*front) if front else math.atan2(feet[0] - target[0], feet[2] - target[2])
    best = None
    for i in range(24):
        ang = base + (i + 1) // 2 * (1 if i % 2 else -1) * math.pi / 12
        x, z = target[0] + math.sin(ang) * dist, target[2] + math.cos(ang) * dist
        if front and (x - target[0]) * front[0] + (z - target[2]) * front[1] < 0.2 * dist:
            continue
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
        score = abs(floor + 1.6 - target[1]) + i * 0.02
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


def press(kind, name, hold, xy=(0, 0)):
    if kind == "key":
        send({"cmd": "input", "type": "key", "key": name, "action": "down"})
        frames(hold)
        send({"cmd": "input", "type": "key", "key": name, "action": "up"})
    else:
        x, y = xy
        send({"cmd": "input", "type": "mouse_button", "button": name, "action": "down", "x": x, "y": y})
        frames(hold)
        send({"cmd": "input", "type": "mouse_button", "button": name, "action": "up", "x": x, "y": y})
    frames(0.2)


def cmd_interact(a):
    aim_entity(a.entity)
    if not focused(a.entity):
        print(f"not focused: {a.entity}")
    frames(0.3)
    press("mouse", "left", a.hold)
    cmd_log(argparse.Namespace(regex=None, all=False))


def cmd_drag(a):
    aim_entity(a.entity)
    if not focused(a.entity):
        print(f"not focused: {a.entity}")
    frames(0.3)
    send({"cmd": "input", "type": "mouse_button", "button": "left", "action": "down"})
    frames(0.2)
    # interact states ignore look input under 0.01 screen heights per frame
    path = 2 * math.pi * abs(a.dx) * a.circles if a.circles else math.hypot(a.dx, a.dy)
    a.steps = max(1, min(a.steps, int(path / 40)))
    if a.circles:
        px = py = 0
        for i in range(1, a.steps + 1):
            t = 2 * math.pi * a.circles * i / a.steps * (1 if a.dy >= 0 else -1)
            x, y = round(a.dx * math.cos(t)) - a.dx, round(a.dx * math.sin(t))
            send({"cmd": "input", "type": "mouse_move", "xrel": str(x - px), "yrel": str(y - py)})
            px, py = x, y
            send({"cmd": "wait_frames", "n": 1, "max_ms": 1000})
    else:
        # same motion every frame: slide/wheel states zero their speed on frames without mouse input
        send({"cmd": "input", "type": "mouse_move", "xrel": str(round(a.dx / a.steps)), "yrel": str(round(a.dy / a.steps)), "frames": a.steps})
        send({"cmd": "wait_frames", "n": a.steps, "max_ms": 30000}, timeout=60)
    frames(0.3)
    send({"cmd": "input", "type": "mouse_button", "button": "left", "action": "up"})
    frames(0.3)
    cmd_log(argparse.Namespace(regex=None, all=False))


def cmd_throw(a):
    aim_entity(a.entity)
    frames(0.3)
    send({"cmd": "input", "type": "mouse_button", "button": "left", "action": "down"})
    frames(0.5)
    print(ex('__print(cLux_GetPlayer().GetCurrentStateName());').strip())
    aim(ent_pos(a.target))
    frames(a.place or 1.5)
    if not a.place:
        press("mouse", "right", 0.1)
    send({"cmd": "input", "type": "mouse_button", "button": "left", "action": "up"})
    frames(1.0)
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
    if a.trace:
        send({"cmd": "input", "type": "key", "key": a.key, "action": "down"})
        for i in range(int(a.secs / a.trace)):
            frames(a.trace)
            print(f"{(i + 1) * a.trace:.2f}", " ".join(f"{v:.3f}" for v in camera_pos()[1]))
        send({"cmd": "input", "type": "key", "key": a.key, "action": "up"})
    elif a.jump is None:
        press("key", a.key, a.secs)
    else:
        send({"cmd": "input", "type": "key", "key": a.key, "action": "down"})
        frames(a.jump)
        press("key", "space", 0.2)
        frames(max(a.secs - a.jump - 0.4, 0))
        send({"cmd": "input", "type": "key", "key": a.key, "action": "up"})
    cmd_state(a)


def crouching():
    return kv('cScript_RunGlobalFunc("MoveState_Normal", "", "_Global_GetCrouching");'
              '__print("c=" + cScript_GetGlobalReturnBool());').get("c") == "true"


def steer(target, tol, deadline, back=False, prev=None):
    best, stuck = 1e9, 0
    while time.time() < deadline:
        _, feet = camera_pos()
        d = math.hypot(target[0] - feet[0], target[2] - feet[2])
        if d < tol:
            return True
        # overshot an intermediate waypoint between polls
        if prev and (target[0] - feet[0]) * (target[0] - prev[0]) + (target[2] - feet[2]) * (target[2] - prev[2]) < 0:
            return True
        stuck = 0 if d < best - 0.05 else stuck + 1
        best = min(best, d)
        if stuck > 20:
            print(f"stuck at {d:.2f} m from {target}")
            return False
        yaw = -math.atan2(target[0] - feet[0], feet[2] - target[2]) + (math.pi if back else 0)
        ex(f"cLuxPlayer@ p = cLux_GetPlayer(); p.GetCharacterBody().SetYaw({yaw}); p.GetCamera().SetYaw({yaw});")
        send({"cmd": "wait_frames", "n": 9, "max_ms": 100})
    return False


def cmd_walkto(a):
    target = [float(v) for v in a.target[:3]] if len(a.target) > 2 else ent_pos(a.target[0])
    route = [target + a.target[3:]]
    if a.nav or a.grid:
        _, feet = camera_pos()
        path = send({"cmd": "nav_path", "x": feet[0], "y": feet[1], "z": feet[2],
                     "x2": target[0], "y2": target[1], "z2": target[2], "grid": "1" if a.grid else ""}).get("path", "")
        route = [l.split() for l in path.splitlines() if l != "partial"] + [target]
        print(f"nav: {len(route) - 1} nodes" + (" (partial)" if path.startswith("partial") else "") if path else "nav: no path")
    deadline = time.time() + a.max

    def key(k, down):
        send({"cmd": "input", "type": "key", "key": k, "action": "down" if down else "up"})

    move = "s" if a.back else "w"
    key(move, True)
    try:
        for i, p in enumerate(route):
            crouch = len(p) > 3 and p[3] == "c"
            if crouch != crouching():
                key("left shift", False)
                press("key", "left ctrl", 0.1)
            key("left shift", a.run and not crouch)
            last = i == len(route) - 1
            if not steer([float(v) for v in p[:3]], a.tol if last else 0.5, deadline, a.back, None if last or i == 0 else [float(v) for v in route[i - 1][:3]]):
                break
    finally:
        key(move, False)
        key("left shift", False)
        if crouching():
            press("key", "left ctrl", 0.1)
    frames(0.2)
    cmd_state(a)
    cmd_log(argparse.Namespace(regex=None, all=False))


def cmd_wait(a):
    frames(a.secs)
    cmd_log(argparse.Namespace(regex=None, all=False))


def cmd_entities(a):
    code = ('array<iLuxEntity@> v; cLux_GetCurrentMap().GetEntityArray("%s", eLuxEntityType_LastEnum, "", v);'
            'cVector3f c = cLux_GetPlayer().GetCamera().GetPosition();'
            'for(uint i = 0; i < v.length(); ++i) { iLuxEntity@ e = v[i]; cVector3f p = e.GetPosition();'
            'float d = cMath_Vector3Dist(p, c); if(d > %f) continue;'
            '__print(e.GetName() + "=" + (e.IsActive() ? "on " : "off ") + e.GetClassName() + " d=" + d + " @" + p.x + "," + p.y + "," + p.z'
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
    r = send(req)
    xy = (0, 0)
    if "screen_x" in r:
        xy = (r["screen_x"], r["screen_y"])
        send({"cmd": "input", "type": "mouse_move", "x": xy[0], "y": xy[1], "xrel": 1, "yrel": 1})
    frames(0.1)
    press("mouse", "left", 0.1, xy)


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
    hits = [t for t in texts if a.text.lower() == t[0].lower()] or [t for t in texts if a.text.lower() in t[0].lower()]
    if len(hits) <= a.nth:
        raise SystemExit(f"no text matching {a.text!r}")
    if len(hits) > 1:
        print(f"{len(hits)} matches, using --nth {a.nth}: " + ", ".join(f"{x:.0f},{y:.0f}" for _, x, y, _ in hits), file=sys.stderr)
    t, x, y, h = hits[a.nth]
    gui_click(a.entity, x + 4, y + h * 0.5)
    print(f"clicked {t!r} at {x + 4:.0f},{y + h * 0.5:.0f}")


def cmd_shot(a):
    bmp = Path(a.out).resolve().with_suffix(".bmp")
    hud = not a.no_hud and not send({"cmd": "dev_hud", "value": True})["prev"]
    if hud:
        send({"cmd": "wait_frames", "n": 3, "max_ms": 5000})
    send({"cmd": "screenshot", "path": str(bmp)})
    if hud:
        send({"cmd": "dev_hud", "value": False})
    for _ in range(50):
        if bmp.exists() and bmp.stat().st_size:
            break
        time.sleep(0.1)
    subprocess.run(["magick", str(bmp), "-alpha", "off", str(Path(a.out).resolve())], check=True)
    bmp.unlink()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("start"); s.add_argument("--map", default="00_03_laboratory"); s.add_argument("--pos"); s.add_argument("--save", help="save file name in the saves dir")
    sub.add_parser("stop"); sub.add_parser("state")
    s = sub.add_parser("goto"); s.add_argument("entity"); s.add_argument("--dist", type=float, default=1.0)
    s.add_argument("--keep-height", action="store_true")
    s = sub.add_parser("look"); s.add_argument("entity", nargs="+", help="name or X Y Z")
    s = sub.add_parser("interact"); s.add_argument("entity"); s.add_argument("--hold", type=float, default=0.1)
    s = sub.add_parser("drag"); s.add_argument("entity"); s.add_argument("dx", type=int); s.add_argument("dy", type=int)
    s.add_argument("--steps", type=int, default=60)
    s.add_argument("--circles", type=float, default=0, help="circle mouse: radius dx, sign of dy = direction")
    s = sub.add_parser("throw"); s.add_argument("entity"); s.add_argument("target"); s.add_argument("--place", type=float)
    s = sub.add_parser("mouse"); s.add_argument("dx", type=int); s.add_argument("dy", type=int)
    s.add_argument("--steps", type=int, default=30)
    s = sub.add_parser("key"); s.add_argument("key"); s.add_argument("--hold", type=float, default=0.1)
    s = sub.add_parser("click"); s.add_argument("--button", default="left"); s.add_argument("--hold", type=float, default=0.1)
    s = sub.add_parser("walk"); s.add_argument("secs", type=float); s.add_argument("--key", default="w")
    s.add_argument("--jump", type=float, help="press space after this many seconds")
    s.add_argument("--trace", type=float, help="print feet every N seconds")
    s = sub.add_parser("walkto"); s.add_argument("target", nargs="+", help="name or X Y Z [c]")
    s.add_argument("--tol", type=float, default=0.5); s.add_argument("--max", type=float, default=30)
    s.add_argument("--nav", action="store_true", help="follow the agent node graph")
    s.add_argument("--grid", action="store_true", help="player-size grid A* (crouches where needed)")
    s.add_argument("--run", action="store_true", help="hold shift")
    s.add_argument("--back", action="store_true", help="walk backwards, facing away from the target")
    s = sub.add_parser("wait"); s.add_argument("secs", type=float)
    s = sub.add_parser("entities"); s.add_argument("pattern", nargs="?", default="*"); s.add_argument("--near", type=float, default=1e9)
    s = sub.add_parser("exec"); s.add_argument("code"); s.add_argument("--module", default="")
    s = sub.add_parser("log"); s.add_argument("regex", nargs="?"); s.add_argument("--all", action="store_true")
    s = sub.add_parser("shot"); s.add_argument("out"); s.add_argument("--no-hud", action="store_true")
    s = sub.add_parser("gui"); s.add_argument("text", nargs="?"); s.add_argument("--entity"); s.add_argument("--at", type=float, nargs=2); s.add_argument("--nth", type=int, default=0)
    a = ap.parse_args()
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(143))  # run finally blocks: release held keys
    globals()["cmd_" + a.cmd](a)


if __name__ == "__main__":
    main()
