#!/bin/bash
# run.sh [lo hi corpus]: run both harness binaries (new with OHPL_IMPL=rust) on the corpus and diff the per-step logs.
R=$(cd "$(dirname "$0")" && pwd)
A4=${A4:-$HOME/.cache/open-hpl/port-a4}
H=$A4/harness
cd "$H"
[ -f corpus.txt ] || zcat "$R/corpus.txt.gz" > corpus.txt
CORPUS=${3:-corpus.txt}
timeout 900 ./harness_orig "$CORPUS" ${1:-0} ${2:-600} > o.txt
OHPL_IMPL=rust timeout 900 ./harness_new "$CORPUS" ${1:-0} ${2:-600} > n.txt
echo "lines o=$(wc -l < o.txt) n=$(wc -l < n.txt) steps=$(grep -c '^S step' o.txt) cases=$(grep -c '^ENDCASE' o.txt)"
echo "differing lines: $(diff o.txt n.txt | grep -c '^<')"
