#!/usr/bin/env python3
"""Compare two scripts/a11a-inv-gui.sh output dirs: inventory save section must match exactly,
screenshots within run-to-run noise (normalised RMSE).
Usage: a11a-inv-compare.py <dirA> <dirB> [max-rmse]
       a11a-inv-compare.py --extract <dir>   (prints the inventory section of every step)"""
import re, subprocess, sys
from pathlib import Path


def inventory(p):
    t = p.read_text(errors="replace")
    m = re.search(r'\n([ \t]*)<class type="cLuxInventory_SaveData".*?\n\1</class>', t, re.S)
    return re.sub(r"(graphics/item/)+", "graphics/item/", m.group(0)) if m else None


if sys.argv[1] == "--extract":
    for s in sorted(Path(sys.argv[2]).glob("*.sav")):
        print(f"== {s.stem}\n{inventory(s)}")
    sys.exit(0)

a, b = Path(sys.argv[1]), Path(sys.argv[2])
limit = float(sys.argv[3]) if len(sys.argv) > 3 else 0.06
bad = 0
for s in sorted(a.glob("*.sav")):
    o = b / s.name
    ia, ib = inventory(s), inventory(o) if o.exists() else None
    same = ia is not None and ia == ib
    r = subprocess.run(["magick", "compare", "-metric", "RMSE", str(a / (s.stem + ".bmp")), str(b / (s.stem + ".bmp")), "null:"],
                       capture_output=True, text=True)
    m = re.search(r"\(([0-9.e-]+)\)", r.stderr)
    rmse = float(m.group(1)) if m else 1.0
    ok = same and rmse <= limit
    bad += not ok
    print(f"{s.stem:16} inventory {'same' if same else 'DIFF'}  rmse {rmse:.4f}  {'ok' if ok else 'FAIL'}")
print(f"steps {len(list(a.glob('*.sav')))} failed {bad}")
sys.exit(1 if bad else 0)
