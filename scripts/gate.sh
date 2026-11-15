#!/usr/bin/env bash
# Port gate: build + ctest, map sweeps per backend (cpp/rust in parallel, separate scratch), scripted play,
# world dumps compared cpp vs rust and against a recorded C++ baseline, save round trips.
# Usage: scripts/gate.sh [--quick] [--impl cpp|rust|both] [--build-dir DIR] [--out DIR] [--no-build] [--no-saves]
#                        [--changed-since REV] [--jobs N] [--baseline DIR] [--record-baseline DIR]
#   --quick            map subset, no dumps, no play (agents' pre-report check); default: every SOMA/Rebirth/Bunker map
#   --impl             OHPL_IMPL values to run (default both)
#   --build-dir        default $OPENHPL_BUILD_DIR or amnesia/src/build of this checkout
#   --out              default $XDG_CACHE_HOME/open-hpl/gate/<checkout>-<time>; summary in <out>/summary.txt
#   --changed-since    only games whose code changed since REV (amnesia/amfp-only or soma-only changes); else all
#   --jobs N           games swept in parallel per backend (default 1; more is faster but adds load-induced hangs)
#   --baseline         recorded C++ dumps to compare the rust dumps with (default $XDG_CACHE_HOME/open-hpl/gate/baseline)
#   --record-baseline  record the C++ (OHPL_IMPL=cpp) dumps into DIR and stop; do it before flipping a family
#   --games "G ..."   only these games (default: all, or those --changed-since selects)
#   --scratch DIR      scratch root prefix (<DIR>-cpp, <DIR>-rust). Default: --quick keeps a per-checkout scratch under
#                      $XDG_CACHE_HOME/open-hpl/gate (warm shader/map caches, faster boots); full gates use a fresh one in --out
# Exit 0 only when every map passes (known failures excepted), every dump agrees and the round trips match.
set -uo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd); cd "$REPO"
CACHE=${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/gate
QUICK=; IMPLS="cpp rust"; BUILD=${OPENHPL_BUILD_DIR:-$REPO/amnesia/src/build}; OUT=; DO_BUILD=1; SAVES=1
SINCE=; ONLY=; JOBS=1; BASELINE=$CACHE/baseline; RECORD=; SCRATCH=
while [ $# -gt 0 ]; do
	case $1 in
	--quick) QUICK=1 ;;
	--impl) shift; [ "$1" = both ] && IMPLS="cpp rust" || IMPLS=$1 ;;
	--build-dir) shift; BUILD=$(realpath "$1") ;;
	--out) shift; OUT=$1 ;;
	--no-build) DO_BUILD= ;;
	--no-saves) SAVES= ;;
	--changed-since) shift; SINCE=$1 ;;
	--jobs) shift; JOBS=$1 ;;
	--baseline) shift; BASELINE=$1 ;;
	--record-baseline) shift; RECORD=$1 ;;
	--games) shift; ONLY=$1 ;;
	--scratch) shift; SCRATCH=$1 ;;
	*) echo "unknown option $1" >&2; exit 2 ;;
	esac; shift
done
[ -n "$RECORD" ] && { IMPLS=cpp; QUICK=; SAVES=; }
OUT=${OUT:-$CACHE/$(basename "$REPO")-$(date +%Y%m%d-%H%M%S)}
mkdir -p "$OUT"; OUT=$(realpath "$OUT")
if [ -z "$SCRATCH" ]; then
	[ -n "$QUICK" ] && SCRATCH=$CACHE/scratch-$(basename "$REPO") || SCRATCH=$OUT/scratch
fi
export OPENHPL_BUILD_DIR=$BUILD OPENHPL_HEADLESS_SLOTS=${OPENHPL_HEADLESS_SLOTS:-4}
export CCACHE_BASEDIR=${CCACHE_BASEDIR:-$REPO} CCACHE_NOHASHDIR=1
SUM=$OUT/summary.txt; : > "$SUM"
say() { echo "$*" | tee -a "$SUM"; }

if [ -n "$QUICK" ]; then
	SOMA_MAPS="00_01_apartment 01_02_upsilon_inside 02_05_theta_inside 03_02_omicron_inside 04_02_tau_inside"
	REBIRTH_MAPS="01_01_plane_wreckage 02_02_courtyard 03_05_vitae_station"
	BUNKER_MAPS="arena trenches"
else
	SOMA_MAPS=; REBIRTH_MAPS=; BUNKER_MAPS=   # empty = every map
fi
AMNESIA_MAPS="00_rainy_hall 01_cells 02_library 03_dungeon 04_final"
AMFP_MAPS="01_mansion_01 06_factory 10_reactor"
PLAY_MAPS="00_01_apartment 01_02_upsilon_inside 02_05_theta_inside"
DUMP_AMNESIA="00_rainy_hall 02_library 04_final"
KNOWN='rebirth:03_06_ghouls_nest.hpm:lum_mean'   # game:map:failure-prefix, space separated

GAMES="soma rebirth bunker amnesia amfp"
if [ -n "$SINCE" ]; then
	changed=$( { git diff --name-only "$SINCE"...HEAD; git diff --name-only HEAD; } | sort -u)
	if [ -z "$changed" ]; then
		GAMES=
	elif ! grep -qvE '^(amnesia/src/game/|amfp/|rust/src/lux|rust/tests/golden/A[0-9]|scripts/(a10b-scenario|gate).sh)' <<<"$changed"; then
		GAMES="amnesia amfp"
	elif ! grep -qvE '^(soma/|rust/src/soma|rust/tests/golden/S[0-9])' <<<"$changed"; then
		GAMES="soma rebirth bunker"
	fi
	say "changed since $SINCE: games ${GAMES:-none}"
fi
[ -n "$ONLY" ] && GAMES=$ONLY
has() { [[ " $GAMES " == *" $1 "* ]]; }

if [ -n "$DO_BUILD" ]; then
	make -C "$BUILD" -j"$(nproc)" Soma Amnesia Amfp Launcher PhysicsNewtonTests CStringTests PlatformXdgPathTests \
		HpslTranspilerTests GlslToVulkanTests TraceRecorderTests > "$OUT/build.log" 2>&1 \
		|| { say "BUILD FAILED"; grep -m10 -E 'error|Error' "$OUT/build.log" | tee -a "$SUM"; exit 1; }
	[ -n "$RECORD" ] || say "ctest: $(ctest --test-dir "$BUILD" 2>&1 | grep -E 'tests passed|tests failed' | tail -1)"
fi

for impl in $IMPLS; do
	for g in $GAMES; do
		OPENHPL_SCRATCH_ROOT=$SCRATCH-$impl scripts/soma-init.sh "$g" > /dev/null 2> "$OUT/init-$impl-$g.log" \
			|| { say "init $impl $g failed (see init-$impl-$g.log)"; exit 1; }
	done
done

# sweep <impl> <game> <out.json> [soma-sweep args...]; extra env via ENVX
sweep() {
	local impl=$1 g=$2 json=$3; shift 3
	env ${ENVX:-} OPENHPL_SCRATCH_ROOT="${SCR:-$SCRATCH-$impl}" OHPL_IMPL="$impl" OHPL_GAME="$g" \
		bash -c 'eval "$(scripts/soma-init.sh --env-only "$OHPL_GAME")" && exec scripts/soma-sweep.py --out "$0" "$@"' "$json" "$@" \
		>> "$OUT/sweep-$impl-$g.log" 2>&1
}
mapargs() { local a=(); for m in "$@"; do a+=(--map "$m"); done; echo "${a[@]}"; }
failing() {  # maps in <json> that are not ok
	python3 -c 'import json,sys
d=json.load(open(sys.argv[1])); m=d.get("maps",d)
for k,v in m.items():
    if isinstance(v,dict) and (v.get("status")!="ok" or v.get("failures")): print(k, v.get("status"), ",".join(v.get("failures") or []))' "$1" 2>/dev/null
}

game() {  # <impl> <game>: sweep, then one retry of failing maps (recorded, reported)
	local impl=$1 g=$2 extra= maps=
	[ "$impl" = rust ] && extra=OHPL_NO_GLEW=1
	case $g in soma) maps=$SOMA_MAPS ;; rebirth) maps=$REBIRTH_MAPS ;; bunker) maps=$BUNKER_MAPS ;;
		amnesia) maps=$AMNESIA_MAPS ;; amfp) maps=$AMFP_MAPS ;; esac
	local json=$OUT/res-$impl-$g.json
	# shellcheck disable=SC2046
	ENVX=$extra sweep "$impl" "$g" "$json" $(mapargs $maps)
	failing "$json" > "$OUT/first-fail-$impl-$g.txt"
	if [ -s "$OUT/first-fail-$impl-$g.txt" ]; then
		# --only-failed counts maps missing from --out as failed, so keep the same map filter
		# shellcheck disable=SC2046
		ENVX=$extra sweep "$impl" "$g" "$json" --only-failed $(mapargs $maps)
	fi
}

half() {
	local impl=$1 extra=
	[ "$impl" = rust ] && extra=OHPL_NO_GLEW=1
	if [ -z "$RECORD" ]; then
		local running=0
		for g in $GAMES; do
			game "$impl" "$g" &
			running=$((running + 1))
			[ "$running" -ge "$JOBS" ] && { wait -n; running=$((running - 1)); }
		done
		wait
	fi
	[ -n "$QUICK" ] && return
	for gm in soma:00_01_apartment rebirth:02_02_courtyard bunker:arena; do
		local G=${gm%%:*} M=${gm#*:}
		has "$G" || continue
		rm -f "$OUT/h4seq-$G-$impl.txt"
		ENVX="$extra OHPL_H4_SEQDUMP=$OUT/h4seq-$G-$impl.txt" sweep "$impl" "$G" "$OUT/dump-$impl-$G.json" --map "$M"
	done
	if has amnesia; then
		for M in $DUMP_AMNESIA; do
			rm -f "$OUT/h13-$M-$impl.txt" "$OUT/a7-$M-$impl.txt" "$OUT/a10a-$M-$impl.txt" "$OUT/a11a-$M-$impl.txt" "$OUT/a11b-$M-$impl.txt" "$OUT/a11c-$M-$impl.txt"
			ENVX="$extra OHPL_H13_MAPDUMP=$OUT/h13-$M-$impl.txt OHPL_A7_MAPDUMP=$OUT/a7-$M-$impl.txt OHPL_A10A_DUMP=$OUT/a10a-$M-$impl.txt OHPL_A11A_INVDUMP=$OUT/a11a-$M-$impl.txt OHPL_A11B_DUMP=$OUT/a11b-$M-$impl.txt OHPL_A11C_DUMP=$OUT/a11c-$M-$impl.txt" \
				sweep "$impl" amnesia "$OUT/dump-$impl-amnesia.json" --map "$M"
		done
	fi
	if has amnesia; then
		# the menu dump lists the profiles on disk, so it gets a scratch no sweep has touched
		rm -f "$OUT/a9-$impl.txt"; rm -rf "$SCRATCH-a9-$impl"
		OPENHPL_SCRATCH_ROOT=$SCRATCH-a9-$impl scripts/soma-init.sh amnesia > /dev/null 2> "$OUT/init-a9-$impl.log"
		SCR=$SCRATCH-a9-$impl ENVX="$extra OHPL_A9_MENUDUMP=$OUT/a9-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a9-$impl.json" --map 00_rainy_hall
	fi
	for g in amnesia amfp; do
		has "$g" || continue
		for mode in boot play; do
			[ "$g$mode" = amfpplay ] && continue
			rm -f "$OUT/a10b-$mode-$g-$impl.txt"
			env $extra OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" OHPL_PLAY_NAME="ohpl-a10b-$impl-$g" OHPL_GAME="$g" \
				scripts/a10b-scenario.sh "$mode" "$OUT/a10b-$mode-$g-$impl.txt" >> "$OUT/a10b-$impl-$g.log" 2>&1
		done
	done
	if [ -z "$RECORD" ] && has soma; then
		# shellcheck disable=SC2046
		ENVX=$extra sweep "$impl" soma "$OUT/play-$impl-soma.json" --play 10 $(mapargs $PLAY_MAPS)
	fi
}
for impl in $IMPLS; do half "$impl" & done; wait

if [ -n "$RECORD" ]; then
	mkdir -p "$RECORD"
	for f in "$OUT"/h4seq-*-cpp.txt "$OUT"/h13-*-cpp.txt "$OUT"/a7-*-cpp.txt "$OUT"/a10a-*-cpp.txt "$OUT"/a10b-*-cpp.txt "$OUT"/a11a-*-cpp.txt "$OUT"/a11b-*-cpp.txt "$OUT"/a11c-*-cpp.txt "$OUT"/a9-cpp.txt; do
		[ -s "$f" ] && cp "$f" "$RECORD/$(basename "$f" | sed 's/-cpp\.txt$/.txt/')"
	done
	{ echo "commit $(git rev-parse HEAD)"; echo "recorded $(date -Iseconds)"; ls "$RECORD"; } > "$RECORD/BASELINE"
	say "baseline recorded in $RECORD: $(ls "$RECORD" | grep -c '\.txt$') dumps"
	exit 0
fi

if [ -n "$SAVES" ]; then
	for impl in $IMPLS; do
		if has soma; then
			env OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" OHPL_PLAY_NAME="ohpl-gate-$$" \
				bash -c 'eval "$(scripts/soma-init.sh --env-only soma)" && timeout 900 scripts/soma-save-roundtrip.py 01_02_upsilon_inside' \
				> "$OUT/rt-soma-$impl.log" 2>&1
			sleep 15
		fi
		if has amnesia; then
			env OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" OHPL_PLAY_NAME="ohpl-gate-$$" \
				timeout 900 scripts/amnesia-save-roundtrip.sh > "$OUT/rt-amnesia-$impl.log" 2>&1
			sleep 5
		fi
	done
fi

if [ -n "$SAVES" ] && has amnesia; then
	INVIMPL=${IMPLS##* }
	env OPENHPL_SCRATCH_ROOT="$SCRATCH-$INVIMPL" OHPL_IMPL="$INVIMPL" OHPL_PLAY_NAME="ohpl-gate-inv-$$" \
		timeout 900 scripts/a11a-inv-roundtrip.sh "$BUILD/Amnesia.bin.aarch64" "$BUILD/Amnesia.bin.aarch64" "$OUT/inv-rt" > "$OUT/inv-rt.log" 2>&1
	if [ -d "$BASELINE/gui" ]; then
		env OPENHPL_SCRATCH_ROOT="$SCRATCH-$INVIMPL" OHPL_IMPL="$INVIMPL" OHPL_PLAY_NAME="ohpl-gate-inv-$$" \
			timeout 900 scripts/a11a-inv-gui.sh "$BUILD/Amnesia.bin.aarch64" "$OUT/inv-gui" > "$OUT/inv-gui.log" 2>&1
		scripts/a11a-inv-compare.py "$BASELINE/gui" "$OUT/inv-gui" 0.02 > "$OUT/inv-gui-cmp.log" 2>&1
	fi
fi

python3 - "$OUT" "$KNOWN" "$IMPLS" "${QUICK:-0}" "${SAVES:-0}" "$BASELINE" "$CACHE/flaky.tsv" "$GAMES" "$REPO" <<'EOF' | tee -a "$SUM"
import datetime, filecmp, json, os, re, subprocess, sys
out, known, impls, quick, saves, base, flaky, games, repo = sys.argv[1:]
known, impls, quick, saves, games = known.split(), impls.split(), quick == "1", saves == "1", games.split()
bad = 0

def rows(p):
    d = json.load(open(p)); m = d.get("maps", d)
    return {k: v for k, v in m.items() if isinstance(v, dict)}

def known_fail(g, m, fails):
    return bool(fails) and all(any(k == f"{g}:{m}:{f.split(':')[0]}" for k in known) for f in fails)

def judge(label, g, p):
    global bad
    if not os.path.exists(p):
        print(f"{label}: NO RESULTS"); bad += 1; return
    r = rows(p)
    fails = [(k, v.get("status"), v.get("failures") or []) for k, v in r.items() if v.get("status") != "ok" or v.get("failures")]
    unexpected = [f for f in fails if f[1] != "ok" or not known_fail(g, f[0], f[2])]
    bad += len(unexpected)
    line = f"{label}: {len(r) - len(fails)}/{len(r)} ok"
    line += "".join(f"\n  FAIL {k} {s} {','.join(fl)}" for k, s, fl in unexpected)
    if len(fails) > len(unexpected):
        line += f"  (known: {', '.join(f[0] for f in fails if f not in unexpected)})"
    print(line)

for impl in impls:
    for g in games:
        judge(f"{impl} {g}", g, os.path.join(out, f"res-{impl}-{g}.json"))
    if not quick and "soma" in games:
        judge(f"{impl} soma --play", "soma", os.path.join(out, f"play-{impl}-soma.json"))

# retries: a map that only passed on the second try is reported and logged; repeat offenders want a bead
history = []
if os.path.exists(flaky):
    history = [l.rstrip("\n").split("\t") for l in open(flaky) if l.strip()]
new = []
for impl in impls:
    for g in games:
        p, res = (os.path.join(out, f"{n}-{impl}-{g}{e}") for n, e in (("first-fail", ".txt"), ("res", ".json")))
        if not os.path.exists(p) or not os.path.exists(res):
            continue
        final = rows(res)
        for l in open(p):
            m, status, fl = (l.rstrip("\n").split(" ", 2) + ["", ""])[:3]
            first_fails = [f for f in fl.split(",") if f]
            if status == "ok" and known_fail(g, m, first_fails):
                continue
            v = final.get(m, {})
            if v.get("status") == "ok" and (not v.get("failures") or known_fail(g, m, v["failures"])):
                seen = 1 + sum(1 for h in history if h[1:4] == [impl, g, m])
                print(f"RETRIED {impl} {g} {m}: first {status} {fl} -> ok on retry (flaky {seen}x"
                      + (", file a bead)" if seen >= 2 else ")"))
                new.append([datetime.date.today().isoformat(), impl, g, m, f"{status} {fl}".strip()])
if new:
    os.makedirs(os.path.dirname(flaky), exist_ok=True)
    with open(flaky, "a") as f:
        for n in new:
            f.write("\t".join(n) + "\n")

def multiset(p):
    return sorted(re.sub(r"CombinedMesh\d+", "", l) for l in open(p, errors="replace"))

def same(name, a, b):
    if name.startswith("a10b-"):
        tool = os.path.join(repo, "rust/tests/golden/A10b/dump-diff.py")
        r = subprocess.run([sys.executable, tool, name.split("-")[1], a, b], capture_output=True, text=True)
        last = (r.stdout.strip().splitlines() or [""])[0]
        return (r.returncode == 0, last if r.returncode == 0 else f"DIFFER {last}")
    if name.startswith("a7-"):
        tool = os.path.join(repo, "rust/tests/golden/A7/mapdump-diff.py")
        r = subprocess.run([sys.executable, tool, a, b], capture_output=True, text=True)
        last = (r.stdout.strip().splitlines() or [""])[-1]
        return (r.returncode == 0, "equal after masking" if r.returncode == 0 else f"DIFFER {last}")
    if name.startswith(("a11b-", "a11c-")):
        # runs stop at different frames: compare up to the last whole frame of the shorter one
        la, lb = (open(x, errors="replace").read().splitlines() for x in (a, b))
        n = min(len(la), len(lb))
        k = max((i for i in range(n) if la[i].startswith("tick ")), default=0)
        ok_ = k > 100 and la[:k] == lb[:k]
        return (ok_, f"equal over {k} lines (run lengths {len(la)}/{len(lb)})" if ok_ else "DIFFER")
    if filecmp.cmp(a, b, shallow=False):
        return (True, "identical")
    if name.startswith("a10a-"):
        mask = lambda p: [re.sub(r"combo=-?\d+\[.*\]|text=\"[^\"]*\"", "", l) if "size=400,18" in l else l for l in open(p, errors="replace")]
        if mask(a) == mask(b):
            return (True, "identical (sound device names masked)")
    if name.startswith("h4seq") and multiset(a) == multiset(b):
        return (True, "same multiset (combine order is pointer-ordered in C++)")
    return (False, "DIFFER")

def ok(p):
    return os.path.exists(p) and os.path.getsize(p) > 0

if not quick:
    names = [f"h4seq-{g}" for g in ("soma", "rebirth", "bunker") if g in games]
    if "amnesia" in games:
        names += [f"{k}-{m}" for m in ("00_rainy_hall", "02_library", "04_final") for k in ("h13", "a7", "a10a", "a11a", "a11b", "a11c")] + ["a9"]
    names += [f"a10b-boot-{g}" for g in ("amnesia", "amfp") if g in games]
    if "amnesia" in games:
        names += ["a10b-play-amnesia"]
    has_base = os.path.isdir(base)
    if not has_base:
        print(f"no C++ baseline at {base} (record one with --record-baseline before flipping a family)")
    for name in names:
        if len(impls) == 2:
            a, b = (os.path.join(out, f"{name}-{i}.txt") for i in impls)
            if ok(a) and ok(b):
                s, why = same(name, a, b)
                print(f"{name} cpp vs rust: {why}"); bad += not s
            else:
                print(f"{name} cpp vs rust: MISSING DUMP"); bad += 1
        if has_base and "rust" in impls:
            ref, got = os.path.join(base, f"{name}.txt"), os.path.join(out, f"{name}-rust.txt")
            if not ok(ref):
                print(f"{name} rust vs baseline: no baseline dump")
            elif ok(got):
                s, why = same(name, ref, got)
                print(f"{name} rust vs baseline: {why}"); bad += not s
            else:
                print(f"{name} rust vs baseline: MISSING DUMP"); bad += 1

if saves:
    soma = {}
    for impl in impls:
        if "soma" in games:
            t = open(os.path.join(out, f"rt-soma-{impl}.log"), errors="replace").read()
            m = re.search(r"round-trip diffs: (\d+)", t)
            soma[impl] = sorted(l.split("|")[0] for l in t.splitlines() if l[:1] in "+-" and "|" in l)
            print(f"soma save round trip {impl}: {m.group(1) + ' diffs' if m else 'NO RESULT'}")
            bad += not m
        if "amnesia" in games:
            a = open(os.path.join(out, f"rt-amnesia-{impl}.log"), errors="replace").read()
            m = re.search(r"structure diffs A-B: (\d+)\s+B-C: (\d+)", a)
            print(f"amnesia save round trip {impl}: " + (f"A-B {m.group(1)} B-C {m.group(2)}" if m else "NO RESULT"))
            bad += not m or m.group(1) != "0" or m.group(2) != "0"
    if "amnesia" in games:
        for label, f, rx in (("inventory save round trip", "inv-rt.log", r"A==B: True\s+B==C: True"),
                             ("inventory GUI vs C++ baseline", "inv-gui-cmp.log", r"steps \d+ failed 0")):
            p = os.path.join(out, f)
            if f == "inv-gui-cmp.log" and not os.path.isdir(os.path.join(base, "gui")):
                print(f"{label}: no baseline gui dir in {base}")
                continue
            good = os.path.exists(p) and re.search(rx, open(p, errors="replace").read())
            print(f"{label}: " + ("ok" if good else "FAIL"))
            bad += not good
    if len(soma) == 2:
        same_e = soma[impls[0]] == soma[impls[1]]
        print("soma round-trip entities: " + ("same on both paths" if same_e else "DIFFER between paths"))
        bad += not same_e
print("GATE " + ("PASS" if bad == 0 else f"FAIL ({bad})"))
sys.exit(1 if bad else 0)
EOF
status=${PIPESTATUS[0]}
echo "summary: $SUM"
exit "$status"
