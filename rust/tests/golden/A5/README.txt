A5 small props (Button, Photocell, LevelDoor, OilBarrel).
small_props.txt: seeded op/state traces of the reference chain (GA4b helpers composed as the pre-A5 C++ shells did), 8 seeds x 4 props.
Replay: cargo test luxprop::a5_tests (Rust only). Re-record: A5_RECORD=1 cargo test a5_small_props_match_golden.
