#!/usr/bin/env python3
"""Drive the official Windows SOMA (Soma_NoSteam.exe) under wine-arm64ec as a reference.

The game runs in the `soma-test-claude` prefix inside the headless `gt` sway session, inside bwrap
with the install dir read-only, loaded as a StandAlone mod (soma/ref/mod) that adds an agent
module. Runtime-added modules are never instantiated, so the agent compiles snippets as player states:
the host fills a pre-created slot `script/ohpl/exec/ohpl_exec_<n>.hps` (only files indexed at
startup resolve), touches C:/ohpl/io/go_<n>, and the agent calls cLuxPlayer::AddState, whose
class constructor runs the snippet. Output goes to hpl.log (`OHPL|out|<n>|...`). Snippets are function bodies
with `__print(s)`, the same convention as `hpl_control.py script_exec`.

  scripts/soma-ref.py start [--map 00_01_apartment] [--pos PlayerStartArea_1] [--size 1280x720]
  scripts/soma-ref.py exec 'code' | -f file.as
  scripts/soma-ref.py ping | perf [--secs 5] | player | shot [out.png] | log [regex] | stop
  scripts/soma-ref.py teleport X Y Z [--yaw DEG] [--pitch DEG]
"""
import argparse, glob, json, os, re, shutil, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOMA = Path(os.environ.get("OHPL_SOMA_WIN_DIR", Path.home() / ".steam/steam/steamapps/common/SOMA"))
PREFIX = Path(os.environ.get("OHPL_REF_PREFIX", Path.home() / ".local/share/wine-prefixes/soma-test-claude"))
SLUG = "soma"
OHPL = PREFIX / "drive_c/ohpl"
MOD, IO = OHPL / "mod", OHPL / "io"
SAVE_FOLDER = "OhplRef"
SLOTS = 500
GT = shutil.which("gt") or str(Path.home() / ".local/src/wine-arm64ec-rpm/tools/gametest/gt")


def docs_dir():
    for d in sorted(glob.glob(str(PREFIX / "drive_c/users/*/Documents/My Games/Soma"))):
        if "/Public/" not in d:
            return Path(d) / "Mods" / SAVE_FOLDER
    return PREFIX / "drive_c/users/steamuser/Documents/My Games/Soma/Mods" / SAVE_FOLDER


def log_path():
    return docs_dir() / "hpl.log"


def gt(*a, check=True):
    return subprocess.run([GT, *a], capture_output=True, text=True, check=check).stdout


def game_pid():
    for pid in filter(str.isdigit, os.listdir("/proc")):
        try:
            cmd = Path(f"/proc/{pid}/cmdline").read_bytes().split(b"\0")[0].decode(errors="replace")
            env = Path(f"/proc/{pid}/environ").read_bytes()
        except OSError:
            continue
        if cmd.replace("\\", "/").endswith("Soma_NoSteam.exe") and f"WINEPREFIX={PREFIX}".encode() in env:
            return int(pid)
    return None


def write_mod(map_file, pos, size, boot=False):
    if MOD.exists():
        shutil.rmtree(MOD)
    (MOD / "config").mkdir(parents=True)
    IO.mkdir(parents=True, exist_ok=True)
    shutil.copytree(ROOT / "soma/ref/mod/script", MOD / "script")
    (MOD / "script/ohpl/exec").mkdir()
    for n in range(1, SLOTS + 1):
        (MOD / f"script/ohpl/exec/ohpl_exec_{n}.hps").write_text("")
    for f in (SOMA / "config").iterdir():
        (MOD / "config" / f.name).symlink_to(f)
    for f in ("materials.cfg", "sounddata.cfg"):
        (MOD / f).symlink_to(SOMA / f)
    (MOD / "entry.hpc").write_text('<Content Version="1.0" Type="StandAlone" Title="ohpl-ref" Author="open-hpl" '
                                   'Description="open-hpl reference agent" InitCfg="config/ohpl_init.cfg"/>\n')
    (MOD / "resources.cfg").write_text('<Resources>\n\t<Directory Path="/config" AddSubDirs="true"/>\n'
                                       '\t<Directory Path="/script" AddSubDirs="true"/>\n</Resources>\n')
    init = (SOMA / "config/main_init.cfg").read_text()
    init = init.replace('"config/Modules.cfg"', '"config/ohpl_modules.cfg"')
    init = re.sub(r'MainSaveFolder\s*=\s*"[^"]*"', f'MainSaveFolder = "{SAVE_FOLDER}"', init)
    if not boot:
        init = re.sub(r'<StartMap[^>]*/>', f'<StartMap File="{map_file}" Folder="maps/" Pos="{pos}"/>', init, flags=re.S)
    (MOD / "config/ohpl_init.cfg").write_text(init)
    mods = (SOMA / "config/Modules.cfg").read_text().replace(
        "</Modules>", '\t<Module Name="OhplAgent" ScriptFile="ohpl/OhplAgent.hps" ScriptClass="cScrOhplAgent" '
                      'ID="100" IsGlobal="true" Container="Default" UseInputCallbacks="false"/>\n</Modules>')
    (MOD / "config/ohpl_modules.cfg").write_text(mods)
    d = docs_dir()
    d.mkdir(parents=True, exist_ok=True)
    w, h = size.split("x")
    settings = (SOMA / "config/default_user_settings.cfg").read_text().replace("<Game />", '<Game MenuPhase="1" />')
    settings += (f'\n<Screen Width="{w}" Height="{h}" FullScreen="false" Vsync="false" />\n'
                 '<Main FirstGameStart="false" SleepWhenOutOfFocus="false"' +
                 ('' if boot else ' ShowMenu="false" ShowPreMenu="false"') + ' />\n')
    for f in d.glob("*_user_settings.cfg"):
        f.unlink()
    (d / "Default_user_settings.cfg").write_text(settings)


class Ref:
    def __init__(self):
        self.log = log_path()

    def lines(self, start=0):
        try:
            with open(self.log, "rb") as f:
                f.seek(start)
                data = f.read()
        except OSError:
            return start, []
        end = start + data.rfind(b"\n") + 1
        return end, data[: end - start].decode(errors="replace").splitlines()

    def size(self):
        try:
            return self.log.stat().st_size
        except OSError:
            return 0

    def wait_for(self, pattern, timeout, start=0):
        pos, t0 = start, time.time()
        while time.time() - t0 < timeout:
            pos, ls = self.lines(pos)
            for l in ls:
                if re.search(pattern, l):
                    return l
            if not game_pid():
                raise SystemExit("reference game exited, see: soma-ref.py log")
            time.sleep(0.2)
        raise SystemExit(f"timeout waiting for {pattern!r}")

    def next_id(self):
        return max((int(f.name[3:]) for f in IO.glob("go_*")), default=0) + 1

    def exec(self, code, timeout=30):
        n = self.next_id()
        if n > SLOTS:
            raise ExecError(f"all {SLOTS} exec slots used, restart", [])
        (MOD / f"script/ohpl/exec/ohpl_exec_{n}.hps").write_text(
            f'#include "player/PlayerState_Null.hps"\n'
            f'void __print(const tString&in s){{ LogNewLine("OHPL|out|{n}|" + s); }}\n'
            f'class cOhplExec{n} : cScrPlayerState_Null {{ cOhplExec{n}(){{ __exec(); }} }}\n'
            f'void __exec()\n{{\n{code}\n}}\n')
        start = self.size()
        (IO / f"go_{n}").touch()
        out, errors, pos, t0, running = [], [], start, time.time(), False
        while time.time() - t0 < timeout:
            pos, ls = self.lines(pos)
            for l in ls:
                if l.startswith(f"OHPL|exec|{n}|"):
                    running = True
                elif l.startswith(f"OHPL|out|{n}|"):
                    out.append(l.split("|", 3)[3])
                elif l.startswith(f"OHPL|done|{n}"):
                    if errors:
                        raise ExecError("\n".join(errors), out)
                    return out
                elif running and not l.startswith("OHPL|") and "ERROR" in l:
                    errors.append(l)
            if not game_pid():
                raise ExecError("reference game exited", out)
            time.sleep(0.05)
        raise ExecError("timeout" + ("\n" + "\n".join(errors) if errors else ""), out)

    def query(self, code, timeout=30):
        return dict(l.split("=", 1) for l in self.exec(code, timeout) if "=" in l)


class ExecError(RuntimeError):
    def __init__(self, msg, out):
        super().__init__(msg)
        self.out = out


SNIPPETS = {
    "ping": '__print("map=" + cLux_GetCurrentMap().GetName()); __print("time=" + cLux_GetGameTime()); '
            '__print("fps=" + cEngine_GetFPS());',
    "perf": '__print("fps=" + cEngine_GetFPS()); __print("frame_ms=" + cEngine_GetAvgFrameTimeInMS()); '
            '__print("render_ms=" + cEngine_GetAvgRenderFrameTimeMS()); __print("logic_ms=" + cEngine_GetAvgLogicFrameTimeMS()); '
            '__print("min_ms=" + cEngine_GetMinMS()); __print("max_ms=" + cEngine_GetMaxMS());',
    "player": 'cLuxPlayer@ p = cLux_GetPlayer(); iCharacterBody@ b = p.GetCharacterBody(); cVector3f v = b.GetFeetPosition(); '
              'cCamera@ c = p.GetCamera(); cVector3f cp = c.GetPosition(); '
              '__print("feet=" + v.x + " " + v.y + " " + v.z); __print("camera=" + cp.x + " " + cp.y + " " + cp.z); '
              '__print("yaw=" + cMath_ToDeg(c.GetYaw())); __print("pitch=" + cMath_ToDeg(c.GetPitch())); '
              '__print("state=" + p.GetCurrentStateName()); __print("move_state=" + p.GetCurrentMoveStateName()); '
              '__print("active=" + p.IsActive());',
}


def teleport_code(x, y, z, yaw=None, pitch=None):
    code = f"cLuxPlayer@ p = cLux_GetPlayer(); iCharacterBody@ b = p.GetCharacterBody(); b.SetFeetPosition(cVector3f({x}, {y}, {z}), false);"
    if yaw is not None:
        code += f" b.SetYaw(cMath_ToRad({yaw})); p.GetCamera().SetYaw(cMath_ToRad({yaw}));"
    if pitch is not None:
        code += f" b.SetPitch(cMath_ToRad({pitch})); p.GetCamera().SetPitch(cMath_ToRad({pitch}));"
    return code


def sway_env():
    st = json.loads((Path(os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")) / "gt/state.json").read_text())
    return dict(os.environ, WAYLAND_DISPLAY=st["wayland"], SWAYSOCK=st["swaysock"], DISPLAY="")


def swaymsg(*a):
    return json.loads(subprocess.run(["swaymsg", "-r", *a], env=sway_env(), capture_output=True, text=True).stdout or "null")


def ref_output(size):
    """A dedicated scale-1 output at the game resolution, so screenshots are the game's pixels 1:1."""
    marker = Path(os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")) / "gt/ohpl-ref-output"
    names = [o["name"] for o in swaymsg("-t", "get_outputs")]
    name = marker.read_text().strip() if marker.exists() else ""
    if name not in names:
        swaymsg("create_output")
        name = next(n for n in (o["name"] for o in swaymsg("-t", "get_outputs")) if n not in names)
        marker.write_text(name)
    w, h = size.split("x")
    swaymsg("output", name, "resolution", f"{w}x{h}", "scale", "1", "position", "8000", "0")
    return name


def screenshot(out):
    name = (Path(os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")) / "gt/ohpl-ref-output").read_text().strip()
    subprocess.run(["grim", "-o", name, str(out)], env=sway_env(), check=True)
    return out


def record_frames(out, secs, fps, t0=None):
    """Screenshots at `fps` for `secs`; names carry ms since `t0` (launch)."""
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    t0 = t0 or time.time()
    name = (Path(os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")) / "gt/ohpl-ref-output").read_text().strip()
    env, end, frames = sway_env(), time.time() + secs, []
    while time.time() < end and game_pid():
        tick = time.time()
        f = out / f"{int((tick - t0) * 1000):06d}.png"
        subprocess.run(["grim", "-o", name, str(f)], env=env)
        frames.append(f)
        time.sleep(max(0, 1 / fps - (time.time() - tick)))
    return frames


def walk_windows(n):
    if n.get("pid"):
        yield n
    for c in n.get("nodes", []) + n.get("floating_nodes", []):
        yield from walk_windows(c)


def stop():
    if game_pid():
        gt("stop", SLUG, check=False)
        for _ in range(50):
            if not game_pid():
                break
            time.sleep(0.2)


def start(map_file="00_01_apartment.hpm", pos="PlayerStartArea_1", size="1280x720", timeout=300, hud=False, boot=False,
          record=None):
    if not map_file.endswith(".hpm"):
        map_file += ".hpm"
    stop()
    gt("up")
    out = ref_output(size)
    prev = next((o["name"] for o in swaymsg("-t", "get_outputs") if o.get("focused")), None)
    swaymsg("focus", "output", out)
    write_mod(map_file, pos, size, boot)
    for f in IO.glob("*"):
        f.unlink()
    if log_path().exists():
        log_path().unlink()
    args = ["run", SLUG, "--wine", str(ROOT / "soma/ref/wine-ro.sh")] + ([] if hud else ["--no-hud"])
    gt(*args, "--", "-mod", r"C:\ohpl\mod\entry.hpc")
    t0 = time.time()
    while not any((w.get("name") or "").startswith("SOMA") for w in walk_windows(swaymsg("-t", "get_tree"))):
        if time.time() - t0 > 60:
            raise SystemExit("no SOMA window after 60 s: " + gt("log", SLUG, "-n", "5", check=False))
        time.sleep(0.5)
    swaymsg('[title="^SOMA"]', "move", "container", "to", "output", out)
    swaymsg('[title="^SOMA"]', "fullscreen", "enable")
    if prev:
        swaymsg("focus", "output", prev)
    if record:
        return record_frames(*record, t0=t0)
    while not log_path().exists():
        if time.time() - t0 > 60:
            raise SystemExit("no hpl.log after 60 s: " + gt("log", SLUG, "-n", "5", check=False))
        time.sleep(0.5)
    r = Ref()
    line = r.wait_for(r"^OHPL\|map\|", timeout)
    print(f"pid {game_pid()} {line.split('|')[2]} in {time.time() - t0:.0f}s")
    return r


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = p.add_subparsers(dest="cmd", required=True)
    s = sp.add_parser("start")
    s.add_argument("--map", default="00_01_apartment.hpm")
    s.add_argument("--pos", default="PlayerStartArea_1")
    s.add_argument("--size", default="1280x720")
    s.add_argument("--hud", action="store_true")
    s.add_argument("--timeout", type=float, default=300)
    e = sp.add_parser("exec")
    e.add_argument("code", nargs="?")
    e.add_argument("-f")
    e.add_argument("--timeout", type=float, default=30)
    for name in SNIPPETS:
        sp.add_parser(name)
    sp.choices["perf"].add_argument("--secs", type=float, default=0)
    t = sp.add_parser("teleport")
    for a in "xyz":
        t.add_argument(a, type=float)
    t.add_argument("--yaw", type=float)
    t.add_argument("--pitch", type=float)
    sp.add_parser("shot").add_argument("out", nargs="?", default="ref.png")
    lg = sp.add_parser("log")
    lg.add_argument("regex", nargs="?")
    lg.add_argument("-n", type=int, default=40)
    sp.add_parser("stop")
    sp.add_parser("setup")
    a = p.parse_args()

    if a.cmd == "start":
        start(a.map, a.pos, a.size, a.timeout, a.hud)
    elif a.cmd == "stop":
        stop()
    elif a.cmd == "setup":
        write_mod("00_01_apartment.hpm", "PlayerStartArea_1", "1280x720")
    elif a.cmd == "log":
        ls = log_path().read_text(errors="replace").splitlines()
        ls = [l for l in ls if not a.regex or re.search(a.regex, l)]
        print("\n".join(ls[-a.n:]))
    elif a.cmd == "shot":
        print(screenshot(a.out))
    else:
        if not game_pid():
            sys.exit("reference game not running: soma-ref.py start")
        if a.cmd == "exec":
            code = Path(a.f).read_text() if a.f else a.code
            timeout = a.timeout
        elif a.cmd == "teleport":
            code, timeout = teleport_code(a.x, a.y, a.z, a.yaw, a.pitch), 30
        else:
            if a.cmd == "perf" and a.secs:
                time.sleep(a.secs)
            code, timeout = SNIPPETS[a.cmd], 30
        try:
            for l in Ref().exec(code, timeout):
                print(l)
        except ExecError as err:
            for l in err.out:
                print(l)
            sys.exit(f"error: {err}")


if __name__ == "__main__":
    main()
