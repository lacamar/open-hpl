A5 small props (Button, Photocell, LevelDoor, OilBarrel).
small_props.txt: seeded op/state traces, 8 seeds x 4 props (7200 lines), recorded from the pre-flip C++ composition.
Rust-only replay: cargo test luxprop::a5_tests. Re-record: A5_RECORD=1 cargo test a5_small_props_match_golden.
