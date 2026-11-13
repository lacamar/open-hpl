A5 small props (Button, Photocell, LevelDoor, OilBarrel), behind OHPL_IMPL=rust (default stays the pre-A5 C++ shells).
small_props.txt: seeded op/state traces, 8 seeds x 4 props (7200 lines). Recorded from the C++ path: harness/a5_harness.cpp "cpp" mode composes the pre-A5 shell logic from the ohpl_ga4b_* helpers (shells need a full engine, so they are not instantiated).
Compare: harness/compare.sh builds the harness and checks cpp == rust (ohpl_a5_* through the C ABI) == small_props.txt byte for byte.
Rust-only replay: cargo test luxprop::a5_tests. Re-record the Rust side: A5_RECORD=1 cargo test a5_small_props_match_golden.
