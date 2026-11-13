#!/usr/bin/env python3
"""Generates the headless-control scripts (hc_*.script) for h22_hc / the cargo replay.
usage: gen_hc.py <out dir> [fuzz seeds]"""
import json
import os
import random
import sys


def esc(raw: bytes) -> str:
    out = []
    for b in raw:
        if b == 0x5C:
            out.append("\\\\")
        elif b == 0x0A:
            out.append("\\n")
        elif b < 0x20 or b >= 0x7F:
            out.append("\\x%02x" % b)
        else:
            out.append(chr(b))
    return "".join(out)


class Script:
    def __init__(self):
        self.lines = []

    def send(self, *reqs, pump=1):
        if reqs:
            self.lines.append("> " + "".join(esc(r if isinstance(r, bytes) else r.encode()) + "\\n" for r in reqs))
            self.lines.append("settle")
        self.lines.append("pump %d" % pump)

    def raw(self, data: bytes):
        self.lines.append(">> " + esc(data))
        self.lines.append("settle")

    def add(self, line):
        self.lines.append(line)

    def write(self, path):
        with open(path, "w") as f:
            f.write("\n".join(self.lines) + "\n")


def j(**kw):
    return json.dumps(kw, separators=(",", ":"))


def table():
    s = Script()
    s.add("log first line")
    s.add("log second \\nwith newline")
    s.add('log tab\\x09and "quote" and \\\\ backslash')
    s.add("log \\xc3\\xa9 utf8")
    s.send(j(cmd="ping"))
    for n in (None, 0, 1, 2, 3, 10, -1, 1000):
        s.send(j(cmd="log_tail") if n is None else j(cmd="log_tail", lines=n))
    s.send('{"cmd":"log_tail","lines":"2"}', '{"cmd":"log_tail","lines":"abc"}', '{"cmd":"log_tail","lines":2.9}')
    for i in range(520):
        s.add("log bulk %d" % i)
    s.send(j(cmd="log_tail", lines=4), j(cmd="log_tail", lines=600))
    for knobs in ("copy_ok 1", "gamma100 100", "gamma100 220", "gamma100 0", "gamma100 -5", "gamma100 100"):
        s.add("set " + knobs)
        s.send(j(cmd="screenshot"), j(cmd="screenshot", path="/does/not/exist/x.bmp"), j(cmd="screenshot", path="out.bmp"),
               j(cmd="screenshot", path=""), j(cmd="screenshot", path="q\"uote.bmp"))
    s.add("set save_ok 0")
    s.send(j(cmd="screenshot"), j(cmd="screenshot", path="/x/y.bmp"), j(cmd="screenshot", path="/x/y.unknownext"))
    s.add("set save_ok 1")
    s.add("set copy_ok 0")
    s.send(j(cmd="screenshot"), j(cmd="frame_stats"), j(cmd="screenshot", path="/a/b.bmp"))
    s.add("set copy_ok 1")
    s.send(j(cmd="frame_stats"), j(cmd="screen_stats"))
    s.add("set float_ok 0")
    s.send(j(cmd="screen_stats"))
    s.add("set float_ok 1")
    s.send(j(cmd="dev_hud"), j(cmd="dev_hud", value=True), j(cmd="dev_hud"), j(cmd="dev_hud", value=False), j(cmd="dev_hud"),
           '{"cmd":"dev_hud","value":"1"}', '{"cmd":"dev_hud","value":"true"}', '{"cmd":"dev_hud","value":"TRUE"}',
           '{"cmd":"dev_hud","value":"0"}', '{"cmd":"dev_hud","value":"yes"}', '{"cmd":"dev_hud","value":""}', j(cmd="dev_hud"))
    s.send(j(cmd="set_focus_wait", enabled=True), j(cmd="set_focus_wait", enabled=False), j(cmd="set_focus_wait"),
           '{"cmd":"set_focus_wait","enabled":"1"}')
    s.add("shader vs_a vertex 1 compiled fine")
    s.add("shader fs_b fragment 0 error: 0:1 \\\"bad\\\"\\nline2")
    s.add("shader p_c program 0 link failed")
    s.add("shader \\xc3\\xa9name fragment 1 ok")
    s.send(j(cmd="shader_report"), j(cmd="shader_report", failed_only=False), j(cmd="shader_report", failed_only=True),
           '{"cmd":"shader_report","failed_only":"0"}', '{"cmd":"shader_report","failed_only":"false"}')
    for w, h in ((800, 600), (0, 600), (800, 0), (-1, 5), (5, -1), (1920, 1080), (None, None)):
        r = {"cmd": "resize"}
        if w is not None:
            r["width"], r["height"] = w, h
        s.send(json.dumps(r, separators=(",", ":")))
    s.add("set resize_ok 0")
    s.send(j(cmd="resize", width=640, height=480), '{"cmd":"resize","width":"64x","height":"48"}')
    s.add("set resize_ok 1")
    s.send(j(cmd="nope"), j(cmd=""), '{"x":1}', j(cmd="PING"), j(cmd="ping ", x=1), j(cmd="wait_framesX"), '{"cmd":"qu\\u0069t"}')
    s.send(j(cmd="quit"))
    s.send(j(cmd="ping"))
    return s


def inputs():
    s = Script()
    names = ["a", "b", "z", "A", "0", "9", "Return", "Space", "Escape", "Tab", "Left", "Right", "Up", "Down", "F1", "F12",
             "Left Shift", "Backspace", "nosuchkey", "", "RETURN", "return", "\\u00e9"]
    for nm in names:
        for act in (None, "down", "up", "tap", "other"):
            r = {"cmd": "input", "type": "key", "key": nm}
            if act:
                r["action"] = act
            s.send(json.dumps(r, separators=(",", ":")), pump=0)
        s.send(pump=1)
    s.send('{"cmd":"input","type":"key"}', '{"cmd":"input","type":"key","action":"tap"}')
    for x, y, xr, yr in ((0, 0, 0, 0), (10, 20, 3, -4), (-5, 7, 100000, -100000), (2147483647, -2147483648, 1, 1)):
        s.send(j(cmd="input", type="mouse_move", x=x, y=y, xrel=xr, yrel=yr))
    s.send('{"cmd":"input","type":"mouse_move","x":"12","y":"abc","xrel":"-3","yrel":"4.7"}')
    s.send(j(cmd="input", type="mouse_move", xrel=2, yrel=3, frames=4), pump=6)
    s.send(j(cmd="input", type="mouse_move", xrel=2, yrel=3, frames=0), pump=3)
    s.send(j(cmd="input", type="mouse_move", xrel=2, yrel=3, frames=-3), pump=3)
    for path in ("1,2;3,4;5,6", "1,2", "1,2;", "1,2;;3,4", " 7 , 8", "7 ,8", "7, 8;9,10", "x", "1,", ",2", "+5,-6;7,8", "99999999999,1;2,3",
                 "1,2;3,4;bad;5,6", "1,2 ;3,4", "1,2;  3,4", "-0,0", "1,2;3", "  ;1,2", "1,2;3,4;"):
        s.send(j(cmd="input", type="mouse_move", xrel=9, yrel=8, frames=2, path=path), pump=7)
    s.send(j(cmd="input", type="mouse_move", xrel=9, yrel=8, frames=3, path="4,4;5,5"), j(cmd="input", type="mouse_move", x=1, y=2), pump=6)
    texts = ["hello", "", "a" * 31, "a" * 32, "a" * 100, "é" * 20, "日本語テキストです" * 4, "x\ny", "\U0001f600" * 9, "tab\there",
             "é" * 15 + "a", "a" * 30 + "é", "a" * 29 + "日"]
    for t in texts:
        s.send(j(cmd="input", type="text", text=t))
    for b in (None, "left", "right", "middle", "x"):
        for act in (None, "down", "up", "other"):
            for xy in ((0, 0), (5, 6)):
                r = {"cmd": "input", "type": "mouse_button", "x": xy[0], "y": xy[1]}
                if b:
                    r["button"] = b
                if act:
                    r["action"] = act
                s.send(json.dumps(r, separators=(",", ":")), pump=0)
        s.send(pump=1)
    for st in ("lost", "gained", "", "other", "LOST"):
        s.send(j(cmd="input", type="focus", state=st))
    s.send(j(cmd="input", type="focus"))
    s.send(j(cmd="input", type="nope"), j(cmd="input"), j(cmd="input", type=""))
    return s


def waits():
    s = Script()
    s.send(j(cmd="wait_frames", n=3), pump=1)
    s.add("frames 1")
    s.add("pump 1")
    s.send(j(cmd="ping"))
    s.add("frames 5")
    s.add("pump 1")
    s.send(j(cmd="wait_frames"), j(cmd="wait_frames", n=0), j(cmd="wait_frames", n=1), j(cmd="wait_frames", n=2), pump=1)
    s.add("pump 2")
    s.add("frames 1")
    s.add("pump 1")
    s.add("frames 1")
    s.add("pump 1")
    s.send(j(cmd="wait_frames", n=1000, max_ms=0), j(cmd="wait_frames", n=1000, max_ms=-1), j(cmd="wait_frames", n=-1),
           j(cmd="wait_frames", n=-2147483648), j(cmd="wait_frames", n=5, max_ms=100000000), j(cmd="wait_frames", n="x"))
    s.add("frames 2")
    s.add("pump 1")
    s.add("frames 4")
    s.add("pump 1")
    s.add("frames 1")
    s.add("pump 1")
    s.add("loading 1")
    s.send(j(cmd="log_tail", lines=1), j(cmd="ping"), j(cmd="screenshot"), j(cmd="quit"), j(cmd="wait_frames", n=1), j(cmd="resize", width=2, height=2),
           j(cmd="screenshot", path="/z/q.bmp"), j(cmd="dev_hud"), j(cmd="ping"))
    s.add("pump 2")
    s.add("loading 0")
    s.add("pump 1")
    s.add("frames 2")
    s.add("pump 1")
    s.send(j(cmd="ping"))
    return s


def parsing():
    s = Script()
    lines = [
        '{"cmd":"ping"}', '  {  "cmd" : "ping"  }  ', '{"cmd":"ping",}', '{"cmd":"ping","cmd":"quit_not"}', '{"cmd":ping}', '{"cmd":"pi\\"ng"}',
        '{"cmd":"ping","a":"b\\nc\\td\\re\\\\f\\/g\\u00e9"}', '{"cmd":"ping","n":12,"b":true,"z":null,"f":1.5e3}', '{"cmd":"ping","k":{"x":1}}',
        'not json', '{', '}', '{}', '{"cmd"}', '{"cmd":}', '{"cmd":"ping"', '{"cmd":"ping"}}}', '[{"cmd":"ping"}]', '  ', '\\t{"cmd":"ping"}',
        '{"cmd" "ping"}', '{"":"x"}', '{"cmd":"ping","":"x"}', '{"cmd":"log_tail","lines":3 }', '{"cmd":"log_tail","lines" :3,}',
        '{"cmd":"log_tail" , "lines":"3"}', '{"cmd":"ping"}\r', '{"cmd":"input","type":"key","key":"a b"}',
        '{"cmd":"input","type":"text","text":"\\u00e9\\n\\t\\"x\\""}', '{"cmd":"input","type":"text","text":"abc\\\\"}', '{"cmd":"x\\xc3\\xa9"}',
        '{"cmd":"log_tail","lines":0x10}', '{"cmd":"log_tail","lines":  7  }', '{"cmd":"ping"}garbage', '{"cmd":"ping","a":"unterminated}',
        '{"cmd":"input","type":"text","text":"a\\u0000b"}', '{"cmd":"screenshot","path":"/tmp/a\\nb.bmp"}',
    ]
    for ln in lines:
        s.send(ln)
    s.raw(b'{"cmd":"pi')
    s.raw(b'ng"}\n{"cmd":"log_tail",')
    s.raw(b'"lines":1}\n\n\n{"cmd":"ping"}')
    s.add("pump 1")
    s.raw(b"\n")
    s.add("pump 1")
    s.raw(b'{"cmd":"ping"}\n' * 40)
    s.add("pump 1")
    s.raw(("{" + '"cmd":"ping","pad":"' + "x" * 6000 + '"}\n').encode())
    s.add("pump 1")
    s.send(j(cmd="ping"))
    return s


def fuzz(seed):
    r = random.Random(seed)
    s = Script()
    cmds = ["ping", "log_tail", "screenshot", "set_focus_wait", "input", "resize", "shader_report", "frame_stats", "screen_stats", "dev_hud",
            "wait_frames", "bogus", "quit"]
    types = ["key", "mouse_move", "text", "mouse_button", "focus", "x"]
    keys = ["a", "b", "Return", "Space", "Escape", "Left", "F5", "zzz", "Tab", "0"]
    vals = ["0", "1", "-1", "7", "true", "TRUE", "false", "abc", "", "3.5", "99999999999", "1,2;3,4", "lost", "gained", "left", "right", "up", "down",
            "tap", "x"]
    fields = ["n", "max_ms", "lines", "path", "enabled", "value", "failed_only", "width", "height", "type", "key", "action", "x", "y", "xrel", "yrel",
              "frames", "text", "button", "state"]
    for _ in range(r.randint(25, 60)):
        c = r.random()
        if c < 0.55:
            req = {"cmd": r.choice(cmds)}
            if req["cmd"] == "input":
                req["type"] = r.choice(types)
            for f in r.sample(fields, r.randint(0, 5)):
                v = r.choice(vals)
                if f == "key":
                    v = r.choice(keys)
                if f == "type":
                    v = r.choice(types)
                req[f] = (int(v) if v.lstrip("-").isdigit() and r.random() < 0.5 and len(v) < 10 else v)
            if req["cmd"] == "quit" and r.random() < 0.8:
                req["cmd"] = "ping"
            s.send(json.dumps(req, separators=(",", ":")), pump=r.choice([0, 1, 1, 2]))
        elif c < 0.65:
            s.add("frames %d" % r.randint(1, 3))
            s.add("pump %d" % r.randint(1, 2))
        elif c < 0.72:
            s.add("log fuzz %d" % r.randint(0, 99))
        elif c < 0.78:
            s.add("loading %d" % r.randint(0, 1))
            s.add("pump 1")
        elif c < 0.86:
            s.add("set %s %d" % (r.choice(["copy_ok", "save_ok", "resize_ok", "float_ok"]), r.randint(0, 1)))
        elif c < 0.9:
            s.add("shader f%d fragment %d log%d" % (r.randint(0, 9), r.randint(0, 1), r.randint(0, 9)))
        else:
            s.add("pump %d" % r.randint(1, 3))
    s.add("loading 0")
    s.add("pump 3")
    return s


def main():
    out = sys.argv[1]
    nfuzz = int(sys.argv[2]) if len(sys.argv) > 2 else 16
    os.makedirs(out, exist_ok=True)
    table().write(os.path.join(out, "hc_table.script"))
    inputs().write(os.path.join(out, "hc_input.script"))
    waits().write(os.path.join(out, "hc_wait.script"))
    parsing().write(os.path.join(out, "hc_parse.script"))
    for i in range(1, nfuzz + 1):
        fuzz(i).write(os.path.join(out, "hc_fuzz%02d.script" % i))


main()
