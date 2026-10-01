#!/usr/bin/env python3
"""Hear what open-hpl and the official game play, and compare them.

Ours plays into the `ohpl-ours` null sink (soma-run.sh, soma-sweep.py, soma-compare.py), the reference
into `claude-test` (gt). Both monitors are recorded at the same time.

  scripts/soma-audio.py record [--secs 10] [--only ours|ref] [--out DIR]     # -> DIR/{ours,ref}.wav
  scripts/soma-audio.py analyze A.wav [B.wav] [--out DIR]   # levels, timeline, bands, glitches, spectrograms
  scripts/soma-audio.py identify X.wav [--bank GLOB ...] [--files F ...] [-n 20]
                                                            # which game samples are in a recording, at what gain
  scripts/soma-audio.py playing                             # ours: live sound entries with volumes
  scripts/soma-audio.py menu [--secs 20]                    # boot both to the main menu, record, analyze
  scripts/soma-audio.py map --map M [--secs 15] [--keep]    # both in a map (soma-compare start), record, analyze
"""
import argparse, fnmatch, hashlib, importlib.util, json, os, subprocess, sys, time, wave
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from hpl_control import HplControl, OURS_SINK, muted_env  # noqa: E402

REF_SINK = "claude-test"
RATE = 48000
XDG_CACHE = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
SCRATCH = Path(os.environ.get("OPENHPL_SOMA_SCRATCH", XDG_CACHE / "open-hpl/soma-scratch"))
SAMPLES = SCRATCH / ".xdg/cache/open-hpl/soma/events-v4"
OUT = XDG_CACHE / "open-hpl/soma-audio"
DECODED = OUT / "decoded"
BANDS = [31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000]


def load_module(name, file):
    spec = importlib.util.spec_from_file_location(name, HERE / file)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def db(x):
    return 20 * np.log10(np.maximum(x, 1e-9))


def unmute_streams():
    """Stream volumes on the test sinks get restored/changed behind our back (seen at 0%)."""
    ids = {l.split("\t")[1]: l.split("\t")[0] for l in subprocess.run(["pactl", "list", "short", "sinks"], capture_output=True,
                                                                     text=True).stdout.splitlines()}
    want = {ids.get(OURS_SINK), ids.get(REF_SINK)} - {None}
    for l in subprocess.run(["pactl", "list", "short", "sink-inputs"], capture_output=True, text=True).stdout.splitlines():
        f = l.split("\t")
        if f[1] in want:
            subprocess.run(["pactl", "set-sink-input-volume", f[0], "100%"])
            subprocess.run(["pactl", "set-sink-input-mute", f[0], "0"])


def record(secs, out, only=None):
    muted_env()
    out.mkdir(parents=True, exist_ok=True)
    if only != "ours":
        # the reference mutes itself while its window is unfocused
        ref = load_module("soma_ref", "soma-ref.py")
        if ref.game_pid():
            ref.swaymsg('[title="^SOMA"]', "focus")
            time.sleep(0.5)
    unmute_streams()
    procs = {}
    for name, sink in (("ours", OURS_SINK), ("ref", REF_SINK)):
        if only and only != name:
            continue
        f = out / f"{name}.wav"
        procs[name] = (f, subprocess.Popen(["parecord", "-d", f"{sink}.monitor", "--file-format=wav", "--format=s16le",
                                            f"--rate={RATE}", "--channels=2", "--latency-msec=20", str(f)]))
    time.sleep(secs)
    for f, p in procs.values():
        p.terminate()
        p.wait()
    return {k: v[0] for k, v in procs.items()}


def read_wav(path):
    with wave.open(str(path)) as w:
        n, ch, rate = w.getnframes(), w.getnchannels(), w.getframerate()
        a = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float32) / 32768
    a = a.reshape(-1, ch)
    if rate != RATE:
        raise SystemExit(f"{path}: {rate} Hz, expected {RATE}")
    return a


def decode(path):
    """Any audio file -> mono float32 at RATE, cached."""
    path = Path(path)
    DECODED.mkdir(parents=True, exist_ok=True)
    key = DECODED / (hashlib.sha1(str(path.resolve()).encode()).hexdigest()[:16] + ".f32")
    if not key.exists() or key.stat().st_mtime < path.stat().st_mtime:
        raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-ac", "1", "-ar", str(RATE), "-f", "f32le", "-"],
                             capture_output=True).stdout
        key.write_bytes(raw)
    return np.fromfile(key, dtype=np.float32)


def band_levels(mono):
    spec = np.abs(np.fft.rfft(mono * np.hanning(len(mono)))) ** 2
    freqs = np.fft.rfftfreq(len(mono), 1 / RATE)
    total = spec.sum() + 1e-20
    return [10 * np.log10(spec[(freqs >= f / 2 ** .5) & (freqs < f * 2 ** .5)].sum() / total + 1e-12) for f in BANDS]


def stats(a, win=0.5):
    mono = a.mean(axis=1)
    n = int(win * RATE)
    frames = mono[: len(mono) // n * n].reshape(-1, n)
    rms_t = np.sqrt((frames ** 2).mean(axis=1))
    rms = float(np.sqrt((mono ** 2).mean()))
    # glitches: sample jumps far above what the local signal does
    d = np.abs(np.diff(mono))
    local = np.convolve(d, np.ones(480) / 480, mode="same") + 1e-4
    clicks = int(((d > 0.05) & (d > 12 * local)).sum())
    # spectral flatness per 50 ms: noise ~1, tones ~0
    m = 2400
    fr = mono[: len(mono) // m * m].reshape(-1, m)
    sp = np.abs(np.fft.rfft(fr * np.hanning(m), axis=1)) ** 2 + 1e-12
    flat = np.exp(np.log(sp).mean(axis=1)) / sp.mean(axis=1)
    loud = (fr ** 2).mean(axis=1) > 1e-6
    lr = np.sqrt((a ** 2).mean(axis=0))
    return {
        "secs": round(len(mono) / RATE, 2),
        "rms_db": round(float(db(rms)), 1),
        "peak_db": round(float(db(np.abs(a).max())), 1),
        "lr_balance_db": round(float(db(lr[0]) - db(lr[1])), 1) if a.shape[1] > 1 else 0,
        "silent_frac": round(float((rms_t < 10 ** (-60 / 20)).mean()), 2),
        "timeline_db": [round(float(x), 1) for x in db(rms_t)],
        "bands_db": dict(zip(map(str, BANDS), (round(float(x), 1) for x in band_levels(mono)))),
        "flatness": round(float(np.median(flat[loud])), 3) if loud.any() else None,
        "clicks": clicks,
    }


def spark(vals, lo=-70, hi=0):
    bars = " ▁▂▃▄▅▆▇█"
    return "".join(bars[int(np.clip((v - lo) / (hi - lo), 0, 1) * (len(bars) - 1))] for v in vals)


def spectrogram(wav, png, title):
    subprocess.run(["sox", str(wav), "-n", "remix", "1,2", "spectrogram", "-x", "1000", "-y", "400", "-z", "90",
                    "-t", title, "-o", str(png)], capture_output=True)


def analyze(files, out=None):
    res = {}
    for f in files:
        res[Path(f).stem] = s = stats(read_wav(f))
        print(f"{Path(f).stem:6} rms {s['rms_db']:6.1f} dB  peak {s['peak_db']:6.1f} dB  L-R {s['lr_balance_db']:+5.1f}  "
              f"flat {s['flatness']}  clicks {s['clicks']}  silent {s['silent_frac']:.0%}  ({s['secs']} s)")
        print(f"       {spark(s['timeline_db'])}")
    print("band   " + " ".join(f"{b:>6}" for b in map(str, BANDS)))
    for k, s in res.items():
        print(f"{k:6} " + " ".join(f"{v:6.1f}" for v in s["bands_db"].values()))
    if len(res) == 2:
        a, b = res.values()
        print(f"diff   rms {a['rms_db'] - b['rms_db']:+.1f} dB  " +
              " ".join(f"{x - y:+6.1f}" for x, y in zip(a["bands_db"].values(), b["bands_db"].values())))
    if out:
        out = Path(out)
        out.mkdir(parents=True, exist_ok=True)
        pngs = []
        for f in files:
            png = out / f"{Path(f).stem}_spec.png"
            spectrogram(f, png, Path(f).stem)
            pngs.append(png)
        if len(pngs) == 2:
            subprocess.run(["magick", *map(str, pngs), "-append", str(out / "spectrograms.png")], capture_output=True)
        (out / "analysis.json").write_text(json.dumps(res, indent=1))
        print(f"-> {out}")
    return res


def candidates(banks, files):
    out = [Path(f) for f in files or []]
    if banks:
        for f in sorted(SAMPLES.iterdir()):
            if any(fnmatch.fnmatch(f.name.split("__")[0].lower(), b.lower()) for b in banks):
                out.append(f)
    return out


def match(rec, sample):
    """Best alignment of sample within rec (or rec within sample): normalized correlation and gain of sample in rec."""
    if len(sample) < RATE // 20 or np.abs(sample).max() == 0:
        return 0, 0, 0
    x, y, swap = (rec, sample, False) if len(rec) >= len(sample) else (sample, rec, True)
    n = 1 << int(np.ceil(np.log2(len(x) + len(y))))
    c = np.fft.irfft(np.fft.rfft(x, n) * np.conj(np.fft.rfft(y, n)), n)[: len(x) - len(y) + 1]
    e = np.concatenate([[0], np.cumsum(x.astype(np.float64) ** 2)])
    ex = e[len(y):] - e[: len(x) - len(y) + 1]
    ey = float((y.astype(np.float64) ** 2).sum())
    ncc = c / np.sqrt(ex * ey + 1e-12)
    i = int(np.argmax(ncc))
    gain = c[i] / (ex[i] if swap else ey)
    return float(ncc[i]), float(gain), i / RATE


def identify(wav, cands, n=20):
    rec = read_wav(wav).mean(axis=1)
    rows = []
    t0 = time.time()
    for f in cands:
        s = decode(f)
        ncc, gain, at = match(rec, s)
        if ncc > 0.15:
            rows.append((ncc, f.name, gain, at))
    rows.sort(reverse=True)
    print(f"{len(cands)} candidates in {time.time() - t0:.0f}s")
    for ncc, name, gain, at in rows[:n]:
        print(f"{ncc:5.2f}  gain {db(abs(gain)):6.1f} dB  @{at:6.2f}s  {name}")
    return rows


def playing(sock, recent=0):
    with HplControl(str(sock), timeout=10) as h:
        r = h.send({"cmd": "sound_stats"})
    print(f"music {r.get('music') or '-'} vol {r.get('music_volume')}  listener {r.get('listener')}")
    rows = [l.split("|") for l in r.get("detail", "").splitlines() if l]
    print(f"{'vol':>6} {'mul':>5} {'dist':>6} {'min':>5} {'max':>5} L 3 {'t':>6}/{'len':<6} entry / file")
    for name, file, typ, vol, mul, loop, d3, dist, mn, mx, el, tot, paused in sorted(rows, key=lambda r: -float(r[3])):
        print(f"{float(vol):6.3f} {float(mul):5.2f} {float(dist):6.1f} {float(mn):5.1f} {float(mx):5.1f} {loop} {d3} "
              f"{float(el):6.1f}/{float(tot):<6.1f} {name} / {file}{' (paused)' if paused == '1' else ''}")
    events = [l for l in r.get("events", "").splitlines() if l]
    if events:
        print(f"\nevents ({len(events)})")
        for l in events:
            print("  " + l)
    if recent:
        now = int(r.get("now", 0))
        starts = [l.split("|") for l in r.get("recent", "").splitlines() if l][-recent:]
        print(f"\nrecent starts ({len(starts)})\n{'ago':>7} {'vol':>6} {'dist':>6} {'min':>5} {'max':>5} L 3 R name")
        for t, name, vol, loop, d3, rel, dist, mn, mx in starts:
            print(f"{(now - int(t)) / 1000:7.2f} {float(vol):6.3f} {float(dist):6.1f} {float(mn):5.1f} {float(mx):5.1f} {loop} {d3} {rel} {name}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = p.add_subparsers(dest="cmd", required=True)
    r = sp.add_parser("record")
    r.add_argument("--secs", type=float, default=10)
    r.add_argument("--only", choices=["ours", "ref"])
    r.add_argument("--out")
    a = sp.add_parser("analyze")
    a.add_argument("files", nargs="+")
    a.add_argument("--out")
    i = sp.add_parser("identify")
    i.add_argument("wav")
    i.add_argument("--bank", nargs="*")
    i.add_argument("--files", nargs="*")
    i.add_argument("-n", type=int, default=20)
    pl = sp.add_parser("playing")
    pl.add_argument("--socket")
    pl.add_argument("--recent", type=int, default=40)
    m = sp.add_parser("menu")
    m.add_argument("--secs", type=float, default=20)
    m.add_argument("--wait", type=float, default=40)
    m.add_argument("--only", choices=["ours", "ref"])
    m.add_argument("--out")
    m.add_argument("--keep", action="store_true", help="leave both games running")
    mp = sp.add_parser("map")
    mp.add_argument("--map", default="00_01_apartment")
    mp.add_argument("--secs", type=float, default=15)
    mp.add_argument("--only", choices=["ours", "ref"])
    mp.add_argument("--out")
    mp.add_argument("--keep", action="store_true", help="leave both games running")
    args = p.parse_args()

    if args.cmd == "record":
        out = Path(args.out or OUT / "rec")
        analyze(list(record(args.secs, out, args.only).values()), out)
    elif args.cmd == "analyze":
        analyze(args.files, args.out)
    elif args.cmd == "identify":
        identify(args.wav, candidates(args.bank, args.files), args.n)
    elif args.cmd == "playing":
        cmp = load_module("soma_compare", "soma-compare.py")
        playing(args.socket or cmp.SOCK, args.recent)
    elif args.cmd == "menu":
        cmp = load_module("soma_compare", "soma-compare.py")
        out = Path(args.out or OUT / "menu")
        if args.only != "ours":
            ref = cmp.ref_mod
            ref.stop()
            ref.start(boot=True, record=(out / "ref-frames", args.wait, 0.5))
        if args.only != "ref":
            o = cmp.Ours()
            o.record_boot(out / "ours-frames", args.wait, 0.5)
        print(f"recording {args.secs:.0f}s")
        try:
            analyze(list(record(args.secs, out, args.only).values()), out)
        finally:
            if not args.keep:
                for t in cmp.targets(args.only):
                    t.stop()
    elif args.cmd == "map":
        cmp = load_module("soma_compare", "soma-compare.py")
        out = Path(args.out or OUT / args.map)
        subprocess.run([str(HERE / "soma-compare.py")] + (["--only", args.only] if args.only else []) + ["start", "--map", args.map],
                       check=True)
        print(f"recording {args.secs:.0f}s")
        try:
            analyze(list(record(args.secs, out, args.only).values()), out)
        finally:
            if not args.keep:
                for t in cmp.targets(args.only):
                    t.stop()


if __name__ == "__main__":
    main()
