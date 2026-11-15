A7 (cLuxMap ownership + area state blocks).
a7_ops.txt: seeded traces (24 map scripts, 6 area scripts, 4 map-handler list scripts), recorded from the std-container originals.
Check: harness/compare.sh builds the harness against libohpl.a and checks the ohpl_a7_* traces against a7_ops.txt.
Rust-only replay: cargo test luxmap::a7_tests. Re-record: A7_RECORD=1 harness/compare.sh (cargo test with A7_RECORD=1 gives the same bytes).
Real maps: mapdump-compare.sh <amnesia|amfp> <build-dir> <work-dir> <map>... runs the game headless with OHPL_A7_MAPDUMP=<file>
(every entity in list order: type/name/id/active/lookups, area save data, after load and after 120 frames, plus timers,
use-item callbacks, start nodes, vars) and compares it with mapdumps/<game>-<map>.txt.gz through mapdump-diff.py, which masks
random draws (loader/script RNG) and counts them. MAPDUMP_RECORD=1 rewrites the golden. The goldens were recorded with the
pre-flip OHPL_IMPL=cpp binary.
