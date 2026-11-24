#!/bin/sh
set -e
name=${1:?usage: golden-diff.sh bv|h10|h11|h12|h12b|h13|h14|h15|l4 <build-dir> [record]}
b=${2:?build dir}
mode=$3
here=$(cd "$(dirname "$0")" && pwd)
g=$here/../rust/tests/golden
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/$name-diff"
mkdir -p "$out"

root() { (eval "$(python3 "$here/ohpl_game.py" "$1")" && realpath "$ROOT" | tr -d "\n"); }
roots() {
	sep=
	for x; do
		printf '%s%s' "$sep" "$(root "$x")"
		sep=:
	done
}
same() {
	if cmp "$1" "$2"; then
		echo "$3"
	else
		diff "$1" "$2" | head -20
		exit 1
	fi
}

case $name in
bv)
	OHPL_BV_DUMP="$out/rust.txt" "$b/BvFrustumTests.bin.aarch64"
	cmp "$out/rust.txt" "$g/BV/dump.txt" && echo "equal to golden: $(wc -l < "$out/rust.txt") lines"
	;;
h10)
	export OHPL_H10_SEEDS=30
	OHPL_H10_DUMP="$out/rust.txt" "$b/SceneH10Tests.bin.aarch64"
	same "$out/rust.txt" "$g/H10/h10_scenes.txt" "equal to golden: $(grep -c '^o ' "$out/rust.txt") ops, $(grep -c '^c\?scene' "$out/rust.txt") scenes"
	;;
h11)
	"$g/gen.sh" "$b" "$g/H11/harness/h11_script.cpp" "$out/h11_script"
	"$g/H11/harness/run.sh" "$out/h11_script" "$out/run"
	;;
h12)
	export OHPL_H12_DIRS=$(roots amnesia rebirth bunker soma amfp)
	export OHPL_H12_FRAMES=24 OHPL_H12_EVERY=22 OHPL_H12_SIM_EVERY=6 OHPL_H12_ATTRS=1
	OHPL_H12_DUMP="$out/rust.txt" "$b/SceneH12Tests.bin.aarch64"
	sed 's|^emitter \([0-9]*\) .*|emitter \1|' "$out/rust.txt" > "$out/stripped.txt"
	if [ "$mode" = record ]; then
		cp "$out/stripped.txt" "$g/H12/ps_emitters.txt"
		echo "recorded $g/H12/ps_emitters.txt"
	else
		cmp "$out/stripped.txt" "$g/H12/ps_emitters.txt" && echo "golden equal"
	fi
	;;
h12b)
	export OHPL_H12B_DIRS=$(roots amnesia rebirth bunker soma amfp)
	export OHPL_H12B_FRAMES="${OHPL_H12B_FRAMES:-40}" OHPL_H12B_EVERY="${OHPL_H12B_EVERY:-3}"
	OHPL_H12B_DUMP="$out/rust.txt" "$b/SceneH12bTests.bin.aarch64"
	echo "$(grep -c '^dec ' "$out/rust.txt") decisions, $(grep -c '^ ps ' "$out/rust.txt") particle systems, $(grep -c '^trace ' "$out/rust.txt") traces"
	grep '^dec ' "$out/rust.txt" > "$out/dec.txt"
	if [ "$mode" = record ]; then
		cp "$out/dec.txt" "$g/H12/dec.txt"
		echo "recorded $g/H12/dec.txt"
	else
		cmp "$out/dec.txt" "$g/H12/dec.txt" && echo "golden equal"
	fi
	;;
h13)
	export OHPL_H13_DIRS=$(roots amnesia amfp)
	OHPL_H13_DUMP="$out/dump.txt" "$b/SceneH13Tests.bin.aarch64"
	command grep -a -v '^map ' "$out/dump.txt" > "$out/reg_plan.txt"
	command grep -a '^map ' "$out/dump.txt" > "$out/maps.txt"
	if [ "$mode" = record ]; then
		cp "$out/reg_plan.txt" "$out/maps.txt" "$g/H13/"
		exit 0
	fi
	cmp "$out/reg_plan.txt" "$g/H13/reg_plan.txt"
	cmp "$out/maps.txt" "$g/H13/maps.txt"
	;;
h14)
	OHPL_H14_DUMP="$out/rust.txt" "$b/PhysicsH14Tests.bin.aarch64"
	same "$g/H14/h14_scenes.txt" "$out/rust.txt" "identical: $(grep -c '^end' "$g/H14/h14_scenes.txt") scenes"
	;;
h15)
	list=
	for game in amnesia amfp rebirth bunker soma; do
		for f in "$(root "$game")"/*materials.cfg; do
			[ -f "$f" ] && list="${list:+$list:}$f"
		done
	done
	OHPL_H15_SURF="$list" OHPL_H15_OUT="$out/surface.txt" "$b/PhysicsH15Tests.bin.aarch64"
	OHPL_H15_DUMP="$out/scenes.txt" OHPL_H15_VERBOSE=1 "$b/PhysicsH15Tests.bin.aarch64"
	cmp "$out/surface.txt" "$g/H15/surface_parse.txt"
	same "$g/H15/h15_scenes.txt" "$out/scenes.txt" "identical: $(grep -c '^end' "$out/scenes.txt") scenes, $(wc -l < "$out/surface.txt") surface files"
	;;
l4)
	OHPL_L4_DUMP="$out/rust.txt" OHPL_L4_VERBOSE=1 "$b/PhysicsNewtonTests.bin.aarch64"
	same "$g/L4/newton_scenes.txt" "$out/rust.txt" "identical: $(grep -c '^end' "$g/L4/newton_scenes.txt") scenes"
	;;
*)
	echo "unknown: $name" >&2
	exit 2
	;;
esac
