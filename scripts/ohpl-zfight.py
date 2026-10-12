#!/usr/bin/env python3
# Z-fight finder: nudge a freecam by a fraction of a millimetre and flag pixels whose G-buffer
# albedo / lit / final colour jumps to a value absent from the 3x3 neighbourhood of the other frame.
# Usage: scripts/ohpl-zfight.py --map 01_02_upsilon_inside [--game soma] [--out DIR] [--pos x,y,z ...]
import argparse, json, math, os, subprocess, sys, time
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hpl_control import HplControl

ap = argparse.ArgumentParser()
ap.add_argument("--map", required=True)
ap.add_argument("--game", default=os.environ.get("OHPL_GAME", "soma"))
ap.add_argument("--out", default=".")
ap.add_argument("--pos", action="append", default=[], help="x,y,z viewpoint (default: light positions)")
ap.add_argument("--max-pos", type=int, default=16)
ap.add_argument("--yaws", type=int, default=6)
ap.add_argument("--nudge", type=float, default=0.0004)
ap.add_argument("--shadows", type=int, default=1)
args = ap.parse_args()

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sock = os.path.join(os.environ["XDG_RUNTIME_DIR"], "ohpl-zf.sock")
env = dict(os.environ, OHPL_GAME=args.game)
pid = int(subprocess.check_output([root + "/scripts/soma-run.sh", args.map + ".hpm", sock], env=env).split()[0])


def pfm(path):
	with open(path, "rb") as f:
		kind = f.readline().strip()
		w, h = map(int, f.readline().split())
		f.readline()
		a = np.frombuffer(f.read(), np.float32).reshape(h, w, 3 if kind == b"PF" else 1)[::-1]
	os.unlink(path)
	return a[:, :, :3]


def mismatch(a, b):
	# smallest difference to any 3x3 neighbour of the other frame
	e = np.full(a.shape[:2], np.inf, np.float32)
	for dy in (-1, 0, 1):
		for dx in (-1, 0, 1):
			s = np.roll(b, (dy, dx), (0, 1))
			e = np.minimum(e, (np.abs(a - s) / (a + s + 0.05)).max(2))
	e[[0, -1], :] = 0
	e[:, [0, -1]] = 0
	return e


try:
	for _ in range(120):
		try:
			hpl = HplControl(sock, timeout=120).connect()
			hpl.send({"cmd": "camera_state"})
			break
		except Exception:
			time.sleep(2)
	else:
		sys.exit("no socket")
	cmd = lambda c, **k: hpl.send(dict(cmd=c, **k))
	cmd("wait_frames", n=30)
	cmd("set_render_setting", name="shadows", value=args.shadows)
	cmd("resize", width=1280, height=720)

	pts = [tuple(map(float, p.split(","))) for p in args.pos]
	if not pts:
		for l in cmd("lights", n=400)["lights"]:
			p = tuple(l["pos"])
			if not l["visible"] or any(math.dist(p, q) < 4 for q in pts):
				continue
			pts.append(p)
		pts = pts[:args.max_pos]

	def shot(target, x, y, z, yaw, pitch):
		cmd("set_camera", x=x, y=y, z=z, pitch=pitch, yaw=yaw)
		cmd("wait_frames", n=3)
		if target == "final":
			path = os.path.join(args.out, "zf-tmp.png")
			cmd("screenshot", path=path)
			return np.asarray(Image.open(path).convert("RGB"), np.float32) / 255
		path = os.path.join(args.out, "zf-tmp.pfm")
		cmd("dump_target", target=target, path=path)
		return pfm(path)

	rows = []
	for (x, y, z) in pts:
		for i in range(args.yaws):
			yaw, pitch = i * 2 * math.pi / args.yaws, -0.3
			n = args.nudge
			res = {"pos": [round(x, 2), round(y, 2), round(z, 2)], "yaw": round(yaw, 3)}
			for name, target in (("albedo", 0), ("lit", 4), ("final", "final")):
				# nudged frame in the middle so a light that flickers once fails a == a2
				a = shot(target, x, y, z, yaw, pitch)
				b = shot(target, x + n, y + n, z + n, yaw, pitch)
				a2 = shot(target, x, y, z, yaw, pitch)
				bad = (mismatch(a, b) > 0.25) & (mismatch(b, a) > 0.25) & (mismatch(a, a2) < 0.05)
				res[name] = int(bad.sum())
				if res[name] > 40:
					ys, xs = np.nonzero(bad)
					res[name + "_box"] = [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())]
					img = (np.clip(np.concatenate([a, b], 1) / max(float(np.percentile(a, 99.5)), 1e-3), 0, 1) ** 0.45 * 255).astype(np.uint8)
					img[:, :1280][bad] = (255, 0, 255)
					ppm = os.path.join(args.out, "zf-%s-%d.ppm" % (name, len(rows)))
					with open(ppm, "wb") as f:
						f.write(b"P6\n%d %d\n255\n" % (img.shape[1], img.shape[0]) + img.tobytes())
					res[name + "_img"] = ppm
			rows.append(res)
			print(json.dumps(res), flush=True)
	rows.sort(key=lambda r: -(r["albedo"] + r["lit"] + r["final"]))
	json.dump(rows, open(os.path.join(args.out, "zfight.json"), "w"), indent=1)
finally:
	try:
		hpl.send({"cmd": "quit"})
	except Exception:
		pass
	time.sleep(2)
	try:
		os.kill(pid, 15)
	except ProcessLookupError:
		pass
