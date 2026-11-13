A7 (cLuxMap ownership + area state blocks), behind OHPL_IMPL=rust (default stays the C++ containers/members).
a7_ops.txt: seeded traces (24 map scripts, 6 area scripts, 4 map-handler list scripts). Recorded from the std-container
originals (harness "cpp" side: std::list/multimap/vector with the pre-A7 algorithms and the ohpl_ga5_* helpers the shells call).
Compare: harness/compare.sh builds the harness against libohpl.a and checks cpp == rust (ohpl_a7_* through the C ABI) == a7_ops.txt.
Rust-only replay: cargo test luxmap::a7_tests. Re-record: A7_RECORD=1 harness/compare.sh (and cargo test with A7_RECORD=1 gives the same bytes).
Real maps: mapdump-compare.sh <amnesia|amfp> <build-dir> <work-dir> <map>... runs the game headless with OHPL_IMPL=cpp and rust,
OHPL_A7_MAPDUMP=<file> dumps every entity in list order (type/name/id/active/lookups, area save data) after load and after
120 frames (OHPL_A7_MAPDUMP_FRAMES) plus timers, use-item callbacks, start nodes, vars; mapdump-diff.py compares them with random
draws (loader/script RNG) masked and counted.
